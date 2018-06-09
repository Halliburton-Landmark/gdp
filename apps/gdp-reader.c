/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  GDP-READER --- read and prints records from a GCL
**
**		This makes the naive assumption that all data values are ASCII
**		text.  Ultimately they should all be encrypted, but for now
**		I wanted to keep the code simple.
**
**		Unfortunately it isn't that simple, since it is possible to read
**		using all the internal mechanisms.  The -c, -m, and -s flags
**		control which approach is being used.
**
**		There are two ways of reading.  The first is to get individual
**		records in a loop (as implemented in do_simpleread), and the
**		second is to request a batch of records (as implemented in
**		do_multiread); these are returned as events that are collected
**		after the initial command completes or as callbacks that are
**		invoked in a separate thread.  There are two interfaces for the
**		event/callback techniques; one only reads existing data, and the
**		other ("subscriptions") will wait for data to be appended by
**		another client.
**
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

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_app.h>
#include <ep/ep_time.h>
#include <gdp/gdp.h>
#include <event2/buffer.h>

#include <unistd.h>
#include <errno.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <sysexits.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp-reader", "GDP Reader Application");

#ifndef USE_GETDATE
# define USE_GETDATE		1
#endif


/*
**  DO_LOG --- log a timestamp (for performance checking).
*/

FILE	*LogFile;
bool	TextData = false;		// set if data should be displayed as text
bool	PrintSig = false;		// set if signature should be printed
bool	Quiet = false;			// don't print metadata
int		NRead = 0;				// number of datums read

void
do_log(const char *tag)
{
	struct timeval tv;

	if (LogFile == NULL)
		return;
	gettimeofday(&tv, NULL);
	fprintf(LogFile, "%s %ld.%06ld\n", tag, tv.tv_sec, (long) tv.tv_usec);
}

#define LOG(tag)	{ if (LogFile != NULL) do_log(tag); }

/*
**  PRINTDATUM --- just print out a datum
*/

void
printdatum(gdp_datum_t *datum, FILE *fp)
{
	uint32_t prflags = 0;

	// logging for simple performance testing
	LOG("R");

	if (TextData)
		prflags |= GDP_DATUM_PRTEXT;
	if (PrintSig)
		prflags |= GDP_DATUM_PRSIG;
	if (Quiet)
		prflags |= GDP_DATUM_PRQUIET;
	flockfile(fp);
	if (!Quiet)
		fprintf(fp, " >>> ");
	gdp_datum_print(datum, fp, prflags);
	funlockfile(fp);
	NRead++;
}


/*
**  DO_SIMPLEREAD --- read from a GCL using the one-record-at-a-time call
*/

EP_STAT
do_simpleread(gdp_gin_t *gin,
		gdp_recno_t firstrec,
		const char *dtstr,
		int numrecs)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_datum_t *datum = gdp_datum_new();
	bool first_record = true;

	// change the "infinity" sentinel to make the loop easier
	if (numrecs == 0)
		numrecs = -1;

	// can't start reading before first record (but negative makes sense)
	if (firstrec == 0)
		firstrec = 1;

	// are we reading by record number or by timestamp?
	if (dtstr == NULL)
	{
		// record number
		estat = gdp_gin_read_by_recno(gin, firstrec, datum);
	}
	else
	{
		// timestamp
		EP_TIME_SPEC ts;

		estat = ep_time_parse(dtstr, &ts, EP_TIME_USE_LOCALTIME);
		if (!EP_STAT_ISOK(estat))
		{
			ep_app_message(estat,
						"Cannot convert date/time string \"%s\"",
						dtstr);
			goto done;
		}

		estat = gdp_gin_read_by_ts(gin, &ts, datum);
	}

	// start reading data, one record at a time
	while (EP_STAT_ISOK(estat) && (numrecs < 0 || --numrecs > 0))
	{
		gdp_recno_t recno;

		// print the previous value
		printdatum(datum, stdout);

		// flush any left over data
		if (gdp_buf_reset(gdp_datum_getbuf(datum)) < 0)
		{
			char nbuf[40];

			(void) (0 == strerror_r(errno, nbuf, sizeof nbuf));
			ep_app_warn("buffer reset failed: %s", nbuf);
		}

		// move to next record
		recno = gdp_datum_getrecno(datum) + 1;
		estat = gdp_gin_read_by_recno(gin, recno, datum);
		first_record = false;
	}

	// print the final value
	if (EP_STAT_ISOK(estat))
		printdatum(datum, stdout);

	// end of data is returned as a "not found" error: turn it into a warning
	//    to avoid scaring the unsuspecting user
	if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOTFOUND) && !first_record)
		estat = EP_STAT_END_OF_FILE;

done:
	gdp_datum_free(datum);
	return estat;
}


/*
**  Multiread and Subscriptions.
*/

EP_STAT
print_event(gdp_event_t *gev, bool subscribe)
{
	EP_STAT estat = gdp_event_getstat(gev);

	// decode it
	switch (gdp_event_gettype(gev))
	{
	  case GDP_EVENT_DATA:
		// this event contains a data return
		LOG("S");
		printdatum(gdp_event_getdatum(gev), stdout);
		break;

	  case GDP_EVENT_DONE:
		// "end of subscription": no more data will be returned
		if (!Quiet)
		{
			ep_app_info("End of %s",
					subscribe ? "Subscription" : "Multiread");
		}
		estat = EP_STAT_END_OF_FILE;
		break;

	  case GDP_EVENT_SHUTDOWN:
		// log daemon has shut down, meaning we lose our subscription
		estat = GDP_STAT_DEAD_DAEMON;
		ep_app_message(estat, "%s terminating because of log daemon shutdown",
				subscribe ? "Subscription" : "Multiread");
		break;

	  case GDP_EVENT_CREATED:
		ep_app_info("Successful append, create, or similar");
		break;

	  default:
		// let the library handle this
		gdp_event_print(gev, stderr, 1, 0);
		break;
	}

	if (EP_STAT_ISFAIL(estat))			// ERROR or higher severity
	{
		char ebuf[100];
		fprintf(stderr, "    STATUS: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


void
multiread_cb(gdp_event_t *gev)
{
	(void) print_event(gev, false);
	gdp_event_free(gev);
}


/*
**  DO_MULTIREAD --- subscribe or multiread
**
**		This routine handles calls that return multiple values via the
**		event interface.  They might include subscriptions.
*/

EP_STAT
do_multiread(gdp_gin_t *gin,
		gdp_recno_t firstrec,
		const char *dtstr,
		int32_t numrecs,
		bool subscribe,
		bool use_callbacks)
{
	EP_STAT estat;
	void (*cbfunc)(gdp_event_t *) = NULL;
	EP_TIME_SPEC ts;

	if (use_callbacks)
		cbfunc = multiread_cb;

	// are we reading by record number or by timestamp?
	if (dtstr != NULL)
	{
		// timestamp
		estat = ep_time_parse(dtstr, &ts, EP_TIME_USE_LOCALTIME);
		if (!EP_STAT_ISOK(estat))
		{
			ep_app_message(estat, "Cannot convert date/time string \"%s\"",
						dtstr);
			return estat;
		}
	}

	if (subscribe)
	{
		// start up a subscription
		if (dtstr == NULL)
			estat = gdp_gin_subscribe_by_recno(gin, firstrec, numrecs,
									NULL, cbfunc, NULL);
		else
			estat = gdp_gin_subscribe_by_ts(gin, &ts, numrecs,
									NULL, cbfunc, NULL);
	}
	else
	{
#if 0
		// make the flags more user-friendly
		if (firstrec == 0)
			firstrec = 1;

		// start up a multiread
		if (dtstr == NULL)
			estat = gdp_gin_multiread(gin, firstrec, numrecs, cbfunc, NULL);
		else
			estat = gdp_gin_multiread_ts(gin, &ts, numrecs, cbfunc, NULL);
#else
		ep_app_error("Multiread not implemented; use async read");
		return GDP_STAT_NOT_IMPLEMENTED;
#endif
	}

	// check to make sure the subscribe/multiread succeeded; if not, bail
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[200];

		ep_app_fatal("Cannot %s:\n\t%s",
				subscribe ? "subscribe" : "multiread",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	// this sleep will allow multiple results to appear before we start reading
	if (ep_dbg_test(Dbg, 100))
		ep_time_nanosleep(500000000);	//DEBUG: one half second

	// now start reading the events that will be generated
	if (!use_callbacks)
	{
		int32_t ndone = 0;
		for (;;)
		{
			// for testing: force early termination and close
			if (ep_dbg_test(Dbg, 127) && ++ndone >= numrecs)
				break;

			// get the next incoming event
			gdp_event_t *gev = gdp_event_next(NULL, 0);

			// print it
			estat = print_event(gev, subscribe);

			// don't forget to free the event!
			gdp_event_free(gev);

			EP_STAT_CHECK(estat, break);
		}
	}
	else
	{
		// hang for an hour waiting for events
		sleep(3600);
	}

	return estat;
}


/*
**  DO_ASYNC_READ --- read asynchronously
*/

EP_STAT
do_async_read(gdp_gin_t *gin,
		gdp_recno_t firstrec,
		int32_t numrecs,
		bool use_callbacks)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_event_cbfunc_t cbfunc = NULL;

	if (use_callbacks)
		cbfunc = multiread_cb;

	// make the flags more user-friendly
	if (firstrec == 0)
		firstrec = 1;
	if (numrecs <= 0)
		numrecs = gdp_gin_getnrecs(gin);
	if (firstrec < 0)
	{
		firstrec += numrecs + 1;
		numrecs -= firstrec - 1;
	}

	// issue the multiread commands without reading results
	estat = gdp_gin_read_by_recno_async(gin, firstrec, numrecs, cbfunc, NULL);
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];
		ep_app_error("async_read: gdp_gin_read_by_recno_async error:\n\t%s",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	// this sleep will allow multiple results to appear before we start reading
	if (ep_dbg_test(Dbg, 100))
		ep_time_nanosleep(500000000);	//DEBUG: one half second

	// now start reading the events that will be generated
	if (!use_callbacks)
	{
		do
		{
			// get the next incoming event
			gdp_event_t *gev = gdp_event_next(NULL, 0);

			// print it
			estat = print_event(gev, false);

			// don't forget to free the event!
			gdp_event_free(gev);

			//EP_STAT_CHECK(estat, break);
		} while (EP_STAT_ISOK(estat));
	}
	else
	{
		// hang for an minute waiting for events
		sleep(60);
	}

	return estat;
}


/*
**  PRINT_METADATA --- get and print the metadata
*/

void
print_metadata(gdp_gin_t *gin)
{
	EP_STAT estat;
	gdp_md_t *gmd;
	gdp_recno_t nrecs;

	nrecs = gdp_gin_getnrecs(gin);
	printf("Number of records: %" PRIgdp_recno "\n", nrecs);
	estat = gdp_gin_getmetadata(gin, &gmd);
	EP_STAT_CHECK(estat, goto fail0);

	gdp_md_print(gmd, stdout, 5, 0);
	gdp_md_free(gmd);
	return;

fail0:
	ep_app_message(estat, "Could not read metadata!");
}

void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-a] [-c] [-d datetime] [-D dbgspec] [-f firstrec]\n"
			"  [-G router_addr] [-m] [-L logfile] [-M] [-n nrecs]\n"
			"  [-s] [-t] [-v] log_name\n"
			"    -a  read asynchronously\n"
			"    -c  use callbacks\n"
			"    -d  first date/time to read from\n"
			"    -D  turn on debugging flags\n"
			"    -f  first record number to read (from 1)\n"
			"    -G  IP host to contact for gdp_router\n"
			"    -L  set logging file name (for debugging)\n"
			"    -m  use multiread\n"
			"    -M  show log metadata\n"
			"    -n  set number of records to read (default all)\n"
			"    -q  be quiet (don't print any metadata)\n"
			"    -s  subscribe to this log\n"
			"    -t  print data as text (instead of hexdump)\n"
			"    -v  print verbose output (include signature)\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}

/*
**  MAIN --- the name says it all
*/

int
main(int argc, char **argv)
{
	gdp_gin_t *gin = NULL;
	EP_STAT estat;
	gdp_name_t gobname;
	int opt;
	char *gdpd_addr = NULL;
	bool subscribe = false;
	bool multiread = false;
	bool async = false;
	bool use_callbacks = false;
	bool showmetadata = false;
	int32_t numrecs = 0;
	gdp_recno_t firstrec = 0;
	bool show_usage = false;
	char *log_file_name = NULL;
	gdp_iomode_t open_mode = GDP_MODE_RO;
	const char *dtstr = NULL;

	setlinebuf(stdout);								//DEBUG
	//char outbuf[65536];							//DEBUG
	//setbuffer(stdout, outbuf, sizeof outbuf);		//DEBUG

	// parse command-line options
	while ((opt = getopt(argc, argv, "aAcd:D:f:G:L:mMn:qstv")) > 0)
	{
		switch (opt)
		{
		  case 'A':				// hidden flag for debugging only
			open_mode = GDP_MODE_RA;
			break;

		  case 'a':
			// do asynchronous read
			async = true;
			break;

		  case 'c':
			// use callbacks
			use_callbacks = true;
			break;

		  case 'd':
			dtstr = optarg;
			break;

		  case 'D':
			// turn on debugging
			ep_dbg_set(optarg);
			break;

		  case 'f':
			// select the first record
			firstrec = atol(optarg);
			break;

		  case 'G':
			// set the port for connecting to the GDP daemon
			gdpd_addr = optarg;
			break;

		  case 'L':
			log_file_name = optarg;
			break;

		  case 'm':
			// turn on multi-read (see also -s)
			multiread = true;
			break;

		  case 'M':
			showmetadata = true;
			break;

		  case 'n':
			// select the number of records to be returned
			numrecs = atol(optarg);
			break;

		  case 'q':
			// be quiet (don't print metadata)
			Quiet = true;
			break;

		  case 's':
			// subscribe to this GCL (see also -m)
			subscribe = true;
			break;

		  case 't':
			// print data as text
			TextData = true;
			break;

		  case 'v':
			PrintSig = true;
			break;

		  default:
			show_usage = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (firstrec != 0 && dtstr != NULL)
	{
		ep_app_error("Cannot specify -f and -d");
		exit(EX_USAGE);
	}

	int ntypes = 0;
	if (async)
		ntypes++;
	if (multiread)
		ntypes++;
	if (subscribe)
		ntypes++;
	if (ntypes > 1)
	{
		ep_app_error("Can only specify one of -a, -m, and -s");
		exit(EX_USAGE);
	}

	// we require a GCL name
	if (show_usage || argc <= 0)
		usage();

	if (log_file_name != NULL)
	{
		// open a log file (for timing measurements)
		LogFile = fopen(log_file_name, "a");
		if (LogFile == NULL)
			ep_app_warn("Cannot open log file %s: %s",
					log_file_name, strerror(errno));
		else
			setlinebuf(LogFile);
	}

	// initialize the GDP library
	estat = gdp_init(gdpd_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	// allow thread to settle to avoid interspersed debug output
	ep_time_nanosleep(INT64_C(100000000));		// 100 msec

	// parse the name (either base64-encoded or symbolic)
	estat = gdp_parse_name(argv[0], gobname);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_message(estat, "illegal GCL name syntax:\n\t%s", argv[0]);
		exit(EX_USAGE);
	}

	// convert it to printable format and tell the user what we are doing
	if (!Quiet)
	{
		gdp_pname_t gobpname;

		gdp_printable_name(gobname, gobpname);
		fprintf(stderr, "Reading GCL %s\n", gobpname);
	}

	// open the GCL; arguably this shouldn't be necessary
	estat = gdp_gin_open(gobname, open_mode, NULL, &gin);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_message(estat, "Cannot open GCL %s", argv[0]);
		exit(EX_NOINPUT);
	}

	// if we are converting a date/time string, set the local timezone
	if (dtstr != NULL)
		tzset();

	if (showmetadata)
		print_metadata(gin);

	// arrange to do the reading via one of the helper routines
	if (async)
		estat = do_async_read(gin, firstrec, numrecs, use_callbacks);
	else if (subscribe || multiread || use_callbacks)
		estat = do_multiread(gin, firstrec, dtstr, numrecs,
						subscribe, use_callbacks);
	else
		estat = do_simpleread(gin, firstrec, dtstr, numrecs);

fail0:
	;				// silly compiler grammar
	int exitstat;

	if (!EP_STAT_ISFAIL(estat))			// WARN or OK
		exitstat = EX_OK;
	else if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOROUTE))
		exitstat = EX_NOINPUT;
	else if (EP_STAT_ISABORT(estat))
		exitstat = EX_SOFTWARE;
	else
		exitstat = EX_UNAVAILABLE;

	if (ep_dbg_test(Dbg, 9))
	{
		char ebuf[100];
		ep_dbg_printf("Cleaning up, exitstat %d, estat %s\n",
				exitstat, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	// might as well let the GDP know we're going away
	if (gin != NULL)
	{
		EP_STAT close_stat = gdp_gin_close(gin);
		if (!EP_STAT_ISOK(close_stat))
			ep_app_message(close_stat, "cannot close GCL");
	}

	// this sleep is to watch for any extraneous results coming back
	if (ep_dbg_test(Dbg, 126))
	{
		int sleep_time = 40;
		ep_dbg_printf("Sleeping for %d seconds\n", sleep_time);
		while (sleep_time-- > 0)
			ep_time_nanosleep(INT64_C(1000000000));		// one second
	}

	// might as well let the user know what's going on....
	if (EP_STAT_ISFAIL(estat))
		ep_app_message(estat, "exiting after %d records", NRead);
	else if (!Quiet)
		fprintf(stderr, "Exiting after %d records\n", NRead);
	return exitstat;
}
