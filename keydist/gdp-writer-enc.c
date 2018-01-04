/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  GDP-WRITER2 --- writes the encrypted records to a GCL
**
**		This reads the records one line at a time from standard input
**		and assumes they are text, but there is no text requirement
**		implied by the GDP.
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

/*
** This function is slightly modified from the existing gdp-wrter.c 
** The modification is about apnd filter to encryp the log records.
**
** modified from gdp-writer by Hwa Shin Moon, ETRI 
**					(angela.moon@berkeley.edu, hsmoon@etri.re.kr)
** Last modified at 2016.12.31. 
*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_string.h>
#include <gdp/gdp.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <sysexits.h>
#include <sys/stat.h>

#include "kdc_api.h"



bool	AsyncIo = false;		// use asynchronous I/O
bool	Quiet = false;			// be silent (no chatty messages)
bool	Hexdump = false;		// echo input in hex instead of ASCII
bool	KeepGoing = false;		// keep going on append errors

static EP_DBG	Dbg = EP_DBG_INIT("gdp-writer", "gdp-writer");

// LATEST KEY INFO 
static EP_THR_MUTEX		keyMutex;
static rKey_info		curKey;
/*
static struct sym_rkey	curKey; 
static int		cur_seqn = 0;
static int		pre_seqn = 0;
EP_TIME_SPEC	key_time;
*/

/*
**  DO_LOG --- log a timestamp (for performance checking).
*/

FILE	*LogFile;

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


static char	*EventTypes[] =
{
	"Free (internal use)",
	"Data",
	"End of Subscription",
	"Shutdown",
	"Asynchronous Status",
};

void
showstat(gdp_event_t *gev)
{
	int evtype = gdp_event_gettype(gev);
	EP_STAT estat = gdp_event_getstat(gev);
	gdp_datum_t *d = gdp_event_getdatum(gev);
	char ebuf[100];
	char tbuf[20];
	char *evname;

	if (evtype < 0 || evtype >= sizeof EventTypes / sizeof EventTypes[0])
	{
		snprintf(tbuf, sizeof tbuf, "%d", evtype);
		evname = tbuf;
	}
	else
	{
		evname = EventTypes[evtype];
	}

	printf("Asynchronous event type %s:\n"
			"\trecno %" PRIgdp_recno ", stat %s\n",
			evname,
			gdp_datum_getrecno(d),
			ep_stat_tostr(estat, ebuf, sizeof ebuf));

	gdp_event_free(gev);
}


EP_STAT
write_record(gdp_datum_t *datum, gdp_gcl_t *gcl)
{
	EP_STAT estat;

	// echo the input for that warm fuzzy feeling
	if (!Quiet)
	{
		gdp_buf_t *dbuf = gdp_datum_getbuf(datum);
		int l = gdp_buf_getlength(dbuf);
		unsigned char *buf = gdp_buf_getptr(dbuf, l);

		if (!Hexdump)
			fprintf(stdout, "Got input %s%.*s%s\n",
					EpChar->lquote, l, buf, EpChar->rquote);
		else
			ep_hexdump(buf, l, stdout, EP_HEXDUMP_ASCII, 0);
	}

	if (ep_dbg_test(Dbg, 60))
		gdp_datum_print(datum, ep_dbg_getfile(), GDP_DATUM_PRDEBUG);

	// then send the buffer to the GDP
	LOG("W");
	if (AsyncIo)
	{
		estat = gdp_gcl_append_async(gcl, datum, showstat, NULL);
		EP_STAT_CHECK(estat, return estat);

		// return value will be printed asynchronously
	}
	else
	{
		estat = gdp_gcl_append(gcl, datum);

		if (EP_STAT_ISOK(estat))
		{
			// print the return value (shows the record number assigned)
			if (!Quiet)
				gdp_datum_print(datum, stdout, 0);
		}
		else if (!Quiet)
		{
			char ebuf[100];
			ep_app_error("Append error: %s",
						ep_stat_tostr(estat, ebuf, sizeof ebuf));
		}
	}
	return estat;
}


EP_STAT
signkey_cb(
		gdp_name_t gname,
		void *udata,
		EP_CRYPTO_KEY **skeyp)
{
	FILE *fp;
	EP_CRYPTO_KEY *skey;
	const char *signing_key_file = udata;

	ep_dbg_cprintf(Dbg, 1, "signkey_cb(%s)\n", signing_key_file);

	fp = fopen(signing_key_file, "r");
	if (fp == NULL)
	{
		ep_app_error("cannot open signing key file %s", signing_key_file);
		return ep_stat_from_errno(errno);
	}

	skey = ep_crypto_key_read_fp(fp, signing_key_file,
			EP_CRYPTO_KEYFORM_PEM, EP_CRYPTO_F_SECRET);
	if (skey == NULL)
	{
		ep_app_error("cannot read signing key file %s", signing_key_file);
		return ep_stat_from_errno(errno);
	}

	*skeyp = skey;
	return EP_STAT_OK;
}


void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-1] [-a] [-D dbgspec] [-G router_addr] [-K key_file]\n"
			"\t[-L log_file] [-q] log_name\n"
			"    -1  write all input as one record\n"
			"    -a  use asynchronous I/O\n"
			"    -D  set debugging flags\n"
			"    -G  IP host to contact for gdp_router\n"
			"    -i  ignore append errors\n"
			"    -K  signing key file\n"
			"    -L  set logging file name (print debug message on log file)\n"
			"    -q  run quietly (no non-error output)\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}


void treat_latestkey_cb( gdp_event_t *gev )
{
	int				rval;
	gdp_datum_t		*datum;


	// update rmsg on session 
	rval = kdc_cb_preprocessor( gev ); 
	if( rval != EX_OK ) goto fail0;

	datum = gdp_event_getdatum(gev);

	ep_thr_mutex_lock( &keyMutex );

	rval = convert_klog_to_symkey( datum, &(curKey.rKey), &(curKey.pre_seqn) );
	curKey.cur_seqn = datum->recno; 
	memcpy( &(curKey.key_time), &(datum->ts), sizeof( EP_TIME_SPEC ) );

	if( rval != EX_OK ) {
		ep_dbg_printf("[ERROR] Fail to convert into symkey \n" );
	} else notify_keychange( );

	ep_thr_mutex_unlock( &keyMutex );

fail0:
	gdp_event_free( gev );
}


int
main(int argc, char **argv)
{
	int						opt, len;
	bool					show_usage			= false;
	bool					one_record			= false;
	char					*log_file_name		= NULL;
	char					*signing_key_file	= NULL;
	EP_STAT					estat;
	gdp_gcl_open_info_t		*info				= NULL;

	char					*gdpd_addr	= NULL;
	gdp_gcl_t				*loggcl		= NULL; // for log 
	gdp_gcl_t				*ksgcl		= NULL;	// for key service 

	char					*ks_pname	= NULL;
	gdp_name_t				loggcliname;  // for log internal name
	gdp_name_t				ksgcliname;  // for log internal name

	void (*cbfunc)(gdp_event_t *)  = NULL;



	// collect command-line arguments
	while ((opt = getopt(argc, argv, "1aD:G:iK:L:q")) > 0)
	{
		switch (opt)
		{
		 case '1':
			one_record = true;
			Hexdump = true;
			break;

		 case 'a':
			AsyncIo = true;
			break;

		 case 'D':
			ep_dbg_set(optarg);
			break;

		 case 'G':
			gdpd_addr = optarg;
			break;

		 case 'i':
			KeepGoing = true;
			break;

		 case 'K':
			signing_key_file = optarg;
			break;

		 case 'L':
			log_file_name = optarg;
			break;

		 case 'q':
			Quiet = true;
			break;

		 default:
			show_usage = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage || argc != 1)
		usage();

	if (log_file_name != NULL)
	{
		// open a log file (for timing measurements)
		LogFile = fopen(log_file_name, "a");
		if (LogFile == NULL)
			ep_app_error("Cannot open log file %s: %s",
					log_file_name, strerror(errno));
		else
			setlinebuf(LogFile);
	}

	// initialize the GDP library
	estat = gdp_init(gdpd_addr);
	kdc_init( );
	ep_thr_mutex_init( &keyMutex, EP_THR_MUTEX_DEFAULT );
	ep_thr_mutex_setorder( &keyMutex, KDS_MUTEX_LORDER_SKEY );

	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	// allow thread to settle to avoid interspersed debug output
	ep_time_nanosleep(INT64_C(100000000));

	// set up any open information
	info = gdp_gcl_open_info_new();

	if (signing_key_file != NULL)
	{
		gdp_gcl_open_info_set_signkey_cb(info, signkey_cb, signing_key_file);
	}

	// open each GCL for log & key service with the provided name
	len = strlen( argv[0] );
	ks_pname = ep_mem_malloc( len + 4 );
	sprintf( ks_pname, "KS_%s", argv[0] );
	ks_pname[len+3] = '\0';

	ep_dbg_printf("[INFO] GCL handle for %s , %s \n", 
					argv[0], ks_pname );

	gdp_parse_name( argv[0], loggcliname);
	gdp_parse_name( ks_pname, ksgcliname );

	memset( &curKey, 0x00, sizeof curKey );

	//
	// Interaction with KSD 
	//
	estat = kdc_gcl_open( ksgcliname, GDP_MODE_RO, &ksgcl, 'S' );
	EP_STAT_CHECK(estat, goto fail1);

	// read the latest key 
	ep_thr_mutex_lock( &keyMutex );
	estat = kdc_get_latestKey( ksgcl, &(curKey.rKey), &(curKey.cur_seqn), 
									&(curKey.pre_seqn), &(curKey.key_time) );
	ep_thr_mutex_unlock( &keyMutex );
	EP_STAT_CHECK(estat, goto fail1);

	// subscribe the later key info... 
	// 2nd argu: 0 means the next generated key.  
	cbfunc = treat_latestkey_cb; 
	estat = kdc_gcl_subscribe( ksgcl, 0, 0, NULL, cbfunc, NULL, 'S' );  


	//
	// Interaction with LogD 
	//

	estat = gdp_gcl_open(loggcliname, GDP_MODE_AO, info, &loggcl);
	EP_STAT_CHECK(estat, goto fail1);

	// modified by hsmoon 
	sapnd_dt		keyInfo;
	keyInfo.mode		= 'I';
	keyInfo.curSession	= NULL;
	keyInfo.a_data		= &curKey;

	gdp_gcl_set_append_filter( loggcl, append_filter_for_kdc, 
								(void *)&keyInfo	);

	if (!Quiet)
	{
		gdp_pname_t pname;

		// dump the internal version of the GCL to facilitate testing
		printf("GDPname: %s (%" PRIu64 " recs)\n",
				gdp_printable_name(*gdp_gcl_getname(loggcl), pname),
				gdp_gcl_getnrecs(loggcl));

		// OK, ready to go!
		fprintf(stdout, "\nStarting to read input\n");
	}

	// we need a place to buffer the input
	gdp_datum_t *datum = gdp_datum_new();

	if (one_record)
	{
		// read the entire stdin into a single datum
		char buf[8 * 1024];
		int l;

		while ((l = fread(buf, 1, sizeof buf, stdin)) > 0)
			gdp_buf_write(gdp_datum_getbuf(datum), buf, l);

		estat = write_record(datum, loggcl);
	}
	else
	{
		// write lines into multiple datums
		char buf[200];

		while (fgets(buf, sizeof buf, stdin) != NULL)
		{
			// strip off newlines
			char *p = strchr(buf, '\n');
			if (p != NULL)
				*p++ = '\0';

			// first copy the text buffer into the datum buffer
			gdp_buf_write(gdp_datum_getbuf(datum), buf, strlen(buf));

			// write the record to the log
			estat = write_record(datum, loggcl);
			if (!EP_STAT_ISOK(estat) && !KeepGoing)
				break;
		}
	}

	// OK, all done.  Free our resources and exit
	gdp_datum_free(datum);

	// give a chance to collect async results
	if (AsyncIo)
		sleep(1);

	// tell the GDP that we are done
	gdp_gcl_close(loggcl);

fail1:
	if (info != NULL)
		gdp_gcl_open_info_free(info);

	if( loggcl	!= NULL ) gdp_gcl_close( loggcl );
	if( ksgcl	!= NULL ) kdc_gcl_close( ksgcl, 'S' );
	if( ks_pname!= NULL ) ep_mem_free( ks_pname );

	kdc_exit( ); 
	free_logkeymodule( );

fail0:
	;			// avoid compiler error
	int exitstat;

	if (EP_STAT_ISOK(estat))
		exitstat = EX_OK;
	else if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOROUTE))
		exitstat = EX_CANTCREAT;
	else if (EP_STAT_ISABORT(estat))
		exitstat = EX_SOFTWARE;
	else
		exitstat = EX_UNAVAILABLE;

	if (ep_dbg_test(Dbg, 10))
	{
		// cheat here and use internal interface
		extern void _gdp_req_pr_stats(FILE *);
		_gdp_req_pr_stats(ep_dbg_getfile());
	}

	// OK status can have values; hide that from the user
	if (EP_STAT_ISOK(estat))
		estat = EP_STAT_OK;
	if (!EP_STAT_ISOK(estat))
		ep_app_message(estat, "exiting with status");
	else if (!Quiet)
		fprintf(stderr, "Exiting with status OK\n");
	return exitstat;
}


