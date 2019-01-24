/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  GDP-NAME-ADD --- add a name to the external to internal name database.
**
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
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

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>	// cheat, for GDP_DEFAULT_HONGDB_HOST

#include <mysql.h>

#include <string.h>
#include <sysexits.h>


static EP_DBG	Dbg = EP_DBG_INIT("gdp-name-add", "Update name database app");

#define MILLISECONDS		* INT64_C(1000000)

static MYSQL		*NameDb;			// name mapping database

/*
**  This is essentially a copy of the standard startup routine but
**  with different database credentials.
*/

EP_STAT
name_init(const char *db_host, const char *db_user, char *db_passwd)
{
	EP_STAT estat = EP_STAT_OK;
	bool i_own_passwd = false;

	// open a connection to the external => internal mapping database
	if (db_host == NULL)
		db_host = ep_adm_getstrparam("swarm.gdp.hongdb.host",
								GDP_DEFAULT_HONGDB_HOST);
	if (db_host == NULL)
	{
		ep_app_error("No database name available; set swarm.gdp.hongdb.host");
		estat = GDP_STAT_NAK_SVCUNAVAIL;
		goto fail0;
	}
	unsigned int db_port = 0;		//TODO: should parse db_host for this

	const char *db_name = ep_adm_getstrparam("swarm.gdp.hongdb.database",
											"gdp_hongd");
	unsigned long db_flags = 0;

	NameDb = mysql_init(NULL);
	if (NameDb == NULL)
	{
		ep_dbg_cprintf(Dbg, 1, "name_init: mysql_init failure\n");
		goto fail1;
	}

	// read database password if we don't have one already
	if (db_passwd == NULL)
	{
		char prompt_buf[100];
		snprintf(prompt_buf, sizeof prompt_buf, "Password for %s: ", db_user);
		db_passwd = getpass(prompt_buf);
		i_own_passwd = true;
	}

	// attempt the actual connect and authentication
	ep_dbg_printf("host %s, user %s, passwd %s, name %s\n",
			db_host, db_user, db_passwd, db_name);
	if (mysql_real_connect(NameDb, db_host, db_user, db_passwd,
							db_name, db_port, NULL, db_flags) == NULL)
	{
		ep_app_error("Cannot connect to name database %s:\n    %s",
				db_name, mysql_error(NameDb));
		mysql_close(NameDb);
		NameDb = NULL;
		estat = GDP_STAT_NAK_SVCUNAVAIL;
		goto fail0;
	}
	if (false)
	{
fail1:
		if (!EP_STAT_ISOK(estat))
			estat = GDP_STAT_MYSQL_ERROR;
	}
fail0:
	// make sure password isn't visible to intruders
	if (db_passwd != NULL && i_own_passwd)
		memset(db_passwd, 0, strlen(db_passwd));
	return estat;
}


void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-D dbgspec] [-H db_host] [-p passwd] [-P pw_file]\n"
			"        [-q] [-u db_user] human_name gdp_name\n"
			"    -D  set debugging flags\n"
			"    -H  database host name (default: %s)\n"
			"    -p  database password\n"
			"    -P  name of file containing password\n"
			"    -q  suppress output (exit status only)\n"
			"    -u  database creation service user name (default: %s)\n"
			"human_name is the human-oriented log name\n"
			"gdp_name is the base-64 encoded 256-bit internal name\n",
			ep_app_getprogname(),
			ep_adm_getstrparam("swarm.gdp.hongdb.host",
								GDP_DEFAULT_HONGDB_HOST),
			ep_adm_getstrparam("swarm.gdp.creation-service.user",
								"gdp_creation_service"));
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	const char *hname;				// external name, human-oriented text
	const char *pname;				// internal name, base64-encoded
	gdp_name_t gname;				// internal name, binary
	const char *db_host = NULL;		// database host name
	const char *db_user = NULL;		// database user name
	char *db_passwd = NULL;			// associated password
	const char *db_passwd_file = NULL;	// file containing associated password
	bool quiet = false;
	int opt;
	EP_STAT estat = EP_STAT_OK;
	int xstat = EX_OK;				// exit status
	bool show_usage = false;
	const char *phase;

	// collect command-line arguments
	while ((opt = getopt(argc, argv, "D:H:p:P:qu:")) > 0)
	{
		switch (opt)
		{
		 case 'D':
			ep_dbg_set(optarg);
			break;

		 case 'H':
			db_host = optarg;
			break;

		 case 'p':
			db_passwd = optarg;
			break;

		 case 'P':
			db_passwd_file = optarg;
			break;

		 case 'q':
			quiet = true;
			break;

		 case 'u':
			db_user = optarg;
			break;

		 default:
			show_usage = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage || argc-- != 2)
		usage();

	hname = *argv++;
	pname = *argv++;

	// initialize the GDP library
	phase = "initialization";
	estat = gdp_init_phase_0(NULL, 0);
	if (!EP_STAT_ISOK(estat))
	{
		if (!quiet)
			ep_app_error("GDP Library Initialization failed");
		goto fail0;
	}

	if (db_passwd == NULL)
	{
		if (db_passwd_file == NULL)
			db_passwd_file = ep_adm_getstrparam(
								"swarm.gdp.creation-service.passwd-file",
								"/etc/gdp/creation_service_pw.txt");
		if (db_passwd_file != NULL)
		{
			FILE *pw_fp = fopen(db_passwd_file, "r");
			if (pw_fp != NULL)
			{
				char pbuf[128];

				if (fgets(pbuf, sizeof pbuf, pw_fp) != NULL && pbuf[0] != '\0')
					db_passwd = strdup(pbuf);
				fclose(pw_fp);
			}
		}
	}

	// translate GDP name from base64 to binary
	phase = "name translate";
	estat = gdp_internal_name(pname, gname);
	if (!EP_STAT_ISOK(estat))
	{
		if (!quiet)
		{
			ep_app_message(estat, "cannot decode %s", pname);
			fprintf(stderr, "\t(Should be base64-encoded log name)\n");
		}
		xstat = EX_DATAERR;
		goto fail0;
	}

	// figure out ultimate human name (perhaps using $GDP_NAME_ROOT)
	const char *root = getenv("GDP_NAME_ROOT");
	ep_dbg_cprintf(Dbg, 10, "GDP_NAME_ROOT=%s\n", root);
	if (root != NULL && root[0] != '\0' && strchr(hname, '.') == NULL)
	{
		int plen = strlen(root) + strlen(hname) + 2;
		char *p = ep_mem_malloc(plen);
		snprintf(p, plen, "%s.%s", root, hname);
		hname = p;
	}

	if (!quiet)
		ep_app_info("adding %s => %s", hname, pname);

	// open database connection
	phase = "database open";
	if (db_user == NULL)
		db_user = ep_adm_getstrparam("swarm.gdp.creation-service.user",
									"gdp_creation_service");
	estat = name_init(db_host, db_user, db_passwd);
	if (!EP_STAT_ISOK(estat))
	{
		if (!quiet)
			ep_app_message(estat, "cannot initialize name database (%s)",
					mysql_error(NameDb));
		xstat = EX_UNAVAILABLE;
		goto fail0;
	}

	// now attempt the actual database update
	phase = "update";
	{
		int l = strlen(hname);
		char xescaped[2 * l + 1];
		char gescaped[2 * sizeof (gdp_name_t) + 1];
		char qbuf[1024];

		if (ep_dbg_test(Dbg, 9))
		{
			gdp_pname_t pname;

			ep_dbg_printf("adding %s -> %s\n",
					hname, gdp_printable_name(gname, pname));
		}

		mysql_real_escape_string(NameDb, xescaped, hname, l);
		mysql_real_escape_string(NameDb, gescaped, (const char *) gname,
						sizeof (gdp_name_t));
		const char *q =
				"INSERT INTO human_to_gdp (hname, gname)\n"
				"        VALUES('%s', '%s')";
		snprintf(qbuf, sizeof qbuf, q, xescaped, gescaped);
		ep_dbg_cprintf(Dbg, 28, "    %s\n", qbuf);
		if (mysql_query(NameDb, qbuf) != 0)
		{
			if (!quiet)
				ep_app_error("INSERT failed: %s", mysql_error(NameDb));
			goto fail1;
		}
	}

	// cleanup and exit
	if (false)
	{
fail1:
		ep_dbg_cprintf(Dbg, 1,
				"MySQL failure (%s): %s\n",
				phase, mysql_error(NameDb));
		if (EP_STAT_ISOK(estat))
			estat = GDP_STAT_MYSQL_ERROR;
	}

fail0:
	if (NameDb != NULL)
		mysql_close(NameDb);
	NameDb = NULL;

	// OK status can have values; hide that from the user
	if (EP_STAT_ISOK(estat))
		estat = EP_STAT_OK;
	else if (xstat == EX_OK)
		xstat = EX_UNAVAILABLE;
	exit(xstat);
}
