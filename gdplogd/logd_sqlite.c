/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
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


/*
**  Implement GDP logs with SQLite.
**
**		Note: this file is "#include"d by apps/gdp-log-check.c with
**		the LOG_CHECK #define set.  Messy.
*/

#include "logd.h"
#include "logd_sqlite.h"

//#include <gdp/gdp_buf.h>
#include <gdp/gdp_gclmd.h>

//#include <ep/ep_hash.h>
//#include <ep/ep_hexdump.h>
//#include <ep/ep_log.h>
//#include <ep/ep_mem.h>
#include <ep/ep_net.h>
#include <ep/ep_string.h>
//#include <ep/ep_thr.h>

#include <dirent.h>
//#include <errno.h>
//#include <fcntl.h>
//#include <inttypes.h>
//#include <stdint.h>
//#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>


static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.sqlite", "GDP Log Daemon SQLite Physical Log");

#define GOB_PATH_MAX		200			// max length of pathname

static bool			SQLiteInitialized = false;
static const char	*LogDir;			// the gob data directory
static int			GOBfilemode;		// the file mode on create
static uint32_t		DefaultLogFlags;	// as indicated

#define GETPHYS(gob)	((gob)->x->physinfo)

#define FLAG_TMPFILE		0x00000001	// this is a temporary file


/*
**  The database schema for logs.
**
**		FIXME: This discards the "accuracy" field of the timestamp.
**		FIXME: Changing this would require breaking this into two
**				columns: ts_nsec INTEGER12 and ts_accuracy FLOAT4.
*/

static const char *LogSchema =
				"CREATE TABLE log_entry (\n"
				"	hash BLOB(32) PRIMARY KEY,\n"
				"	recno INTEGER,\n"
				"	timestamp INTEGER,\n"	// 64 bit, nanoseconds since 1/1/70
				"	prevhash BLOB(32),\n"
				"	value BLOB,\n"
				"	sig BLOB);\n"
				"CREATE INDEX recno_index\n"
				"	ON log_entry(recno);\n"
				"CREATE INDEX timestamp_index\n"
				"	ON log_entry(timestamp);\n";


/*
**  FSIZEOF --- return the size of a file
*/

#if 0		// currently unused
static off_t
fsizeof(FILE *fp)
{
	struct stat st;

	if (fstat(fileno(fp), &st) < 0)
	{
		char errnobuf[200];

		(void) (0 == strerror_r(errno, errnobuf, sizeof errnobuf));
		ep_dbg_cprintf(Dbg, 1, "fsizeof: fstat failure: %s\n", errnobuf);
		return -1;
	}

	return st.st_size;
}
#endif


/*
**  POSIX_ERROR --- flag error caused by a Posix (Unix) syscall
*/

static EP_STAT EP_TYPE_PRINTFLIKE(2, 3)
posix_error(int _errno, const char *fmt, ...)
{
	va_list ap;
	EP_STAT estat = ep_stat_from_errno(_errno);

	va_start(ap, fmt);
	if (EP_UT_BITSET(LOG_POSIX_ERRORS, DefaultLogFlags))
		ep_logv(estat, fmt, ap);
	else if (!SQLiteInitialized || ep_dbg_test(Dbg, 1))
		ep_app_messagev(estat, fmt, ap);
	va_end(ap);

	return estat;
}


/*
**  SQLITE_RC_SUCCESS --- return TRUE if response code is an OK value
*/

static bool
sqlite_rc_success(int rc)
{
	return rc == SQLITE_OK || rc == SQLITE_ROW || rc == SQLITE_DONE;
}

#define CHECK_RC(rc, action)	if (!sqlite_rc_success(rc)) action


// can we use SQLITE_STATIC for greater efficiency?
#define BLOB_DESTRUCTOR		SQLITE_TRANSIENT

/*
**  SQLITE_ERROR --- flag error caused by an SQLite error
*/

static EP_STAT
ep_stat_from_sqlite_result_code(int rc)
{
	int sev = EP_STAT_SEV_ERROR;
	int registry = EP_REGISTRY_USER;
	int module = SQLITE_MODULE;

	switch (rc & 0x00ff)
	{
		case SQLITE_OK:
			return EP_STAT_OK;

		case SQLITE_ROW:
		case SQLITE_DONE:
			sev = EP_STAT_SEV_OK;

		case SQLITE_ABORT:
			sev = EP_STAT_SEV_ABORT;
			break;

		case SQLITE_WARNING:
			sev = EP_STAT_SEV_WARN;
			break;
	}

	return EP_STAT_NEW(sev, registry, module, rc & 0x00ff);
}

static EP_STAT
sqlite_error(int rc, const char *where, const char *phase)
{
	if (!sqlite_rc_success(rc))
		ep_dbg_cprintf(Dbg, 1, "SQLite error (%s during %s): %s\n",
				where, phase, sqlite3_errstr(rc));
	return ep_stat_from_sqlite_result_code(rc);
}


static void
sqlite_logger(void *unused, int rc, const char *msg)
{
	ep_dbg_printf("SQLite error %d: %s\n", rc, msg);
}


/*
**  Initialize the physical I/O module
**
**		Note this is always called before threads have been spawned.
*/

static EP_STAT
sqlite_init(void)
{
	EP_STAT estat = EP_STAT_OK;

	// find physical location of GOB directory
	LogDir = ep_adm_getstrparam("swarm.gdplogd.log.dir", GDP_LOG_DIR);

	// we will run out of that directory
	if (chdir(LogDir) != 0)
	{
		estat = ep_stat_from_errno(errno);
		ep_app_message(estat, "sqlite_init: chdir(%s)", LogDir);
		return estat;
	}

	// find the file creation mode
	GOBfilemode = ep_adm_getintparam("swarm.gdplogd.gob.mode", 0600);

	if (ep_adm_getboolparam("swarm.gdplogd.sqlite.log-posix-errors", false))
		DefaultLogFlags |= LOG_POSIX_ERRORS;

	// arrange to log SQLite errors
	if (ep_adm_getboolparam("swarm.gdplogd.sqlite.log-sqlite-errors", true))
		sqlite3_config(SQLITE_CONFIG_LOG, sqlite_logger, NULL);

	sqlite3_config(SQLITE_CONFIG_SERIALIZED);	// fully threadable
	sqlite3_initialize();

	SQLiteInitialized = true;
	ep_dbg_cprintf(Dbg, 8, "sqlite_init: log dir = %s, mode = 0%o\n",
			LogDir, GOBfilemode);

	return estat;
}


/*
**  SQL_BIND_* --- GDP-specific type bindings
*/

static int
sql_bind_hash(sqlite3_stmt *stmt, int index, gdp_hash_t *hash)
{
	size_t hashlen;
	void *hashptr = gdp_hash_getptr(hash, &hashlen);
	int rc = sqlite3_bind_blob(stmt, index, hashptr, hashlen, BLOB_DESTRUCTOR);
	return rc;
}

static int
sql_bind_recno(sqlite3_stmt *stmt, int index, gdp_recno_t recno)
{
	int rc = sqlite3_bind_int64(stmt, index, recno);
	return rc;
}

static int
sql_bind_timestamp(sqlite3_stmt *stmt, int index, EP_TIME_SPEC *ts)
{
	int64_t int_time = ts->tv_sec * (10 ^ 9);
	int_time += ts->tv_nsec;
	int rc = sqlite3_bind_int64(stmt, index, int_time);
	return rc;
}

static int
sql_bind_signature(sqlite3_stmt *stmt, int index, gdp_datum_t *datum)
{
	//FIXME: need sigmdalg and siglen fields
	size_t siglen;
	void *sigptr = gdp_sig_getptr(datum->sig, &siglen);

	int rc = sqlite3_bind_blob(stmt, index, sigptr, siglen, BLOB_DESTRUCTOR);
	return rc;
}

static int
sql_bind_buf(sqlite3_stmt *stmt, int index, gdp_buf_t *buf)
{
	size_t buflen = gdp_buf_getlength(buf);
	int rc = sqlite3_bind_blob(stmt, index, gdp_buf_getptr(buf, buflen),
							buflen, BLOB_DESTRUCTOR);
	return rc;
}


/*
**	GET_LOG_PATH --- get the pathname to an on-disk version of the gob
*/

static EP_STAT
get_log_path(gdp_gob_t *gob,
		const char *sfx,
		char *pbuf,
		int pbufsiz)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_pname_t pname;
	int i;
	struct stat st;

	EP_ASSERT_POINTER_VALID(gob);

	errno = 0;
	gdp_printable_name(gob->name, pname);

	// find the subdirectory based on the first part of the name
	i = snprintf(pbuf, pbufsiz, "%s/_%02x", LogDir, gob->name[0]);
	if (i >= pbufsiz)
		goto fail1;
	if (stat(pbuf, &st) < 0)
	{
		// doesn't exist; we need to create it
		ep_dbg_cprintf(Dbg, 11, "get_log_path: creating %s\n", pbuf);
		i = mkdir(pbuf, 0775);
		if (i < 0)
			goto fail0;
	}
	else if ((st.st_mode & S_IFMT) != S_IFDIR)
	{
		errno = ENOTDIR;
		goto fail0;
	}

	// now return the final complete name
	i = snprintf(pbuf, pbufsiz, "%s/_%02x/%s%s",
				LogDir, gob->name[0], pname, sfx);
	if (i < pbufsiz)
		return EP_STAT_OK;

fail1:
	estat = EP_STAT_BUF_OVERFLOW;

fail0:
	{
		char ebuf[100];

		if (EP_STAT_ISOK(estat))
		{
			if (errno == 0)
				estat = EP_STAT_ERROR;
			else
				estat = ep_stat_from_errno(errno);
		}

		ep_dbg_cprintf(Dbg, 1, "get_log_path(%s):\n\t%s\n",
				pbuf, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**  Allocate/Free the in-memory version of the physical representation
**		of a GOB.
**
**		XXX Currently allocates space for the first segment.
**		XXX That should probably be deferred until it is actually
**			read off of disk.
*/

static gob_physinfo_t *
physinfo_alloc(gdp_gob_t *gob)
{
	gob_physinfo_t *phys = (gob_physinfo_t *) ep_mem_zalloc(sizeof *phys);

	if (ep_thr_rwlock_init(&phys->lock) != 0)
		goto fail1;

	return phys;

fail1:
	ep_mem_free(phys);
	return NULL;
}


static void
physinfo_free(gob_physinfo_t *phys)
{
	if (phys == NULL)
		return;

	if (phys->db != NULL)
	{
		int rc;

		ep_dbg_cprintf(Dbg, 41, "physinfo_free: closing db @ %p\n", phys->db);

		// clean up previously prepared statements
		if (phys->read_by_recno_stmt != NULL)
			sqlite3_finalize(phys->read_by_recno_stmt);
		if (phys->read_by_hash_stmt != NULL)
			sqlite3_finalize(phys->read_by_hash_stmt);
		if (phys->read_by_timestamp_stmt != NULL)
			sqlite3_finalize(phys->read_by_timestamp_stmt);
		if (phys->insert_stmt != NULL)
			sqlite3_finalize(phys->insert_stmt);

		// we can now close the database
		rc = sqlite3_close(phys->db);
		if (rc != 0)		//XXX
			(void) sqlite_error(rc, "physinfo_free", "cannot close db");
		phys->db = NULL;
	}

	if (ep_thr_rwlock_destroy(&phys->lock) != 0)
		(void) posix_error(errno, "physinfo_free: cannot destroy rwlock");

	ep_mem_free(phys);
}


static void
physinfo_dump(gob_physinfo_t *phys, FILE *fp)
{
	fprintf(fp, "physinfo @ %p: min_recno %" PRIgdp_recno
			", max_recno %" PRIgdp_recno "\n",
			phys, phys->min_recno, phys->max_recno);
	fprintf(fp, "\tdb %p, ver %d\n", phys->db, phys->ver);
}



/*
**  SQLITE_CREATE --- create a brand new GOB on disk
*/

static EP_STAT
sqlite_create(gdp_gob_t *gob, gdp_gclmd_t *gmd)
{
	EP_STAT estat = EP_STAT_OK;
	gob_physinfo_t *phys;
	const char *phase;
	int rc;

	EP_ASSERT_POINTER_VALID(gob);

	// allocate space for the physical information
	phys = physinfo_alloc(gob);
	if (phys == NULL)
		goto fail0;
	gob->x->physinfo = phys;

	// allocate a name
	if (!gdp_name_is_valid(gob->name))
	{
		estat = _gdp_gob_newname(gob);
		EP_STAT_CHECK(estat, goto fail0);
	}

	// create an empty file to hold the log database (this is required
	// to get the file mode right)
	char db_path[GOB_PATH_MAX];
	estat = get_log_path(gob, GLOG_SUFFIX, db_path, sizeof db_path);
	EP_STAT_CHECK(estat, goto fail0);

	ep_dbg_cprintf(Dbg, 20, "sqlite_create: creating %s\n", db_path);
	int fd = open(db_path, O_RDWR | O_CREAT | O_APPEND | O_EXCL,
					GOBfilemode);
	if (fd < 0)
	{
		char nbuf[40];

		estat = ep_stat_from_errno(errno);
		(void) (0 == strerror_r(errno, nbuf, sizeof nbuf));
		ep_log(estat, "sqlite_create(%s): %s", db_path, nbuf);
		goto fail0;
	}
	close(fd);

	// now open (and thus create) the actual database
	phase = "sqlite3_open_v2";
	rc = sqlite3_open_v2(db_path, &phys->db,
					SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	CHECK_RC(rc, goto fail1);

	phase = "sqlite3_extended_result_codes";
	rc = sqlite3_extended_result_codes(phys->db, 1);
	CHECK_RC(rc, goto fail1);

	// tweak database header using PRAGMAs
	//XXX could these be done with sqlite3_exec?
	phase = "pragma prepare";
	{
		// https://www.sqlite.org/pragma.html
		char qbuf[200];
		sqlite3_stmt *stmt;

		// set up application ID (the GDP itself)
		snprintf(qbuf, sizeof qbuf,
				"PRAGMA application_id = %d;\n", GLOG_MAGIC);
		rc = sqlite3_prepare_v2(phys->db, qbuf, -1, &stmt, NULL);
		CHECK_RC(rc, goto fail1);
		phase = "pragma step 1";
		while ((rc = sqlite3_step(stmt)) != SQLITE_DONE)
		{
			ep_dbg_cprintf(Dbg, 7, "create(%s), pragma setting => %s\n",
					db_path, sqlite3_errstr(rc));
		}
		sqlite3_finalize(stmt);

		// set up a version code (we use privately)
		snprintf(qbuf, sizeof qbuf,
				"PRAGMA user_version = %d;\n", GLOG_VERSION);

		rc = sqlite3_prepare_v2(phys->db, qbuf, -1, &stmt, NULL);
		CHECK_RC(rc, goto fail1);
		phase = "pragma step 2";
		while ((rc = sqlite3_step(stmt)) != SQLITE_DONE)
		{
			ep_dbg_cprintf(Dbg, 7, "create(%s), pragma setting => %s\n",
					db_path, sqlite3_errstr(rc));
		}
		sqlite3_finalize(stmt);
		phys->ver = GLOG_VERSION;
	}

	phase = "create schema";
	{
		// create the database schema: primary table and indices
		// https://www.sqlite.org/c3ref/exec.html
		// sqlite3_exec(db, cmd, callback, closure, *errmsg)
		rc = sqlite3_exec(phys->db, LogSchema, NULL, NULL, NULL);
		CHECK_RC(rc, goto fail1);
	}

	//XXX should probably use Write Ahead Logging
	// http://www.sqlite.org/wal.html

	// write metadata to log
	uint8_t *obuf;
	size_t mdsize = _gdp_gclmd_serialize(gmd, &obuf);
	if (obuf != NULL)
	{
		sqlite3_stmt *stmt;
		//FIXME: need values for hash, prevhash

		phase = "metadata prepare";
		rc = sqlite3_prepare_v2(phys->db,
					"INSERT INTO log_entry"
					"	(hash, recno, value)"
					"	VALUES (?, 0, ?);",
					-1, &stmt, NULL);
		CHECK_RC(rc, goto fail3);
		phase = "metadata bind 1";
		rc = sqlite3_bind_blob(stmt, 1, gob->name, sizeof gob->name,
							SQLITE_STATIC);
		CHECK_RC(rc, goto fail3);
		phase = "metadata bind 2";
		//rc = sqlite3_bind_blob(stmt, 2, TS, sizeof TS, BLOB_DESTRUCTOR);
		//CHECK_RC(rc, goto fail3);
		phase = "metadata bind 3";
		rc = sqlite3_bind_blob(stmt, 2, obuf, mdsize, ep_mem_free);
		CHECK_RC(rc, goto fail3);
		phase = "metadata step";
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
		{
fail3:
//FIXME: obuf doesn't get freed if one of the earlier binds fails first
			estat = ep_stat_from_sqlite_result_code(rc);
		}
		sqlite3_finalize(stmt);
		EP_STAT_CHECK(estat, goto fail1);
	}

	phys->min_recno = 1;
	phys->max_recno = 0;
	phys->flags |= DefaultLogFlags;
	ep_dbg_cprintf(Dbg, 11, "Created new GDP Log %s\n", gob->pname);
	return estat;

fail1:
	// failure resulted from an SQLite error
	estat = sqlite_error(rc, "sqlite_create", phase);

fail0:
	// turn OK into an errno-based code
	if (EP_STAT_ISOK(estat))
		estat = ep_stat_from_errno(errno);
	if (EP_STAT_ISOK(estat))
		estat = GDP_STAT_NAK_INTERNAL;

	// turn "file exists" into a meaningful response code
	if (EP_STAT_IS_SAME(estat, ep_stat_from_errno(EEXIST)))
			estat = GDP_STAT_NAK_CONFLICT;

	// free up resources
	if (phys != NULL)
	{
		physinfo_free(phys);
		gob->x->physinfo = phys = NULL;
	}

	if (ep_dbg_test(Dbg, 1))
	{
		char ebuf[100];

		ep_dbg_printf("Could not create GOB during %s: %s\n",
				phase, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**	SQLITE_OPEN --- do physical open of a GOB
*/

static EP_STAT
sqlite_open(gdp_gob_t *gob)
{
	EP_STAT estat = EP_STAT_OK;
	int rc = SQLITE_OK;
	gob_physinfo_t *phys;
	const char *phase;

	ep_dbg_cprintf(Dbg, 20, "sqlite_open(%s)\n", gob->pname);

	// allocate space for physical data
	EP_ASSERT(GETPHYS(gob) == NULL);
	phase = "physinfo_alloc";
	errno = 0;
	gob->x->physinfo = phys = physinfo_alloc(gob);
	if (phys == NULL)
		goto fail0;
	phys->flags |= DefaultLogFlags;

	/*
	** Time to open the database!
	*/

	phase = "get db path";
	char db_path[GOB_PATH_MAX];
	estat = get_log_path(gob, GLOG_SUFFIX, db_path, sizeof db_path);
	EP_STAT_CHECK(estat, goto fail1);

	phase = "sqlite3_open_v2";
	rc = sqlite3_open_v2(db_path, &phys->db, SQLITE_OPEN_READWRITE, NULL);
	if (rc != SQLITE_OK)
		goto fail1;

	phase = "sqlite3_extended_result_codes";
	rc = sqlite3_extended_result_codes(phys->db, 1);
	if (rc != SQLITE_OK)
		goto fail1;

	// verify that db is actually one we understand (application_id)
	// and it's the correct version (user_version)
	phase = "database verification";
	{
		sqlite3_stmt *stmt;
		rc = sqlite3_prepare_v2(phys->db,
					"SELECT * FROM pragma_application_id, pragma_user_version;",
					-1, &stmt, NULL);
		CHECK_RC(rc, goto fail1);
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_ROW)
		{
			sqlite3_finalize(stmt);
			goto fail1;
		}

		// check to make sure this is really a database we understand
		{
			int32_t magic = sqlite3_column_int(stmt, 0);
			if (magic != GLOG_MAGIC)
			{
				estat = GDP_STAT_CORRUPT_LOG;
				ep_log(estat, "database corruption error: unknown application_id %d expected %d",
						magic, GLOG_MAGIC);
				sqlite3_finalize(stmt);
				goto fail2;
			}
		}

		// check that we can understand this version of the dataase
		// (this may become more complex with time when we understand
		//		more version numbers)
		{
			int32_t vers = sqlite3_column_int(stmt, 1);
			if (vers != GLOG_VERSION)
			{
				estat = GDP_STAT_LOG_VERSION_MISMATCH;
				ep_log(estat, "unknown database version %d expected %d",
						vers, GLOG_VERSION);
				sqlite3_finalize(stmt);
				goto fail2;
			}
			phys->ver = vers;
		}
		sqlite3_finalize(stmt);
	}

	// read metadata
	phase = "metadata read";
	if (gob->gclmd == NULL)
	{
		sqlite3_stmt *stmt;
		rc = sqlite3_prepare_v2(phys->db,
						"SELECT value FROM log_entry"
						"	WHERE recno = 0",
						-1, &stmt, NULL);
		CHECK_RC(rc, goto fail2);
		rc = sqlite3_step(stmt);
		CHECK_RC(rc, goto fail2);
		const uint8_t *md_blob;
		size_t md_size = sqlite3_column_bytes(stmt, 0);
		md_blob = sqlite3_column_blob(stmt, 0);
		if (md_blob != NULL && md_size > 0)
		{
			gob->gclmd = _gdp_gclmd_deserialize(md_blob, md_size);
		}
		sqlite3_finalize(stmt);
	}

	if (ep_dbg_test(Dbg, 20))
	{
		ep_dbg_printf("sqlite_open => ");
		physinfo_dump(phys, ep_dbg_getfile());
	}
	return estat;

fail2:
	if (EP_STAT_ISOK(estat))
	{
		if (rc != SQLITE_OK)
			estat = ep_stat_from_sqlite_result_code(rc);
		else if (errno != 0)
			estat = ep_stat_from_errno(errno);
		else
			estat = GDP_STAT_NAK_INTERNAL;
	}
fail1:
	physinfo_free(phys);
	gob->x->physinfo = phys = NULL;

fail0:
	if (ep_dbg_test(Dbg, 9))
	{
		char ebuf[100];

		ep_dbg_printf("sqlite_open(%s): couldn't open GOB %s:\n\t%s\n",
				phase, gob->pname, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}

/*
**	SQLITE_CLOSE --- physically close an open GOB
*/

static EP_STAT
sqlite_close(gdp_gob_t *gob)
{
	EP_ASSERT_POINTER_VALID(gob);
	ep_dbg_cprintf(Dbg, 20, "sqlite_close(%s)\n", gob->pname);

	if (gob->x == NULL || GETPHYS(gob) == NULL)
	{
		// close as a result of incomplete open; just ignore it
		return EP_STAT_OK;
	}
	physinfo_free(GETPHYS(gob));
	gob->x->physinfo = NULL;

	return EP_STAT_OK;
}


/*
**  SQLITE_REMOVE --- remove a disk-based log
**
**		It is assume that permission has already been granted.
*/

static EP_STAT
sqlite_remove(gdp_gob_t *gob)
{
	EP_STAT estat = EP_STAT_OK;

	if (!EP_ASSERT_POINTER_VALID(gob) || !EP_ASSERT_POINTER_VALID(gob->x))
		return EP_STAT_ASSERT_ABORT;

	ep_dbg_cprintf(Dbg, 18, "sqlite_remove(%s)\n", gob->pname);

	DIR *dir;
	char dbuf[GOB_PATH_MAX];

	snprintf(dbuf, sizeof dbuf, "%s/_%02x", LogDir, gob->name[0]);
	ep_dbg_cprintf(Dbg, 21, "  remove directory %s%s%s\n",
					EpChar->lquote, dbuf, EpChar->rquote);
	dir = opendir(dbuf);
	if (dir == NULL)
	{
		estat = ep_stat_from_errno(errno);
		goto fail0;
	}

	for (;;)
	{
		struct dirent dentbuf;
		struct dirent *dent;

		// read the next directory entry
		int i = readdir_r(dir, &dentbuf, &dent);
		if (i != 0)
		{
			estat = ep_stat_from_errno(i);
			ep_log(estat, "sqlite_remove: readdir_r(%s) failed", dbuf);
			break;
		}
		if (dent == NULL)
			break;

		ep_dbg_cprintf(Dbg, 50, "  remove trial %s%s%s ",
						EpChar->lquote, dent->d_name, EpChar->rquote);
		if (strncmp(gob->pname, dent->d_name, GDP_GCL_PNAME_LEN) == 0)
		{
			char filenamebuf[GOB_PATH_MAX];

			ep_dbg_cprintf(Dbg, 50, "unlinking\n");
			snprintf(filenamebuf, sizeof filenamebuf, "_%02x/%s",
					gob->name[0], dent->d_name);
			if (unlink(filenamebuf) < 0)
				estat = posix_error(errno, "unlink(%s)", filenamebuf);
		}
		else
		{
			ep_dbg_cprintf(Dbg, 50, "skipping\n");
		}
	}
	closedir(dir);

fail0:
	physinfo_free(GETPHYS(gob));
	gob->x->physinfo = NULL;

	return estat;
}


/*
**  Process SQL results and produce a datum
*/

static void
read_blob(sqlite3_stmt *stmt, int index, gdp_buf_t **blobp)
{
	if (*blobp == NULL)
		*blobp = gdp_buf_new();
	else
		gdp_buf_reset(*blobp);

	const void *blob = sqlite3_column_blob(stmt, index);
	int bloblen = sqlite3_column_bytes(stmt, index);
	if (bloblen > 0)
		gdp_buf_write(*blobp, blob, bloblen);
}

//FIXME: this should deal with hash length and type
static void
read_hash(sqlite3_stmt *stmt, int index, gdp_hash_t **hashp)
{
	int mdalg = EP_CRYPTO_MD_NULL;		//FIXME
	if (*hashp == NULL)
		*hashp = gdp_hash_new(mdalg);
	else
		gdp_hash_reset(*hashp);
	gdp_buf_t *buf = _gdp_hash_getbuf(*hashp);
	read_blob(stmt, index, &buf);
}

//FIXME: this should deal with signature length and type
static void
read_signature(sqlite3_stmt *stmt, int index, gdp_sig_t **sigp)
{
	int mdalg = EP_CRYPTO_MD_NULL;		//FIXME
	if (*sigp == NULL)
		*sigp = gdp_sig_new(mdalg);
	else
		gdp_sig_reset(*sigp);
	gdp_buf_t *buf = _gdp_sig_getbuf(*sigp);
	read_blob(stmt, index, &buf);
}

static void
process_row(sqlite3_stmt *stmt, gdp_datum_t *datum)
{
	// hash value of this record
	//XXX unneeded except to validate the data
//	{
//		read_hash(stmt, 0, &datum->hash);
//	}

	// record number
	{
		datum->recno = sqlite3_column_int64(stmt, 1);
	}

	// timestamp
	{
		int64_t int_ts = sqlite3_column_int64(stmt, 2);
		ep_time_from_nsec(int_ts, &datum->ts);
	}

	// hash of previous record
	{
		read_hash(stmt, 3, &datum->prevhash);
	}

	// the actual value
	{
		read_blob(stmt, 4, &datum->dbuf);
	}

	// signature (if it exists)
	{
		read_signature(stmt, 5, &datum->sig);
	}
}

static EP_STAT
process_select_results(sqlite3_stmt *stmt, gdp_datum_t *datum)
{
	int rc;

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
	{
		process_row(stmt, datum);
		break;		//TODO: allow multiple rows in one query
	}
	return sqlite_error(rc, "process_select_results", "step");
}


/*
**  SQLITE_READ_BY_HASH --- read record indexed by record hash
*/

static EP_STAT
sqlite_read_by_hash(gdp_gob_t *gob,
		gdp_datum_t *datum)
{
	EP_STAT estat = EP_STAT_OK;
	int rc = SQLITE_OK;
	gob_physinfo_t *phys = GETPHYS(gob);
	const char *phase = "init";

	EP_ASSERT_POINTER_VALID(gob);
	gdp_buf_reset(datum->dbuf);
	if (datum->sig != NULL)
		gdp_sig_reset(datum->sig);

	ep_dbg_cprintf(Dbg, 44, "sqlite_read_by_hash(%s %" PRIgdp_recno "): ",
			gob->pname, datum->recno);

	ep_thr_rwlock_rdlock(&phys->lock);

	if (phys->read_by_hash_stmt == NULL)
	{
		phase = "prepare";
		rc = sqlite3_prepare_v2(phys->db,
						"SELECT hash, recno, timestamp, prevhash, value, sig"
						"	FROM log_entry"
						"	WHERE recno = ?;",
						-1, &phys->read_by_hash_stmt, NULL);
		CHECK_RC(rc, goto fail2);
	}

	phase = "bind";
	rc = sqlite3_bind_int64(phys->read_by_hash_stmt, 1, datum->recno);
	CHECK_RC(rc, goto fail2);

	phase = "step";
	estat = process_select_results(phys->read_by_hash_stmt, datum);

	if (!EP_STAT_ISOK(estat))
	{
fail2:
		ep_dbg_cprintf(Dbg, 1, "sqlite_read_by_hash: %s fread failed: %s\n",
				phase, strerror(errno));
		estat = ep_stat_from_errno(errno);
	}
	if (phys->read_by_hash_stmt != NULL)
		sqlite3_clear_bindings(phys->read_by_hash_stmt);
	ep_thr_rwlock_unlock(&phys->lock);

	return estat;
}


/*
**  SQLITE_READ_BY_RECNO --- read record indexed by record number
**
**		Reads in a message indicated by datum->recno into datum.
*/

static EP_STAT
sqlite_read_by_recno(gdp_gob_t *gob,
		gdp_datum_t *datum)
{
	EP_STAT estat = EP_STAT_OK;
	int rc = SQLITE_OK;
	gob_physinfo_t *phys = GETPHYS(gob);
	const char *phase = "init";

	EP_ASSERT_POINTER_VALID(gob);
	gdp_buf_reset(datum->dbuf);
	if (datum->sig != NULL)
		gdp_sig_reset(datum->sig);

	ep_dbg_cprintf(Dbg, 44, "sqlite_read_by_recno(%s %" PRIgdp_recno "): ",
			gob->pname, datum->recno);

	ep_thr_rwlock_rdlock(&phys->lock);

	if (phys->read_by_recno_stmt == NULL)
	{
		phase = "prepare";
		rc = sqlite3_prepare_v2(phys->db,
						"SELECT hash, recno, timestamp, prevhash, value, sig"
						"	FROM log_entry"
						"	WHERE recno = ?;",
						-1, &phys->read_by_recno_stmt, NULL);
		CHECK_RC(rc, goto fail2);
	}

	phase = "bind";
	rc = sqlite3_bind_int64(phys->read_by_recno_stmt, 1, datum->recno);
	CHECK_RC(rc, goto fail2);

	phase = "step";
	estat = process_select_results(phys->read_by_recno_stmt, datum);

	if (!EP_STAT_ISOK(estat))
	{
fail2:
		ep_dbg_cprintf(Dbg, 1, "sqlite_read_by_recno: %s fread failed: %s\n",
				phase, strerror(errno));
		estat = ep_stat_from_errno(errno);
	}
	if (phys->read_by_recno_stmt != NULL)
		sqlite3_clear_bindings(phys->read_by_recno_stmt);
	ep_thr_rwlock_unlock(&phys->lock);

	return estat;
}


/*
**  SQLITE_READ_BY_TIMESTAMP --- read record indexed by timestamp
**
**		Reads in a message indicated by datum->ts into datum.
*/

static EP_STAT
sqlite_read_by_timestamp(gdp_gob_t *gob,
		gdp_datum_t *datum)
{
	EP_STAT estat = EP_STAT_OK;
	int rc = SQLITE_OK;
	gob_physinfo_t *phys = GETPHYS(gob);
	const char *phase = "init";

	EP_ASSERT_POINTER_VALID(gob);
	gdp_buf_reset(datum->dbuf);
	if (datum->sig != NULL)
		gdp_sig_reset(datum->sig);

	if (ep_dbg_test(Dbg, 44))
	{
		char time_buf[100];
		ep_time_format(&datum->ts, time_buf, sizeof time_buf,
					EP_TIME_FMT_HUMAN);
		ep_dbg_cprintf(Dbg, 44, "sqlite_read_by_timestamp(%s, %s)\n",
					gob->pname, time_buf);
	}

	ep_thr_rwlock_rdlock(&phys->lock);

	if (phys->read_by_timestamp_stmt == NULL)
	{
		phase = "prepare";
		rc = sqlite3_prepare_v2(phys->db,
						"SELECT hash, recno, timestamp, prevhash, value, sig"
						"	FROM log_entry"
						"	WHERE timestamp >= ?"
						"	ORDER BY timestamp"
						"	LIMIT 1;",
						-1, &phys->read_by_timestamp_stmt, NULL);
		CHECK_RC(rc, goto fail2);
	}

	phase = "bind";
	rc = sqlite3_bind_int64(phys->read_by_timestamp_stmt, 1, datum->recno);
	CHECK_RC(rc, goto fail2);

	phase = "step";
	estat = process_select_results(phys->read_by_timestamp_stmt, datum);

	if (!EP_STAT_ISOK(estat))
	{
fail2:
		ep_dbg_cprintf(Dbg, 1, "sqlite_read_by_timestamp(%s) failed: %s\n",
				phase, strerror(errno));
		estat = ep_stat_from_errno(errno);
	}
	if (phys->read_by_timestamp_stmt != NULL)
		sqlite3_clear_bindings(phys->read_by_timestamp_stmt);
	ep_thr_rwlock_unlock(&phys->lock);

	return estat;
}


/*
**  SQLITE_RECNO_EXISTS --- determine if a record number already exists
*/

#if 0			// currently unused
static int
sqlite_recno_exists(gdp_gob_t *gob, gdp_recno_t recno)
{
	int rc;
	gob_physinfo_t *phys = GETPHYS(gob);
	int rval = 0;
	const char *phase;

	ep_thr_rwlock_rdlock(&phys->lock);

	phase = "prepare";
	sqlite3_stmt *stmt;
	rc = sqlite3_prepare_v2(phys->db,
					"SELECT COUNT(hash) WHERE recno = ?;",
					-1, &stmt, NULL);
	CHECK_RC(rc, goto fail1);

	phase = "bind";
	rc = sqlite3_bind_int64(stmt, 1, recno);
	CHECK_RC(rc, goto fail1);

	phase = "step";
	rc = sqlite3_step(stmt);
	CHECK_RC(rc, goto fail1);

	phase = "column";
	rval = sqlite3_column_int(stmt, 0);

	if (false)
	{
fail1:
		sqlite_error(rc, "sqlite_recno_exists", phase);
	}
	sqlite3_finalize(stmt);

	ep_thr_rwlock_unlock(&phys->lock);
	return rval;
}
#endif


/*
**	SQLITE_APPEND --- append a message to a writable gob
*/

static EP_STAT
sqlite_append(gdp_gob_t *gob,
			gdp_datum_t *datum)
{
	EP_STAT estat = EP_STAT_OK;
	int rc = SQLITE_OK;
	gob_physinfo_t *phys;
	const char *phase;

	if (ep_dbg_test(Dbg, 44))
	{
		ep_dbg_printf("sqlite_append(%s):\n    ", gob->pname);
		gdp_datum_print(datum, ep_dbg_getfile(),
					GDP_DATUM_PRDEBUG |
						(ep_dbg_test(Dbg, 24) ? 0 : GDP_DATUM_PRMETAONLY));
	}

	phys = GETPHYS(gob);
	EP_ASSERT_POINTER_VALID(phys);
	EP_ASSERT_POINTER_VALID(datum);

	ep_thr_rwlock_wrlock(&phys->lock);

	//FIXME: bind datum values into SQL
	gdp_hash_t *hash = NULL;
	{
		sqlite3_stmt *stmt;
		phase = "append prepare";
		rc = sqlite3_prepare_v2(phys->db,
					"INSERT INTO log_entry"
					"	(hash, recno, timestamp, prevhash, value, sig)"
					"	VALUES(?, ?, ?, ?, ?, ?);",
					-1, &stmt, NULL);
		CHECK_RC(rc, goto fail3);

		phase = "append bind 1";
		hash = gdp_datum_hash(datum);
		rc = sql_bind_hash(stmt, 1, hash);
		CHECK_RC(rc, goto fail3);

		phase = "append bind 2";
		rc = sql_bind_recno(stmt, 2, datum->recno);
		CHECK_RC(rc, goto fail3);

		phase = "append bind 3";
		rc = sql_bind_timestamp(stmt, 3, &datum->ts);
		CHECK_RC(rc, goto fail3);

		if (datum->prevhash != NULL)
		{
			phase = "append bind 4";
			rc = sql_bind_hash(stmt, 4, datum->prevhash);
			CHECK_RC(rc, goto fail3);
		}

		phase = "append bind 5";
		rc = sql_bind_buf(stmt, 6, datum->dbuf);
		CHECK_RC(rc, goto fail3);

		if (datum->sig != NULL)
		{
			phase = "append bind 6";
			rc = sql_bind_signature(stmt, 5, datum);
			CHECK_RC(rc, goto fail3);
		}

		phase = "append step";
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
		{
fail3:
			estat = sqlite_error(rc, "sqlite_append", phase);
			if (hash != NULL)
				gdp_hash_free(hash);
		}
		sqlite3_finalize(stmt);
	}

	ep_thr_rwlock_unlock(&phys->lock);

	return estat;
}


/*
**  GOB_PHYSGETMETADATA --- read metadata from disk
**
**		This is depressingly similar to _gdp_gclmd_deserialize.
*/

#define STDIOCHECK(tag, targ, f)	\
			do	\
			{	\
				int t = f;	\
				if (t != targ)	\
				{	\
					ep_dbg_cprintf(Dbg, 1,	\
							"%s: stdio failure; expected %d got %d (errno=%d)\n"	\
							"\t%s\n",	\
							tag, targ, t, errno, #f)	\
					goto fail_stdio;	\
				}	\
			} while (0);

static EP_STAT
sqlite_getmetadata(gdp_gob_t *gob,
		gdp_gclmd_t **gmdp)
{
//	gdp_gclmd_t *gmd;
//	gob_physinfo_t *phys = GETPHYS(gob);
	EP_STAT estat = EP_STAT_OK;

	//FIXME
	estat = GDP_STAT_NOT_IMPLEMENTED;

	return estat;
}


/*
**  GOB_PHYSFOREACH --- call function for each GOB in directory
**
**		Return the highest severity error code found
*/

static EP_STAT
sqlite_foreach(EP_STAT (*func)(gdp_name_t, void *), void *ctx)
{
	int subdir;
	EP_STAT estat = EP_STAT_OK;

	for (subdir = 0; subdir < 0x100; subdir++)
	{
		DIR *dir;
		char dbuf[400];

		snprintf(dbuf, sizeof dbuf, "%s/_%02x", LogDir, subdir);
		dir = opendir(dbuf);
		if (dir == NULL)
			continue;

		for (;;)
		{
			struct dirent dentbuf;
			struct dirent *dent;

			// read the next directory entry
			int i = readdir_r(dir, &dentbuf, &dent);
			if (i != 0)
			{
				ep_log(ep_stat_from_errno(i),
						"gob_physforeach: readdir_r(%s) failed", dbuf);
				break;
			}
			if (dent == NULL)
				break;

			// we're only interested in .gdpndx files
			char *p = strrchr(dent->d_name, '.');
			if (p == NULL || strcmp(p, GLOG_SUFFIX) != 0)
				continue;

			// strip off the file extension
			*p = '\0';

			// convert the base64-encoded name to internal form
			gdp_name_t gname;
			EP_STAT estat = gdp_internal_name(dent->d_name, gname);
			EP_STAT_CHECK(estat, continue);

			// now call the function
			EP_STAT tstat = (*func)((uint8_t *) gname, ctx);

			// adjust return status only if new one more severe than existing
			if (EP_STAT_SEVERITY(tstat) > EP_STAT_SEVERITY(estat))
				estat = tstat;
		}
		closedir(dir);
	}
	return estat;
}


/*
**  Deliver statistics for management visualization
*/

static void
sqlite_getstats(
		gdp_gob_t *gob,
		struct gob_phys_stats *st)
{
//	unsigned int segno;
//	gob_physinfo_t *phys = GETPHYS(gob);

	st->nrecs = gob->nrecs;
//FIXME:	st->size = fsizeof(x);
}


__BEGIN_DECLS
struct gob_phys_impl	GdpSqliteImpl =
{
	.init =				sqlite_init,
	.read_by_hash		= sqlite_read_by_hash,
	.read_by_recno =	sqlite_read_by_recno,
	.read_by_timestamp	= sqlite_read_by_timestamp,
	.create =			sqlite_create,
	.open =				sqlite_open,
	.close =			sqlite_close,
	.append =			sqlite_append,
	.getmetadata =		sqlite_getmetadata,
	.remove =			sqlite_remove,
	.foreach =			sqlite_foreach,
	.getstats =			sqlite_getstats,
};
__END_DECLS
