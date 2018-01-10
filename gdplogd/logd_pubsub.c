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
#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>

#include <sys/queue.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.pubsub",
								"GDP Log Daemon pub/sub handling");

extern EP_HASH	*_OpenGOBCache;		// associative cache


/*
**  SUB_SEND_MESSAGE_NOTIFICATION --- inform a subscriber of a new message
**
**		Assumes req is locked.
*/

void
sub_send_message_notification(gdp_req_t *pubreq, gdp_req_t *req)
{
	EP_STAT estat;

	if (ep_dbg_test(Dbg, 33))
	{
		ep_dbg_printf("sub_send_message_notification: ");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	gdp_msg_t *msg = _gdp_msg_new(GDP_ACK_CONTENT,
							req->cpdu->msg->rid, req->cpdu->msg->seqno);
	GdpDatum *pubdatum = pubreq->cpdu->msg->body->cmd_append->datum;
	GdpDatum *subdatum = msg->body->ack_content->datum;

	// cheat here: two pointers to one memory area
	msg->body->ack_content->datum = pubdatum;

	gdp_pdu_t *pdu = _gdp_pdu_new(msg, req->cpdu->dst, req->cpdu->src);
	estat = _gdp_pdu_out(pdu, req->chan, NULL);

	// undo the cheat so we don't double-free
	msg->body->ack_content->datum = subdatum;

	if (!EP_STAT_ISOK(estat))
	{
		ep_dbg_cprintf(Dbg, 1,
				"sub_send_message_notification: couldn't write PDU!\n");
	}

	// XXX: This won't really work in case of holes.
	req->nextrec++;

	if (req->numrecs > 0 && --req->numrecs <= 0)
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

	GDP_GOB_ASSERT_ISLOCKED(pubreq->gob);

	if (ep_dbg_test(Dbg, 32))
	{
		ep_dbg_printf("sub_notify_all_subscribers(%s) of pub",
				_gdp_proto_cmd_name(cmd));
		_gdp_req_dump(pubreq, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	// set up for subscription timeout
	{
		EP_TIME_SPEC sub_delta;
		long timeout = ep_adm_getlongparam("swarm.gdplogd.subscr.timeout", 0);

		if (timeout == 0)
			timeout = ep_adm_getlongparam("swarm.gdp.subscr.timeout",
									GDP_SUBSCR_TIMEOUT_DEF);
		ep_time_from_nsec(-timeout SECONDS, &sub_delta);
		ep_time_deltanow(&sub_delta, &sub_timeout);
	}

	pubreq->gob->flags |= GCLF_KEEPLOCKED;
	for (req = LIST_FIRST(&pubreq->gob->reqs); req != NULL; req = nextreq)
	{
		_gdp_req_lock(req);
		nextreq = LIST_NEXT(req, goblist);
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
			sub_send_message_notification(pubreq, req);
		}
		else
		{
			// this subscription seems to be dead
			if (ep_dbg_test(Dbg, 18))
			{
				ep_dbg_printf("sub_notify_all_subscribers: subscription timeout: ");
				_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
				_gdp_gob_dump(req->gob, ep_dbg_getfile(), GDP_PR_BASIC, 0);
			}

			// actually remove the subscription
			//XXX isn't this done by _gdp_req_free???
			//LIST_REMOVE(req, goblist);

			EP_ASSERT(req->gob != NULL);
			EP_ASSERT(EP_UT_BITSET(GDP_REQ_ON_GOB_LIST, req->flags));
			_gdp_req_free(&req);
		}
		if (req != NULL)
			_gdp_req_unlock(req);
	}
	pubreq->gob->flags &= ~GCLF_KEEPLOCKED;
}


/*
**  SUB_END_SUBSCRIPTION --- terminate a subscription
**
**		req and req->gob should be locked when this is called.
*/

void
sub_end_subscription(gdp_req_t *req)
{

	EP_THR_MUTEX_ASSERT_ISLOCKED(&req->mutex);
	GDP_GOB_ASSERT_ISLOCKED(req->gob);

	// make it not persistent and not a subscription
	req->flags &= ~(GDP_REQ_PERSIST | GDP_REQ_SRV_SUBSCR);

	// remove the request from the work list
	if (EP_UT_BITSET(GDP_REQ_ON_GOB_LIST, req->flags))
	{
		gdp_gob_t *gob = req->gob;
		LIST_REMOVE(req, goblist);
		req->flags &= ~GDP_REQ_ON_GOB_LIST;
		EP_ASSERT(gob->refcnt > 1);
		_gdp_gob_decref(&gob, true);
	}

	// send an "end of subscription" event
	req->rpdu->msg->cmd = GDP_ACK_DELETED;

	if (ep_dbg_test(Dbg, 39))
	{
		ep_dbg_printf("sub_end_subscription removing:\n  ");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	(void) _gdp_pdu_out(req->rpdu, req->chan, NULL);
}


/*
**  Unsubscribe all requests for a given gob and destination.
**  Can also optionally select a particular request id.
*/

EP_STAT
sub_end_all_subscriptions(
		gdp_gob_t *gob,
		gdp_name_t dest,
		gdp_rid_t rid)
{
	EP_STAT estat;
	gdp_req_t *req;
	gdp_req_t *nextreq;

	if (ep_dbg_test(Dbg, 29))
	{
		ep_dbg_printf("sub_end_all_subscriptions: ");
		_gdp_gob_dump(gob, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	GDP_GOB_ASSERT_ISLOCKED(gob);
	if (EP_UT_BITSET(GCLF_KEEPLOCKED, gob->flags) && ep_dbg_test(Dbg, 1))
		ep_dbg_printf("sub_end_all_subscriptions: GCLF_KEEPLOCKED on entry\n");
	gob->flags |= GCLF_KEEPLOCKED;

	do
	{
		estat = EP_STAT_OK;
		for (req = LIST_FIRST(&gob->reqs); req != NULL; req = nextreq)
		{
			estat = _gdp_req_lock(req);
			EP_STAT_CHECK(estat, break);
			nextreq = LIST_NEXT(req, goblist);
			if (!GDP_NAME_SAME(req->rpdu->dst, dest) ||
					(rid != GDP_PDU_NO_RID && rid != req->rpdu->msg->rid) ||
					!EP_ASSERT(req->gob == gob))
			{
				_gdp_req_unlock(req);
				continue;
			}

			// remove subscription for this destination (but keep GOB locked)
			if (ep_dbg_test(Dbg, 39))
			{
				ep_dbg_printf("sub_end_all_subscriptions removing ");
				_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
			}
			LIST_REMOVE(req, goblist);
			req->flags &= ~GDP_REQ_ON_GOB_LIST;
			_gdp_gob_decref(&req->gob, false);
			_gdp_req_free(&req);
		}
	} while (!EP_STAT_ISOK(estat));
	gob->flags &= ~GCLF_KEEPLOCKED;
	return estat;
}


/*
**  SUB_RECLAIM_RESOURCES --- remove any expired subscriptions
**
**		This is a bit tricky to get lock ordering correct.  The
**		obvious implementation is to loop through the channel
**		list, but when you try to lock a GOB or a request you
**		have a lock ordering problem (the channel is quite low
**		in the locking hierarchy).  Instead you run through
**		the GOB hash table.
*/

// helper (does most of the work)
static void
gob_reclaim_subscriptions(gdp_gob_t *gob)
{
	int istat;
	gdp_req_t *req;
	gdp_req_t *nextreq;
	EP_TIME_SPEC sub_timeout;

	// just in case
	if (gob == NULL)
		return;

	{
		EP_TIME_SPEC sub_delta;
		long timeout = ep_adm_getlongparam("swarm.gdp.subscr.timeout",
								GDP_SUBSCR_TIMEOUT_DEF);

		ep_time_from_nsec(-timeout SECONDS, &sub_delta);
		ep_time_deltanow(&sub_delta, &sub_timeout);
		ep_dbg_cprintf(Dbg, 39,
				"gob_reclaim_subscriptions: GOB = %p, refcnt = %d, timeout = %ld\n",
				gob, gob->refcnt, timeout);
	}

	// don't even try locked GOBs
	// first check is to avoid extraneous errors
	if (EP_UT_BITSET(GCLF_ISLOCKED, gob->flags))
	{
		ep_dbg_cprintf(Dbg, 39, " ... skipping locked GOB\n");
		return;
	}
	istat = ep_thr_mutex_trylock(&gob->mutex);
	if (istat != 0)
	{
		if (ep_dbg_test(Dbg, 21))
		{
			ep_dbg_printf("gob_reclaim_subscriptions: gob already locked:\n    ");
			_gdp_gob_dump(gob, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		}
		return;
	}
	gob->flags |= GCLF_ISLOCKED;

	nextreq = LIST_FIRST(&gob->reqs);
	while ((req = nextreq) != NULL)
	{
		if (ep_dbg_test(Dbg, 59))
		{
			ep_dbg_printf("gob_reclaim_subscriptions: checking ");
			_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		}

		// now that GOB is locked, we lock the request
		istat = ep_thr_mutex_trylock(&req->mutex);
		if (istat != 0)		// checking on status of req lock attempt
		{
			// already locked
			if (ep_dbg_test(Dbg, 41))
			{
				ep_dbg_printf("gob_reclaim_subscriptions: req already locked:\n    ");
				_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
			}
			_gdp_gob_unlock(req->gob);
			continue;
		}

		// get next request while locked and do sanity checks
		nextreq = LIST_NEXT(req, goblist);
		if (!EP_ASSERT(req != nextreq) || !EP_ASSERT(req->gob == gob))
		{
			_gdp_gob_unlock(req->gob);
			break;
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
				_gdp_gob_dump(req->gob, ep_dbg_getfile(), GDP_PR_BASIC, 0);
			}

			// have to manually remove req from lists to avoid lock inversion
			if (EP_UT_BITSET(GDP_REQ_ON_GOB_LIST, req->flags))
			{
				// gob is already locked
				LIST_REMOVE(req, goblist);
			}
			if (EP_UT_BITSET(GDP_REQ_ON_CHAN_LIST, req->flags))
			{
				LIST_REMOVE(req, chanlist);			// chan already locked
			}
			req->flags &= ~(GDP_REQ_ON_GOB_LIST | GDP_REQ_ON_CHAN_LIST);
			_gdp_gob_decref(&req->gob, true);
			_gdp_req_free(&req);
		}
		else if (ep_dbg_test(Dbg, 59))
		{
			ep_dbg_printf("    ... not yet time\n");
		}

		if (req != NULL)
			_gdp_req_unlock(req);
	}

	if (gob != NULL)
		_gdp_gob_unlock(gob);
}

void
sub_reclaim_resources(gdp_chan_t *chan)
{
	_gdp_gob_cache_foreach(gob_reclaim_subscriptions);
}
