/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  GDP Request handling
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
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

#include <string.h>
#include <sys/queue.h>

#include "gdp.h"
#include "gdp_event.h"
#include "gdp_priv.h"

#include <ep/ep_dbg.h>
#include <ep/ep_log.h>
#include <ep/ep_prflags.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.req", "GDP request processing");

// unused request structures
static struct req_head	ReqFreeList = LIST_HEAD_INITIALIZER(ReqFreeList);
static EP_THR_MUTEX		ReqFreeListMutex	EP_THR_MUTEX_INITIALIZER;

static const char *ReqStates[] =
{
	"FREE",			// 0
	"ACTIVE",		// 1
	"WAITING",		// 2
	"IDLE",			// 3
};


/*
**  Show string version of state (for debugging output)
**
**		Not thread safe, but only for impossible states.
*/

static const char *
statestr(const gdp_req_t *req)
{
	static char sbuf[20];
	int state;

	if (req == NULL)
		return "(NONE)";

	state = req->state;
	if (state >= 0 && state < sizeof ReqStates)
	{
		return ReqStates[state];
	}
	else
	{
		snprintf(sbuf, sizeof sbuf, "IMPOSSIBLE(%d)", state);
		return sbuf;
	}
}

/*
**  _GDP_REQ_NEW --- allocate a new request
**
**	Parameters:
**		cmd --- the command to be issued
**		gcl --- the associated GCL handle, if any
**		chan --- the channel associated with the request
**		pdu --- the existing PDU; if none, one will be allocated
**		flags --- modifier flags
**		reqp --- a pointer to the output area
**
**	Returns:
**		status
**		The request has been allocated an id (possibly unique to gcl),
**			but the request has not been linked onto the GCL's request list.
**			This allows the caller to adjust the request without locking it.
**		The request is always returned locked.
*/

EP_STAT
_gdp_req_new(int cmd,
		gdp_gcl_t *gcl,
		gdp_chan_t *chan,
		gdp_pdu_t *pdu,
		uint32_t flags,
		gdp_req_t **reqp)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;
	bool newpdu = pdu == NULL;

#if ALWAYS_ALLOC_NEW_RID			//DEBUG
	flags |= GDP_REQ_ALLOC_RID;		//DEBUG
#endif								//DEBUG

	// if the caller wants asynchronous results, tweak flags accordingly
	if (EP_UT_BITSET(GDP_REQ_ASYNCIO, flags))
		flags |= GDP_REQ_PERSIST | GDP_REQ_ALLOC_RID;

	EP_ASSERT_REQUIRE(gcl == NULL || EP_UT_BITSET(GCLF_INUSE, gcl->flags));

	// simplify the simple case
	if (chan == NULL)
		chan = _GdpChannel;

	// get memory, off free list if possible
	ep_thr_mutex_lock(&ReqFreeListMutex);
	if ((req = LIST_FIRST(&ReqFreeList)) != NULL)
	{
		LIST_REMOVE(req, gcllist);
		EP_ASSERT(req->state == GDP_REQ_FREE);
	}
	ep_thr_mutex_unlock(&ReqFreeListMutex);
	if (req == NULL)
	{
		// nothing on free list; allocate another
		req = ep_mem_zalloc(sizeof *req);
		ep_thr_mutex_init(&req->mutex, EP_THR_MUTEX_DEFAULT);
		ep_thr_cond_init(&req->cond);
		STAILQ_INIT(&req->events);
	}
	else
	{
		// sanity checks
		EP_ASSERT(!EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags));
		EP_ASSERT(!EP_UT_BITSET(GDP_REQ_ON_CHAN_LIST, req->flags));
	}

	// make it active so that _gdp_req_lock doesn't object
	req->state = GDP_REQ_ACTIVE;
	(void) _gdp_req_lock(req);

	// initialize request
	if (pdu == NULL)
		pdu = _gdp_pdu_new();
	req->pdu = pdu;
	req->gcl = gcl;
	req->stat = EP_STAT_OK;
	req->flags = flags;
	req->chan = chan;

	// keep track of all outstanding requests on a channel
	if (chan != NULL)
	{
		ep_thr_mutex_lock(&chan->mutex);
		LIST_INSERT_HEAD(&chan->reqs, req, chanlist);
		req->flags |= GDP_REQ_ON_CHAN_LIST;
		ep_thr_mutex_unlock(&chan->mutex);
	}

	// if we're not passing in a PDU, initialize the new one
	if (newpdu)
	{
		req->pdu->cmd = cmd;
		if (gcl != NULL)
			memcpy(req->pdu->dst, gcl->name, sizeof req->pdu->dst);
		if ((gcl == NULL || !EP_UT_BITSET(GDP_REQ_PERSIST, flags)) &&
				!EP_UT_BITSET(GDP_REQ_ALLOC_RID, flags))
		{
			// just use constant zero; any value would be fine
			req->pdu->rid = GDP_PDU_NO_RID;
		}
		else
		{
			// allocate a new unique request id
			req->pdu->rid = _gdp_rid_new(gcl, chan);
		}
	}

	// success
	*reqp = req;
	ep_dbg_cprintf(Dbg, 48, "_gdp_req_new(gcl=%p, cmd=%s) => %p (rid=%d)\n",
			gcl, _gdp_proto_cmd_name(cmd), req, req->pdu->rid);
	return estat;
}


/*
**  _GDP_REQ_FREE --- return a request to the free list
**
**		Note that we grab the GCL linked list as the free list, since
**		it's impossible for a free request to be attached to a GCL.
**
**		The request must be locked on entry.
*/

void
_gdp_req_free(gdp_req_t **reqp)
{
	gdp_req_t *req = *reqp;

	if (req == NULL)
		return;

	ep_dbg_cprintf(Dbg, 48, "_gdp_req_free(%p)  state=%d, gcl=%p\n",
			req, req->state, req->gcl);

	if (req->state == GDP_REQ_FREE)
	{
		// req was freed after a reference was taken
		return;
	}

	// remove the request from the channel subscription list
	if (EP_UT_BITSET(GDP_REQ_ON_CHAN_LIST, req->flags))
	{
		ep_thr_mutex_lock(&req->chan->mutex);
		LIST_REMOVE(req, chanlist);
		req->flags &= ~GDP_REQ_ON_CHAN_LIST;
		ep_thr_mutex_unlock(&req->chan->mutex);
	}

	// remove the request from the GCL list
	if (EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags))
	{
		ep_thr_mutex_lock(&req->gcl->mutex);
		LIST_REMOVE(req, gcllist);
		req->flags &= ~GDP_REQ_ON_GCL_LIST;
		ep_thr_mutex_unlock(&req->gcl->mutex);
	}

	// remove any pending events from the request
	{
		gdp_event_t *gev;
		while ((gev = STAILQ_FIRST(&req->events)) != NULL)
			STAILQ_REMOVE_HEAD(&req->events, queue);
	}

	// req should be unreferencable now
	_gdp_req_unlock(req);

	// free the associated PDU(s)
	if (req->rpdu != NULL && req->rpdu != req->pdu)
		_gdp_pdu_free(req->rpdu);
	if (req->pdu != NULL)
		_gdp_pdu_free(req->pdu);
	req->pdu = req->rpdu = NULL;

	// dereference the gcl
	if (req->gcl != NULL)
		_gdp_gcl_decref(&req->gcl);

	req->state = GDP_REQ_FREE;

    // initialize parameters for replication service
    req->fwdcnt = 0;
    req->acksnt = 0;

	// add the empty request to the free list
	ep_thr_mutex_lock(&ReqFreeListMutex);
	LIST_INSERT_HEAD(&ReqFreeList, req, gcllist);
	ep_thr_mutex_unlock(&ReqFreeListMutex);

	// make sure the pointer is invalid
	*reqp = NULL;
}


/*
**  _GDP_REQ_FREEALL --- free all requests for a given GCL
**
**		If _gdp_req_lock fails, we start over.
*/

void
_gdp_req_freeall(struct req_head *reqlist, void (*shutdownfunc)(gdp_req_t *))
{
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 49, ">>> _gdp_req_freeall(%p)\n", reqlist);
	do
	{
		gdp_req_t *req;
		gdp_req_t *nextreq;

		estat = EP_STAT_OK;
		for (req = LIST_FIRST(reqlist); req != NULL; req = nextreq)
		{
			estat = _gdp_req_lock(req);
			if (!EP_STAT_ISOK(estat))
			{
				ep_log(estat, "_gdp_req_freeall: couldn't acquire req lock");
				break;
			}

			nextreq = LIST_NEXT(req, gcllist);
			if (shutdownfunc != NULL)
				(*shutdownfunc)(req);
			_gdp_req_free(&req);
		}
	} while (!EP_STAT_ISOK(estat));

	ep_dbg_cprintf(Dbg, 49, "<<< _gdp_req_freeall(%p)\n", reqlist);
}


/*
**  Lock/unlock a request
*/

EP_STAT
_gdp_req_lock(gdp_req_t *req)
{
	ep_dbg_cprintf(Dbg, 60, "_gdp_req_lock: req @ %p\n", req);
	ep_thr_mutex_lock(&req->mutex);

	// if this request was being freed, the reference might be dead now
	if (req->state != GDP_REQ_FREE)
		return EP_STAT_OK;

	// oops, unlock it and return failure
	ep_thr_mutex_unlock(&req->mutex);
	return GDP_STAT_DEAD_REQ;
}

void
_gdp_req_unlock(gdp_req_t *req)
{
	ep_dbg_cprintf(Dbg, 60, "_gdp_req_unlock: req @ %p\n", req);
	ep_thr_mutex_unlock(&req->mutex);
}


/*
**   _GDP_REQ_SEND --- send a request to the GDP daemon
**
**		This makes no attempt to read results.
**
**		This routine also links the request onto the GCL list (if any)
**		so that the matching response PDU can find the request (the
**		PDU contains the GCL and the RID, which are enough to find
**		the corresponding request).  If it's already on a GCL list we
**		work on the assumption that it is this one.  We might want to
**		verify that for debugging purposes.
**
**		The request must be locked.
*/

EP_STAT
_gdp_req_send(gdp_req_t *req)
{
	EP_STAT estat;
	gdp_gcl_t *gcl = req->gcl;

	if (ep_dbg_test(Dbg, 45))
	{
		flockfile(ep_dbg_getfile());
		ep_dbg_printf("_gdp_req_send: ");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		funlockfile(ep_dbg_getfile());
	}

	req->flags &= ~GDP_REQ_DONE;
	if (gcl != NULL && !EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags))
	{
		// link the request to the GCL
		ep_thr_mutex_lock(&gcl->mutex);
		LIST_INSERT_HEAD(&gcl->reqs, req, gcllist);
		req->flags |= GDP_REQ_ON_GCL_LIST;
		ep_thr_mutex_unlock(&gcl->mutex);

		// register this handle so we can process the results
		//		(it's likely that it's already in the cache)
		_gdp_gcl_cache_add(gcl, 0);
	}

	// write the message out
	estat = _gdp_pdu_out(req->pdu, req->chan, req->md);

	// done
	return estat;
}


/*
**  _GDP_REQ_UNSEND --- pull a request off a GCL list
**
**		Used when the attempt to do an invocation fails.
*/

EP_STAT
_gdp_req_unsend(gdp_req_t *req)
{
	gdp_gcl_t *gcl = req->gcl;

	if (ep_dbg_test(Dbg, 17))
	{
		ep_dbg_printf("_gdp_req_unsend: ");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	if (gcl == NULL)
	{
		ep_dbg_cprintf(Dbg, 4, "_gdp_req_unsend: req %p has NULL GCL\n",
				req);
		return GDP_STAT_NULL_GCL;
	}
	else if (!EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags))
	{
		ep_dbg_cprintf(Dbg, 4, "_gdp_req_unsend: req %p not on GCL list\n",
				req);
	}
	else
	{
		ep_thr_mutex_lock(&gcl->mutex);
		LIST_REMOVE(req, gcllist);
		req->flags &= ~GDP_REQ_ON_GCL_LIST;
		ep_thr_mutex_unlock(&gcl->mutex);
	}

	return EP_STAT_OK;
}


/*
**  _GDP_REQ_FIND --- find a request in a GCL
**
**		The state must show that the req is not currently active; if it
**		is we would clobber one another.  Note that we can't just keep
**		the req locked because that would require passing a lock between
**		threads, which is a non-starter.  To get around that the req
**		has a state; if it is currently in active use by another thread
**		we have to wait.  However, this does return the req pre-locked.
**
**		This may be the wrong place to do this, since this blocks the
**		I/O thread.  Arguably the I/O thread should read PDUs, put them
**		on a service list, and let another thread handle it.  This is
**		more-or-less what gdplogd does now, so this problem only shows
**		up in clients that may be working with many GCLs at the same
**		time.  Tomorrow is another day.
*/

gdp_req_t *
_gdp_req_find(gdp_gcl_t *gcl, gdp_rid_t rid)
{
	gdp_req_t *req;

	ep_dbg_cprintf(Dbg, 50, "_gdp_req_find(gcl=%p, rid=%" PRIgdp_rid")\n",
			gcl, rid);
	GDP_ASSERT_GOOD_GCL(gcl);

	for (;;)
	{
		EP_STAT estat;
		gdp_req_t *nextreq;

		do
		{
			estat = EP_STAT_OK;
			ep_thr_mutex_lock(&gcl->mutex);
			req = LIST_FIRST(&gcl->reqs);
			for (; req != NULL; req = nextreq)
			{
				estat = _gdp_req_lock(req);
				EP_STAT_CHECK(estat, break);
				nextreq = LIST_NEXT(req, gcllist);
				if (req->pdu->rid == rid)
					break;
				_gdp_req_unlock(req);
			}
			ep_thr_mutex_unlock(&gcl->mutex);
		} while (!EP_STAT_ISOK(estat));
		if (req == NULL)
			break;				// nothing to find

		EP_ASSERT(req->state != GDP_REQ_FREE);
		if (req->state != GDP_REQ_ACTIVE)
			break;				// this is what we are looking for!

		// it's in the wrong state; wait for a change and then try again
		ep_dbg_cprintf(Dbg, 20, "_gdp_req_find: wrong state: %s\n",
				statestr(req));
		//XXX should have a timeout here
		ep_thr_cond_wait(&req->cond, &req->mutex, NULL);
	}
	if (req != NULL)
	{
		if (!EP_UT_BITSET(GDP_REQ_PERSIST, req->flags))
		{
			EP_ASSERT(EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags));
			LIST_REMOVE(req, gcllist);
			req->flags &= ~GDP_REQ_ON_GCL_LIST;
		}
	}

	ep_dbg_cprintf(Dbg, 48,
			"_gdp_req_find(gcl=%p, rid=%" PRIgdp_rid ") => %p, state %s\n",
			gcl, rid, req, statestr(req));
	return req;
}


/*
**  Print a request (for debugging)
**
**		This potentially references the req while it is unlocked,
**		which isn't good, but since this is only for debugging and
**		is read-only we'll take the risk.
*/

static EP_PRFLAGS_DESC	ReqFlags[] =
{
	{ GDP_REQ_ASYNCIO,		GDP_REQ_ASYNCIO,		"ASYNCIO"		},
	{ GDP_REQ_DONE,			GDP_REQ_DONE,			"DONE"			},
	{ GDP_REQ_CLT_SUBSCR,	GDP_REQ_CLT_SUBSCR,		"CLT_SUBSCR"	},
	{ GDP_REQ_SRV_SUBSCR,	GDP_REQ_SRV_SUBSCR,		"SRV_SUBSCR"	},
	{ GDP_REQ_PERSIST,		GDP_REQ_PERSIST,		"PERSIST"		},
	{ GDP_REQ_SUBUPGRADE,	GDP_REQ_SUBUPGRADE,		"SUBUPGRADE"	},
	{ GDP_REQ_ALLOC_RID,	GDP_REQ_ALLOC_RID,		"ALLOC_RID"		},
	{ GDP_REQ_ON_GCL_LIST,	GDP_REQ_ON_GCL_LIST,	"ON_GCL_LIST"	},
	{ GDP_REQ_ON_CHAN_LIST,	GDP_REQ_ON_CHAN_LIST,	"ON_CHAN_LIST"	},
	{ GDP_REQ_CORE,			GDP_REQ_CORE,			"CORE"			},
	{ GDP_REQ_ROUTEFAIL,	GDP_REQ_ROUTEFAIL,		"ROUTEFAIL"		},
	{ 0,					0,						NULL			}
};

void
_gdp_req_dump(const gdp_req_t *req, FILE *fp, int detail, int indent)
{
	char ebuf[200];

	if (req == NULL)
	{
		fprintf(fp, "req@%p: null\n", req);
		return;
	}
	flockfile(fp);
	fprintf(fp, "req@%p:\n", req);
	fprintf(fp, "    nextrec=%" PRIgdp_recno ", numrecs=%" PRIu32 ", chan=%p\n"
			"    postproc=%p, sub_cb=%p, udata=%p\n"
			"    state=%s, stat=%s\n",
			req->nextrec, req->numrecs, req->chan,
			req->postproc, req->sub_cb, req->udata,
			statestr(req), ep_stat_tostr(req->stat, ebuf, sizeof ebuf));
	fprintf(fp, "    act_ts=");
	ep_time_print(&req->act_ts, fp, EP_TIME_FMT_HUMAN);
	fprintf(fp, "\n    flags=");
	ep_prflags(req->flags, ReqFlags, fp);
	fprintf(fp, "\n    ");
	_gdp_gcl_dump(req->gcl, fp, GDP_PR_BASIC, 0);
	fprintf(fp, "    ");
	_gdp_pdu_dump(req->pdu, fp);
	if (req->rpdu != NULL)
	{
		fprintf(fp, "    r");
		_gdp_pdu_dump(req->rpdu, fp);
	}
	funlockfile(fp);
}


/***********************************************************************
**
**	Request ID handling
**
**		Very simplistic for now.  RIDs really only need to be unique
**		within a given GCL/channel tuple.
*/

static gdp_rid_t	MaxRid = 0;

gdp_rid_t
_gdp_rid_new(gdp_gcl_t *gcl, gdp_chan_t *chan)
{
	if (MaxRid == UINT32_MAX)
	{
		// overflow!!!  at least log something
		ep_log(EP_STAT_SEVERE, "Request ID overflow");
	}
	return ++MaxRid;
}

char *
gdp_rid_tostr(gdp_rid_t rid, char *buf, size_t len)
{
	snprintf(buf, len, "%d", rid);
	return buf;
}
