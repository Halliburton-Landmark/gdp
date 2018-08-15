/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  GDP_EVENT.C --- event handling
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

#include <ep/ep_thr.h>
#include <ep/ep_dbg.h>
#include <ep/ep_funclist.h>
#include <ep/ep_log.h>

#include "gdp.h"
#include "gdp_priv.h"
#include "gdp_event.h"

#include <string.h>
#include <sys/errno.h>


static EP_DBG	Dbg = EP_DBG_INIT("gdp.event", "GDP event handling");


// free (unused) events
static EP_THR_MUTEX		FreeListMutex	EP_THR_MUTEX_INITIALIZER2(GDP_MUTEX_LORDER_LEAF);
static struct gev_list	FreeList		= TAILQ_HEAD_INITIALIZER(FreeList);

// active events (synchronous, ready for gdp_event_next)
static EP_THR_MUTEX		ActiveListMutex	EP_THR_MUTEX_INITIALIZER2(GDP_MUTEX_LORDER_LEAF);
static EP_THR_COND		ActiveListSig	EP_THR_COND_INITIALIZER;
static struct gev_list	ActiveList		= TAILQ_HEAD_INITIALIZER(ActiveList);

// callback events (asynchronous, ready for delivery in callback thread)
static EP_THR_MUTEX		CallbackListMutex	EP_THR_MUTEX_INITIALIZER2(GDP_MUTEX_LORDER_LEAF);
static EP_THR_COND		CallbackListSig		EP_THR_COND_INITIALIZER;
static struct gev_list	CallbackList		= TAILQ_HEAD_INITIALIZER(CallbackList);
static EP_THR			CallbackThread;
static bool				CallbackThreadStarted	= false;


/*
**  Create a new event.
*/

static EP_STAT
_gdp_event_new(gdp_event_t **gevp)
{
	gdp_event_t *gev = NULL;

	for (;;)
	{
		ep_thr_mutex_lock(&FreeListMutex);
		if ((gev = TAILQ_FIRST(&FreeList)) != NULL)
			TAILQ_REMOVE(&FreeList, gev, queue);
		ep_thr_mutex_unlock(&FreeListMutex);
		if (gev == NULL || gev->type == _GDP_EVENT_FREE)
			break;

		// error: abandon this event
		EP_ASSERT_PRINT("_gdp_event_new: allocated event %p on free list", gev);
	}
	if (gev == NULL)
	{
		gev = (gdp_event_t *) ep_mem_zalloc(sizeof *gev);
	}
	VALGRIND_HG_CLEAN_MEMORY(gev, sizeof *gev);
	*gevp = gev;
	ep_dbg_cprintf(Dbg, 48, "_gdp_event_new => %p\n", gev);
	return EP_STAT_OK;
}


/*
**  Free an event.
**		The client using the event interface must call this.
**		Clients using callbacks do not need this and MUST NOT
**			attempt to do so.
*/

EP_STAT
gdp_event_free(gdp_event_t *gev)
{
	if (!EP_ASSERT_POINTER_VALID(gev))
		return EP_STAT_NULL_POINTER;

	ep_dbg_cprintf(Dbg, 48, "gdp_event_free(%p)\n", gev);
	if (!EP_ASSERT(gev->type != _GDP_EVENT_FREE))
	{
		// apparently this is already free
		return EP_STAT_ASSERT_ABORT;
	}

	gev->type = _GDP_EVENT_FREE;
	if (gev->datum != NULL)
		gdp_datum_free(gev->datum);
	gev->datum = NULL;
#if GDP_DEBUG_NO_FREE_LISTS		// avoid helgrind complaints
	ep_mem_free(gev);
#else
	ep_thr_mutex_lock(&FreeListMutex);
	TAILQ_INSERT_HEAD(&FreeList, gev, queue);
	ep_thr_mutex_unlock(&FreeListMutex);
#endif
	return EP_STAT_OK;
}


/*
**  Return next event.
**		Optionally, specify a GCL that must match and/or a timeout.
**		If the timeout is zero this acts like a poll.
*/

gdp_event_t *
gdp_event_next(gdp_gin_t *gin, EP_TIME_SPEC *timeout)
{
	gdp_event_t *gev;
	EP_TIME_SPEC *abs_to = NULL;
	EP_TIME_SPEC tv;

	ep_dbg_cprintf(Dbg, 59, "gdp_event_next: gin %p\n", gin);

	if (timeout != NULL)
	{
		ep_time_deltanow(timeout, &tv);
		abs_to = &tv;
	}

	ep_thr_mutex_lock(&ActiveListMutex);
restart:
	for (;;)
	{
		int err;

		while ((gev = TAILQ_FIRST(&ActiveList)) == NULL)
		{
			// wait until we have at least one thing to try
			ep_dbg_cprintf(Dbg, 58, "gdp_event_next: empty ActiveList; waiting\n");
			err = ep_thr_cond_wait(&ActiveListSig, &ActiveListMutex, abs_to);
			ep_dbg_cprintf(Dbg, 58, "gdp_event_next: ep_thr_cond_wait => %d\n",
					err);
			if (err != 0)
			{
				if (err != ETIMEDOUT)
				{
					char errno_buf[40];

					(void) (0 == strerror_r(err, errno_buf, sizeof errno_buf));
					EP_ASSERT_PRINT("gdp_event_next: ep_thr_cond_wait => %s",
							errno_buf);
				}
				goto fail0;
			}
		}
		while (gev != NULL)
		{
			// if this isn't the GCL we want, keep searching the list
			if (gin == NULL || gev->gin == gin)
				break;

			// not the event we want
			gev = TAILQ_NEXT(gev, queue);
		}

		if (gev != NULL)
		{
			// found a match!
			break;
		}
	}

	if (gev != NULL)
	{
		TAILQ_REMOVE(&ActiveList, gev, queue);
		if (!EP_ASSERT(gev->type != _GDP_EVENT_FREE))
		{
			// bad news, this event is on two lists (Active and Free)
			goto restart;
		}
	}
fail0:
	ep_thr_mutex_unlock(&ActiveListMutex);

	// the callback must call gdp_event_free(gev)
	ep_dbg_cprintf(Dbg, 52, "gdp_event_next => %p\n", gev);
	return gev;
}


/*
**  Free all events associated with a given GCL.
**		Used when closing a log.
*/

EP_STAT
_gdp_event_free_all(gdp_gin_t *gin)
{
	gdp_event_t *gev, *next_gev;

	GDP_GIN_CHECK_RETURN_STAT(gin);

	ep_thr_mutex_lock(&ActiveListMutex);
	for (gev = TAILQ_FIRST(&ActiveList); gev != NULL; gev = next_gev)
	{
		next_gev = TAILQ_NEXT(gev, queue);
		if (gev->gin != gin)
			continue;
		else if (gev->type == _GDP_EVENT_FREE)
			TAILQ_REMOVE(&ActiveList, gev, queue);
		else
			gdp_event_free(gev);
	}
	ep_thr_mutex_unlock(&ActiveListMutex);

	return EP_STAT_OK;
}


/*
**  Insert an event into a queue.
**		This is essentially an insertion sort.
**		The list (or other enclosing data structure) must be locked
**			before this is called.
**		TODO: This should use a TAILQ and start searching from the
**			tail for a fit so that a merge of two sorted lists is
**			more efficient.  Even better would be to just concatenate
**			them.
*/

static void
insert_event(gdp_event_t *gev,
		struct gev_list *list)
{
	EP_ASSERT_POINTER_VALID(gev);

	if (ep_dbg_test(Dbg, 48))
	{
		const char *where;

		if (list == &ActiveList)
			where = "active";
		else if (list == &CallbackList)
			where = "callback";
		else
			where = "deferred";
		ep_dbg_printf("_gdp_event_trigger: adding event %p (%d) to %s list\n",
				gev, gev->type, where);
	}
	if (gev->type == _GDP_EVENT_FREE)
	{
		ep_dbg_cprintf(Dbg, 1, "_gdp_event_trigger(%p): event is free\n", gev);
		return;
	}

	// sort the event into the correct place in the list
	gdp_event_t *next_ev;
	if (gev->type == GDP_EVENT_DONE)
	{
		// done ("no more results") always goes at end of list
		gev->sortkey = UINT64_MAX;
		TAILQ_INSERT_TAIL(list, gev, queue);
	}
	else if ((next_ev = TAILQ_LAST(list, gev_list)) == NULL ||
			next_ev->sortkey < gev->sortkey)
	{
		TAILQ_INSERT_TAIL(list, gev, queue);
	}
	else
	{
		// find correct position in list
		gdp_event_t *this_ev = next_ev;
		while ((next_ev = TAILQ_PREV(this_ev, gev_list, queue)) != NULL &&
				next_ev->sortkey < gev->sortkey)
			this_ev = next_ev;
		TAILQ_INSERT_BEFORE(this_ev, gev, queue);
	}
}


/*
**  Handle timeout for GDP_EVENT_DONE events.
**		If a DONE PDU comes in before all data has been received,
**		the event is saved.  But if nothing else happens before this
**		timeout, assume that nothing else will come.
*/

static void
done_timeout(int unused, short what, void *req_)
{
	gdp_req_t *req = req_;

	if (req->state == GDP_REQ_WAITING)
		return;
	_gdp_event_insert_pending(&req->events, req);
}


/*
**  Trigger an event (i.e., add to an event queue).
**		There are two lists, depending on whether a callback was specified.
**
**	This is a bit tricky because of asynchronous results, which may
**	appear out of order.  In particular, we want to avoid returning the
**	"done" event since it really means "no more results" if there are
**	actually going to be more results appearing.  When we do get the
**	"done" event, it should contain the number of results that
**	preceded it.  If we haven't seen that many results yet we'll need
**	to tuck it away for a while.
*/

#define DONE_TIMEOUT	5000		// in microseconds

static void
_gdp_event_trigger(gdp_event_t *gev, gdp_req_t *req)
{
	if (gev->type == GDP_EVENT_DONE &&
			EP_UT_BITSET(GDP_REQ_PERSIST, req->flags))
	{
		// more results will come later
		static int32_t timeout = -1;

		if (timeout < 0)
			timeout = ep_adm_getlongparam("swarm.gdp.timeout.done",
											DONE_TIMEOUT);
		insert_event(gev, &req->events);
		if (timeout > 0)
			req->ev_to = _gdp_evloop_timer_set(timeout, &done_timeout, req);
	}
	else if (gev->cb == NULL)
	{
		ep_thr_mutex_lock(&ActiveListMutex);
		insert_event(gev, &ActiveList);
		ep_thr_cond_broadcast(&ActiveListSig);
		ep_thr_mutex_unlock(&ActiveListMutex);
	}
	else
	{
		ep_thr_mutex_lock(&CallbackListMutex);
		insert_event(gev, &CallbackList);
		ep_thr_cond_signal(&CallbackListSig);
		ep_thr_mutex_unlock(&CallbackListMutex);
	}
}


/*
**  Make pending events current (when a request leaves WAITING state)
**
**		The gdp_req_t containing events is locked when this is called
**		so we don't have to worry about locking it ourself.
*/

void
_gdp_event_insert_pending(struct gev_list *glist, gdp_req_t *req)
{
	gdp_event_t *gev;

	ep_dbg_cprintf(Dbg, 48,
			"_gdp_event_insert_pending(%p): %s\n",
			glist,
			TAILQ_FIRST(glist) == NULL ? "empty" : "events");
	for (gev = TAILQ_FIRST(glist); gev != NULL; gev = TAILQ_NEXT(gev, queue))
	{
		// if this is a DONE event but we don't have all the results, leave it
		//TODO: have to allow for timeout
		if (gev->type == GDP_EVENT_DONE &&
				EP_UT_BITSET(GDP_REQ_PERSIST, req->flags))
			continue;
		TAILQ_REMOVE(glist, gev, queue);
		_gdp_event_trigger(gev, req);
	}
}


/*
**  This is the thread that processes callbacks.
**		The event is freed, so the callback should NOT call
**			gdp_event_free.
*/

static void *
_gdp_event_thread(void *ctx)
{
	for (;;)
	{
		gdp_event_t *gev;

		// get the next event off the list
		ep_thr_mutex_lock(&CallbackListMutex);
		while ((gev = TAILQ_FIRST(&CallbackList)) == NULL)
		{
			ep_thr_cond_wait(&CallbackListSig, &CallbackListMutex, NULL);
		}
		TAILQ_REMOVE(&CallbackList, gev, queue);
		ep_thr_mutex_unlock(&CallbackListMutex);

		// sanity checks...
		EP_ASSERT(gev->cb != NULL);
		EP_ASSERT(gev->type != _GDP_EVENT_FREE);

		// now invoke it
		if (gev->cb != NULL)
			(*gev->cb)(gev);

		// don't forget to clean up (unless it's already free)
		if (gev->type != _GDP_EVENT_FREE)
			gdp_event_free(gev);
	}

	// not reached, but make gcc happy
	return NULL;
}


/*
**  _GDP_EVENT_SETCB --- set the callback function & start thread if needed
*/

void
_gdp_event_setcb(
			gdp_req_t *req,
			gdp_event_cbfunc_t cbfunc,
			void *cbarg)
{
	req->sub_cbfunc = cbfunc;
	req->sub_cbarg = cbarg;

	// if using callbacks, make sure we have a callback thread running
	if (cbfunc != NULL && !CallbackThreadStarted)
	{
		int err = ep_thr_spawn(&CallbackThread, &_gdp_event_thread, NULL);
		if (err != 0 && ep_dbg_test(Dbg, 1))
			ep_log(ep_stat_from_errno(err),
					"_gdp_gcl_setcb: cannot start callback thread");
		CallbackThreadStarted = true;
	}
}


/*
**  Create an event and link it into the queue based on a acknak req.
*/

EP_STAT
_gdp_event_add_from_req(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;
	int evtype;
	GdpMessage *msg = req->rpdu->msg;

	GDP_MSG_CHECK(req->rpdu, return EP_STAT_ASSERT_ABORT);

	// make note that we've seen activity for this subscription
	ep_time_now(&req->act_ts);

	// for the moment we only understand data responses (for subscribe)
	switch (msg->cmd)
	{
	  case GDP_ACK_SUCCESS:
		// success with no further information (many commands)
		evtype = GDP_EVENT_SUCCESS;
		break;

	  case GDP_ACK_CONTENT:
		evtype = GDP_EVENT_DATA;
		break;

	  case GDP_ACK_END_OF_RESULTS:
		// end of subscription
		evtype = GDP_EVENT_DONE;
		break;

	  case GDP_ACK_CREATED:
		// response to APPEND
		evtype = GDP_EVENT_CREATED;
		break;

	  case GDP_NAK_S_LOST_SUBSCR:
		evtype = GDP_EVENT_SHUTDOWN;
		req->flags &= ~GDP_REQ_PERSIST;
		break;

	  case GDP_NAK_C_REC_MISSING:
		evtype = GDP_EVENT_MISSING;
		break;

	  default:
		if (msg->cmd >= GDP_ACK_MIN && msg->cmd <= GDP_ACK_MAX)
		{
			// some sort of success
			evtype = GDP_EVENT_SUCCESS;
			req->stat = _gdp_stat_from_acknak(msg->cmd);
			break;
		}
		if (msg->cmd >= GDP_NAK_C_MIN && msg->cmd <= GDP_NAK_R_MAX)
		{
			// some sort of failure
			evtype = GDP_EVENT_FAILURE;
			req->stat = _gdp_stat_from_acknak(msg->cmd);
			break;
		}
		ep_dbg_cprintf(Dbg, 1,
				"_gdp_event_add_from_req: unexpected ack/nak %d\n",
				msg->cmd);
		estat = GDP_STAT_PROTOCOL_FAIL;
		return estat;
	}

	gdp_event_t *gev;
	estat = _gdp_event_new(&gev);
	EP_STAT_CHECK(estat, return estat);

	gev->type = evtype;
	gev->gin = req->gin;
	gev->stat = req->stat;
	gev->udata = req->sub_cbarg;
	gev->cb = req->sub_cbfunc;
	gev->datum = gdp_datum_new();
	if (msg->cmd == GDP_ACK_CONTENT)
	{
		EP_ASSERT(msg->ack_content->dl->n_d == 1);		//FIXME: should handle multiples
		_gdp_datum_from_pb(gev->datum, msg->ack_content->dl->d[0], msg->sig);
		gev->sortkey = gev->datum->recno;
	}

	// schedule the event for delivery
	if (req->state == GDP_REQ_WAITING)
	{
		// can't deliver yet: make it pending
		ep_dbg_cprintf(Dbg, 40,
				"_gdp_event_add_from_req: event %p pending\n", gev);
		insert_event(gev, &req->events);
	}
	else
	{
		// go ahead and deliver
		_gdp_event_trigger(gev, req);
	}

	return estat;
}


/*
**  Print an event (for debugging)
*/

void
gdp_event_print(const gdp_event_t *gev, FILE *fp)
{
	_gdp_event_dump(gev, fp, GDP_PR_PRETTY, 0);
}

void
_gdp_event_dump(const gdp_event_t *gev, FILE *fp, int detail, int indent)
{
	gdp_recno_t recno = -1;
	char ebuf[100];

	if (fp == NULL)
		fp = ep_dbg_getfile();

	if (detail >= GDP_PR_BASIC + 1)
	{
		fprintf(fp, "Event type %d, sortkey %"PRIu64 ", cbarg %p, stat %s\n",
				gev->type, gev->sortkey, gev->udata,
				ep_stat_tostr(gev->stat, ebuf, sizeof ebuf));
		indent++;
	}

	if (gev->datum != NULL)
		recno = gev->datum->recno;

	fprintf(fp, "%s", _gdp_pr_indent(indent));

	switch (gev->type)
	{
	  case GDP_EVENT_DATA:
		gdp_datum_print(gev->datum, fp, GDP_DATUM_PRTEXT);
		break;

	  case GDP_EVENT_CREATED:
		fprintf(fp, "Data created\n");
		break;

	  case GDP_EVENT_DONE:
		fprintf(fp, "End of data\n");
		break;

	  case GDP_EVENT_SHUTDOWN:
		fprintf(fp, "Log daemon shutdown\n");
		break;

	  case GDP_EVENT_SUCCESS:
		fprintf(fp, "Success: %s\n",
					ep_stat_tostr(gev->stat, ebuf, sizeof ebuf));
		break;

	  case GDP_EVENT_FAILURE:
		fprintf(fp, "Failure: %s\n",
					ep_stat_tostr(gev->stat, ebuf, sizeof ebuf));
		break;

	  case GDP_EVENT_MISSING:
		fprintf(fp, "Record %" PRIgdp_recno " missing\n", recno);
		break;

	  default:
		fprintf(fp, "Unknown event type %d: %s\n",
					gev->type, ep_stat_tostr(gev->stat, ebuf, sizeof ebuf));
		break;
	}
}


/*
**  Dump all events (debugging)
**
**		Intentionally doesn't lock the queues.
*/

void
_gdp_event_dump_all(void *unused_, void *fp_)
{
	gdp_event_t *gev;
	FILE *fp = fp_;
	int detail = GDP_PR_DETAILED;

	if (fp == NULL)
		fp = ep_dbg_getfile();

	fprintf(fp, "\n<<< Active Events >>>\n");
	TAILQ_FOREACH(gev, &ActiveList, queue)
	{
		_gdp_event_dump(gev, fp, detail, 0);
	}

	fprintf(fp, "\n<<< Callback Events >>>\n");
	TAILQ_FOREACH(gev, &CallbackList, queue)
	{
		_gdp_event_dump(gev, fp, detail, 0);
	}
}


/*
**  Getter functions for various event fields.
*/

int
gdp_event_gettype(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);
	return gev->type;
}


gdp_gin_t *
gdp_event_getgin(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);
	return gev->gin;
}


gdp_datum_t *
gdp_event_getdatum(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);
	return gev->datum;
}


void *
gdp_event_getudata(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);
	return gev->udata;
}


EP_STAT
gdp_event_getstat(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);
	return gev->stat;
}
