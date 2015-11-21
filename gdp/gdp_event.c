/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  GDP_EVENT.C --- event handling
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**	----- END LICENSE BLOCK -----
*/

#include <ep/ep_thr.h>
#include <ep/ep_dbg.h>
#include <ep/ep_log.h>

#include "gdp.h"
#include "gdp_priv.h"
#include "gdp_event.h"

#include <sys/errno.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.event", "GDP event handling");


STAILQ_HEAD(ev_head, gdp_event);

// free (unused) events
static EP_THR_MUTEX		FreeListMutex	EP_THR_MUTEX_INITIALIZER;
static struct ev_head	FreeList		= STAILQ_HEAD_INITIALIZER(FreeList);

// active events (synchronous, ready for gdp_event_next)
static EP_THR_MUTEX		ActiveListMutex	EP_THR_MUTEX_INITIALIZER;
static EP_THR_COND		ActiveListSig	EP_THR_COND_INITIALIZER;
static struct ev_head	ActiveList		= STAILQ_HEAD_INITIALIZER(ActiveList);

// callback events (asynchronous, ready for delivery in callback thread)
static EP_THR_MUTEX		CallbackListMutex	EP_THR_MUTEX_INITIALIZER;
static EP_THR_COND		CallbackListSig		EP_THR_COND_INITIALIZER;
static struct ev_head	CallbackList		= STAILQ_HEAD_INITIALIZER(CallbackList);
static pthread_t		CallbackThread;
static bool				CallbackThreadStarted	= false;

EP_STAT
_gdp_event_new(gdp_event_t **gevp)
{
	gdp_event_t *gev = NULL;

	ep_thr_mutex_lock(&FreeListMutex);
	if ((gev = STAILQ_FIRST(&FreeList)) != NULL)
		STAILQ_REMOVE_HEAD(&FreeList, queue);
	ep_thr_mutex_unlock(&FreeListMutex);
	if (gev == NULL)
	{
		gev = ep_mem_zalloc(sizeof *gev);
	}
	*gevp = gev;
	ep_dbg_cprintf(Dbg, 48, "_gdp_event_new => %p\n", gev);
	return EP_STAT_OK;
}


EP_STAT
gdp_event_free(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);

	ep_dbg_cprintf(Dbg, 48, "gdp_event_free(%p)\n", gev);

	if (gev->datum != NULL)
		gdp_datum_free(gev->datum);
	gev->datum = NULL;
	gev->type = _GDP_EVENT_FREE;
	ep_thr_mutex_lock(&FreeListMutex);
	STAILQ_INSERT_HEAD(&FreeList, gev, queue);
	ep_thr_mutex_unlock(&FreeListMutex);
	return EP_STAT_OK;
}


gdp_event_t *
gdp_event_next(gdp_gcl_t *gcl, EP_TIME_SPEC *timeout)
{
	gdp_event_t *gev;
	EP_TIME_SPEC *abs_to = NULL;
	EP_TIME_SPEC tv;

	if (timeout != NULL)
	{
		ep_time_deltanow(timeout, &tv);
		abs_to = &tv;
	}

	ep_thr_mutex_lock(&ActiveListMutex);
	for (;;)
	{
		int err;

		while ((gev = STAILQ_FIRST(&ActiveList)) == NULL)
		{
			// wait until we have at least one thing to try
			err = ep_thr_cond_wait(&ActiveListSig, &ActiveListMutex, abs_to);
			if (err == ETIMEDOUT)
				goto fail0;
		}
		while (gev != NULL)
		{
			// if this isn't the GCL we want, keep searching the list
			if (gcl == NULL || gev->gcl == gcl)
				break;

			// not the event we want
			gev = STAILQ_NEXT(gev, queue);
		}

		if (gev != NULL)
		{
			// found a match!
			break;
		}

		// if there is no match, wait until something is added and try again
		err = ep_thr_cond_wait(&ActiveListSig, &ActiveListMutex, abs_to);
		if (err == ETIMEDOUT)
			break;
	}

	if (gev != NULL)
		STAILQ_REMOVE(&ActiveList, gev, gdp_event, queue);
fail0:
	ep_thr_mutex_unlock(&ActiveListMutex);
	return gev;
}


void
_gdp_event_trigger(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);

	ep_dbg_cprintf(Dbg, 48,
			"_gdp_event_trigger: adding event %p (%d) to %s list\n",
			gev, gev->type, gev->cb == NULL ? "active" : "callback");

	if (gev->cb == NULL)
	{
		// signal the user thread (synchronous delivery)
		ep_thr_mutex_lock(&ActiveListMutex);
		STAILQ_INSERT_TAIL(&ActiveList, gev, queue);
		ep_thr_cond_signal(&ActiveListSig);
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


static void *
_gdp_event_thread(void *ctx)
{
	for (;;)
	{
		gdp_event_t *gev;

		// get the next event off the list
		ep_thr_mutex_lock(&CallbackListMutex);
		do
		{
			ep_thr_cond_wait(&CallbackListSig, &CallbackListMutex, NULL);
			gev = STAILQ_FIRST(&CallbackList);
		} while (gev == NULL);
		STAILQ_REMOVE_HEAD(&CallbackList, queue);
		ep_thr_mutex_unlock(&CallbackListMutex);

		// sanity checks...
		EP_ASSERT(gev->cb != NULL);
		EP_ASSERT(gev->type != _GDP_EVENT_FREE);

		// now invoke it
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
	req->sub_cb = cbfunc;
	req->udata = cbarg;

	// if using callbacks, make sure we have a callback thread running
	if (cbfunc != NULL && !CallbackThreadStarted)
	{
		int err = pthread_create(&CallbackThread, NULL,
						&_gdp_event_thread, NULL);
		if (err != 0 && ep_dbg_test(Dbg, 1))
			ep_log(ep_stat_from_errno(err),
					"_gdp_gcl_setcb: cannot start callback thread");
		CallbackThreadStarted = true;
	}
}


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
	return gev->gcl;
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
