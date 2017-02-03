/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  ----- BEGIN LICENSE BLOCK -----
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
**  ----- END LICENSE BLOCK -----
*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_log.h>

#include "gdp.h"
#include "gdp_event.h"
#include "gdp_priv.h"

#include <string.h>
#include <sys/errno.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.subscr", "GDP subscriptions");


/*
**  Subscription disappeared; remove it from list
*/

static void
subscr_lost(gdp_req_t *req)
{
	//TODO IMPLEMENT ME!
	if (ep_dbg_test(Dbg, 1))
	{
		ep_dbg_printf("subscr_lost: ");
		_gdp_req_dump(req, ep_dbg_getfile(), 0, 0);
	}
}


/*
**  Re-subscribe to a GCL
*/

static EP_STAT
subscr_resub(gdp_req_t *req)
{
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 39, "subscr_resub: refreshing req@%p\n", req);
	EP_ASSERT_ELSE(req != NULL, return EP_STAT_ASSERT_ABORT);
	GDP_ASSERT_MUTEX_ISLOCKED(&req->mutex, );
	EP_ASSERT_ELSE(req->gcl != NULL, );
	EP_ASSERT_ELSE(req->cpdu != NULL, );

	req->state = GDP_REQ_ACTIVE;
	req->cpdu->cmd = GDP_CMD_SUBSCRIBE;
	memcpy(req->cpdu->dst, req->gcl->name, sizeof req->cpdu->dst);
	memcpy(req->cpdu->src, _GdpMyRoutingName, sizeof req->cpdu->src);
	//XXX it seems like this should be in a known state
	if (req->cpdu->datum == NULL)
		req->cpdu->datum = gdp_datum_new();
	else if (req->cpdu->datum->dbuf != NULL)
		gdp_buf_reset(req->cpdu->datum->dbuf);
	req->cpdu->datum->recno = req->gcl->nrecs + 1;
	gdp_buf_put_uint32(req->cpdu->datum->dbuf, req->numrecs);

	estat = _gdp_invoke(req);

	if (ep_dbg_test(Dbg, EP_STAT_ISOK(estat) ? 20 : 1))
	{
		char ebuf[200];

		ep_dbg_printf("subscr_resub(%s) ->\n\t%s\n",
				req->gcl == NULL ? "(no gcl)" : req->gcl->pname,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	req->state = GDP_REQ_IDLE;
	if (req->rpdu->datum != NULL)
		gdp_datum_free(req->rpdu->datum);
	req->rpdu->datum = NULL;

	return estat;
}


/*
**  Periodically ping all open subscriptions to make sure they are
**  still happy.
*/

static void *
subscr_poker_thread(void *chan_)
{
	gdp_chan_t *chan = chan_;
	long delta_poke = ep_adm_getlongparam("swarm.gdp.subscr.pokeintvl", 60L);
	long delta_dead = ep_adm_getlongparam("swarm.gdp.subscr.deadintvl", 180L);

	ep_dbg_cprintf(Dbg, 10, "Starting subscription poker thread\n");
	chan->flags |= GDP_CHAN_HAS_SUB_THR;

	// loop forever poking subscriptions
	for (;;)
	{
		EP_STAT estat;
		gdp_req_t *req;
		gdp_req_t *nextreq;
		EP_TIME_SPEC now;
		EP_TIME_SPEC t_poke;	// poke if older than this
		EP_TIME_SPEC t_dead;	// abort if older than this

		// wait for a while to avoid hogging CPU
		ep_time_nanosleep(delta_poke / 2 SECONDS);
		ep_dbg_cprintf(Dbg, 40, "\nsubscr_poker_thread: poking\n");

		ep_time_now(&now);
		ep_time_from_nsec(-delta_poke SECONDS, &t_poke);
		ep_time_add_delta(&now, &t_poke);
		ep_time_from_nsec(-delta_dead SECONDS, &t_dead);
		ep_time_add_delta(&now, &t_dead);

		// do loop is in case _gdp_req_lock fails
		do
		{
			estat = EP_STAT_OK;
			for (req = LIST_FIRST(&chan->reqs); req != NULL; req = nextreq)
			{
				estat = _gdp_req_lock(req);
				EP_STAT_CHECK(estat, break);

				nextreq = LIST_NEXT(req, chanlist);
				if (ep_dbg_test(Dbg, 51))
				{
					char tbuf[60];

					ep_time_format(&now, tbuf, sizeof tbuf, EP_TIME_FMT_HUMAN);
					ep_dbg_printf("subscr_poker_thread: at %s checking ", tbuf);
					_gdp_req_dump(req, ep_dbg_getfile(), 0, 0);
				}

				if (!EP_UT_BITSET(GDP_REQ_CLT_SUBSCR, req->flags))
				{
					// not a subscription: skip this entry
				}
				else if (ep_time_before(&t_poke, &req->act_ts))
				{
					// we've seen activity recently, no need to poke
				}
				else if (ep_time_before(&req->act_ts, &t_dead))
				{
					// this subscription is dead
					//XXX should be impossible: subscription refreshed each time
					subscr_lost(req);
				}
				else
				{
					// t_dead < act_ts <= t_poke: refresh this subscription
					(void) subscr_resub(req);
				}

				// if _gdp_invoke failed, try again at the next poke interval
				_gdp_req_unlock(req);
			}
		}
		while (!EP_STAT_ISOK(estat));
	}

	// not reached; keep gcc happy
	ep_log(EP_STAT_SEVERE, "subscr_poker_thread: fell out of loop");
	return NULL;
}


/*
**	_GDP_GCL_SUBSCRIBE --- subscribe to a GCL
**
**		This also implements multiread.
*/

EP_STAT
_gdp_gcl_subscribe(gdp_req_t *req,
		int32_t numrecs,
		EP_TIME_SPEC *timeout,
		gdp_event_cbfunc_t cbfunc,
		void *cbarg)
{
	EP_STAT estat = EP_STAT_OK;

	errno = 0;				// avoid spurious messages

	EP_ASSERT_POINTER_VALID(req);

	// arrange for responses to appear as events or callbacks
	_gdp_event_setcb(req, cbfunc, cbarg);

	gdp_buf_put_uint32(req->cpdu->datum->dbuf, req->numrecs);

	// issue the subscription --- no data returned
	estat = _gdp_invoke(req);
	EP_ASSERT(req->state == GDP_REQ_ACTIVE);

	if (!EP_STAT_ISOK(estat))
	{
		_gdp_req_free(&req);
	}
	else
	{
		// at this point remaining results will be asynchronous
		req->flags |= GDP_REQ_ASYNCIO;

		// now waiting for other events; go ahead and unlock
		req->state = GDP_REQ_IDLE;
		if (req->rpdu != NULL)
			_gdp_pdu_free(req->rpdu);
		req->rpdu = NULL;
		if (req->cpdu->datum != NULL)
			gdp_buf_reset(req->cpdu->datum->dbuf);
		ep_thr_cond_signal(&req->cond);
		_gdp_req_unlock(req);

		// the req is still on the channel list

		// start a subscription poker thread if needed
		if (req->cpdu->cmd == GDP_CMD_SUBSCRIBE)
		{
			long poke = ep_adm_getlongparam("swarm.gdp.subscr.pokeintvl", 60L);
			bool spawnthread = false;

			ep_thr_mutex_lock(&req->chan->mutex);
			if (poke > 0 && !EP_UT_BITSET(GDP_CHAN_HAS_SUB_THR, req->chan->flags))
			{
				spawnthread = true;
				req->chan->flags |= GDP_CHAN_HAS_SUB_THR;
			}
			ep_thr_mutex_unlock(&req->chan->mutex);
			if (spawnthread)
			{
				int istat = ep_thr_spawn(&req->chan->sub_thr_id,
									subscr_poker_thread, req->chan);
				if (istat != 0)
				{
					EP_STAT spawn_stat = ep_stat_from_errno(istat);
					ep_log(spawn_stat, "_gdp_gcl_subscribe: thread spawn failure");
				}
			}
		}
	}

	return estat;
}


/*
**  Unsubscribe all requests for a given gcl and destination.
*/

EP_STAT
_gdp_gcl_unsubscribe(
		gdp_gcl_t *gcl,
		gdp_name_t dest)
{
	EP_STAT estat;
	gdp_req_t *req;
	gdp_req_t *nextreq;

	do
	{
		estat = EP_STAT_OK;
		for (req = LIST_FIRST(&gcl->reqs); req != NULL; req = nextreq)
		{
			estat = _gdp_req_lock(req);
			EP_STAT_CHECK(estat, break);
			nextreq = LIST_NEXT(req, gcllist);
			if (!GDP_NAME_SAME(req->cpdu->dst, dest))
			{
				_gdp_req_unlock(req);
				continue;
			}

			// remove subscription for this destination
			EP_ASSERT_INVARIANT(req->gcl == gcl);
			LIST_REMOVE(req, gcllist);
			_gdp_req_free(&req);
		}
	} while (!EP_STAT_ISOK(estat));
	return estat;
}
