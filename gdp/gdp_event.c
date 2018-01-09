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
#include <ep/ep_log.h>

#include "gdp.h"
#include "gdp_priv.h"
#include "gdp_event.h"

#include <string.h>
#include <sys/errno.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.event", "GDP event handling");


// free (unused) events
static EP_THR_MUTEX		FreeListMutex	EP_THR_MUTEX_INITIALIZER2(GDP_MUTEX_LORDER_LEAF);
static struct gev_list	FreeList		= STAILQ_HEAD_INITIALIZER(FreeList);

// active events (synchronous, ready for gdp_event_next)
static EP_THR_MUTEX		ActiveListMutex	EP_THR_MUTEX_INITIALIZER2(GDP_MUTEX_LORDER_LEAF);
static EP_THR_COND		ActiveListSig	EP_THR_COND_INITIALIZER;
static struct gev_list	ActiveList		= STAILQ_HEAD_INITIALIZER(ActiveList);

// callback events (asynchronous, ready for delivery in callback thread)
static EP_THR_MUTEX		CallbackListMutex	EP_THR_MUTEX_INITIALIZER2(GDP_MUTEX_LORDER_LEAF);
static EP_THR_COND		CallbackListSig		EP_THR_COND_INITIALIZER;
static struct gev_list	CallbackList		= STAILQ_HEAD_INITIALIZER(CallbackList);
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
		if ((gev = STAILQ_FIRST(&FreeList)) != NULL)
			STAILQ_REMOVE_HEAD(&FreeList, queue);
		ep_thr_mutex_unlock(&FreeListMutex);
		if (gev == NULL || gev->type == _GDP_EVENT_FREE)
			break;

		// error: abandon this event
		EP_ASSERT_PRINT("_gdp_event_new: allocated event %p on free list", gev);
	}
	if (gev == NULL)
	{
		gev = ep_mem_zalloc(sizeof *gev);
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
	STAILQ_INSERT_HEAD(&FreeList, gev, queue);
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
gdp_event_next(gdp_gcl_t *gin, EP_TIME_SPEC *timeout)
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

		while ((gev = STAILQ_FIRST(&ActiveList)) == NULL)
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

					strerror_r(err, errno_buf, sizeof errno_buf);
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
			gev = STAILQ_NEXT(gev, queue);
		}

		if (gev != NULL)
		{
			// found a match!
			break;
		}
	}

	if (gev != NULL)
	{
		STAILQ_REMOVE(&ActiveList, gev, gdp_event, queue);
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
	for (gev = STAILQ_FIRST(&ActiveList); gev != NULL; gev = next_gev)
	{
		next_gev = STAILQ_NEXT(gev, queue);
		if (gev->gin != gin)
			continue;
		else if (gev->type == _GDP_EVENT_FREE)
			STAILQ_REMOVE(&ActiveList, gev, gdp_event, queue);
		else
			gdp_event_free(gev);
	}
	ep_thr_mutex_unlock(&ActiveListMutex);

	return EP_STAT_OK;
}


/*
**  Trigger an event (i.e., add to event queue)
**		There are two lists, depending on whether a callback was
**			specified.
*/

static void
_gdp_event_trigger(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);

	ep_dbg_cprintf(Dbg, 48,
			"_gdp_event_trigger: adding event %p (%d) to %s list\n",
			gev, gev->type, gev->cb == NULL ? "active" : "callback");
	if (gev->type == _GDP_EVENT_FREE)
	{
		ep_dbg_cprintf(Dbg, 1, "_gdp_event_trigger(%p): event is free\n", gev);
		return;
	}

	if (gev->cb == NULL)
	{
		// signal the user thread (synchronous delivery)
		ep_thr_mutex_lock(&ActiveListMutex);
		STAILQ_INSERT_TAIL(&ActiveList, gev, queue);
		ep_thr_cond_broadcast(&ActiveListSig);
		ep_thr_mutex_unlock(&ActiveListMutex);
	}
	else
	{
		// signal the callback thread (asynchronous delivery via callback)
		ep_thr_mutex_lock(&CallbackListMutex);
		STAILQ_INSERT_TAIL(&CallbackList, gev, queue);
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
_gdp_event_trigger_pending(struct gev_list *events)
{
	gdp_event_t *gev;

	ep_dbg_cprintf(Dbg, 48,
			"_gdp_event_trigger_pending(%p): %s\n",
			events,
			STAILQ_FIRST(events) == NULL ? "empty" : "events");
	while ((gev = STAILQ_FIRST(events)) != NULL)
	{
		STAILQ_REMOVE_HEAD(events, queue);
		_gdp_event_trigger(gev);
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
		while ((gev = STAILQ_FIRST(&CallbackList)) == NULL)
		{
			ep_thr_cond_wait(&CallbackListSig, &CallbackListMutex, NULL);
		}
		STAILQ_REMOVE_HEAD(&CallbackList, queue);
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

	GDP_MSG_CHECK(req->rpdu, return EP_STAT_ASSERT_ABORT);

	// make note that we've seen activity for this subscription
	ep_time_now(&req->act_ts);

	// for the moment we only understand data responses (for subscribe)
	switch (req->rpdu->msg->cmd)
	{
	  case GDP_ACK_SUCCESS:
		// success with no further information (many commands)
		evtype = GDP_EVENT_SUCCESS;
		break;

	  case GDP_ACK_CONTENT:
		evtype = GDP_EVENT_DATA;
		break;

	  case GDP_ACK_DELETED:
		// end of subscription
		evtype = GDP_EVENT_EOS;
		req->flags &= ~GDP_REQ_PERSIST;
		break;

	  case GDP_ACK_CREATED:
		// response to APPEND
		evtype = GDP_EVENT_CREATED;
		break;

	  case GDP_NAK_S_LOSTSUB:
		evtype = GDP_EVENT_SHUTDOWN;
		req->flags &= ~GDP_REQ_PERSIST;
		break;

	  case GDP_NAK_C_REC_MISSING:
		evtype = GDP_EVENT_MISSING;
		break;

	  default:
		if (req->rpdu->msg->cmd >= GDP_ACK_MIN &&
				req->rpdu->msg->cmd <= GDP_ACK_MAX)
		{
			// some sort of success
			evtype = GDP_EVENT_SUCCESS;
			req->stat = _gdp_stat_from_acknak(req->rpdu->msg->cmd);
			break;
		}
		if (req->rpdu->msg->cmd >= GDP_NAK_C_MIN &&
				req->rpdu->msg->cmd <= GDP_NAK_R_MAX)
		{
			// some sort of failure
			evtype = GDP_EVENT_FAILURE;
			req->stat = _gdp_stat_from_acknak(req->rpdu->msg->cmd);
			break;
		}
		ep_dbg_cprintf(Dbg, 1,
				"_gdp_event_add_from_req: unexpected ack/nak %d\n",
				req->rpdu->msg->cmd);
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
	if (req->rpdu->msg->cmd == GDP_ACK_CONTENT)
	{
		_gdp_datum_from_pb(gev->datum, req->rpdu->msg->body->ack_content->datum);
	}

	// schedule the event for delivery
	if (req->state == GDP_REQ_WAITING)
	{
		// can't deliver yet: make it pending
		ep_dbg_cprintf(Dbg, 40,
				"_gdp_event_add_from_req: event %p pending\n", gev);
		STAILQ_INSERT_TAIL(&req->events, gev, queue);
	}
	else
	{
		// go ahead and deliver
		_gdp_event_trigger(gev);
	}

	return estat;
}


/*
**  Print an event (for debugging)
*/

void
gdp_event_print(const gdp_event_t *gev, FILE *fp, int detail)
{
	gdp_recno_t recno = -1;
	char ebuf[100];

	if (detail > GDP_PR_BASIC + 1)
		fprintf(fp, "Event type %d, cbarg %p, stat %s\n",
				gev->type, gev->udata,
				ep_stat_tostr(gev->stat, ebuf, sizeof ebuf));

	if (gev->datum != NULL)
		recno = gev->datum->recno;

	switch (gev->type)
	{
	  case GDP_EVENT_DATA:
		fprintf(fp, "    ");
		gdp_datum_print(gev->datum, fp, GDP_DATUM_PRTEXT);
		break;

	  case GDP_EVENT_CREATED:
		fprintf(fp, "    Data created\n");
		break;

	  case GDP_EVENT_EOS:
		fprintf(fp, "    End of data\n");
		break;

	  case GDP_EVENT_SHUTDOWN:
		fprintf(fp, "    Log daemon shutdown\n");
		break;

	  case GDP_EVENT_SUCCESS:
		if (detail > GDP_PR_BASIC + 1)
			fprintf(fp, "    Generic success\n");
		else
			fprintf(fp, "    Success: %s\n",
					ep_stat_tostr(gev->stat, ebuf, sizeof ebuf));
		break;

	  case GDP_EVENT_FAILURE:
		if (detail > GDP_PR_BASIC + 1)
			fprintf(fp, "    Generic failure\n");
		else
			fprintf(fp, "    Failure: %s\n",
					ep_stat_tostr(gev->stat, ebuf, sizeof ebuf));
		break;

	  case GDP_EVENT_MISSING:
		fprintf(fp, "    Record %" PRIgdp_recno " missing\n",
				recno);
		break;

	  default:
		if (detail > 0)
			fprintf(fp, "    Unknown event type %d: %s\n",
					gev->type, ep_stat_tostr(gev->stat, ebuf, sizeof ebuf));
		break;
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


gdp_gcl_t *
gdp_event_getgcl(gdp_event_t *gev)
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
