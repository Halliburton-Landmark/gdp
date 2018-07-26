/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  These are split off from gdp_api.c because they are used by
**  by the router.
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015-2017, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
**	----- END LICENSE BLOCK -----
*/

#include "gdp.h"

#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>

#include <mysql.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.name", "GDP name resolution and processing");


static const char	*_GdpNameRoot;		// root of names ("current directory")
static MYSQL		*NameDb;			// name mapping database
static const char	*DbTable;			// the name of the table with the mapping

/*
**  _GDP_NAME_INIT, _SHUTDOWN --- initialize/shut down name subsystem
*/

static void
_gdp_name_shutdown(void)
{
	mysql_close(NameDb);
	NameDb = NULL;
}

void
_gdp_name_init()
{
	// see if the user wants a root (namespace, "working directory", ...)
	if (_GdpNameRoot == NULL)
		_GdpNameRoot = getenv("GDP_NAME_ROOT");
	ep_dbg_cprintf(Dbg, 17, "_gdp_name_init: GDP_NAME_ROOT=%s\n",
			_GdpNameRoot == NULL ? "" : _GdpNameRoot);

	// open a connection to the external => internal mapping database
	const char *db_host = ep_adm_getstrparam("swarm.gdp.namedb.host",
											NULL);
	if (db_host == NULL)
	{
		ep_dbg_cprintf(Dbg, 1, "_gdp_name_init: no name database available\n");
		return;
	}
	unsigned int db_port = 0;		//TODO: should parse db_host for this

	const char *db_name = ep_adm_getstrparam("swarm.gdp.namedb.database",
											"gdp_names");
	const char *db_user = ep_adm_getstrparam("swarm.gdp.namedb.user",
											"anonymous");
	const char *db_passwd = ep_adm_getstrparam("swarm.gdp.namedb.passwd",
											"");
	unsigned long db_flags = 0;
	DbTable = ep_adm_getstrparam("swarm.gdp.namedb.table", "gdp_names");

	NameDb = mysql_init(NULL);
	if (NameDb == NULL)
	{
		ep_dbg_cprintf(Dbg, 1, "_gdp_name_init: mysql_init failure\n");
		return;
	}
	if (mysql_real_connect(NameDb, db_host, db_user, db_passwd,
							db_name, db_port, NULL, db_flags) == NULL)
	{
		ep_dbg_cprintf(Dbg, 1,
				"_gdp_name_init(%s@%s:%d db %s): cannot connect: %s\n",
				db_user, db_host, db_port, db_name,
				mysql_error(NameDb));
		mysql_close(NameDb);
		NameDb = NULL;
	}

	if (NameDb != NULL)
		atexit(_gdp_name_shutdown);
}


/*
**  GDP_PRINTABLE_NAME --- make a printable GCL name from an internal name
**
**		Returns the external name buffer for ease-of-use.
*/

char *
gdp_printable_name(const gdp_name_t internal, gdp_pname_t external)
{
	EP_STAT estat = ep_b64_encode(internal, sizeof (gdp_name_t),
							external, sizeof (gdp_pname_t),
							EP_B64_ENC_URL);

	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 2,
				"gdp_printable_name: ep_b64_encode failure\n"
				"\tstat = %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		strcpy(external, "(unknown)");
	}
	else if (EP_STAT_TO_INT(estat) != GDP_GOB_PNAME_LEN)
	{
		ep_dbg_cprintf(Dbg, 2,
				"gdp_printable_name: ep_b64_encode length failure (%d != %d)\n",
				EP_STAT_TO_INT(estat), GDP_GOB_PNAME_LEN);
	}
	return external;
}


/*
**  GDP_PRINT_NAME --- print a GDP name to a file
*/

void
gdp_print_name(const gdp_name_t name, FILE *fp)
{
	gdp_pname_t pname;

	if (!gdp_name_is_valid(name))
		fprintf(fp, "(none)");
	else
		fprintf(fp, "%s", gdp_printable_name(name, pname));
}


/*
**  Get/Set root of GDP names.
*/

EP_STAT
gdp_name_root_set(const char *root)
{
	_GdpNameRoot = root;
	return EP_STAT_OK;
}

const char *
gdp_name_root_get(void)
{
	return _GdpNameRoot;
}


/*
**	GDP_PARSE_NAME --- parse a (possibily human-friendly) GDP object name
**
**		An externally printable version of an internal name must be
**		exactly GDP_GOB_PNAME_LEN (43) characters long and contain only
**		valid URL-Base64 characters.  These are base64 decoded.
**		All other names are considered human-friendly and are
**		sha256-encoded to get the internal name.
*/

EP_STAT
gdp_parse_name(const char *xname, gdp_name_t gname)
{
	EP_STAT estat = GDP_STAT_NAME_UNKNOWN;
	char xnamebuf[256];			// for root-based extended name

	if (strlen(xname) == GDP_GOB_PNAME_LEN)
	{
		// see if this is a base64-encoded name
		estat = gdp_internal_name(xname, gname);
	}
	if (!EP_STAT_ISOK(estat))
	{
		// try to resolve the name using some resolution service
		estat = gdp_name_resolve(xname, gname);
	}
	if (!EP_STAT_ISOK(estat) && _GdpNameRoot != NULL)
	{
		// resolve using name resolution service but extended name
		snprintf(xnamebuf, sizeof xnamebuf, "%s.%s", _GdpNameRoot, xname);
		xname = xnamebuf;
		estat = gdp_name_resolve(xname, gname);
	}

	// following should be removed soon
	if (!EP_STAT_ISOK(estat))
	{
		// assume GDP name is just the hash of the external name
		ep_dbg_cprintf(Dbg, 8, "gdp_parse_name: using SHA256(%s)\n", xname);
		ep_crypto_md_sha256((const uint8_t *) xname, strlen(xname), gname);
		estat = EP_STAT_OK;
	}
	return estat;
}


/*
**	GDP_NAME_IS_VALID --- test whether a GDP object name is valid
**
**		Unfortunately, since SHA-256 is believed to be surjective
**		(that is, all values are possible), there is a slight
**		risk of a collision.
**
**		Arguably this should also check for `_GdpMyRoutingName`
**		and `RoutingLayerAddr`.
*/

bool
gdp_name_is_valid(const gdp_name_t name)
{
	const uint32_t *up;
	unsigned int i;

	up = (uint32_t *) name;
	for (i = 0; i < sizeof (gdp_name_t) / 4; i++)
		if (*up++ != 0)
			return true;
	return false;
}


/*
**  GDP_INTERNAL_NAME --- parse a string GDP name to internal representation
*/

EP_STAT
gdp_internal_name(const gdp_pname_t external, gdp_name_t internal)
{
	EP_STAT estat;

	if (strlen(external) != GDP_GOB_PNAME_LEN)
	{
		estat = GDP_STAT_GDP_NAME_INVALID;
	}
	else
	{
		estat = ep_b64_decode(external, sizeof (gdp_pname_t) - 1,
							internal, sizeof (gdp_name_t),
							EP_B64_ENC_URL);
	}

	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 2,
				"gdp_internal_name: ep_b64_decode failure\n"
				"\tname = %s\n"
				"\tstat = %s\n",
				external,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	else if (EP_STAT_TO_INT(estat) != sizeof (gdp_name_t))
	{
		ep_dbg_cprintf(Dbg, 2,
				"gdp_internal_name: ep_b64_decode length failure (%d != %zd)\n",
				EP_STAT_TO_INT(estat), sizeof (gdp_name_t));
		estat = EP_STAT_ABORT;
	}

	return estat;
}


/*
**  GDP_NAME_RESOLVE --- resolve an external name to an internal name
*/

EP_STAT
gdp_name_resolve(const char *xname, gdp_name_t gname)
{
	EP_STAT estat = EP_STAT_OK;
	const char *phase;

	ep_dbg_cprintf(Dbg, 19, "gdp_name_resolve(%s)\n", xname);

	if (NameDb == NULL)
	{
		ep_dbg_cprintf(Dbg, 19, "    ... no database\n");
		return GDP_STAT_NAME_UNKNOWN;
	}

	phase = "query";
	{
		int l = strlen(xname);
		char escaped[2 * l + 1];
		mysql_real_escape_string(NameDb, escaped, xname, l);
		const char *q = "SELECT gname FROM %s WHERE xname = '%s' LIMIT 1";
		char qbuf[strlen(q) + sizeof escaped + 1];
		snprintf(qbuf, sizeof qbuf, q, DbTable, escaped);
		if (mysql_query(NameDb, qbuf) != 0)
			goto fail1;
	}

	phase = "results";
	MYSQL_RES *res = mysql_store_result(NameDb);
	if (res == NULL)
		goto fail1;

	phase = "fetch";
	MYSQL_ROW row = mysql_fetch_row(res);
	if (row == NULL)
	{
		estat = GDP_STAT_NAME_UNKNOWN;
	}
	else
	{
		unsigned long *len = mysql_fetch_lengths(res);
		if (len[0] == sizeof (gdp_name_t))
		{
			memcpy(gname, row[0], sizeof (gdp_name_t));
		}
		else
		{
			ep_dbg_cprintf(Dbg, 1,
					"gdp_name_resolve(%s): field len %ld, should be %zd\n",
					xname, len[0], sizeof (gdp_name_t));
			estat = GDP_STAT_NAME_UNKNOWN;
		}
	}

	mysql_free_result(res);

	if (false)
	{
fail1:
		ep_dbg_cprintf(Dbg, 1,
				"gdp_name_resolve(%s): %s\n",
				phase, mysql_error(NameDb));
		if (EP_STAT_ISOK(estat))
			estat = GDP_STAT_MYSQL_ERROR;
	}
	if (ep_dbg_test(Dbg, 19))
	{
		char ebuf[100];
		ep_dbg_printf("gdp_name_resolve => %s\n",
			ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}
