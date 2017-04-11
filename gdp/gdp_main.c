/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  GDP Initialization and main event loop.
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
#include "gdp_event.h"
#include "gdp_priv.h"
#include "gdp_version.h"

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_log.h>
#include <ep/ep_syslog.h>

#include <event2/buffer.h>
#include <event2/thread.h>

#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.main", "GDP initialization and main loop");
static EP_DBG	DbgEvLock = EP_DBG_INIT("gdp.libevent.locks", "GDP libevent lock debugging");
static EP_DBG	DbgProcResp = EP_DBG_INIT("gdp.response", "GDP response processing");

struct event_base	*GdpIoEventBase;	// the base for GDP I/O events
gdp_name_t			_GdpMyRoutingName;	// source name for PDUs
bool				_GdpLibInitialized;	// are we initialized?
gdp_chan_t			*_GdpChannel;		// our primary app-level protocol port


/*
**  INIT_ERROR --- issue error on initialization problem
*/

static EP_STAT
init_error(const char *datum, const char *where)
{
	EP_STAT estat = ep_stat_from_errno(errno);
	char nbuf[40];

	strerror_r(errno, nbuf, sizeof nbuf);
	ep_log(estat, "gdp_init: %s: %s", where, datum);
	ep_app_error("gdp_init: %s: %s: %s", where, datum, nbuf);
	return estat;
}


static int
acknak_from_estat(EP_STAT estat, int def)
{
	int resp = def;

	if (EP_STAT_ISOK(estat))
	{
		if (def < GDP_ACK_MIN || def > GDP_ACK_MAX)
			resp = GDP_ACK_SUCCESS;
	}
	else
	{
		if (def < GDP_NAK_C_MIN || def > GDP_NAK_R_MAX)
			resp = GDP_NAK_S_INTERNAL;		// default to panic code
	}

	// if the estat contains the detail, prefer that
	if (EP_STAT_REGISTRY(estat) == EP_REGISTRY_UCB &&
		EP_STAT_MODULE(estat) == GDP_MODULE)
	{
		int d = EP_STAT_DETAIL(estat);

		if (EP_STAT_ISOK(estat))
		{
			if (d >= 200 && d < (200 + GDP_ACK_MAX - GDP_ACK_MIN))
				resp = d - 200 + GDP_ACK_MIN;
		}
		else if (d >= 400 && d < (400 + GDP_NAK_C_MAX - GDP_NAK_C_MIN))
			resp = d - 400 + GDP_NAK_C_MIN;
		else if (d >= 500 && d < (500 + GDP_NAK_S_MAX - GDP_NAK_S_MIN))
				resp = d - 500 + GDP_NAK_S_MIN;
		else if (d >= 600 && d < (600 + GDP_NAK_R_MAX - GDP_NAK_R_MIN))
			resp = d - 600 + GDP_NAK_R_MIN;
	}

	if (ep_dbg_test(Dbg, 41))
	{
		char ebuf[100];

		ep_dbg_printf("acknak_from_estat: %s -> %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf),
				_gdp_proto_cmd_name(resp));
	}
	return resp;
}


/*
**  GDP_PDU_PROC_CMD --- process command PDU
**
**		Usually done in a thread since it may be heavy weight.
*/

static void
gdp_pdu_proc_cmd(void *cpdu_)
{
	gdp_pdu_t *cpdu = cpdu_;
	int cmd = cpdu->cmd;
	EP_STAT estat;
	gdp_gcl_t *gcl;
	gdp_req_t *req = NULL;
	int resp;

	ep_dbg_cprintf(Dbg, 40,
			"gdp_pdu_proc_cmd(%s, thread %p)\n",
			_gdp_proto_cmd_name(cmd), (void *) ep_thr_gettid());

	gcl = _gdp_gcl_cache_get(cpdu->dst, 0);
	if (gcl != NULL)
		EP_THR_MUTEX_ASSERT_ISLOCKED(&gcl->mutex, );

	ep_dbg_cprintf(Dbg, 43,
			"gdp_pdu_proc_cmd: allocating new req for GCL %p\n", gcl);
	estat = _gdp_req_new(cmd, gcl, cpdu->chan, cpdu, GDP_REQ_CORE, &req);
	EP_STAT_CHECK(estat, goto fail0);
	EP_THR_MUTEX_ASSERT_ISLOCKED(&req->mutex, );

	ep_dbg_cprintf(Dbg, 40, "gdp_pdu_proc_cmd >>> req=%p\n", req);

	// if command will need an ack, allocate space for that PDU
	if (GDP_CMD_NEEDS_ACK(cmd))
		req->rpdu = _gdp_pdu_new();

	// swap source and destination for command response
	if (req->rpdu != NULL)
	{
		memcpy(req->rpdu->src, req->cpdu->dst, sizeof req->rpdu->src);
		memcpy(req->rpdu->dst, req->cpdu->src, sizeof req->rpdu->dst);
		req->rpdu->rid = req->cpdu->rid;
	}

	estat = _gdp_req_dispatch(req, cmd);
	if (ep_dbg_test(Dbg, 59))
	{
		ep_dbg_printf("gdp_pdu_proc_cmd: after dispatch, ");
		_gdp_req_dump(req, ep_dbg_getfile(), 0, 0);
	}

	// figure out potential response code
	// we compute even if unused so we can log server errors
	resp = acknak_from_estat(estat, req->rpdu->cmd);

	if (resp >= GDP_NAK_S_MIN && resp <= GDP_NAK_S_MAX)
	{
		ep_log(estat, "gdp_pdu_proc_cmd(%s): server error",
				_gdp_proto_cmd_name(cmd));
	}

	// do sanity checks on signature
	if (req->rpdu->datum != NULL)
	{
		gdp_datum_t *datum = req->rpdu->datum;
		int siglen = 0;

		ep_thr_mutex_lock(&req->rpdu->datum->mutex);
		if (datum->sig != NULL)
			siglen = gdp_buf_getlength(datum->sig);
		if (siglen != datum->siglen)
		{
			ep_dbg_cprintf(Dbg, 1, "gdp_pdu_proc_cmd(%s): datum->siglen = %d, actual = %d\n",
					_gdp_proto_cmd_name(cmd),
					datum->siglen, siglen);
			datum->siglen = siglen;
		}
	}

	// send response PDU if appropriate
	if (req->rpdu != NULL)
	{
		ep_dbg_cprintf(Dbg, 47,
				"gdp_pdu_proc_cmd: sending %zd bytes\n",
				gdp_buf_getlength(req->rpdu->datum->dbuf));
		req->rpdu->cmd = resp;
		req->stat = _gdp_pdu_out(req->rpdu, req->chan, NULL);
		//XXX anything to do with estat here?
	}

	// do command post processing
	if (req->postproc)
	{
		ep_dbg_cprintf(Dbg, 44, "gdp_pdu_proc_cmd: doing post processing\n");
		(req->postproc)(req);
		req->postproc = NULL;
	}

	// free up resources
	if (req->rpdu->datum != NULL)
		ep_thr_mutex_unlock(&req->rpdu->datum->mutex);
	if (EP_UT_BITSET(GDP_REQ_CORE, req->flags) &&
			!EP_UT_BITSET(GDP_REQ_PERSIST, req->flags))
	{
		if (req->gcl != NULL)
			_gdp_gcl_unlock(req->gcl);
		_gdp_req_free(&req);
	}
	else
	{
		if (req->gcl != NULL && EP_UT_BITSET(GCLF_ISLOCKED, req->gcl->flags))
			_gdp_gcl_unlock(req->gcl);
		_gdp_req_unlock(req);
	}

	ep_dbg_cprintf(Dbg, 40, "gdp_pdu_proc_cmd <<< done\n");

	return;

fail0:
	ep_log(estat, "gdp_pdu_proc_cmd: cannot allocate request; dropping PDU");
	if (cpdu != NULL)
		_gdp_pdu_free(cpdu);
}


/*
**  Search for a request in a channel list.
**
**		This is a fall-back that should be used only for finding requests
**		sent by FWD_APPEND to a server that isn't accessible.  Since that
**		command links the request to a different GCL than the destination
**		address in the PDU, a NOROUTE response won't find it.
*/

static EP_STAT
find_req_in_channel_list(
				const gdp_pdu_t *rpdu,
				gdp_chan_t *chan,
				gdp_req_t **reqp)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;

	ep_dbg_cprintf(DbgProcResp, 14, "find_req_in_channel_list: searching\n");
	ep_thr_mutex_lock(&chan->mutex);
	LIST_FOREACH(req, &chan->reqs, chanlist)
	{
		if (req->rpdu != NULL &&
				req->rpdu->rid == rpdu->rid &&
				GDP_NAME_SAME(req->rpdu->src, rpdu->dst) &&
				GDP_NAME_SAME(req->rpdu->dst, rpdu->src))
			break;
	}
	if (ep_dbg_test(DbgProcResp, 40))
	{
		if (req == NULL)
			ep_dbg_printf("    ... not found\n");
		else
			ep_dbg_printf("    ... found req @ %p\n", req);
	}
	ep_thr_mutex_unlock(&chan->mutex);

	// request should be locked for symmetry with _gdp_req_find
	if (req != NULL)
	{
		char ebuf[100];
		estat = _gdp_req_lock(req);
		ep_dbg_cprintf(DbgProcResp, 44,
				"find_req_in_channel_list: _gdp_req_lock => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	*reqp = req;
	return estat;
}


/*
**  GDP_PDU_PROC_RESP --- process response (ack/nak) PDU
**
**		When this is called, the rpdu passed in will be the actual
**		PDU off the wire and req->cpdu should be the original command
**		PDU that prompted this response.  We save the passed rpdu
**		into req->rpdu for processing in _gdp_req_dispatch.
*/

static void
gdp_pdu_proc_resp(gdp_pdu_t *rpdu, gdp_chan_t *chan)
{
	int cmd = rpdu->cmd;
	EP_STAT estat = EP_STAT_OK;
	gdp_gcl_t *gcl;
	gdp_req_t *req = NULL;
	int resp;
	int ocmd;					// original command prompting this response

	gcl = _gdp_gcl_cache_get(rpdu->src, 0);
	ep_dbg_cprintf(DbgProcResp, 20,
			"gdp_pdu_proc_resp(%p %s) gcl %p\n",
			rpdu, _gdp_proto_cmd_name(cmd), gcl);
	if (gcl == NULL)
	{
		estat = find_req_in_channel_list(rpdu, chan, &req);

		if (!EP_STAT_ISOK(estat) || req == NULL)
		{
			if (ep_dbg_test(DbgProcResp,
						rpdu->cmd == GDP_NAK_R_NOROUTE ? 19 : 1))
			{
				gdp_pname_t pname;
				ep_dbg_printf("gdp_pdu_proc_resp: discarding %d (%s) PDU"
							" for unknown GCL\n",
							rpdu->cmd, _gdp_proto_cmd_name(rpdu->cmd));
				if (ep_dbg_test(DbgProcResp, 24))
					_gdp_pdu_dump(rpdu, ep_dbg_getfile());
				else
					ep_dbg_printf("    %s\n", gdp_printable_name(rpdu->src, pname));
			}
			_gdp_pdu_free(rpdu);
			return;
		}

		EP_ASSERT_ELSE(req->state != GDP_REQ_FREE, return);

		// remove the request from the GCL list it is already on
		// req is already locked by find_req_in_channel_list
		if (EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags))
		{
			LIST_REMOVE(req, gcllist);
			req->flags &= ~GDP_REQ_ON_GCL_LIST;

			// code below expects request to remain locked
		}

	}
	else
	{
		// find the corresponding request
		ep_dbg_cprintf(DbgProcResp, 23,
				"gdp_pdu_proc_resp: searching gcl %p for rid %" PRIgdp_rid "\n",
				gcl, rpdu->rid);
		req = _gdp_req_find(gcl, rpdu->rid);
		if (ep_dbg_test(DbgProcResp, 51))
		{
			ep_dbg_printf("... found ");
			_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		}

		// req is already locked by _gdp_req_find
		if (req == NULL)
		{
			// no req for incoming response --- "can't happen"
			if (ep_dbg_test(DbgProcResp, 1))
			{
				ep_dbg_printf("gdp_pdu_proc_resp: no req for incoming response\n");
				_gdp_pdu_dump(rpdu, ep_dbg_getfile());
				_gdp_gcl_dump(gcl, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
			}
			_gdp_pdu_free(rpdu);
			return;
		}
		else if (EP_ASSERT_TEST(req->state != GDP_REQ_FREE))
		{
			if (ep_dbg_test(DbgProcResp, 1))
			{
				ep_dbg_printf("gdp_pdu_proc_resp: trying to use free ");
				_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
			}
			_gdp_pdu_free(rpdu);
			return;
		}
		else if (rpdu == req->rpdu)
		{
			// this could be an assertion
			if (ep_dbg_test(DbgProcResp, 1))
			{
				ep_dbg_printf("gdp_pdu_proc_resp(%d): rpdu == req->rpdu\n",
							rpdu->cmd);
				_gdp_pdu_dump(rpdu, ep_dbg_getfile());
			}
		}
	}

	if (gcl != NULL)
		_gdp_gcl_unlock(gcl);

	if (req->cpdu == NULL)
	{
		ep_dbg_cprintf(DbgProcResp, 1,
				"gdp_pdu_proc_resp(%d): no corresponding command PDU\n",
				rpdu->cmd);
		ocmd = rpdu->cmd;
		//XXX return here?  with req->pdu == NULL, _gdp_req_dispatch
		//XXX will probably die
	}
	else
	{
		ocmd = req->cpdu->cmd;
	}

	// save the response PDU for further processing
	if (req->rpdu != NULL)
	{
		// this can happen in multiread/subscription and async I/O
		if (ep_dbg_test(DbgProcResp, 41))
		{
			ep_dbg_printf("gdp_pdu_proc_resp: req->rpdu already set\n    ");
			_gdp_pdu_dump(req->rpdu, ep_dbg_getfile());
		}
		_gdp_pdu_free(req->rpdu);
	}
	req->rpdu = rpdu;

	if (ep_dbg_test(DbgProcResp, 43))
	{
		ep_dbg_printf("gdp_pdu_proc_resp: ");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	// request is locked

	// mark this request as active (for subscriptions)
	ep_time_now(&req->act_ts);

	// do ack/nak specific processing
	estat = _gdp_req_dispatch(req, cmd);

	// figure out potential response code
	// we compute even if unused so we can log server errors
	resp = acknak_from_estat(estat, req->rpdu->cmd);

	if (ep_dbg_test(DbgProcResp,
				(resp >= GDP_NAK_S_MIN && resp <= GDP_NAK_S_MAX) ? 1 : 44))
	{
		char ebuf[100];

		ep_dbg_printf("gdp_pdu_proc_resp(%s for %s): %s\n",
				_gdp_proto_cmd_name(cmd),
				_gdp_proto_cmd_name(ocmd),
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		if (ep_dbg_test(DbgProcResp, 55))
			_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	// ASSERT(all data from chan has been consumed);

	if (EP_UT_BITSET(GDP_REQ_ASYNCIO, req->flags))
	{
		// send the status as an event
		estat = _gdp_event_add_from_req(req);

		// since this is asynchronous we can release the PDU
		_gdp_pdu_free(req->rpdu);
		req->rpdu = NULL;
	}
	else if (req->state == GDP_REQ_WAITING)
	{
		// return our status via the request
		req->stat = estat;
		req->flags |= GDP_REQ_DONE;

		// any further data or status is delivered via event
		req->flags |= GDP_REQ_ASYNCIO;

		if (ep_dbg_test(DbgProcResp, 40))
		{
			ep_dbg_printf("gdp_pdu_proc_resp: signaling ");
			_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		}

		// wake up invoker, which will return the status
		ep_thr_cond_signal(&req->cond);

		// give _gdp_invoke a chance to run; not necessary, but
		// avoids having to wait on condition variables
		ep_thr_yield();
	}
	else if (req->rpdu->cmd == GDP_NAK_R_NOROUTE)
	{
		// since this is common and expected, don't sully output
		ep_dbg_cprintf(DbgProcResp, 19,
				"gdp_pdu_proc_resp: discarding GDP_NAK_R_NOROUTE\n");
	}
	else if (ep_dbg_test(DbgProcResp, 1))
	{
		ep_dbg_printf("gdp_pdu_proc_resp: discarding response ");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	// free up resources
	if (EP_UT_BITSET(GDP_REQ_CORE, req->flags) &&
		!EP_UT_BITSET(GDP_REQ_PERSIST, req->flags))
	{
		_gdp_req_free(&req);
	}
	else
	{
		_gdp_req_unlock(req);
	}

	ep_dbg_cprintf(DbgProcResp, 40, "gdp_pdu_proc_resp <<< done\n");
}


/*
**  _GDP_PDU_PROCESS --- process a PDU
**
**		This is responsible for the lightweight stuff that can happen
**		in the I/O thread, such as matching an ack/nak PDU with the
**		corresponding req.  It should never block.  The heavy lifting
**		is done in the routine above.
*/

void
_gdp_pdu_process(gdp_pdu_t *pdu, gdp_chan_t *chan)
{
	// cmd: dispatch in thread; ack: dispatch directly
	if (GDP_CMD_IS_COMMAND(pdu->cmd))
		ep_thr_pool_run(&gdp_pdu_proc_cmd, pdu);
	else
		gdp_pdu_proc_resp(pdu, chan);
}


/*
**	Base loop to be called for event-driven systems.
**	Their events should have already been added to the event base.
**
**		GdpIoEventLoopThread is also used by gdplogd, hence non-static.
*/

EP_THR			_GdpIoEventLoopThread;

// to ensure event loop is running before we proceed
bool					GdpIoEventLoopRunning		= false;
static EP_THR_MUTEX		GdpIoEventLoopRunningMutex	EP_THR_MUTEX_INITIALIZER;
static EP_THR_COND		GdpIoEventLoopRunningCond	EP_THR_COND_INITIALIZER;

static void
event_loop_timeout(int fd, short what, void *eli_)
{
	struct event_loop_info *eli = eli_;

	ep_dbg_cprintf(Dbg, 79, "%s event loop timeout\n", eli->where);
}

static void *
run_event_loop(void *eli_)
{
	struct event_loop_info *eli = eli_;
	struct event_base *evb = eli->evb;
	long evdelay = ep_adm_getlongparam("swarm.gdp.event.loopdelay", 100000L);

	// keep the loop alive if EVLOOP_NO_EXIT_ON_EMPTY isn't available
	long ev_timeout = ep_adm_getlongparam("swarm.gdp.event.looptimeout", 30L);
	struct timeval tv = { ev_timeout, 0 };
	struct event *evtimer = event_new(evb, -1, EV_PERSIST,
			&event_loop_timeout, eli);
	event_add(evtimer, &tv);

	ep_thr_mutex_lock(&GdpIoEventLoopRunningMutex);
	GdpIoEventLoopRunning = true;
	ep_thr_cond_broadcast(&GdpIoEventLoopRunningCond);
	ep_thr_mutex_unlock(&GdpIoEventLoopRunningMutex);

	for (;;)
	{
		if (ep_dbg_test(Dbg, 20))
		{
			ep_dbg_printf("gdp_event_loop: starting up %s base loop\n",
					eli->where);
			event_base_dump_events(evb, ep_dbg_getfile());
		}

#ifdef EVLOOP_NO_EXIT_ON_EMPTY
		event_base_loop(evb, EVLOOP_NO_EXIT_ON_EMPTY);
#else
		event_base_loop(evb, 0);
#endif

		if (ep_dbg_test(Dbg, 1))
		{
			ep_dbg_printf("gdp_event_loop: %s event_base_loop returned\n",
					eli->where);
			if (event_base_got_break(evb))
				ep_dbg_printf(" ... as a result of loopbreak\n");
			if (event_base_got_exit(evb))
				ep_dbg_printf(" ... as a result of loopexit\n");
		}
		if (event_base_got_exit(evb))
		{
			// the GDP daemon went away
			break;
		}

		if (evdelay > 0)
			ep_time_nanosleep(evdelay * 1000LL);		// avoid CPU hogging
	}

	ep_log(GDP_STAT_DEAD_DAEMON, "lost channel to gdp");
	ep_app_abort("lost channel to gdp");
}

static EP_STAT
_gdp_start_event_loop_thread(EP_THR *thr,
		struct event_base *evb,
		const char *where)
{
	struct event_loop_info *eli = ep_mem_malloc(sizeof *eli);

	eli->evb = evb;
	eli->where = where;
	if (ep_thr_spawn(thr, run_event_loop, eli) != 0)
		return init_error("cannot create event loop thread", where);
	else
		return EP_STAT_OK;
}


/*
**   Logging callback for event library (for debugging).
*/

static EP_DBG	EvlibDbg = EP_DBG_INIT("gdp.evlib", "GDP Eventlib");

static void
evlib_log_cb(int severity, const char *msg)
{
	char *sev;
	char *sevstrings[] = { "debug", "msg", "warn", "error" };

	if (severity < 0 || severity > 3)
		sev = "?";
	else
		sev = sevstrings[severity];
	ep_dbg_cprintf(EvlibDbg, ((4 - severity) * 20) + 2, "[%s] %s\n", sev, msg);
}


/*
**  Arrange to call atexit(3) functions on SIGINT and SIGTERM
*/

static void
exit_on_signal(int sig)
{
	fprintf(stderr, "\nExiting on signal %d\n", sig);
	exit(sig);
}


/*
**  Change user id to something innocuous.
*/

static void
run_as(const char *runasuser)
{
	if (runasuser != NULL && *runasuser != '\0')
	{
		uid_t uid;
		gid_t gid;
		struct passwd *pw = getpwnam(runasuser);
		if (pw == NULL)
		{
			ep_app_warn("User %s unknown; running as 1:1 (daemon)",
					runasuser);
			gid = 1;
			uid = 1;
		}
		else
		{
			gid = pw->pw_gid;
			uid = pw->pw_uid;
		}
		if (setgid(gid) < 0 || setuid(uid) < 0)
			ep_app_warn("Cannot set user/group id (%d:%d)", uid, gid);
	}
}


/*
**  Initialization, Part 1:
**		Initialize the various external libraries.
**		Set up the I/O event loop base.
**		Initialize the GCL cache.
**		Start the event loop.
*/

// locks out multiple calls to gdp_lib_init
static EP_THR_MUTEX		GdpInitMutex	EP_THR_MUTEX_INITIALIZER;

EP_STAT
gdp_lib_init(const char *myname)
{
	EP_STAT estat = EP_STAT_OK;
	const char *progname;
	static bool initialized = false;

	ep_dbg_cprintf(Dbg, 4, "_gdp_lib_init(%s)\n\t%s\n",
			myname == NULL ? "NULL" : myname,
			GdpVersion);

	// initialize the EP library
	ep_lib_init(EP_LIB_USEPTHREADS);

	ep_thr_mutex_lock(&GdpInitMutex);
	if (initialized)
		goto done;

	if (ep_dbg_test(EvlibDbg, 80))
	{
		// according to the book...
		//event_enable_debug_logging(EVENT_DBG_ALL);
		// according to the code...
		event_enable_debug_mode();
	}

	ep_adm_readparams("gdp");
	progname = ep_app_getprogname();
	if (progname != NULL)
		ep_adm_readparams(progname);
	ep_crypto_init(0);

	// clear out spurious errors
	errno = 0;

	// we can now re-adjust debugging
	ep_dbg_setfile(NULL);

	// arrange to call atexit(3) functions on SIGTERM
	if (ep_adm_getboolparam("swarm.gdp.catch.sigint", true))
		(void) signal(SIGINT, exit_on_signal);
	if (ep_adm_getboolparam("swarm.gdp.catch.sigterm", true))
		(void) signal(SIGTERM, exit_on_signal);

	// get assertion behavior information
	EpAssertAllAbort = ep_adm_getboolparam("swarm.gdp.debug.assert.allabort",
									false);

	// register status strings
	_gdp_stat_init();

	// figure out or generate our name (for routing)
	if (myname == NULL && progname != NULL)
	{
		char argname[100];

		snprintf(argname, sizeof argname, "swarm.%s.gdpname", progname);
		myname = ep_adm_getstrparam(argname, NULL);
	}

	if (myname != NULL)
	{
		gdp_pname_t pname;

		estat = gdp_parse_name(myname, _GdpMyRoutingName);
		ep_dbg_cprintf(Dbg, 19, "Setting my name:\n\t%s\n\t%s\n",
				myname, gdp_printable_name(_GdpMyRoutingName, pname));
		EP_STAT_CHECK(estat, myname = NULL);
	}

	if (!gdp_name_is_valid(_GdpMyRoutingName))
		_gdp_newname(_GdpMyRoutingName, NULL);

	// avoid running as root if possible (and another user specified)
	if (progname != NULL)
	{
		char argname[100];
		const char *logfac;

		if (getuid() == 0)
		{
			snprintf(argname, sizeof argname, "swarm.%s.runasuser", progname);
			run_as(ep_adm_getstrparam(argname, NULL));
		}

		// allow log facilities on a per-app basis
		snprintf(argname, sizeof argname, "swarm.%s.syslog.facility", progname);
		logfac = ep_adm_getstrparam(argname, NULL);
		if (logfac == NULL)
			logfac = ep_adm_getstrparam("swarm.gdp.syslog.facility", "local4");
		ep_log_init(progname, ep_syslog_fac_from_name(logfac), stderr);
	}

	if (getuid() == 0)
		run_as(ep_adm_getstrparam("swarm.gdp.runasuser", NULL));

	if (ep_dbg_test(Dbg, 1))
	{
		gdp_pname_t pname;

		ep_dbg_printf("My GDP routing name = %s\n",
				gdp_printable_name(_GdpMyRoutingName, pname));
	}

	// initialize the GCL cache.  In theory this "cannot fail"
	estat = _gdp_gcl_cache_init();
	EP_STAT_CHECK(estat, goto fail0);

	// tell the event library that we're using pthreads
	if (evthread_use_pthreads() < 0)
		return init_error("cannot use pthreads", "gdp_lib_init");
	if (ep_dbg_test(DbgEvLock, 90))
	{
		evthread_enable_lock_debuging();
	}

	// use our debugging printer
	event_set_log_callback(evlib_log_cb);

	// set up the event base
	if (GdpIoEventBase == NULL)
	{
		// Initialize for I/O events
		struct event_config *ev_cfg = event_config_new();

		event_config_require_features(ev_cfg, 0);
		GdpIoEventBase = event_base_new_with_config(ev_cfg);
		if (GdpIoEventBase == NULL)
			estat = init_error("could not create event base", "gdp_lib_init");
		event_config_free(ev_cfg);
		EP_STAT_CHECK(estat, goto fail0);
	}

done:
	{
		char ebuf[200];

		ep_dbg_cprintf(Dbg, 4, "gdp_lib_init: %s\n",
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

fail0:
	initialized = true;
	ep_thr_mutex_unlock(&GdpInitMutex);
	return estat;
}


EP_STAT
_gdp_evloop_init(void)
{
	EP_STAT estat;

	// set up synchronization for event loop thread startup
	ep_thr_mutex_lock(&GdpIoEventLoopRunningMutex);

	if (!GdpIoEventLoopRunning)
	{
		// create a thread to run the event loop
		estat = _gdp_start_event_loop_thread(&_GdpIoEventLoopThread,
											GdpIoEventBase, "I/O");
	}

	while (!GdpIoEventLoopRunning)
		ep_thr_cond_wait(&GdpIoEventLoopRunningCond,
						&GdpIoEventLoopRunningMutex, NULL);
	ep_thr_mutex_unlock(&GdpIoEventLoopRunningMutex);

	return estat;
}
