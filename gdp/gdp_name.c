/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  These are split off from gdp_api.c because they are used by
**  by the router.
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015-2019, Regents of the University of California.
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
#include "gdp_priv.h"

#include <ep/ep_app.h>
#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>

#include <mysql.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.name", "GDP name resolution and processing");


static char			*_GdpNameRoot;		// root of names ("current directory")
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
_gdp_name_init(void)
{
	// see if the user wants a root (namespace, "working directory", ...)
	gdp_name_root_set(getenv("GDP_NAME_ROOT"));
	ep_dbg_cprintf(Dbg, 17, "_gdp_name_init: GDP_NAME_ROOT=%s\n",
			_GdpNameRoot == NULL ? "" : _GdpNameRoot);

	// open a connection to the external => internal mapping database
	const char *db_host = ep_adm_getstrparam("swarm.gdp.hongdb.host",
											GDP_DEFAULT_HONGDB_HOST);
	if (db_host == NULL)
	{
		ep_dbg_cprintf(Dbg, 1, "_gdp_name_init: no name database available\n");
		ep_app_error("Human-Oriented Name to GDPname Directory not configured");
		return;
	}
	unsigned int db_port = 0;		//TODO: should parse db_host for this

	const char *db_name = ep_adm_getstrparam("swarm.gdp.hongdb.database",
											"gdp_hongd");
	const char *db_user = ep_adm_getstrparam("swarm.gdp.hongdb.user",
											GDP_DEFAULT_HONGD_USER);
	const char *db_passwd = ep_adm_getstrparam("swarm.gdp.hongdb.passwd",
											GDP_DEFAULT_HONGD_PASSWD);
	unsigned long db_flags = 0;
	DbTable = ep_adm_getstrparam("swarm.gdp.hongdb.table", "human_to_gdp");

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
		ep_app_error("Cannot connect to %s@%s:%d db %s: %s",
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
	if (_GdpNameRoot != NULL)
		ep_mem_free(_GdpNameRoot);
	if (root == NULL || root[0] == '\0')
		_GdpNameRoot = NULL;
	else
		_GdpNameRoot = ep_mem_strdup(root);
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
**		looked up in the GDP name directory to get the internal name.
**
**		If the GDP_NAME_ROOT environment variable is set, then the
**		following algorithm is used:
**
**		(1) if the human name `hname` has a dot in it, try that name
**			first without extension.
**		(2) if not found, try GDP_NAME_ROOT.hname (whether or not there
**			is a dot in `hname`).
**		(3) if still not found and `hname` has no dot, try the unextended
**			`hname` anyway.
**
**		For a transition period, the SHA256 of the hname is tried,
**		either with or without extension depending on whether the
**		name has a dot in it.
**
**		A caller may optionally pass in an `xname` buffer pointer to
**		receive the name actually used after `GDP_NAME_ROOT`
**		extension.  It is up to the application to free that memory.
**
**		Returns a warning if the old back compatible algorithm (hash
**		the human name to get the internal name) was used.
*/

EP_STAT
gdp_name_parse(const char *hname, gdp_name_t gname, char **xnamep)
{
	EP_STAT estat = GDP_STAT_NAME_UNKNOWN;
	char *xname = NULL;
	int xnamelen;

	if (hname == NULL || hname[0] == '\0')
	{
		// no human name to parse
		estat = GDP_STAT_GDP_NAME_INVALID;
		goto done;
	}

	if (strlen(hname) == GDP_GOB_PNAME_LEN)
	{
		// see if this is a base64-encoded name
		estat = gdp_internal_name(hname, gname);
		if (EP_STAT_ISOK(estat))
		{
			estat = GDP_STAT_OK_NAME_PNAME;
			xname = ep_mem_strdup(hname);
			goto done;
		}
	}

	if (strchr(hname, '.') != NULL || _GdpNameRoot == NULL)
	{
		// have a dot or GDP_NAME_ROOT is not set
		estat = gdp_name_resolve(hname, gname);
		if (EP_STAT_ISOK(estat))
			goto done;
	}

	if (_GdpNameRoot != NULL)
	{
		// try the extended name
		xnamelen = strlen(_GdpNameRoot) + strlen(hname) + 2;
		xname = ep_mem_malloc(xnamelen);
		snprintf(xname, xnamelen, "%s.%s", _GdpNameRoot, hname);
		estat = gdp_name_resolve(xname, gname);
		if (EP_STAT_ISOK(estat))
			goto done;

		// final case, try the unextended, undotted hname
		if (strchr(hname, '.') == NULL)
		{
			estat = gdp_name_resolve(hname, gname);
			if (EP_STAT_ISOK(estat))
				goto done;
		}
	}

#if GDP_COMPAT_OLD_LOG_NAMES
	//XXX temporary: fall back to SHA-256(human_name)
	if (ep_adm_getboolparam("swarm.gdp.compat.lognames", true))
	{
		if (xname == NULL || strchr(hname, '.') != NULL)
			xname = ep_mem_strdup(hname);
		ep_dbg_cprintf(Dbg, 28, "gdp_name_parse: using SHA256(%s)\n", xname);
		ep_crypto_md_sha256(xname, strlen(xname), gname);
		estat = GDP_STAT_NAME_SHA;
	}
#endif
done:
	if (xnamep == NULL || EP_STAT_ISFAIL(estat))
	{
		if (xname != NULL)
			ep_mem_free(xname);
		xname = NULL;
	}
	if (xnamep != NULL)
		*xnamep = xname;

	if (ep_dbg_test(Dbg, 12))
	{
		gdp_pname_t pname;
		char ebuf[100];
		ep_dbg_printf("gdp_name_parse: %s", hname);
		if (xname != NULL && xname != hname)
			ep_dbg_printf(" => %s", xname);
		if (EP_STAT_ISOK(estat))
			ep_dbg_printf(" => %s\n", gdp_printable_name(gname, pname));
		else
			ep_dbg_printf(": %s\n", ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}

EP_STAT
gdp_parse_name(const char *hname, gdp_name_t gname)
{
	// back compat
	EP_STAT estat;
	estat = gdp_name_parse(hname, gname, NULL);
	if (EP_STAT_ISWARN(estat))
		estat = EP_STAT_OK;
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
**  Convert a base-64 encoded GDP name to internal representation.
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

		ep_dbg_cprintf(Dbg, 22,
				"gdp_internal_name: ep_b64_decode failure\n"
				"\tname = %s\n"
				"\tstat = %s\n",
				external,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	else if (EP_STAT_TO_INT(estat) != sizeof (gdp_name_t))
	{
		ep_dbg_cprintf(Dbg, 22,
				"gdp_internal_name: ep_b64_decode length failure (%d != %zd)\n",
				EP_STAT_TO_INT(estat), sizeof (gdp_name_t));
		estat = EP_STAT_ABORT;
	}

	return estat;
}


/*
**  GDP_NAME_RESOLVE --- resolve a human name to an internal name
**
**		This uses the external human-to-internal name database.
*/

EP_STAT
gdp_name_resolve(const char *hname, gdp_name_t gname)
{
	EP_STAT estat = GDP_STAT_OK_NAME_HONGD;
	const char *phase;

	ep_dbg_cprintf(Dbg, 19, "gdp_name_resolve(%s)\n", hname);

	if (NameDb == NULL)
	{
		ep_dbg_cprintf(Dbg, 19, "    ... no database\n");
		estat = GDP_STAT_NAME_UNKNOWN;
		goto fail0;
	}

	phase = "query";
	{
		int hname_len = strlen(hname);
		char escaped_hname[2 * hname_len + 1];
		mysql_real_escape_string(NameDb, escaped_hname, hname, hname_len);
		int dbtable_len = strlen(DbTable);
		char escaped_dbtable[2 * dbtable_len + 1];
		mysql_real_escape_string(NameDb, escaped_dbtable, DbTable, dbtable_len);
		const char *q = "SELECT gname FROM `%s` WHERE hname = '%s' LIMIT 1;";
		char qbuf[strlen(q) + sizeof escaped_dbtable + sizeof escaped_hname + 1];
		snprintf(qbuf, sizeof qbuf, q, escaped_dbtable, escaped_hname);
		if (mysql_query(NameDb, qbuf) != 0)
		{
			ep_dbg_cprintf(Dbg, 1, "MySQL >>> %s\n", qbuf);
			goto fail1;
		}
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
					hname, len[0], sizeof (gdp_name_t));
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
fail0:
	if (ep_dbg_test(Dbg, 19))
	{
		char ebuf[100];
		ep_dbg_printf("gdp_name_resolve => %s\n",
			ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}
