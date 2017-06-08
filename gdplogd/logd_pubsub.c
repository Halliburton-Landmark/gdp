/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  Handle publish/subscribe requests
**
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
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

#include "logd.h"
#include "logd_pubsub.h"

#include <gdp/gdp_priv.h>
#include <ep/ep_dbg.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.pubsub",
								"GDP Log Daemon pub/sub handling");


/*
**  SUB_SEND_MESSAGE_NOTIFICATION --- inform a subscriber of a new message
**
**		Assumes req is locked.
*/

void
sub_send_message_notification(gdp_req_t *req, gdp_datum_t *datum, int cmd)
{
	EP_STAT estat;

	req->rpdu->cmd = cmd;
	req->rpdu->datum = datum;

	if (ep_dbg_test(Dbg, 33))
	{
		ep_dbg_printf("sub_send_message_notification(%d): ", cmd);
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	estat = _gdp_pdu_out(req->rpdu, req->chan, NULL);
	if (!EP_STAT_ISOK(estat))
	{
		ep_dbg_cprintf(Dbg, 1,
				"sub_send_message_notification: couldn't write PDU!\n");
	}
	req->rpdu->datum = NULL;				// we just borrowed the datum

	// XXX: This won't really work in case of holes.
	req->nextrec++;

	if (cmd == GDP_ACK_CONTENT && req->numrecs > 0 && --req->numrecs <= 0)
		sub_end_subscription(req);
}


/*
**  SUB_NOTIFY_ALL_SUBSCRIBERS --- send something to all interested parties
**
**		Both pubreq and pubreq->pdu->datum should be locked when
**			this is called.
*/

void
sub_notify_all_subscribers(gdp_req_t *pubreq, int cmd)
{
	gdp_req_t *req;
	gdp_req_t *nextreq;
	EP_TIME_SPEC sub_timeout;

	EP_THR_MUTEX_ASSERT_ISLOCKED(&pubreq->gcl->mutex);

	if (ep_dbg_test(Dbg, 32))
	{
		ep_dbg_printf("sub_notify_all_subscribers(%s) of pub",
				_gdp_proto_cmd_name(cmd));
		_gdp_req_dump(pubreq, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	{
		EP_TIME_SPEC sub_delta;
		long timeout = ep_adm_getlongparam("swarm.gdplogd.subscr.timeout", 0);

		if (timeout == 0)
			timeout = ep_adm_getlongparam("swarm.gdp.subscr.timeout",
									GDP_SUBSCR_TIMEOUT_DEF);
		ep_time_from_nsec(-timeout SECONDS, &sub_delta);
		ep_time_deltanow(&sub_delta, &sub_timeout);
	}

	pubreq->gcl->flags |= GCLF_KEEPLOCKED;
	for (req = LIST_FIRST(&pubreq->gcl->reqs); req != NULL; req = nextreq)
	{
		_gdp_req_lock(req);
		nextreq = LIST_NEXT(req, gcllist);
		EP_ASSERT_ELSE(req != nextreq, break);

		// make sure we don't tell ourselves
		if (req == pubreq)
		{
			_gdp_req_unlock(req);
			continue;
		}

		if (ep_dbg_test(Dbg, 59))
		{
			ep_dbg_printf("sub_notify_all_subscribers: checking ");
			_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		}

		// notify subscribers
		if (!EP_UT_BITSET(GDP_REQ_SRV_SUBSCR, req->flags))
		{
			ep_dbg_cprintf(Dbg, 59, "   ... not a subscription (flags = 0x%x)\n", req->flags);
		}
		else if (!ep_time_before(&req->act_ts, &sub_timeout))
		{
			sub_send_message_notification(req, pubreq->cpdu->datum, cmd);
		}
		else
		{
			// this subscription seems to be dead
			if (ep_dbg_test(Dbg, 18))
			{
				ep_dbg_printf("sub_notify_all_subscribers: subscription timeout: ");
				_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
				_gdp_gcl_dump(req->gcl, ep_dbg_getfile(), GDP_PR_BASIC, 0);
			}

			// actually remove the subscription
			//XXX isn't this done by _gdp_req_free???
			//LIST_REMOVE(req, gcllist);

			EP_ASSERT(req->gcl != NULL);
			EP_ASSERT(EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags));
			_gdp_req_free(&req);
		}
		if (req != NULL)
			_gdp_req_unlock(req);
	}
	pubreq->gcl->flags &= ~GCLF_KEEPLOCKED;
}


/*
**  SUB_END_SUBSCRIPTION --- terminate a subscription
**
**		req and req->gcl should be locked when this is called.
*/

void
sub_end_subscription(gdp_req_t *req)
{
	EP_THR_MUTEX_ASSERT_ISLOCKED(&req->mutex);
	EP_THR_MUTEX_ASSERT_ISLOCKED(&req->gcl->mutex);

	// make it not persistent and not a subscription
	req->flags &= ~(GDP_REQ_PERSIST | GDP_REQ_SRV_SUBSCR);

	// remove the request from the work list
	if (EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags))
		LIST_REMOVE(req, gcllist);
	req->flags &= ~GDP_REQ_ON_GCL_LIST;
	_gdp_gcl_decref(&req->gcl);

	// send an "end of subscription" event
	req->rpdu->cmd = GDP_ACK_DELETED;

	if (ep_dbg_test(Dbg, 61))
	{
		ep_dbg_printf("sub_end_subscription req:\n  ");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	(void) _gdp_pdu_out(req->rpdu, req->chan, NULL);
}


/*
**  Unsubscribe all requests for a given gcl and destination.
*/

EP_STAT
sub_end_all_subscriptions(
		gdp_gcl_t *gcl,
		gdp_name_t dest)
{
	EP_STAT estat;
	gdp_req_t *req;
	gdp_req_t *nextreq;

	if (ep_dbg_test(Dbg, 29))
	{
		ep_dbg_printf("sub_end_all_subscriptions: ");
		_gdp_gcl_dump(gcl, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	EP_THR_MUTEX_ASSERT_ISLOCKED(&gcl->mutex);
	if (EP_UT_BITSET(GCLF_KEEPLOCKED, gcl->flags) && ep_dbg_test(Dbg, 1))
		ep_dbg_printf("sub_end_all_subscriptions: GCLF_KEEPLOCKED on entry\n");
	gcl->flags |= GCLF_KEEPLOCKED;

	do
	{
		estat = EP_STAT_OK;
		for (req = LIST_FIRST(&gcl->reqs); req != NULL; req = nextreq)
		{
			estat = _gdp_req_lock(req);
			EP_STAT_CHECK(estat, break);
			nextreq = LIST_NEXT(req, gcllist);
			if (!GDP_NAME_SAME(req->rpdu->dst, dest) ||
					!EP_ASSERT(req->gcl == gcl))
			{
				_gdp_req_unlock(req);
				continue;
			}

			// remove subscription for this destination (but keep GCL locked)
			LIST_REMOVE(req, gcllist);
			req->flags &= ~GDP_REQ_ON_GCL_LIST;
			_gdp_gcl_decref(&req->gcl);
			_gdp_req_free(&req);
		}
	} while (!EP_STAT_ISOK(estat));
	gcl->flags &= ~GCLF_KEEPLOCKED;
	return estat;
}


/*
**  SUB_RECLAIM_RESOURCES --- remove any expired subscriptions
*/

void
sub_reclaim_resources(gdp_chan_t *chan)
{
	gdp_req_t *req;
	gdp_req_t *nextreq;
	EP_TIME_SPEC sub_timeout;

	{
		EP_TIME_SPEC sub_delta;
		long timeout = ep_adm_getlongparam("swarm.gdp.subscr.timeout",
								GDP_SUBSCR_TIMEOUT_DEF);

		ep_time_from_nsec(-timeout SECONDS, &sub_delta);
		ep_time_deltanow(&sub_delta, &sub_timeout);
		ep_dbg_cprintf(Dbg, 29,
				"sub_reclaim_resources: timeout = %ld\n", timeout);
	}

	ep_thr_mutex_lock(&_GdpSubscriptionMutex);
	for (req = LIST_FIRST(&_GdpSubscriptionRequests); req != NULL;
											req = nextreq)
	{
		int istat;

		// if this req is already locked it must be current
		istat = ep_thr_mutex_trylock(&req->mutex);
		nextreq = LIST_NEXT(req, chanlist);			//XXX race if istat != 0?
		EP_ASSERT_ELSE(req != nextreq, break);
		if (istat != 0)
		{
			// already locked
			if (ep_dbg_test(Dbg, 21))
			{
				ep_dbg_printf("sub_reclaim_resources: req already locked:\n    ");
				_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
			}
			continue;
		}

		if (ep_dbg_test(Dbg, 59))
		{
			ep_dbg_printf("sub_reclaim_resources: checking ");
			_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		}

		if (!EP_UT_BITSET(GDP_REQ_SRV_SUBSCR, req->flags))
		{
			ep_dbg_cprintf(Dbg, 59, "   ... not a subscription (flags = 0x%x)\n",
					req->flags);
		}
		else if (ep_time_before(&req->act_ts, &sub_timeout))
		{
			// this subscription seems to be dead
			if (ep_dbg_test(Dbg, 18))
			{
				ep_dbg_printf("    ...  subscription timeout: ");
				_gdp_gcl_dump(req->gcl, ep_dbg_getfile(), GDP_PR_BASIC, 0);
			}

			// have to manually remove req from lists to avoid lock inversion
			EP_ASSERT(req->gcl != NULL);
			if (EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags))
			{
				_gdp_gcl_lock(req->gcl);
				LIST_REMOVE(req, gcllist);
				_gdp_gcl_decref(&req->gcl);			// also unlocks GCL
			}
			if (EP_UT_BITSET(GDP_REQ_ON_CHAN_LIST, req->flags))
			{
				LIST_REMOVE(req, chanlist);			// chan already locked
			}
			req->flags &= ~(GDP_REQ_ON_GCL_LIST | GDP_REQ_ON_CHAN_LIST);
			_gdp_req_free(&req);
		}
		else if (ep_dbg_test(Dbg, 59))
		{
			ep_dbg_printf("    ... not yet time\n");
		}
		if (req != NULL)
			_gdp_req_unlock(req);
	}
	ep_thr_mutex_unlock(&_GdpSubscriptionMutex);
}
