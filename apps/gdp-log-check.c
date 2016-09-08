/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
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
**  GDP-LOG-CHECK --- a standalone app to check and rebuild log indices
**
**		So far this only rebuilds indices.  There are also possible
**		problems in the data file itself (e.g., two records sharing
**		one recno) that could be fixed.  This is obviously more
**		dangerous, since trashing the data file trashes the log.
**		Indices are expendable.
**
**	There are still a lot of improvements possible:
**
**		* Don't start ridx from recno 1 if old segments have expired.
**		* If duplicate records are found, discard them if they are
**		  identical.  (This would require updating the data file
**		  as well as the indices.)
**		* Continue after duplicate records found, preferably
**		  summarizing the range of dups.
**		* Others as noted in the code.
*/

// leverage the existing code (not all used, of course)
#define ep_log		log_override
#define ep_logv		logv_override
#define Dbg			DbgLogdGcl
#include "../gdplogd/logd_gcl.c"
#undef Dbg
#define Dbg			DbgLogdDisklog
#define LOG_CHECK	1
#include "../gdplogd/logd_disklog.c"
#undef Dbg

#include <sysexits.h>


static EP_DBG	Dbg = EP_DBG_INIT("gdp-log-rebuild", "GDP Log Rebuilder");

struct ctx
{
	// info about the log we are working on
	const char		*logpath;		// path to the log directory
	gcl_physinfo_t	*phys;			// physical manifestation
	gdp_gcl_t		*gcl;
	
	// info about the record number index

	// info about the timestamp index
	DB				*tidx;			// timestamp index (if it exists)
};


/*
**  Stubs to replace logging routines
*/

void
logv_override(EP_STAT estat, const char *fmt, va_list av)
{
	if (ep_dbg_test(Dbg, 1))
	{
		char ebuf[100];
		fprintf(stdout, "%s", EpVid->vidfgcyan);
		vfprintf(stdout, fmt, av);
		fprintf(stdout, ": %s%s\n", ep_stat_tostr(estat, ebuf, sizeof ebuf),
				EpVid->vidnorm);
	}
}

void
log_override(EP_STAT estat, const char *fmt, ...)
{
	va_list av;
	va_start(av, fmt);
	logv_override(estat, fmt, av);
	va_end(av);
}


/*
**  Open up a gdp_gcl_t structure but without sending protocol
*/

EP_STAT
open_fake_gcl(const char *logxname, gdp_gcl_t **pgcl)
{
	EP_STAT estat = EP_STAT_OK;
	const char *phase;
	gdp_gcl_t *gcl = NULL;
	gdp_name_t loginame;

	phase = "gdp_name_parse";
	estat = gdp_parse_name(logxname, loginame);
	EP_STAT_CHECK(estat, goto fail0);

	phase = "gcl_alloc";
	estat = gcl_alloc(loginame, GDP_MODE_ANY, &gcl);
	EP_STAT_CHECK(estat, goto fail0);

	phase = "physinfo_alloc";
	gcl->x->physinfo = physinfo_alloc(gcl);
	if (gcl->x->physinfo == NULL)
		goto fail0;

	// fill in GCL fields we will be using
	gdp_printable_name(loginame, gcl->pname);

	if (!EP_STAT_ISOK(estat))
	{
fail0:
		if (EP_STAT_ISOK(estat))
			estat = EP_STAT_ERROR;
		ep_app_message(estat, "open_fake_gcl(%s during %s)",
					logxname, phase);
	}
	*pgcl = gcl;
	return estat;
}


/*
**  Scan the directory for the list of segments comprising this log (if any).
*/

EP_STAT
find_segs(gdp_gcl_t *gcl)
{
	EP_STAT estat = EP_STAT_OK;
	int allocsegs = 0;				// allocation size of phys->segments
	bool pre_segment = false;		// set if created before we had segments
	char dirname[GCL_PATH_MAX];
	struct physinfo *phys = GETPHYS(gcl);

	// get the path name (only need the directory part of the path)
	estat = get_gcl_path(gcl, -1, GCL_LDF_SUFFIX, dirname, sizeof dirname);
	EP_STAT_CHECK(estat, return estat);
	char *p = strrchr(dirname, '/');
	if (p == NULL)
	{
		ep_app_error("find_segs: cannot get root path for %s", gcl->pname);
		return GDP_STAT_CORRUPT_GCL;
	}
	*p = '\0';

	DIR *dir = opendir(dirname);
	if (dir == NULL)
	{
		// if directory does not exist, the log does not exist
		return GDP_STAT_NOTFOUND;
	}

	for (;;)
	{
		int segno;

		errno = 0;
		struct dirent *dent = readdir(dir);
		if (dent == NULL)
		{
			if (errno != 0)
				estat = ep_stat_from_errno(errno);
			break;
		}

		if (strncmp(dent->d_name, gcl->pname, GDP_GCL_PNAME_LEN) != 0)
		{
			// not relevant
			continue;
		}

		p = strrchr(dent->d_name, '.');
		if (p == NULL || strcmp(p, GCL_LDF_SUFFIX) != 0)
		{
			// not a segment file
			continue;
		}

		/*
		**  OK, we have a segment (data) file.  It might be old style
		**  (with no segment number) or new style (with -NNNNNNN added
		**  to the file name).  We keep track of both.
		*/

		// drop the extension and check to make sure we have a full name
		*p = '\0';
		if (strlen(dent->d_name) < GDP_GCL_PNAME_LEN)
			continue;

		// if this length is that of a pname, assume old style
		if (strlen(dent->d_name) == GDP_GCL_PNAME_LEN)
		{
			// looks like an old style name
			pre_segment = true;
			segno = 0;
		}
		else if (dent->d_name[GDP_GCL_PNAME_LEN] != '-')
		{
			// if not, next character should be a hyphen
			continue;

			//XXX should really check that all remaining characters are digits
		}
		else
		{
			segno = atol(&dent->d_name[GDP_GCL_PNAME_LEN + 1]);
		}
		// add segment number to known segment list
		if (allocsegs <= segno)
		{
			// allocate 50% more space for segment indices
			allocsegs = (segno + 1) * 3 / 2;
			phys->segments = ep_mem_zrealloc(phys->segments,
					allocsegs * sizeof *phys->segments);
		}
		if (phys->segments[segno] == NULL)
			phys->segments[segno] = segment_alloc(segno);
		if (phys->nsegments <= segno)
			phys->nsegments = segno + 1;
	}

	EP_STAT_CHECK(estat, return estat);

	// should not have both a pre-segment name and an segment-based name
	if (pre_segment && phys->nsegments > 1)
	{
		ep_app_error("Can't fathom both pre- and post-segment log names for\n"
				"    %s", gcl->pname);
	}

	// success: return the number of segments found
	return EP_STAT_FROM_INT(phys->nsegments);
}


/*
**  Scan the records in a log
**
**		Calls a function for each record in the log.  Understands
**		about segments.
*/

EP_STAT
scan_recs(gdp_gcl_t *gcl,
		EP_STAT (*per_seg_f)(
						segment_t *seg,
						struct ctx *ctx),
		EP_STAT (*per_rec_f)(
						gdp_gcl_t *gcl,
						segment_record_t *segrec,
						off_t offset,
						segment_t *seg,
						struct ctx *ctx),
		struct ctx *ctx)
{
	EP_STAT estat;
	int segno;
	struct physinfo *phys = GETPHYS(gcl);

	for (segno = 0; segno < phys->nsegments; segno++)
	{
		segment_t *seg = phys->segments[segno];
		if (seg == NULL)
			continue;

		gdp_recno_t recno;
		char pbuf[GCL_PATH_MAX];
		segment_header_t seghdr;

		// get full path of segment file
		// we use seg->segno here because it might be -1 (for old log)
		estat = get_gcl_path(gcl, seg->segno, GCL_LDF_SUFFIX,
						pbuf, sizeof pbuf);

		estat = segment_hdr_open(pbuf, seg, O_RDONLY, &seghdr);
		if (!EP_STAT_ISOK(estat))
		{
			ep_app_message(estat, "could not open segment %d\n    (%s)",
					segno, pbuf);
			continue;
		}

		if (ep_dbg_test(Dbg, 4))		//XXX should this be a -v flag?
		{
			printf("Segment file %s\n", pbuf);
			printf("     header_size %" PRIu32 "\n", seghdr.header_size);
			printf("         segment %" PRIu32 "\n", seghdr.segment);
			printf("    recno_offset %" PRIgdp_recno "\n", seghdr.recno_offset);
		}

		// set expected starting recno to what the header claims
		recno = seg->recno_offset;

		if (per_seg_f != NULL)
		{
			estat = (*per_seg_f)(seg, ctx);
			if (EP_STAT_ISFAIL(estat))		// warnings are OK
				goto fail1;
		}

		//XXX should check metadata for consistency here
		//XXX instead, we just skip to the end of the header
		if (fseek(seg->fp, seghdr.header_size, SEEK_SET) < 0)
		{
			estat = posix_error(errno, "%s:\n"
							"    fseek(%" PRIu32 " failed",
							pbuf, seghdr.header_size);
			goto fail1;
		}

		off_t record_offset = ftello(seg->fp);
		for (;;)
		{
			segment_record_t log_record;

			ep_dbg_cprintf(Dbg, 22, "Reading recno %" PRIgdp_recno " @ %jd\n",
					recno + 1, (intmax_t) record_offset);

			if (record_offset != ftello(seg->fp))
				ep_dbg_cprintf(Dbg, 1, "WARNING: Recno %" PRIgdp_recno
							" offset should be %jd, is %jd\n",
							recno + 1,
							(intmax_t) record_offset,
							(intmax_t) ftello(seg->fp));

			// read and convert byte order for record header
			if (fread(&log_record, sizeof log_record, 1, seg->fp) < 1)
			{
				if (ferror(seg->fp))
				{
					estat = posix_error(errno, "record %" PRIgdp_recno
									" offset %jd: read error",
									recno + 1, (intmax_t) record_offset);
				}
				break;
			}
			log_record.recno = ep_net_ntoh64(log_record.recno);
			ep_net_ntoh_timespec(&log_record.timestamp);
			log_record.sigmeta = ep_net_ntoh16(log_record.sigmeta);
			log_record.flags = ep_net_ntoh16(log_record.flags);
			log_record.data_length = ep_net_ntoh32(log_record.data_length);

			ep_dbg_cprintf(Dbg, 23, "   recno %" PRIgdp_recno
					", sigmeta %x, flags %x, data_length %d\n",
					log_record.recno, log_record.sigmeta, log_record.flags,
					log_record.data_length);

			// minor sanity check
			if (log_record.recno != recno + 1)
			{
				if (log_record.recno > recno + 1)
				{
					// gap in data
					estat = GDP_STAT_RECORD_MISSING;
					ep_app_message(estat, "%s\n"
							"   data records missing, offset %jd,"
							" records %" PRIgdp_recno "-%" PRIgdp_recno,
							gcl->pname, (intmax_t) record_offset,
							recno + 1, log_record.recno);
				}
				else
				{
					// duplicated record
					estat = GDP_STAT_RECORD_DUPLICATED;
					ep_app_message(estat, "%s\n"
							"    data records duplicated, got %" PRIgdp_recno
								" expected %" PRIgdp_recno "\n"
							"    (delta = %" PRIgdp_recno ", offset = %jd)",
							gcl->pname, log_record.recno, recno + 1,
							recno + 1 - log_record.recno,
							(intmax_t) record_offset);
				}
			}

			// reset the expected recno to whatever we actually have
			recno = log_record.recno;

			// do per-record processing
			estat = (*per_rec_f)(gcl, &log_record, record_offset, seg, ctx);
			if (EP_STAT_ISFAIL(estat))		// warnings are OK
				break;

			// skip over header and data (that part is opaque)
			record_offset += sizeof log_record + log_record.data_length;
			ep_dbg_cprintf(Dbg, 32, "Seeking to %jd\n",
					(intmax_t) record_offset);
			if (fseek(seg->fp, record_offset, SEEK_SET) < 0)
			{
				estat = posix_error(errno, "record %" PRIgdp_recno
									": data seek error", log_record.recno);
				break;
			}

			// skip over signature (should we be checking this?)
			record_offset += log_record.sigmeta & 0x0fff;
			ep_dbg_cprintf(Dbg, 33, "Seeking %d further (to %jd)\n",
					log_record.sigmeta & 0x0fff,
					(intmax_t) record_offset);
			if (fseek(seg->fp, log_record.sigmeta & 0x0fff, SEEK_CUR) < 0)
			{
				estat = posix_error(errno, "record %" PRIgdp_recno
									": signature seek error", log_record.recno);
				break;
			}
		}
fail1:
		segment_free(seg);
		phys->segments[segno] = NULL;
	}

	return estat;
}


/***********************************************************************
**  Check a named log for consistency
*/

EP_STAT
testfail(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	return GDP_STAT_CORRUPT_INDEX;
}

// check a segment header
EP_STAT
check_segment(segment_t *seg, struct ctx *ctx)
{
	EP_STAT estat = EP_STAT_OK;

	if (ep_dbg_test(Dbg, 10))
	{
		ep_dbg_printf("check_segment: seg = %p\n", seg);
		//XXX should probably print more here
	}

	//XXX do consistency checks here
	//XXX are segments consistent with each other?

	return estat;
}


// check a record header
EP_STAT
check_record(
			gdp_gcl_t *gcl,				// the log containing this record
			segment_record_t *rec,		// the record header (from disk)
			off_t offset,				// the file offset of that record
			segment_t *seg,				// the segment information
			struct ctx *ctx)			// our internal data
{
	EP_STAT estat = EP_STAT_OK;
	gcl_physinfo_t *phys = GETPHYS(gcl);

	// check record number to offset index
	{
		ridx_entry_t xentbuf;
		ridx_entry_t *xent = &xentbuf;

		estat = ridx_entry_read(gcl, rec->recno, ctx->gcl->pname, xent);
		EP_STAT_CHECK(estat, goto fail0);

		// do consistency checks
		if (xent->recno != rec->recno)
			estat = testfail("ridx recno inconsistency: %" PRIgdp_recno
							" != %" PRIgdp_recno "\n",
							xent->recno, rec->recno);
		else if (xent->offset != offset)
			estat = testfail("ridx offset inconsistency: %jd != %jd\n",
							xent->offset, offset);
		else if (xent->segment != seg->segno)
			estat = testfail("ridx segment inconsistency: %d != %d\n",
							xent->segment, seg->segno);
	}

	// check timestamp to record number index
	if (phys->tidx.db != NULL)
	{
		tidx_key_t tkey;
		tidx_value_t *tvalp;
		DBT tkey_dbt;
		DBT tval_dbt;

		memset(&tkey, 0, sizeof tkey);
		memset(&tkey_dbt, 0, sizeof tkey_dbt);
		tkey.sec = ep_net_hton64(rec->timestamp.tv_sec);
		tkey.nsec = ep_net_hton32(rec->timestamp.tv_nsec);
		tkey_dbt.data = &tkey;
		tkey_dbt.size = sizeof tkey;
		memset(&tval_dbt, 0, sizeof tval_dbt);

		estat = bdb_get(phys->tidx.db, &tkey_dbt, &tval_dbt);
		if (!EP_STAT_ISOK(estat))
		{
			char ebuf[100];

			(void) testfail("tidx read failure: %s\n",
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
			goto fail0;
		}

		if (tval_dbt.size != sizeof *tvalp)
		{
			estat = testfail("tidx inconsistency");
			goto fail0;
		}

		tvalp = tval_dbt.data;
		if (tvalp->recno != rec->recno)
		{
			estat = testfail("tidx recno inconsistency: %" PRIgdp_recno
							" != %" PRIgdp_recno "\n",
							tvalp->recno, rec->recno);
			goto fail0;
		}
	}

fail0:
	return estat;
}


EP_STAT
do_check(gdp_gcl_t *gcl, struct ctx *ctx)
{
	EP_STAT estat;
	const char *phase;

	// open recno index for read
	phase = "ridx_open";
	estat = ridx_open(gcl, GCL_RIDX_SUFFIX, O_RDONLY);
	EP_STAT_CHECK(estat, goto fail0);

	// open timestamp index for read
	phase = "tidx_open";
	estat = tidx_open(gcl, GCL_TIDX_SUFFIX, O_RDONLY);
	EP_STAT_CHECK(estat, goto fail0);

	phase = NULL;
	estat = scan_recs(gcl, check_segment, check_record, ctx);
	EP_STAT_CHECK(estat, goto fail0);

fail0:
	if (EP_STAT_ISOK(estat))
		ep_app_info("Log %s looks OK", gcl->pname);
	else if (phase == NULL)
		ep_app_message(estat, "%s", gcl->pname);
	else
		ep_app_message(estat, "%s:\n    error during during %s",
					gcl->pname, phase);
	return estat;
}


/***********************************************************************
**  Rebuild a named log with the given segments available
*/

EP_STAT
rebuild_segment(
			segment_t *seg,
			struct ctx *ctx)
{
	EP_STAT estat = EP_STAT_OK;

	// nothing in particular to do per-segment

	return estat;
}


EP_STAT
rebuild_record(
			gdp_gcl_t *gcl,
			segment_record_t *segrec,
			off_t offset,
			segment_t *seg,
			struct ctx *ctx)
{
	// output info to ridx
	EP_STAT estat1 = ridx_put(gcl, segrec->recno, seg->segno, offset);

	// output info to tidx
	EP_STAT estat2 = tidx_put(gcl,
					segrec->timestamp.tv_sec, segrec->timestamp.tv_nsec,
					segrec->recno);

	EP_STAT_CHECK(estat1, return estat1);
	EP_STAT_CHECK(estat2, return estat2);
	return EP_STAT_OK;
}


EP_STAT
do_rebuild(gdp_gcl_t *gcl, struct ctx *ctx)
{
	EP_STAT estat;
	const char *phase;
	gcl_physinfo_t *phys = GETPHYS(gcl);

	// create temporary recno index
	phase = "create ridx temp";
	estat = ridx_create(gcl, ".tmpridx", (gdp_recno_t) 1);
	EP_STAT_CHECK(estat, goto fail0);

	// create temporary timestamp index
	phase = "create tidx temp";
	estat = tidx_create(gcl, ".tmptidx");
	EP_STAT_CHECK(estat, goto fail1);

	// do the actual scan
	phase = "rebuild";
	estat = scan_recs(gcl, rebuild_segment, rebuild_record, ctx);

	// close the temporary indices
	fclose(phys->ridx.fp);
	phys->ridx.fp = NULL;
	bdb_close(phys->tidx.db);
	phys->tidx.db = NULL;

	if (EP_STAT_ISWARN(estat))
	{
		ep_app_message(estat, "changes made to log %s", gcl->pname);

#if DEBUGGING
		// move the new indexes into place
		char real_path[GCL_PATH_MAX];
		get_gcl_path(gcl, -1, GCL_RIDX_SUFFIX, real_path, sizeof real_path);
		char save_path[GCL_PATH_MAX];
		get_gcl_path(gcl, -1, ".oldridx", save_path, sizeof save_path);
		char temp_path[GCL_PATH_MAX];
		get_gcl_path(gcl, -1, ".tmpridx", temp_path, sizeof temp_path);

		rename(real_path, save_path);
		rename(temp_path, real_path);

		get_gcl_path(gcl, -1, GCL_TIDX_SUFFIX, real_path, sizeof real_path);
		get_gcl_path(gcl, -1, ".oldtidx", save_path, sizeof save_path);
		get_gcl_path(gcl, -1, ".tmptidx", temp_path, sizeof temp_path);

		rename(real_path, save_path);
		rename(temp_path, real_path);
#endif
	}
	else
	{
		if (!EP_STAT_ISOK(estat))
		{
			ep_app_message(estat, "could not rebuild log %s", gcl->pname);
		}
		else
		{
			ep_app_info("no changes to %s", gcl->pname);
		}

#if DEBUGGING
		// remove the temporary indexes
		char temp_path[GCL_PATH_MAX];
		get_gcl_path(gcl, -1, ".tmpridx", temp_path, sizeof temp_path);
		unlink(temp_path);

		get_gcl_path(gcl, -1, ".tmptidx", temp_path, sizeof temp_path);
		unlink(temp_path);
#endif
	}

	if (false)
	{
fail1:
		fclose(phys->ridx.fp);
		phys->ridx.fp = NULL;

fail0:
		ep_app_message(estat, "do_rebuild: failure during %s",
				phase);
	}

	return estat;
}


/***********************************************************************
**  Scan a log.  If rebuild is set, it will repair indexes.
*/

EP_STAT
scan_log(const char *logxname, bool rebuild)
{
	EP_STAT estat;
	gdp_gcl_t *gcl;
	char pbuf[GCL_PATH_MAX];
	struct ctx ctxbuf;
	struct ctx *ctx = &ctxbuf;

	memset(&ctxbuf, 0, sizeof ctxbuf);

	estat = open_fake_gcl(logxname, &gcl);
	EP_STAT_CHECK(estat, return estat);

	// this is just to get the root of the log name
	estat = get_gcl_path(gcl, -1, "", pbuf, sizeof pbuf);

	char *lname = strrchr(pbuf, '/');
	if (lname == NULL)
	{
		ep_app_severe("Bad GCL path %s", pbuf);
		return EP_STAT_SEVERE;
	}
	*lname++ = '\0';

	// pbuf now has the directory name, lname has the root (printable) log name

	// find the segment files applicable to this log
	estat = find_segs(gcl);
	if (!EP_STAT_ISOK(estat))
	{
		// nothing found or error
		ep_app_message(estat, "%s", logxname);
		return estat;
	}

	if (rebuild)
		estat = do_rebuild(gcl, ctx);
	else
		estat = do_check(gcl, ctx);

	// free up physical info (also closes files, etc.)
	physinfo_free(GETPHYS(gcl));
	gcl->x->physinfo = NULL;
	ep_mem_free(gcl->x);
	gcl->x = NULL;
	ep_mem_free(gcl);

	return estat;
}


/***********************************************************************
**  Startup
*/


void
initialize(void)
{
	ep_lib_init(0);
	const char *progname = ep_app_getprogname();	// should be in ep_lib_init
	if (progname != NULL)							// should be in ep_lib_init
		ep_adm_readparams(progname);				// should be in ep_lib_init
	ep_dbg_setfile(NULL);
	_gdp_stat_init();
	
	disk_init();
}


void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-D dbg_spec] [-r] log-name ...\n"
			"    -D  set debugging flags\n"
			"    -r  rebuild the log (rather than just check consistency)\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	int opt;
	bool rebuild = false;
	bool show_usage = false;
	int exitstat = EX_OK;

	initialize();

	while ((opt = getopt(argc, argv, "D:r")) > 0)
	{
		switch (opt)
		{
		 case 'D':
			 ep_dbg_set(optarg);
			 break;

		 case 'r':
			 rebuild = true;
			 break;

		 default:
			 show_usage = true;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage || argc < 1)
		usage();

	while (argc-- > 0)
	{
		EP_STAT estat = scan_log(*argv++, rebuild);
		EP_STAT_CHECK(estat, exitstat = EX_DATAERR);
	}

	exit(exitstat);
}
