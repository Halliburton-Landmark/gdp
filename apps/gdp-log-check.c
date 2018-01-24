/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
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
#define LOG_CHECK	1
#define ep_log		log_override
#define ep_logv		logv_override
#define Dbg			DbgLogdGcl
#include "../gdplogd/logd_gcl.c"
#undef Dbg
#define Dbg			DbgLogdDisklog
#include "../gdplogd/logd_disklog.c"
#undef Dbg

#include <ep/ep_app.h>

#include <signal.h>
#include <sysexits.h>


static EP_DBG	Dbg = EP_DBG_INIT("gdp-log-check", "GDP Log Checker/Rebuilder");

#define LOGCHECK_STAT(sev, det)	EP_STAT_NEW(sev, EP_REGISTRY_USER, 1, det)

static struct ep_stat_to_string	Stats[] =
{
#define LOGCHECK_MISSING_INDEX			LOGCHECK_STAT(WARN, 1)
	{ LOGCHECK_MISSING_INDEX,		"missing index",						},
#define LOGCHECK_DUPLICATE_TIMESTAMP	LOGCHECK_STAT(ERROR, 2)
	{ LOGCHECK_DUPLICATE_TIMESTAMP,	"duplicate timestamp",					},
#define LOGCHECK_MISSING_TIMESTAMP		LOGCHECK_STAT(ERROR, 3)
	{ LOGCHECK_MISSING_TIMESTAMP,	"missing timestamp in index",			},
	{ EP_STAT_OK,					NULL,									}
};

struct
{
	bool	verbose:1;
	bool	silent:1;
	bool	force:1;
	bool	summaryonly:1;
	bool	tidx_only:1;
} Flags;

uint32_t		GdplogdForgive;

struct ctx
{
	// info about the log we are working on
	const char		*logpath;		// path to the log directory
	const char		*logxname;		// external name of log
	gob_physinfo_t	*phys;			// physical manifestation

	// info about the record number index

	// info about the timestamp index
	DB				*tidx;			// timestamp index (if it exists)

	// data structures to track missing or duplicated record numbers
	DB				*recseqdb;
	DBC				*recseqdbc;
	gdp_recno_t		begin;
	gdp_recno_t		recno;
};

gdp_recno_t		MaxRecno;				// maximum recno value


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
**  Open up a gdp_gob_t structure but without sending protocol
*/

EP_STAT
open_fake_gob(const char *logxname, gdp_gob_t **pgob)
{
	EP_STAT estat = EP_STAT_OK;
	const char *phase;
	gdp_gob_t *gob = NULL;
	gdp_name_t loginame;

	phase = "gdp_name_parse";
	estat = gdp_parse_name(logxname, loginame);
	EP_STAT_CHECK(estat, goto fail0);

	phase = "gob_alloc";
	estat = gob_alloc(loginame, GDP_MODE_ANY, &gob);
	EP_STAT_CHECK(estat, goto fail0);

	phase = "physinfo_alloc";
	gob->x->physinfo = physinfo_alloc(gob);
	if (gob->x->physinfo == NULL)
		goto fail0;

	// fill in GOB fields we will be using
	gdp_printable_name(loginame, gob->pname);

	if (!EP_STAT_ISOK(estat))
	{
fail0:
		if (EP_STAT_ISOK(estat))
			estat = EP_STAT_ERROR;
		ep_app_message(estat, "open_fake_gob(%s during %s)",
					logxname, phase);
	}
	*pgob = gob;
	return estat;
}



/*
**  Scan data file to find recno use.
**
**  This is a two pass algorithm.  There's probably a clever way to
**  do this in one pass.
**
**  Pass 1: Scan the data file looking for sequential blocks of record
**				numbers.
**	Pass 2: Read the database created in Pass 1 and output a list
**				of missing or duplicated record numbers.
*/


/*
**  Since recnos are integers, we need to sort properly in the btree.
*/

int
#if DB_VERSION_MAJOR >= DB_VERSION_THRESHOLD
recno_cmpf(DB *dbp, const DBT *a, const DBT *b)
#else
recno_cmpf(const DBT *a, const DBT *b)
#endif
{
	gdp_recno_t ar[3], br[3];

	memcpy(&ar, a->data, sizeof ar);
	memcpy(&br, b->data, sizeof br);
	if (ar[0] != br[0])
		return ar[0] - br[0];
	else if (ar[1] != br[1])
		return ar[1] - br[1];
	else
		return ar[2] - br[2];
}

/*
**  Initialization
*/

EP_STAT
recseq_init(struct ctx *ctx)
{
	EP_STAT estat;

	// initialize context
	ctx->begin = ctx->recno = 0;

	// create temporary database
	estat = bdb_open(NULL, DB_CREATE, 0600,
					DB_BTREE, &recno_cmpf, &ctx->recseqdb);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_message(estat, "Cannot open record sequence temp db");
	}
	else
	{
		estat = bdb_cursor_open(ctx->recseqdb, &ctx->recseqdbc);
		if (!EP_STAT_ISOK(estat))
			ep_app_message(estat, "Cannot open sequence sequence db cursor");
	}
	return estat;
}

void
recseq_cleanup(struct ctx *ctx)
{
	if (ctx->recseqdb != NULL)
		bdb_close(ctx->recseqdb);
	ctx->recseqdb = NULL;
}


/*
**  Write a record to the temporary database
**
**		We use a btree so that we'll read a sorted list.  We could use
**		insertion sort, although there's a risk that a very large log
**		might not fit in memory.
**
**		The database key consists of the starting record number, the
**		ending record number, and a sequence number.  The sequence number
**		is only to make sure that duplicates are not deleted.
*/

EP_STAT
recseq_db_write(DB *db, gdp_recno_t start, gdp_recno_t end)
{
	DBT key_thang, val_thang;
	gdp_recno_t key[3];
	EP_STAT estat;
	static gdp_recno_t seq = 0;		// not really a recno

	ep_dbg_cprintf(Dbg, 91,
				"recseq_db_write %" PRIgdp_recno " .. %" PRIgdp_recno "\n",
				start, end);

	memset(&key_thang, 0, sizeof key_thang);
	key[0] = start;
	key[1] = end;
	key[2] = seq++;
	key_thang.data = key;
	key_thang.size = 3 * sizeof *key;

	memset(&val_thang, 0, sizeof val_thang);

	estat = bdb_put(db, &key_thang, &val_thang);
	return estat;
}


EP_STAT
recseq_db_getnext(DBC *dbc, gdp_recno_t *startp, gdp_recno_t *endp)
{
	DBT key_thang, val_thang;
	gdp_recno_t *key;
	EP_STAT estat;

	memset(&key_thang, 0, sizeof key_thang);
	memset(&val_thang, 0, sizeof val_thang);
	estat = bdb_cursor_next(dbc, &key_thang, &val_thang);
	EP_STAT_CHECK(estat, return estat);

	EP_ASSERT(key_thang.size == 3 * sizeof *key);
	key = (gdp_recno_t *) key_thang.data;
	*startp = key[0];
	*endp = key[1];

	return estat;
}


/*
**  Pass 1.
**		Write each sequential block (start and end recno) to a BTree
**			database.  The key is the concatenation of the start recno
**			and the end recno, and the value is empty.  For example,
**			if the sequential blocks were {1,3}, {5,7}, {5,6}, {4,5},
**			they would sort as {1,3}, {4,5}, {5,6}, {5,7}.
*/

void
recseq_add_recno(gdp_recno_t recno, struct ctx *ctx)
{
	if (ctx->begin > 0)
	{
		// if this is the next record, just extend the sequence
		if (recno == ctx->recno + 1)
		{
			ctx->recno = recno;
			return;
		}

		// end of a sequential block: write to database
		recseq_db_write(ctx->recseqdb, ctx->begin, ctx->recno);
	}

	// start a new sequential block
	ctx->begin = ctx->recno = recno;
}


void
recseq_last_recno(struct ctx *ctx)
{
	recseq_db_write(ctx->recseqdb, ctx->begin, ctx->recno);
}



/*
**  Pass 2.
**		Here we are trying to detect gaps and overlaps in the sequence
**		of record numbers.  Since the database is ordered with the high
**		order bits of the key being the starting record number, we know
**		that there is a sliding window on the dataset; specifically,
**		whenever we read recno N, we can be sure that all recnos < N
**		are completed and we can discard them.  This allows us to use
**		a data structure that stores the starting recno of interest
**		and a list of ending recnos.  Note that this list will only
**		have more than one entry if there are overlapping record
**		numbers.
**
**		One exception to the "immediate discard" rule is if a new
**		database entry is read that immediately follows an existing
**		sequence in the list.  In this case the new sequence is simply
**		coalesced with the existing one.  This might happen when
**		records are out of order.  For example, if the input contains
**		sequences 1..3, 6..8, 4..5, then three database entries will
**		be written.  When read after sorting, these will be come in
**		the order 1..3, 4..5, 6..8, which can be trivially coalesced
**		into the sequence 1..8.
**
**		The output is a series of sequences with counts of the number
**		of times that sequence appears.  For example, given the input
**		1..3, 2..3, 5..7, the output would be {1..1: 1}, {2..3: 2},
**		{4..4: 0}, {5..7: 1}.
*/


void
recseq_output(gdp_recno_t start, gdp_recno_t end, int n)
{
	ep_dbg_cprintf(Dbg, 91,
				"recseq_output: %" PRIgdp_recno " .. %" PRIgdp_recno ", %d\n",
				start, end, n);
	if (Flags.silent || Flags.summaryonly)
		return;
	if (n == 0)
		printf("Missing records   %" PRIgdp_recno " .. %" PRIgdp_recno "\n",
			start, end);
	else if (n != 1)
		printf("Duplicate records %" PRIgdp_recno " .. %" PRIgdp_recno
				" (%d copies)\n", start, end, n);
}

struct recno_vect
{
	int			nalloc;				// number of entries allocated
	int			nused;				// number of entries in use
	gdp_recno_t	start;				// all entries start here
	gdp_recno_t	*ends;				// array of ending recnos
};


static int
recseq_sortfunc(const void *a, const void *b)
{
	gdp_recno_t arec = *(gdp_recno_t *) a;
	gdp_recno_t brec = *(gdp_recno_t *) b;

	if (arec < brec)
		return -1;
	else if (arec == brec)
		return 0;
	else
		return 1;
}

void
recseq_sort(struct recno_vect *rv)
{
	qsort(rv->ends, rv->nused, sizeof rv->ends[0], recseq_sortfunc);
}


/*
**  Flush everything that comes before the current starting recno.
**
**		Requires counting the number of overlaps.
*/

int
recseq_flush(struct ctx *ctx,
			struct recno_vect *rv,
			int pos,
			gdp_recno_t before)
{
	int epos = pos + 1;

	// see if there are other entries that overlap
	while (epos < rv->nused && rv->ends[pos] == rv->ends[epos])
		epos++;

	if (ep_dbg_test(Dbg, 91))
	{
		int i;
		ep_dbg_printf("recseq_flush: pos %d, epos %d, nused %d, ends:",
					pos, epos, rv->nused);
		for (i = 0; i < rv->nused; i++)
			ep_dbg_printf(" %" PRIgdp_recno "", rv->ends[i]);
		ep_dbg_printf("\n");
	}


	// output starting recno, ending recno, and the number of dups
	int n = rv->nused - pos;
	before--;
	if (rv->ends[pos] < before)
	{
		before = rv->ends[pos];
	}
	recseq_output(rv->start, before, n);
	rv->start = before + 1;
	return epos;
}


void
recseq_process(struct ctx *ctx)
{
	struct recno_vect *rv = (struct recno_vect *) ep_mem_zalloc(sizeof *rv);
	gdp_recno_t start, end;
	int i;

	ep_dbg_cprintf(Dbg, 30, "recseq_process:\n");

	while (EP_STAT_ISOK(recseq_db_getnext(ctx->recseqdbc, &start, &end)))
	{
		gdp_recno_t last_end = start;

		// the list needs to be sorted
		recseq_sort(rv);

		if (ep_dbg_test(Dbg, 91))
		{
			ep_dbg_printf("recseq_process: input %" PRIgdp_recno
					" .. %" PRIgdp_recno ", start %" PRIgdp_recno " ends",
					start, end, rv->start);
			int x = 0;
			while (x < rv->nused)
				ep_dbg_printf(" %" PRIgdp_recno, rv->ends[x++]);
			ep_dbg_printf("\n");
		}

		// see if this just extends an existing entry
		for (i = 0; i < rv->nused; i++)
		{
			if (rv->ends[i] + 1 == start)
			{
				ep_dbg_cprintf(Dbg, 91,
							"recseq_process: extending entry %d, now %" PRIgdp_recno
							" .. %" PRIgdp_recno "\n",
							i, rv->start, end);
				rv->ends[i] = end;
				break;
			}
		}
		if (i < rv->nused)
			continue;

		// flush old entries
		if (start > rv->start)
		{
			// find the entries that are completely dead
			i = 0;
			while (i < rv->nused && rv->ends[i] < start)
				i = recseq_flush(ctx, rv, i, start);

			// keep track of our last ending record
			if (i < rv->nused)
				last_end = rv->ends[i];
			else if (rv->nused > 0)
				last_end = rv->ends[rv->nused - 1];

			// can now compress the records that are flushed, if any
			if (i > 0)
			{
				if (ep_dbg_test(Dbg, 91))
				{
					int j;
					ep_dbg_printf("recseq_process: pre-compress stack (i %d, nused %d):",
								i, rv->nused);
					for (j = 0; j < rv->nused; j++)
						ep_dbg_printf(" %" PRIgdp_recno "", rv->ends[j]);
					ep_dbg_printf("\n");
				}
				memmove(&rv->ends[0], &rv->ends[i], i * sizeof rv->ends[0]);
				rv->nused -= i;
				if (ep_dbg_test(Dbg, 91))
				{
					int j;
					ep_dbg_printf("recseq_process: post-compress stack (i %d, nused %d):",
								i, rv->nused);
					for (j = 0; j < rv->nused; j++)
						ep_dbg_printf(" %" PRIgdp_recno "", rv->ends[j]);
					ep_dbg_printf("\n");
				}
			}

			// there may be some entries that are still active
			for (i = 0; i < rv->nused; )
				i = recseq_flush(ctx, rv, i, start);

			// now reset our starting point
			rv->start = start;
		}

		// if the list is now empty, there may be a gap
		if (last_end < start - 1)
		{
			recseq_output(last_end + 1, start - 1, 0);
		}

		// add the new info
		if (rv->nused >= rv->nalloc)
		{
			// allocate more space
			int new_nalloc = (rv->nalloc + 1) * 3 / 2;
			ep_dbg_cprintf(Dbg, 94,
						"recseq_process: expanding to %d entries\n",
						new_nalloc);
			rv->ends = (gdp_recno_t *) ep_mem_realloc(rv->ends,
											new_nalloc * sizeof rv->ends[0]);
			rv->nalloc = new_nalloc;
		}
		ep_dbg_cprintf(Dbg, 91,
					"recseq_process: adding entry %" PRIgdp_recno
					" .. %" PRIgdp_recno "\n",
					rv->start, end);
		rv->ends[rv->nused++] = end;
	}

	// output anything left in the sequence vector
	ep_dbg_cprintf(Dbg, 91,
				"recseq_process: flushing final values, nused %d\n",
				rv->nused);
	for (i = 0; i < rv->nused; )
		i = recseq_flush(ctx, rv, i, rv->ends[i] + 1);
}


/*
**  Scan the directory for the list of segments comprising this log (if any).
*/

EP_STAT
find_segs(gdp_gob_t *gob)
{
	EP_STAT estat = EP_STAT_OK;
	unsigned int allocsegs = 0;		// allocation size of phys->segments
	bool pre_segment = false;		// set if created before we had segments
	char dirname[GOB_PATH_MAX];
	struct physinfo *phys = GETPHYS(gob);

	// get the path name (only need the directory part of the path)
	estat = get_gob_path(gob, -1, GCL_LDF_SUFFIX, dirname, sizeof dirname);
	EP_STAT_CHECK(estat, return estat);
	char *p = strrchr(dirname, '/');
	if (p == NULL)
	{
		ep_app_error("find_segs: cannot get root path for %s", gob->pname);
		return GDP_STAT_CORRUPT_GCL;
	}
	*p = '\0';

	DIR *dir = opendir(dirname);
	if (dir == NULL)
	{
		// if directory does not exist, the log does not exist
		ep_app_severe("Cannot open directory %s", dirname);
		return GDP_STAT_NOTFOUND;
	}

	for (;;)
	{
		unsigned int segno = 0;

		errno = 0;
		struct dirent *dent = readdir(dir);
		if (dent == NULL)
		{
			if (errno != 0)
				estat = ep_stat_from_errno(errno);
			break;
		}

		if (strncmp(dent->d_name, gob->pname, GDP_GCL_PNAME_LEN) != 0)
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
			long ssegno = atol(&dent->d_name[GDP_GCL_PNAME_LEN + 1]);
			if (ssegno > 0)
				segno = ssegno;
		}
		// add segment number to known segment list
		if (allocsegs <= segno)
		{
			// allocate 50% more space for segment indices
			allocsegs = (segno + 1) * 3 / 2;
			phys->segments = (segment_t **) ep_mem_zrealloc(phys->segments,
					allocsegs * sizeof *phys->segments);
		}
		if (phys->segments[segno] == NULL)
			phys->segments[segno] = segment_alloc(segno);
		if (phys->nsegments <= segno)
			phys->nsegments = segno + 1;
	}
	closedir(dir);

	EP_STAT_CHECK(estat, return estat);

	// should not have both a pre-segment name and an segment-based name
	if (pre_segment && phys->nsegments > 1)
	{
		ep_app_error("Can't fathom both pre- and post-segment log names for\n"
				"    %s", gob->pname);
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
scan_recs(gdp_gob_t *gob,
		EP_STAT (*per_seg_f)(
						segment_t *seg,
						struct ctx *ctx),
		EP_STAT (*per_rec_f)(
						gdp_gob_t *gob,
						segment_record_t *segrec,
						off_t offset,
						segment_t *seg,
						struct ctx *ctx),
		struct ctx *ctx)
{
	EP_STAT estat = EP_STAT_OK;
	EP_STAT return_stat = EP_STAT_OK;
	unsigned int segno;
	struct physinfo *phys = GETPHYS(gob);
	off_t record_offset = 0;

	for (segno = 0; segno < phys->nsegments; segno++)
	{
		segment_t *seg = phys->segments[segno];
		if (seg == NULL)
			continue;

		gdp_recno_t recno;
		char pbuf[GOB_PATH_MAX];
		segment_header_t seghdr;

		// get full path of segment file
		// we use seg->segno here because it might be -1 (for old log)
		estat = get_gob_path(gob, seg->segno, GCL_LDF_SUFFIX,
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

		record_offset = ftello(seg->fp);
		for (;;)
		{
			segment_record_t log_record;

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

			ep_dbg_cprintf(Dbg, 23, "   offset %jd: recno %" PRIgdp_recno
					", sigmeta %x, flags %x, data_length %d\n",
					(intmax_t) record_offset,
					log_record.recno, log_record.sigmeta, log_record.flags,
					log_record.data_length);

			// minor sanity check
			if (log_record.recno != recno + 1)
			{
				if (MaxRecno > 0 && log_record.recno > MaxRecno)
				{
					// unreasonable record number, probably trashed
					estat = GDP_STAT_CORRUPT_GCL;
					if (!Flags.summaryonly && !Flags.silent)
						ep_app_message(estat, "%s\n"
								"  unreasonable recno %" PRIgdp_recno
								" max %" PRIgdp_recno ",\n"
								"  (after recno %" PRIgdp_recno "),"
								" data offset %jd",
								gob->pname, log_record.recno, MaxRecno, recno,
								(intmax_t) record_offset);
				}
				else if (log_record.recno > recno + 1)
				{
					// gap in data
					estat = GDP_STAT_RECORD_MISSING;
					if (Flags.verbose)
						ep_app_message(estat, "%s\n"
								"   data records missing, offset %jd,"
								" records %" PRIgdp_recno "-%" PRIgdp_recno,
								gob->pname, (intmax_t) record_offset,
								recno + 1, log_record.recno);
				}
				else
				{
					// duplicated record
					estat = GDP_STAT_RECORD_DUPLICATED;
					if (Flags.verbose)
						ep_app_message(estat, "%s\n"
								"    data records duplicated, got %"
								PRIgdp_recno " expected %" PRIgdp_recno "\n"
								"    (delta = %" PRIgdp_recno ", offset = %jd)",
								gob->pname, log_record.recno, recno + 1,
								recno + 1 - log_record.recno,
								(intmax_t) record_offset);
				}
			}
			if (EP_STAT_SEVERITY(estat) > EP_STAT_SEVERITY(return_stat))
				return_stat = estat;

			// skip crazy recnos
			if (MaxRecno <= 0 || log_record.recno <= MaxRecno)
			{
				// reset the expected recno to whatever we actually have
				recno = log_record.recno;

				// do per-record processing
				estat = (*per_rec_f)(gob, &log_record, record_offset, seg, ctx);
				if (EP_STAT_SEVERITY(estat) > EP_STAT_SEVERITY(return_stat))
					return_stat = estat;
			}

			// skip over header and data (that part is opaque)
			record_offset += sizeof log_record + log_record.data_length;
			if (fseek(seg->fp, record_offset, SEEK_SET) < 0)
			{
				estat = posix_error(errno, "record %" PRIgdp_recno
									": data seek error", log_record.recno);
				break;
			}

			// skip over signature (should we be checking this?)
			if ((log_record.sigmeta & 0x0fff) != 0)
			{
				record_offset += log_record.sigmeta & 0x0fff;
				ep_dbg_cprintf(Dbg, 33, "Seeking %d further (to %jd)\n",
						log_record.sigmeta & 0x0fff,
						(intmax_t) record_offset);
				if (fseek(seg->fp, log_record.sigmeta & 0x0fff, SEEK_CUR) < 0)
				{
					estat = posix_error(errno, "record %" PRIgdp_recno
										": signature seek error",
										log_record.recno);
					break;
				}
			}
		}
fail1:
		segment_free(seg);
		phys->segments[segno] = NULL;
		if (EP_STAT_SEVERITY(estat) > EP_STAT_SEVERITY(return_stat))
			return_stat = estat;
	}

	if (EP_STAT_SEVERITY(estat) > EP_STAT_SEVERITY(return_stat))
		return_stat = estat;

	if (ep_dbg_test(Dbg, 9))
		ep_app_dumpfds(stdout);
	return return_stat;
}


/***********************************************************************
**  Check a named log for consistency
*/

void
testfail(const char *fmt, ...)
{
	va_list ap;

	if (Flags.summaryonly || Flags.silent)
		return;
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
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
			gdp_gob_t *gob,				// the log containing this record
			segment_record_t *rec,		// the record header (from disk)
			off_t offset,				// the file offset of that record
			segment_t *seg,				// the segment information
			struct ctx *ctx)			// our internal data
{
	EP_STAT estat = EP_STAT_OK;
	EP_STAT return_stat = EP_STAT_OK;
	gob_physinfo_t *phys = GETPHYS(gob);

	// do some sanity checking on the record header; if it isn't good
	// we'll just report the record and skip it for rebuilding
	if (rec->recno < GETPHYS(gob)->ridx.min_recno)
	{
		if (!Flags.silent && !Flags.summaryonly)
		{
			ep_app_message(GDP_STAT_CORRUPT_GCL, "%s\n"
					"    Corrupt GOB: recno = %" PRIgdp_recno
					", min = %" PRIgdp_recno " (ignoring)",
					gob->pname,
					rec->recno, GETPHYS(gob)->ridx.min_recno);
		}
		return EP_STAT_OK;
	}

	// check record number to offset index
	{
		ridx_entry_t xentbuf;
		ridx_entry_t *xent = &xentbuf;

		estat = ridx_entry_read(gob, rec->recno, gob->pname, xent);
		if (!EP_STAT_ISOK(estat))
		{
			char ebuf[80];
			testfail("ridx entry read fail: off=%jd recno=%" PRIgdp_recno
					": %s\n",
					(intmax_t) offset,
					rec->recno,
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
		}
		EP_STAT_CHECK(estat, goto fail0);

		// do consistency checks
		if (xent->recno != rec->recno)
		{
			estat = GDP_STAT_CORRUPT_INDEX;			// this seems bad
			testfail("ridx recno inconsistency: %" PRIgdp_recno
							" != %" PRIgdp_recno ", offset %jd\n",
							xent->recno, rec->recno, (intmax_t) offset);
		}
		else if (xent->offset != offset)
		{
			estat = GDP_STAT_RECORD_DUPLICATED;		// most likely
			if (!EP_UT_BITSET(FORGIVE_LOG_DUPS, GdplogdForgive) || Flags.verbose)
				testfail("ridx offset inconsistency: recno %" PRIgdp_recno
						": %jd != %jd\n",
								xent->offset, offset);
		}
		else if (xent->segment != seg->segno)
		{
			estat = GDP_STAT_RECORD_DUPLICATED;		// most likely, but ...
			testfail("ridx segment inconsistency: %d != %d\n",
							xent->segment, seg->segno);
		}
	}
	if (EP_STAT_SEVERITY(estat) > EP_STAT_SEVERITY(return_stat))
		return_stat = estat;

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
#if DB_VERSION_MAJOR >= DB_VERSION_THRESHOLD
		tidx_value_t tval;
		memset(&tval, 0, sizeof tval);
		tval_dbt.data = &tval;
		tval_dbt.flags = DB_DBT_USERMEM;
		tval_dbt.size = tval_dbt.ulen = sizeof tval;
#endif

		estat = bdb_get(phys->tidx.db, &tkey_dbt, &tval_dbt);
		if (!EP_STAT_ISOK(estat))
		{
			char ebuf[100];

			if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOTFOUND))
				estat = LOGCHECK_MISSING_TIMESTAMP;
			testfail("tidx read failure: %s\n",
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
			goto fail0;
		}

		if (tval_dbt.size != sizeof *tvalp)
		{
			estat = GDP_STAT_CORRUPT_INDEX;
			testfail("tidx size inconsistency: %zd != %zd",
					tval_dbt.size, sizeof *tvalp);
			goto fail0;
		}

		tvalp = (tidx_value_t *) tval_dbt.data;
		if (tvalp->recno != rec->recno)
		{
			estat = LOGCHECK_DUPLICATE_TIMESTAMP;
			testfail("tidx recno inconsistency: %" PRIgdp_recno
							" != %" PRIgdp_recno "\n",
							tvalp->recno, rec->recno);
			goto fail0;
		}
	}

	// keep track of record numbers to detect dups and holes
	recseq_add_recno(rec->recno, ctx);

fail0:
	if (EP_STAT_SEVERITY(estat) > EP_STAT_SEVERITY(return_stat))
		return_stat = estat;
	return return_stat;
}


EP_STAT
check_tidx_db(gdp_gob_t *gob, struct ctx *ctx, const char **phasep)
{
	EP_STAT estat = EP_STAT_OK;

#if DB_VERSION_MAJOR >= DB_VERSION_THRESHOLD
	const char *phase;
	DB_ENV *dbenv = NULL;
	DB *dbp = NULL;
	int istat;
	char tidx_pbuf[GOB_PATH_MAX];

	phase = "tidx_get_gob_path";
	estat = get_gob_path(gob, -1, GCL_TIDX_SUFFIX,
					tidx_pbuf, sizeof tidx_pbuf);

	// first do a low-level verify on the database
	phase = "tidx_db_env_create";
	if ((istat = db_env_create(&dbenv, 0)) != 0)
		goto fail1;
	if (!Flags.silent)
	{
		dbenv->set_errfile(dbenv, stderr);
		dbenv->set_errpfx(dbenv, ep_app_getprogname());
	}
	phase = "tidx_db_create";
	if ((istat = db_create(&dbp, dbenv, 0)) != 0)
		goto fail1;
	phase = "tidx_db_verify";
	istat = dbp->verify(dbp, tidx_pbuf, ctx->logxname, NULL, 0);
fail1:
	if (istat != 0)
	{
		dbenv->err(dbenv, istat, "during %s", phase);
		estat = GDP_STAT_CORRUPT_TIDX;
	}
	// dbp->verify has released the dbp
	if (dbenv != NULL)
		(void) dbenv->close(dbenv, 0);
	*phasep = phase;
#endif
	return estat;
}


EP_STAT
do_check(gdp_gob_t *gob, struct ctx *ctx)
{
	EP_STAT estat;
	const char *phase;

	// set up info for recno tracking
	phase = "recseq_init";
	estat = recseq_init(ctx);
	EP_STAT_CHECK(estat, goto fail0);

	// open recno index for read
	phase = "ridx_open";
	estat = ridx_open(gob, GCL_RIDX_SUFFIX, O_RDONLY);
	EP_STAT_CHECK(estat, goto fail0);

	// check timestamp database
	phase = "tidx_check";
	estat = check_tidx_db(gob, ctx, &phase);
	EP_STAT_CHECK(estat, goto fail0);

	// open timestamp index for read
	phase = "tidx_open";
	estat = tidx_open(gob, GCL_TIDX_SUFFIX, O_RDONLY);
	EP_STAT_CHECK(estat, goto fail0);

	phase = NULL;
	estat = scan_recs(gob, check_segment, check_record, ctx);

	// output result of record number scanning
	recseq_last_recno(ctx);
	recseq_process(ctx);

fail0:
	recseq_cleanup(ctx);
	if (!Flags.silent)
	{
		const char *fgcolor;
		char ebuf[100];

		if (EP_STAT_ISOK(estat))
			fgcolor = EpVid->vidfggreen;
		else if (EP_STAT_ISWARN(estat))
			fgcolor = EpVid->vidfgyellow;
		else if (EP_STAT_ISERROR(estat))
			fgcolor = EpVid->vidfgmagenta;
		else
			fgcolor = EpVid->vidfgred;
		printf("%s%s%s", fgcolor, EpVid->vidbgblack, ctx->logxname);
		if (phase != NULL)
			printf(" (during %s)", phase);
		printf(": %s%s\n", ep_stat_tostr(estat, ebuf, sizeof ebuf),
				EpVid->vidnorm);
	}
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
			gdp_gob_t *gob,
			segment_record_t *rec,
			off_t offset,
			segment_t *seg,
			struct ctx *ctx)
{
	EP_STAT estat1 = EP_STAT_OK;

	// do some sanity checking on the record header; if it isn't good
	// we'll just report the record and skip it for rebuilding
	if ((rec->recno < GETPHYS(gob)->ridx.min_recno) ||
			(MaxRecno > 0 && rec->recno > MaxRecno))
	{
		if (!Flags.silent && !Flags.summaryonly)
		{
			ep_app_message(GDP_STAT_CORRUPT_GCL, "%s\n"
					"    Corrupt GOB: recno = %" PRIgdp_recno
					", min = %" PRIgdp_recno " (ignoring)",
					gob->pname,
					rec->recno, GETPHYS(gob)->ridx.min_recno);
		}
		// return OK since (presumably) the damage has been repaired
		return EP_STAT_OK;
	}

	if (!Flags.tidx_only)
	{
		// output info to ridx
		estat1 = ridx_put(gob, rec->recno, seg->segno, offset);
	}

	// output info to tidx
	EP_STAT estat2 = tidx_put(gob,
					rec->timestamp.tv_sec, rec->timestamp.tv_nsec,
					rec->recno);

	EP_STAT_CHECK(estat1, return estat1);
	EP_STAT_CHECK(estat2, return estat2);
	return EP_STAT_OK;
}


bool
askuser(const char *query, bool nullanswer)
{
	char buf[20];

	printf("%s ", query);
	fflush(stdout);
	if (fgets(buf, sizeof buf, stdin) == NULL)
		return nullanswer;
	if (strchr("yYtT1", buf[0]) != NULL)
		return true;
	if (strchr("nNfF0", buf[0]) != NULL)
		return false;
	return nullanswer;
}


void
remove_temp_files(gdp_gob_t *gob)
{
	// remove the temporary indexes
	char temp_path[GOB_PATH_MAX];
	if (!Flags.tidx_only)
	{
		get_gob_path(gob, -1, ".tmpridx", temp_path, sizeof temp_path);
		unlink(temp_path);
	}

	get_gob_path(gob, -1, ".tmptidx", temp_path, sizeof temp_path);
	unlink(temp_path);
}


EP_STAT
do_rebuild(gdp_gob_t *gob, struct ctx *ctx)
{
	EP_STAT estat;
	const char *phase;
	gob_physinfo_t *phys = GETPHYS(gob);
	bool install_new_files = Flags.force;

	// check the tidx database (this is just to see if we need to reinstall)
	estat = check_tidx_db(gob, ctx, &phase);
	EP_STAT_CHECK(estat, install_new_files = true);

	if (!Flags.tidx_only)
	{
		// create temporary recno index
		phase = "create ridx temp";
		estat = ridx_create(gob, ".tmpridx", (gdp_recno_t) 1, FLAG_TMPFILE);
		EP_STAT_CHECK(estat, goto fail0);
	}

	// create temporary timestamp index
	phase = "create tidx temp";
	estat = tidx_create(gob, ".tmptidx", FLAG_TMPFILE);
	EP_STAT_CHECK(estat, goto fail1);

	// do the actual scan
	phase = "rebuild";
	estat = scan_recs(gob, rebuild_segment, rebuild_record, ctx);

	// close the temporary indices
	if (!Flags.tidx_only)
	{
		fclose(phys->ridx.fp);
		phys->ridx.fp = NULL;
	}
	bdb_close(phys->tidx.db);
	phys->tidx.db = NULL;

	if (EP_STAT_ISOK(estat))
	{
		struct stat tidx_stat;
		char pbuf[GOB_PATH_MAX];

		// if no tidx file exists, always ask if you want to install
		estat = get_gob_path(gob, -1, GCL_TIDX_SUFFIX, pbuf, sizeof pbuf);
		if (!EP_STAT_ISOK(estat))
		{
			install_new_files = true;
		}
		else if (stat(pbuf, &tidx_stat) < 0)
		{
			estat = LOGCHECK_MISSING_INDEX;
			install_new_files = true;
		}
		else
		{
			char tmptidx[GOB_PATH_MAX];
			struct stat tmptidx_stat;

			// heuristic: check to see if size has changed
			// XXX: should really check to see if content has changed
			estat = get_gob_path(gob, -1, ".tmptidx", tmptidx, sizeof tmptidx);
			if (EP_STAT_ISOK(estat) &&
					stat(tmptidx, &tmptidx_stat) >= 0 &&
					tmptidx_stat.st_size != tidx_stat.st_size)
			{
				install_new_files = true;
			}
		}

	}

	if (install_new_files || EP_STAT_ISWARN(estat))
	{
		ep_app_message(estat, "changes made to log %s", ctx->logxname);
		if (install_new_files ||
			askuser("Do you want to install the new indices [Yn]?", true))
		{
			install_new_files = true;
		}
	}
	else if (!EP_STAT_ISOK(estat))
	{
		ep_app_message(estat, "could not rebuild log %s", ctx->logxname);
	}
	else
	{
		ep_app_info("no changes to %s %s", ctx->logxname,
				Flags.force ? "(forcing new index installation anyway)"
							: "(use -f to force new index installation)");
	}

	if (install_new_files)
	{
		// move the new indexes into place
		char real_path[GOB_PATH_MAX];
		char save_path[GOB_PATH_MAX];
		char temp_path[GOB_PATH_MAX];

		ep_app_info("installing new files for %s", ctx->logxname);

		if (!Flags.tidx_only)
		{
			get_gob_path(gob, -1, GCL_RIDX_SUFFIX, real_path, sizeof real_path);
			get_gob_path(gob, -1, ".oldridx", save_path, sizeof save_path);
			get_gob_path(gob, -1, ".tmpridx", temp_path, sizeof temp_path);
			rename(real_path, save_path);
			rename(temp_path, real_path);
		}

		get_gob_path(gob, -1, GCL_TIDX_SUFFIX, real_path, sizeof real_path);
		get_gob_path(gob, -1, ".oldtidx", save_path, sizeof save_path);
		get_gob_path(gob, -1, ".tmptidx", temp_path, sizeof temp_path);
		rename(real_path, save_path);
		rename(temp_path, real_path);
	}
	else
	{
		remove_temp_files(gob);
	}

	if (false)
	{
fail1:
		if (phys->ridx.fp != NULL)
		{
			fclose(phys->ridx.fp);
			phys->ridx.fp = NULL;
		}

fail0:
		ep_app_message(estat, "do_rebuild: failure during %s",
				phase);
	}

	return estat;
}


gdp_gob_t	*CurrentGcl;		// only for cleanup on signal during rebuild

void
sigint(int sig)
{
	if (CurrentGcl != NULL)
		remove_temp_files(CurrentGcl);
	ep_app_warn("Exiting on signal %d", sig);
	exit(EX_UNAVAILABLE);
}


/***********************************************************************
**  Scan a log.  If rebuild is set, it will repair indexes.
*/

EP_STAT
scan_log(const char *logxname, bool rebuild)
{
	EP_STAT estat;
	gdp_gob_t *gob;
	char pbuf[GOB_PATH_MAX];
	struct ctx ctxbuf;
	struct ctx *ctx = &ctxbuf;

	if (!Flags.silent && !Flags.summaryonly)
	{
		printf("\n%s log %s\n",
				rebuild ? "Rebuilding" : "Scanning", logxname);
	}

	memset(&ctxbuf, 0, sizeof ctxbuf);
	ctx->logxname = logxname;

	estat = open_fake_gob(logxname, &gob);
	EP_STAT_CHECK(estat, return estat);

	// this is just to get the root of the log name
	estat = get_gob_path(gob, -1, "", pbuf, sizeof pbuf);

	char *lname = strrchr(pbuf, '/');
	if (lname == NULL)
	{
		ep_app_severe("Bad GOB path %s", pbuf);
		return EP_STAT_SEVERE;
	}
	*lname++ = '\0';

	// pbuf now has the directory name, lname has the root (printable) log name

	// find the segment files applicable to this log
	estat = find_segs(gob);
	if (!EP_STAT_ISOK(estat))
	{
		// nothing found or error
		ep_app_message(estat, "%s", logxname);
		return estat;
	}

	if (rebuild)
	{
		CurrentGcl = gob;
		estat = do_rebuild(gob, ctx);
		CurrentGcl = NULL;
	}
	else
	{
		estat = do_check(gob, ctx);
	}

	// free up physical info (also closes files, etc.)
	physinfo_free(GETPHYS(gob));
	gob->x->physinfo = NULL;
	ep_mem_free(gob->x);
	gob->x = NULL;
	ep_mem_free(gob);

	return estat;
}


/***********************************************************************
**  Startup
*/


/*
**  We don't call gdp_init (or even _gdp_lib_init) because we aren't
**  actually a gdp program.  The downside of this is that we have to
**  replicate some of the initialization here.
*/

void
initialize(void)
{
	extern void _gdp_run_as(const char *);

	ep_lib_init(0);

	// pretend we are gdplogd (at least a little bit)
	ep_adm_readparams("gdplogd");					// include gdplogd defs

	// possible local overrides
	const char *progname = ep_app_getprogname();
	if (progname != NULL)
		ep_adm_readparams(progname);

	ep_dbg_setfile(NULL);
	_gdp_stat_init();
	ep_stat_reg_strings(Stats);
	if (getuid() == 0)
		_gdp_run_as(ep_adm_getstrparam("swarm.gdplogd.runasuser", NULL));
	signal(SIGINT, sigint);
}


void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-D dbg_spec] [-f] [-M maxrecno] [-q] [-r] [-s]\n"
			"\t[-t] [-v] log-name ...\n"
			"    -D  set debugging flags\n"
			"    -f  force rebuilt index installation (with -r)\n"
			"    -M  maximum recno value\n"
			"    -q  run quietly\n"
			"    -r  rebuild the log (rather than just check consistency)\n"
			"    -s  print summary only\n"
			"    -t  update timestamp index only\n"
			"    -v  run verbosely\n",
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
	MaxRecno = ep_adm_getintmaxparam("swarm.gdplogd.recno.max", 0);

	while ((opt = getopt(argc, argv, "D:fM:qrstv")) > 0)
	{
		switch (opt)
		{
		 case 'D':
			 ep_dbg_set(optarg);
			 break;

		 case 'f':
			 Flags.force = true;
			 break;

		 case 'M':
			 MaxRecno = strtoll(optarg, NULL, 0);
			 break;

		 case 'q':
			 Flags.silent = true;
			 break;

		 case 'r':
			 rebuild = true;
			 break;

		 case 's':
			 Flags.summaryonly = true;
			 break;

		 case 't':
			 Flags.tidx_only = true;
			 break;

		 case 'v':
			 Flags.verbose = true;
			 break;

		 default:
			 show_usage = true;
			 break;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage || argc < 1)
		usage();

	// have to hold this until after -D flag is processed
	disk_init_internal(false);

	if (ep_adm_getboolparam("swarm.gdplogd.sequencing.allowgaps", true))
		GdplogdForgive |= FORGIVE_LOG_GAPS;
	if (ep_adm_getboolparam("swarm.gdplogd.sequencing.allowdups", true))
		GdplogdForgive |= FORGIVE_LOG_DUPS;

	ep_dbg_cprintf(Dbg, 1, "Running as %d:%d (%d:%d)\n",
						getuid(), getgid(), geteuid(), getegid());

	while (argc-- > 0)
	{
		EP_STAT estat = scan_log(*argv++, rebuild);
		EP_STAT_CHECK(estat, exitstat = EX_DATAERR);
	}

	exit(exitstat);
}
