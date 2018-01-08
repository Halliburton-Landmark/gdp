/* vim: set ai sw=4 sts=4 ts=4 : */

/*
** 
**	----- BEGIN LICENSE BLOCK -----
**  KEY Generation / Distribution Service Daemon
**
**	Copyright (c) 2015-2017, Electronics and Telecommunications 
**	Research Institute (ETRI). All rights reserved. 
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
**  KEYDISTD -- Key Gen/Distribution Service Daemon (main)     
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sysexits.h>

#include <ep/ep_sd.h>
#include <ep/ep_thr.h>
#include <ep/ep_assert.h>



#include "keydistd.h"
#include "ksd_data_manager.h"
#include "session_manager.h"
#include "kds_pubsub.h"


static EP_DBG	Dbg = EP_DBG_INIT("kdistd.main", 
									"Key Distribution Service Daemon");


long	basic_period = 60;
int		adv_index = 3;
int		sch_index = 5;



/*
** KDISTD_SOCK_CLOSE_CB --- free resources when we lose a router connection
*/
void kdistd_sock_close_cb(gdp_chan_t *chan)
{
	// free any requests tied to this channel
	gdp_req_t *req = LIST_FIRST(&chan->reqs);

	while (req != NULL)
	{
		gdp_req_t *req2 = LIST_NEXT(req, chanlist);
		_gdp_req_free(&req);
		req = req2;
	}

	// all session & gcl state change... 
	reflect_lost_channel( );
}


/*
**  LOGD_RECLAIM_RESOURCES --- called periodically to prune old resources
**			(EX:: remove any expired resources or subscriptions... )
*/ 
static void keydistd_reclaim_resources(void *null)
{
	_gdp_reclaim_resources(NULL);  
	kds_sub_reclaim_resources(_GdpChannel); 

	// LATER::  consider resources in key_data_manager in more detail
}


// stub for libevent
// need to consider LATER
static void keydistd_reclaim_resources_callback(int fd, short what, void *ctx)
{
	ep_dbg_cprintf(Dbg, 69, "keydistd_reclaim_resources_callback\n");
	if (ep_adm_getboolparam("swarm.kdistd.reclaim.inthread", false))
		ep_thr_pool_run(keydistd_reclaim_resources, NULL);
	else
		keydistd_reclaim_resources(NULL);
}



void work_periodically(int fd, short what, void *u)
{
	int				max_val    =  adv_index * sch_index ; 
	static int		time_index = 0;
	time_index++;


	// 
	// 1. check key generation  
	// 
	notify_elapse_time( );



	if( time_index % adv_index == 0 ) {
		// re advertisement 
		kds_advertise_all( GDP_CMD_ADVERTISE ); 
	}

	if( time_index % sch_index == 0 ) {
		// re check ksd related info  
		check_info_state( );
	}

	if( time_index == max_val ) time_index = 0;
}


/*
**  Do shutdown at exit
*/
// need to consider LATER
void
shutdown_req(gdp_req_t *req)
{
	if (ep_dbg_test(Dbg, 59))
	{
		ep_dbg_printf("shutdown_req: ");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	if (EP_UT_BITSET(GDP_REQ_SRV_SUBSCR, req->flags))
		sub_send_message_notification(req, NULL, GDP_NAK_S_LOSTSUB);
} 

// need to consider LATER
void
ksd_shutdown(void)
{
	if (ep_adm_getboolparam("kdistd.shutdown.flushcache", true))
	{
		ep_dbg_cprintf(Dbg, 1, "\n\n*** Shutting down GCL cache ***\n");
		_gdp_gcl_cache_shutdown(&shutdown_req);
	}


	ep_dbg_cprintf(Dbg, 1, "\n\n*** Withdrawing all advertisements ***\n");
	kds_advertise_all(GDP_CMD_WITHDRAW);


	exit_ks_info_manager(); 
	exit_session_manager(); 
}


// LATER print the statistics information for debugging... 
void show_statistics()
{
	printf("[STATISTICS INFO] \n");
}

/*
**  SIGTERM --- called on interrupt or kill to do clean shutdown
*/  
void sigterm(int sig)
{
	signal(sig, SIG_DFL);
	ep_sd_notifyf("STOPPING=1\nSTATUS=Terminating on signal %d\n", sig);
	ep_log(EP_STAT_OK, "Terminating on signal %d", sig);
	if (ep_dbg_test(Dbg, 1))
		_gdp_dump_state(GDP_PR_DETAILED);

//	show_statistics();

	exit(EX_UNAVAILABLE);		// this will do cleanup
}


/*
**  SIGABORT --- called on quit or abort to dump state
*/  
void sigabort(int sig)
{
	signal(sig, SIG_DFL);
	ep_sd_notifyf("STOPPING=1\nSTATUS=Aborting on signal %d\n", sig);
	ep_log(EP_STAT_ABORT, "Aborting on signal %d", sig);
	_gdp_dump_state(GDP_PR_DETAILED);

//	show_statistics();

	kill(getpid(), sig);		// this will not do cleanup
}


/*
**  Dump state when an assertion fails.
*/
static void assertion_dump(void)
{
	_gdp_dump_state(GDP_PR_DETAILED);
}


/*
static uint32_t
sig_strictness(const char *s)
{
	uint32_t strictness = 0;

	while (*s != '\0')
	{
		while (isspace(*s) || ispunct(*s))
			s++;
		switch (*s)
		{
			case 'v':
			case 'V':
				strictness |= GDP_SIG_MUSTVERIFY;
				break;

			case 'r':
			case 'R':
				strictness |= GDP_SIG_REQUIRED;
				break;

			case 'p':
			case 'P':
				strictness |= GDP_SIG_PUBKEYREQ;
				break;
		}

		while (isalnum(*++s))
			continue;
	}

	return strictness;
}
*/

// OPTIONS: D:FG:n:N:s? 
static void
usage(const char *err)
{
	fprintf(stderr,
			"Usage error: %s\n"
			"Usage: %s [-D dbgspec] [-G router_addr] [-n nworkers]\n"
			"\t[-N myname] [-s strictness]\n"
			"    -D  set debugging flags\n"
			"    -G  IP host to contact for gdp router\n"
			"    -n  number of worker threads\n"
			"    -N  set my GDP name (address)\n"
			"    -s  set signature strictness; comma-separated subflags are:\n"
			"\t    verify (if present, signature must verify)\n"
			"\t    required (signature must be included)\n"
			"\t    pubkey (public key must be present)\n",
			err, ep_app_getprogname());
	exit(EX_USAGE);
}



// Server for GDP writer and readers. 
// Client in GDP Log servers to handle the ac and key logs 
/*
**  MAIN!
**
**		XXX	Currently always runs in foreground.  This will change
**			to run in background unless -D or -F are specified.
**			Running in background should probably also turn off
**			SIGINFO, since it doesn't make sense in that context.
*/

// write_start
int main(int argc, char **argv)
{
	int				opt;
	EP_STAT			estat;

	int				nworkers		= -1;
	const char		*phase;
	const char		*router_addr	= NULL;
	const char		*myname			= NULL;
	const char		*progname;



// OPTIONS: G:N:s? 
	while ((opt = getopt(argc, argv, "D:G:n:N:s:")) > 0)
	{
		switch (opt)
		{
		case 'D':
			ep_dbg_set(optarg);
			break;

		case 'G':
			router_addr = optarg;
			break;

		case 'n':
			nworkers = atoi(optarg);
			break;

		case 'N':
			myname = optarg;
			break;

		case 's':
//			GdpSignatureStrictness |= sig_strictness(optarg);
			break;

		default:
			usage("unknown flag");
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage("no positional arguments allowed");


	/*
	**  Do initialization.  This is very order-sensitive
	*/
	// initialize GDP and the EVENT library
	// the part for gdp_init 
	phase = "gdp library";
	estat = gdp_lib_init(myname);
	EP_STAT_CHECK(estat, goto fail0); 


	// 
	// Initialize key distribution service. 
	// 

	// 1. load the info of logs to be handled in this key distribution service 
	//    This service acts as Server for GDP writer and readers 
	//			to get the key for log. 
	//    In addition, this service acts as Client in GDP Log servers 
	//			to handle the ac and key logs 
	if( init_ks_info_before_chopen() != EX_OK ) {
		goto fail0;
	}


	//
	// 2. Initialization routine for key distribution service 
	// EX> Protocol module  / flags or variables... 
	//

	// initialize the protocol module
	phase = "Key service Daemon protocol module";
	estat = ksd_proto_init();
	EP_STAT_CHECK(estat, goto fail0);

/*

	// set up signature strictness
	{
		char argname[100];
		const char *p;

		snprintf(argname, sizeof argname, "swarm.%s.crypto.strictness",
				progname);
		p = ep_adm_getstrparam(argname, "v");
		GdpSignatureStrictness |= sig_strictness(p);
	}
	ep_dbg_cprintf(Dbg, 8, "Signature strictness = 0x%x\n",
			GdpSignatureStrictness);
*/


	//
	// 3. channel open with routing name  
	// 
	progname = ep_app_getprogname();

	// print our name as a reminder
	if (myname == NULL)
	{
		if (progname != NULL)
		{
			char argname[100];

			snprintf(argname, sizeof argname, "swarm.%s.gdpname", progname);
			myname = ep_adm_getstrparam(argname, NULL);
		}
	}

	{
		gdp_pname_t pname;

		if (myname == NULL)
			myname = gdp_printable_name(_GdpMyRoutingName, pname);

		fprintf(stdout, "My GDP routing name = %s\n", myname);
	}

	// initialize the thread pool
	ep_thr_pool_init(nworkers, nworkers, 0);


	// do cleanup on termination
	signal(SIGINT,  sigterm);
	signal(SIGTERM, sigterm);
	signal(SIGQUIT, sigabort);
	signal(SIGABRT, sigabort);

	// dump state on assertion failure
	EpAssertInfo = assertion_dump;

	// arrange to clean up resources periodically
	_gdp_reclaim_resources_init(&keydistd_reclaim_resources_callback);

	// 
	// Channel open 
	// LATER: if necessary, new function ( _extended_pdu_process )
	//         including _gdp_pdu_process 
	//		if PDU cannot support all types to handle key service. 

	// next part for gdp_init 
	void _gdp_pdu_process(gdp_pdu_t *, gdp_chan_t *);
	phase = "connection to router";
	_GdpChannel = NULL;

	// At current time, (after called chan_open), 
	//	 when occuring an input event (receiving pdu), 
	//			_gdp_pdu_process is called and then 
	//	On receiving command, ep_thr_pool_run is called. 
	//			pool_run insert the thread_work (gdp_pdu_proc_cmd, pdu) 
	//					and signal has_work...  
	//  On receiving response, gdp_pdu_proc_resp is called. 
	estat = _gdp_chan_open(router_addr, _gdp_pdu_process, &_GdpChannel);
	EP_STAT_CHECK(estat, goto fail0);
	_GdpChannel->close_cb = &kdistd_sock_close_cb;

	// start the event loop
	phase = "start event loop";
	estat = _gdp_evloop_init();
	EP_STAT_CHECK(estat, goto fail0);
	// end of second part for gdp_init 


	//
	//  4. Initialize the AC & key info for logs managed in this service
	// 
	load_ks_info();


	// Third part for gdp_init 
	// 5. advertise all of our GCLs (related with key service of each log) 
	// advertise: all KS_log* handled in this service  
	_GdpChannel->advertise = &kds_advertise_all;  
	phase = "advertise GCLs";
	estat = kds_advertise_all(GDP_CMD_ADVERTISE);  
	EP_STAT_CHECK(estat, goto fail0);
	// end of Third part for gdp_init 



	// arrange to periodic action including re-advertising
	{
		basic_period = ep_adm_getlongparam( 
								"swarm.kdistd.basic.interval", 60 );

		long t_intvl = ep_adm_getlongparam(
								"swarm.kdistd.advertise.interval", 150);
		
		adv_index = t_intvl / basic_period ; 
		if( basic_period * adv_index < t_intvl ) adv_index++;


		t_intvl = ep_adm_getlongparam( 
								"swarm.kdistd.recheck.interval", 300 );

		sch_index = t_intvl / basic_period ; 
		if( basic_period * sch_index < t_intvl ) sch_index++;


		struct event *advtimer = event_new(GdpIoEventBase, -1, EV_PERSIST,
											&work_periodically, NULL);
		struct timeval tv = { basic_period, 0 };
		event_add(advtimer, &tv);
	}

	// arrange for clean shutdown
	atexit(&ksd_shutdown);  // MUST LATER 
	// MUST be called exit_ks_info_manager() 

// write_end

	/*
	**  At this point we should be running
	*/

	ep_sd_notifyf("READY=1\n");

	pthread_join(_GdpIoEventLoopThread, NULL);

	// should never get here
	ep_sd_notifyf("STOPPING=1\n");
	ep_app_fatal("Fell out of GdpIoEventLoopThread");


fail0:
	{
		char ebuf[100];

		ep_app_fatal("Cannot initialize %s:\n\t%s",
				phase,
		        ep_stat_tostr(estat, ebuf, sizeof ebuf));

		exit_ks_info_manager(); 
	} 
}
