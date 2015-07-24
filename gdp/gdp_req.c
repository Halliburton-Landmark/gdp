/* vim: set ai sw=4 sts=4 ts=4 :*/

/***********************************************************************
**
**  GDP Request handling
*/

#include <string.h>

#include "gdp.h"
#include "gdp_priv.h"

#include <ep/ep_dbg.h>
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
statestr(int state)
{
	static char sbuf[20];

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
	}
	else
	{
		// sanity checks
		EP_ASSERT(!EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags));
		EP_ASSERT(!EP_UT_BITSET(GDP_REQ_ON_CHAN_LIST, req->flags));
	}

	_gdp_req_lock(req);

	// initialize request
	if (pdu != NULL)
		req->pdu = pdu;
	else
		req->pdu = pdu = _gdp_pdu_new();
	req->gcl = gcl;
	if (gcl != NULL)
		_gdp_gcl_incref(gcl);
	req->stat = EP_STAT_OK;
	req->flags = flags;
	req->chan = chan;

	// keep track of all outstanding requests on a channel
	if (chan != NULL)
	{
		LIST_INSERT_HEAD(&chan->reqs, req, chanlist);
		req->flags |= GDP_REQ_ON_CHAN_LIST;
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
	req->state = GDP_REQ_ACTIVE;
	*reqp = req;
	ep_dbg_cprintf(Dbg, 48, "gdp_req_new(gcl=%p) => %p\n", gcl, req);
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
_gdp_req_free(gdp_req_t *req)
{
	ep_dbg_cprintf(Dbg, 48, "gdp_req_free(%p)  gcl=%p\n", req, req->gcl);

	EP_ASSERT(req->state != GDP_REQ_FREE);

	// remove the request from the channel subscription list
	if (EP_UT_BITSET(GDP_REQ_ON_CHAN_LIST, req->flags))
		LIST_REMOVE(req, chanlist);
	req->flags &= ~GDP_REQ_ON_CHAN_LIST;

	// remove the request from the GCL list
	if (EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags))
		LIST_REMOVE(req, gcllist);
	req->flags &= ~GDP_REQ_ON_GCL_LIST;

	// free the associated PDU
	if (req->pdu != NULL)
		_gdp_pdu_free(req->pdu);
	req->pdu = NULL;

	// dereference the gcl
	if (req->gcl != NULL)
		_gdp_gcl_decref(req->gcl);
	req->gcl = NULL;

	req->state = GDP_REQ_FREE;

	// add the empty request to the free list
	ep_thr_mutex_lock(&ReqFreeListMutex);
	LIST_INSERT_HEAD(&ReqFreeList, req, gcllist);
	ep_thr_mutex_unlock(&ReqFreeListMutex);

	_gdp_req_unlock(req);
}


/*
**  _GDP_REQ_FREEALL --- free all requests for a given GCL
*/

void
_gdp_req_freeall(struct req_head *reqlist, void (*shutdownfunc)(gdp_req_t *))
{
	gdp_req_t *req = LIST_FIRST(reqlist);

	ep_dbg_cprintf(Dbg, 49, ">>> _gdp_req_freeall(%p)\n", reqlist);
	while (req != NULL)
	{
		gdp_req_t *nextreq = LIST_NEXT(req, gcllist);
		if (shutdownfunc != NULL)
			(*shutdownfunc)(req);
		_gdp_req_free(req);
		req = nextreq;
	}
	ep_dbg_cprintf(Dbg, 49, "<<< _gdp_req_freeall(%p)\n", reqlist);
}


/*
**  Lock/unlock a request
*/

void
_gdp_req_lock(gdp_req_t *req)
{
	ep_dbg_cprintf(Dbg, 60, "_gdp_req_lock: req @ %p\n", req);
	ep_thr_mutex_lock(&req->mutex);
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

	if (ep_dbg_test(Dbg, 45))
	{
		ep_dbg_printf("_gdp_req_unsend: ");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	if (gcl == NULL)
	{
		ep_dbg_cprintf(Dbg, 4, "_gdp_req_unsend: req %p has NULL GCL\n",
				req);
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
**
**		XXX Race Condition: if the req is freed between the time
**		XXX the GCL is unlocked and the req is locked we have a
**		XXX problem.  But if we keep the GCL locked we have a lock
**		XXX ordering problem.
*/

gdp_req_t *
_gdp_req_find(gdp_gcl_t *gcl, gdp_rid_t rid)
{
	gdp_req_t *req;

	ep_dbg_cprintf(Dbg, 50, "_gdp_req_find(gcl=%p, rid=%" PRIgdp_rid")\n",
			gcl, rid);

	for (;;)
	{
		ep_thr_mutex_lock(&gcl->mutex);
		req = LIST_FIRST(&gcl->reqs);
		ep_thr_mutex_unlock(&gcl->mutex);
		while (req != NULL)
		{
			_gdp_req_lock(req);
			gdp_req_t *req2 = LIST_NEXT(req, gcllist);
			if (req->pdu->rid == rid)
				break;
			_gdp_req_unlock(req);
			req = req2;
		}
		if (req == NULL)
			break;				// nothing to find

		EP_ASSERT(req->state != GDP_REQ_FREE);
		if (req->state != GDP_REQ_ACTIVE)
			break;				// this is what we are looking for!

		// it's in the wrong state; wait for a change and then try again
		ep_dbg_cprintf(Dbg, 20, "_gdp_req_find: wrong state: %s\n",
				statestr(req->state));
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
			"gdp_req_find(gcl=%p, rid=%" PRIgdp_rid ") => %p, state %s\n",
			gcl, rid, req, statestr(req->state));
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
	{ GDP_REQ_DONE,			GDP_REQ_DONE,			"DONE"			},
	{ GDP_REQ_PERSIST,		GDP_REQ_PERSIST,		"PERSIST"		},
	{ GDP_REQ_CLT_SUBSCR,	GDP_REQ_CLT_SUBSCR,		"CLT_SUBSCR"	},
	{ GDP_REQ_SRV_SUBSCR,	GDP_REQ_SRV_SUBSCR,		"SRV_SUBSCR"	},
	{ GDP_REQ_SUBUPGRADE,	GDP_REQ_SUBUPGRADE,		"SUBUPGRADE"	},
	{ GDP_REQ_ALLOC_RID,	GDP_REQ_ALLOC_RID,		"ALLOC_RID"		},
	{ GDP_REQ_ON_GCL_LIST,	GDP_REQ_ON_GCL_LIST,	"ON_GCL_LIST"	},
	{ GDP_REQ_ON_CHAN_LIST,	GDP_REQ_ON_CHAN_LIST,	"ON_CHAN_LIST"	},
	{ GDP_REQ_CORE,			GDP_REQ_CORE,			"CORE"			},
	{ GDP_REQ_ASYNCIO,		GDP_REQ_ASYNCIO,		"ASYNCIO"		},
	{ 0,					0,						NULL			}
};

void
_gdp_req_dump(gdp_req_t *req, FILE *fp, int detail, int indent)
{
	char ebuf[200];

	if (req == NULL)
	{
		fprintf(fp, "req@%p: null\n", req);
		return;
	}

	if (req == NULL)
	{
		fprintf(fp, "req@%p: null\n", req);
		return;
	}
	flockfile(fp);
	fprintf(fp, "req@%p: ", req);
	fprintf(fp, "\n    numrecs=%" PRIu32
			", chan=%p, postproc=%p, sub_cb=%p, udata=%p\n"
			"    state=%s, stat=%s\n",
			req->numrecs, req->chan, req->postproc,
			req->sub_cb, req->udata, statestr(req->state),
			ep_stat_tostr(req->stat, ebuf, sizeof ebuf));
	fprintf(fp, "    flags=");
	ep_prflags(req->flags, ReqFlags, fp);
	fprintf(fp, "\n    ");
	_gdp_gcl_dump(req->gcl, fp, GDP_PR_BASIC, 0);
	fprintf(fp, "    ");
	_gdp_pdu_dump(req->pdu, fp);
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
	return ++MaxRid;
}

char *
gdp_rid_tostr(gdp_rid_t rid, char *buf, size_t len)
{
	snprintf(buf, len, "%d", rid);
	return buf;
}
