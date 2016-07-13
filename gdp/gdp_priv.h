/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  ----- BEGIN LICENSE BLOCK -----
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
**  ----- END LICENSE BLOCK -----
*/

/*
**	These headers are not intended for external use.
*/

#ifndef _GDP_PRIV_H_
#define _GDP_PRIV_H_

#ifndef GDP_EXTENDED_LOCKING_CHECK
#define GDP_EXTENDED_LOCKING_CHECK		1
#endif

#include <ep/ep_crypto.h>
#include <ep/ep_thr.h>

#include <event2/buffer.h>

#include <stdio.h>

typedef struct gdp_chan		gdp_chan_t;
typedef struct gdp_req		gdp_req_t;

extern EP_THR		_GdpIoEventLoopThread;
extern gdp_chan_t	*_GdpChannel;		// our primary app-level protocol port
extern gdp_name_t	_GdpMyRoutingName;	// source name for PDUs
extern bool			_GdpLibInitialized;	// are we initialized?

#define GDP_CHECK_INITIALIZED											\
				{														\
					if (!_GdpLibInitialized)							\
						(void) gdp_init(NULL);							\
				}

#include "gdp_pdu.h"

// declare the type of the gdp_req linked list (used multiple places)
LIST_HEAD(req_head, gdp_req);


/*
**  Some generic constants
*/

// "dump" routine detail parameters (XXX should these be public?)
#define GDP_PR_PRETTY		0		// suitable for end users
#define GDP_PR_BASIC		1		// basic debug information
#define GDP_PR_DETAILED		16		// detailed information
#define GDP_PR_RECURSE		32		// recurse into substructures
									// add N to recurse N+1 levels deep


/*
**  GDP Channels
**
**		These represent a communications path to the routing layer.
**		At the moment those are connections.
*/

struct gdp_chan
{
	EP_THR_MUTEX		mutex;			// lock before changes
	EP_THR_COND			cond;			// wake up after state change
	int16_t				state;			// current state of channel
	uint16_t			flags;			// status flags
	struct bufferevent	*bev;			// associated bufferevent (socket)
	struct req_head		reqs;			// reqs associated with this channel
	void				(*close_cb)(	// called on channel close
							gdp_chan_t *chan);
	EP_STAT				(*advertise)(	// called to do our advertisements
							int cmd);
	void				(*process)(		// called to process a PDU
							gdp_pdu_t *pdu,
							gdp_chan_t *chan);
	EP_THR				sub_thr_id;		// subscription poker thread id
};

/* Channel states */
#define GDP_CHAN_UNCONNECTED	0		// channel is not connected yet
#define GDP_CHAN_CONNECTING		1		// connection being initiated
#define GDP_CHAN_CONNECTED		2		// channel is connected and active
#define GDP_CHAN_ERROR			3		// channel has had error
#define GDP_CHAN_CLOSING		4		// channel is closing

/* Channel flags */
#define GDP_CHAN_HAS_SUB_THR	0x0001	// subscription poker thread is running

EP_STAT			_gdp_chan_open(				// open channel to routing layer
						const char *gdpd_addr,
						void (*process)(gdp_pdu_t *, gdp_chan_t *),
						gdp_chan_t **pchan);
void			_gdp_chan_close(			// close channel
						gdp_chan_t **pchan);


/*
**	 Datums
**		These are the underlying data unit that is passed through a GCL.
**
**		The timestamp here is the database commit timestamp; any sample
**		timestamp must be added by the sensor itself as part of the data.
*/

struct gdp_datum
{
	EP_THR_MUTEX		mutex;			// locking mutex (mostly for dbuf)
	struct gdp_datum	*next;			// next in free list
	gdp_recno_t			recno;			// the record number
	EP_TIME_SPEC		ts;				// commit timestamp
	gdp_buf_t			*dbuf;			// data buffer
	gdp_buf_t			*sig;			// signature (may be NULL)
	short				sigmdalg;		// message digest algorithm
	short				siglen;			// signature length
	bool				inuse:1;		// the datum is in use (for debugging)
};

// dump data record (for debugging)
extern void		_gdp_datum_dump(
					const gdp_datum_t *datum,	// message to print
					FILE *fp);					// file to print it to



/*
**  GDP Channel/Logs
*/

struct gdp_gcl
{
	EP_THR_MUTEX		mutex;			// lock on this data structure
	time_t				utime;			// last time used (seconds only)
	LIST_ENTRY(gdp_gcl)	ulist;			// list sorted by use time
	struct req_head		reqs;			// list of outstanding requests
	gdp_name_t			name;			// the internal name
	gdp_pname_t			pname;			// printable name (for debugging)
	gdp_iomode_t		iomode;			// read only or append only
	uint16_t			flags;			// flag bits, see below
	int					refcnt;			// reference counter
	void				(*freefunc)(struct gdp_gcl *);
										// called when this is freed
	gdp_recno_t			nrecs;			// number of records (= last recno)
	gdp_gclmd_t			*gclmd;			// metadata
	EP_CRYPTO_MD		*digest;		// base crypto digest
	EP_STAT				(*apndfilter)(	// append filter function
							gdp_datum_t *,
							void *);
	void				*apndfpriv;		// private data for apndfilter
	EP_STAT				(*readfilter)(	// read filter function
							gdp_datum_t *,
							void *);
	void				*readfpriv;		// private data for readfilter
	struct gdp_gcl_xtra	*x;				// for use by gdpd, gdp-rest
};

/* flags for GCL handles */
#define GCLF_DROPPING		0x0001		// handle is being deallocated
#define GCLF_INCACHE		0x0002		// handle is in cache
#define GCLF_ISLOCKED		0x0004		// GclCacheMutex already locked
#define GCLF_INUSE			0x0008		// handle is allocated
#define GCLF_DEFER_FREE		0x0010		// defer actual free until reclaim

#define GDP_ASSERT_GOOD_GCL(gcl)	\
				(EP_ASSERT_REQUIRE((gcl) != NULL &&	\
				EP_UT_BITSET(GCLF_INUSE, (gcl)->flags)))
#if GDP_EXTENDED_LOCKING_CHECK
#define GDP_ASSERT_LOCKED(x)											\
				(EP_ASSERT_REQUIRE((x) != NULL &&						\
					ep_thr_mutex_trylock(&(x)->mutex) != 0))
#else
#define GDP_ASSERT_LOCKED(x)
#endif

/*
**  GCL cache.
**
**		Implemented in gdp/gdp_gcl_cache.c.
*/

EP_STAT			_gdp_gcl_cache_init(void);	// initialize cache

void			_gdp_gcl_cache_dump(		// print cache (for debugging)
						int plev,
						FILE *fp);

gdp_gcl_t		*_gdp_gcl_cache_get(		// get entry from cache
						gdp_name_t gcl_name,
						gdp_iomode_t mode);

void			_gdp_gcl_cache_add(			// add entry to cache
						gdp_gcl_t *gcl,
						gdp_iomode_t mode);

void			_gdp_gcl_cache_changename(	// update the name of a GCL
						gdp_gcl_t *gcl,
						gdp_name_t newname);

void			_gdp_gcl_cache_drop(		// drop entry from cache
						gdp_gcl_t *gcl);

void			_gdp_gcl_cache_reclaim(		// flush old entries
						time_t maxage);

void			_gdp_gcl_cache_shutdown(	// immediately shut down cache
						void (*shutdownfunc)(gdp_req_t *));

void			_gdp_gcl_touch(				// move to front of LRU list
						gdp_gcl_t *gcl);

void			_gdp_gcl_incref(			// increase reference count
						gdp_gcl_t *gcl);

void			_gdp_gcl_decref(			// decrease reference count
						gdp_gcl_t **gclp);

/*
**  Other GCL handling.  These are shared between client access
**  and the GDP daemon.
**
**  Implemented in gdp/gdp_gcl_ops.c.
*/

EP_STAT			_gdp_gcl_newhandle(			// create new in-mem handle
						gdp_name_t gcl_name,
						gdp_gcl_t **gclhp);

void			_gdp_gcl_freehandle(		// free in-memory handle
						gdp_gcl_t *gcl);

void			_gdp_gcl_newname(			// create new name based on metadata
						gdp_gcl_t *gcl);

void			_gdp_gcl_dump(				// dump for debugging
						const gdp_gcl_t *gcl,	// GCL to print
						FILE *fp,				// where to print it
						int detail,				// how much to print
						int indent);			// unused at this time

EP_STAT			_gdp_gcl_create(			// create a new GCL
						gdp_name_t gclname,
						gdp_name_t logdname,
						gdp_gclmd_t *gmd,
						gdp_chan_t *chan,
						uint32_t reqflags,
						gdp_gcl_t **pgcl);

EP_STAT			_gdp_gcl_open(				// open a GCL
						gdp_gcl_t *gcl,
						int cmd,
						EP_CRYPTO_KEY *skey,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_close(				// close a GCL (handle)
						gdp_gcl_t *gcl,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_read(				// read a GCL record (gdpd shared)
						gdp_gcl_t *gcl,
						gdp_datum_t *datum,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_read_async(		// read asynchronously
						gdp_gcl_t *gcl,
						gdp_datum_t *datum,
						gdp_event_cbfunc_t cbfunc,
						void *cbarg,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_append(			// append a record (gdpd shared)
						gdp_gcl_t *gcl,
						gdp_datum_t *datum,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_append_async(		// append asynchronously
						gdp_gcl_t *gcl,
						gdp_datum_t *datum,
						gdp_event_cbfunc_t cbfunc,
						void *cbarg,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_subscribe(			// subscribe to data
						gdp_gcl_t *gcl,
						int cmd,
						gdp_recno_t start,
						int32_t numrecs,
						EP_TIME_SPEC *timeout,
						gdp_event_cbfunc_t cbfunc,
						void *cbarg,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_unsubscribe(		// unsubscribe
						gdp_gcl_t *gcl,			// the GCL with the subscription
						gdp_name_t dest);		// the name of the subscriber

EP_STAT			_gdp_gcl_getmetadata(		// retrieve metadata
						gdp_gcl_t *gcl,
						gdp_gclmd_t **gmdp,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_newsegment(		// create a new physical segment
						gdp_gcl_t *gcl,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_fwd_append(		// forward APPEND (replication)
						gdp_gcl_t *gcl,
						gdp_datum_t *datum,
						gdp_name_t to_server,
						gdp_event_cbfunc_t cbfunc,
						void *cbarg,
						gdp_chan_t *chan,
						uint32_t reqflags);

/*
**  GCL Open Information
*/

struct gdp_gcl_open_info
{
	EP_CRYPTO_KEY		*signkey;		// signing key
};


/*
**  A Work Request (and associated Response)
**
**		A GDP request is packaged up in one of these things and
**		submitted.  Responses are returned in the same structure.
**
**		There is one PDU header here that is used both for
**		sending and receiving.  The associated gdp_buf_t does not
**		have a write callback, so it can be used without having
**		any side effects.  The PDU header has much of the
**		information, including the command/response code, the rid,
**		the record number, the timestamp, and the data buffer.
**
**		There can be mulitple requests active on a single GCL at
**		any time, but they should have unique rids.  Rids can be
**		reused if desired once an operation is complete.  Note:
**		some operations (e.g., subscriptions) can return multiple
**		results, but they will have the same rid.
**
**		Requests are potentially linked on lists.  Every request
**		that is active on a channel is linked to that channel
**		(with the GDP_REQ_ON_CHAN_LIST flag set); this is so that
**		requests can be cleaned up if the channel goes away.  At
**		this point we try to recover the channel, so this should
**		be rare, but that list is also used to find requests that
**		need to be timed out.
**
**		For active requests --- that is, requests that are either
**		waiting for a response (in _gdp_invoke) or represent
**		potential points for subscriptions --- are also linked to
**		the corresponding GCL, and will have the GDP_REQ_ON_GCL_LIST
**		flag set.  Subscription listeners also have the
**		GDP_REQ_CLT_SUBSCR flag set.  GDP_REQ_SRV_SUBSCR is used
**		by gdplogd to find the other end of the subscription, i.e,
**		subscription data producers.
**
**		In both the case of applications and gdplogd, requests may
**		get passed between threads.  To prevent someone from finding
**		a request on one of these lists and using it at the same time
**		someone else has it in use, you would like to lock the data
**		structure while it is active.  But you can't pass a mutex
**		between threads.  This is a particular problem if subscription
**		or multiread data comes in faster than it can be processed;
**		since the I/O thread is separate from the processing thread
**		things can clobber each other.
**
**		We solve this by assigning a state to each request:
**
**		FREE means that this request is on the free list.  It
**			should never appear in any other context.
**		ACTIVE means that there is currently an operation taking
**			place on the request, and no one other than the owner
**			should use it.  If you need it, you can wait on the
**			condition variable.
**		WAITING means that the request has been sent from a client
**			to a server but hasn't gotten the response yet.  It
**			shouldn't be possible for a WAITING request to also
**			have an active subscription, but it will be in the GCL
**			list.
**		IDLE means that the request is not free, but there is no
**			operation in process on it.  This will generally be
**			because it is a subscription that does not have any
**			currently active data.
**
**		If you want to deliver data to a subscription, you have to
**		first make sure the req is in IDLE state, turn it to ACTIVE
**		state, and then process it.  If it is not in IDLE state you
**		sleep on the condition variable and try again.
**
**		Passing a request to another thread is basically the same.
**		The invariant is that any req being passed between threads
**		should always be ACTIVE.
**
**		Implemented in gdp_req.c.
*/

struct gdp_req
{
	EP_THR_MUTEX		mutex;		// lock on this data structure
	EP_THR_COND			cond;		// pthread wakeup condition variable
	LIST_ENTRY(gdp_req)	gcllist;	// linked list for cache management
	LIST_ENTRY(gdp_req)	chanlist;	// reqs associated with a given channel
	gdp_gcl_t			*gcl;		// the corresponding GCL handle
	gdp_pdu_t			*pdu;		// PDU buffer
	gdp_pdu_t			*rpdu;		// PDU for ack/nak responses
	gdp_chan_t			*chan;		// the network channel for this req
	EP_STAT				stat;		// status code from last operation
	gdp_recno_t			nextrec;	// next record to return (subscriptions)
	int32_t				numrecs;	// remaining number of records to return
	uint16_t			state;		// see below
	uint32_t			flags;		// see below
	EP_TIME_SPEC		act_ts;		// timestamp of last successful activity
	void				(*postproc)(struct gdp_req *);
									// do post processing after ack sent
	gdp_event_cbfunc_t	sub_cb;		// callback function (subscribe & async I/O)
	void				*udata;		// user-supplied opaque data to cb
	EP_CRYPTO_MD		*md;		// message digest context
};

// states
#define GDP_REQ_FREE			0			// request is free
#define GDP_REQ_ACTIVE			1			// currently being processed
#define GDP_REQ_WAITING			2			// waiting on cond variable
#define GDP_REQ_IDLE			3			// subscription waiting for data

// flags
#define GDP_REQ_ASYNCIO			0x00000001	// async I/O operation
#define GDP_REQ_DONE			0x00000002	// operation complete
#define GDP_REQ_CLT_SUBSCR		0x00000004	// client-side subscription
#define GDP_REQ_SRV_SUBSCR		0x00000008	// server-side subscription
#define GDP_REQ_PERSIST			0x00000010	// request persists after response
#define GDP_REQ_SUBUPGRADE		0x00000020	// can upgrade to subscription
#define GDP_REQ_ALLOC_RID		0x00000040	// force allocation of new rid
#define GDP_REQ_ON_GCL_LIST		0x00000080	// this is on a GCL list
#define GDP_REQ_ON_CHAN_LIST	0x00000100	// this is on a channel list
#define GDP_REQ_CORE			0x00000200	// internal to the core code
#define GDP_REQ_ROUTEFAIL		0x00000400	// fail immediately on route failure

EP_STAT			_gdp_req_new(				// create new request
						int cmd,
						gdp_gcl_t *gcl,
						gdp_chan_t *chan,
						gdp_pdu_t *pdu,
						uint32_t flags,
						gdp_req_t **reqp);

void			_gdp_req_free(				// free old request
						gdp_req_t **reqp);

EP_STAT			_gdp_req_lock(				// lock a request mutex
						gdp_req_t *);

void			_gdp_req_unlock(			// unlock a request mutex
						gdp_req_t *);

gdp_req_t		*_gdp_req_find(				// find a request in a GCL
						gdp_gcl_t *gcl, gdp_rid_t rid);

gdp_rid_t		_gdp_rid_new(				// create new request id
						gdp_gcl_t *gcl, gdp_chan_t *chan);

EP_STAT			_gdp_req_send(				// send request to daemon (async)
						gdp_req_t *req);

EP_STAT			_gdp_req_unsend(			// pull failed request off GCL list 
						gdp_req_t *req);

EP_STAT			_gdp_req_dispatch(			// do local req processing
						gdp_req_t *req);

EP_STAT			_gdp_invoke(				// send request to daemon (sync)
						gdp_req_t *req);

void			_gdp_req_freeall(			// free all requests in list
						struct req_head *reqlist,
						void (*shutdownfunc)(gdp_req_t *));

void			_gdp_req_dump(				// print (debug) request
						gdp_req_t *req,
						FILE *fp,
						int detail,
						int indent);

void			_gdp_chan_drain_input(		// drain all input from channel
						gdp_chan_t *chan);


/*
**  Structure used for registering command functions
**
**		The names are already known to the GDP library, so this is just
**		to bind functions that implement the individual commands.
*/

typedef EP_STAT	cmdfunc_t(			// per command dispatch entry
					gdp_req_t *req);	// the request to be processed

struct cmdfuncs
{
	int			cmd;				// command number
	cmdfunc_t	*func;				// pointer to implementing function
};

void			_gdp_register_cmdfuncs(
						struct cmdfuncs *);

const char		*_gdp_proto_cmd_name(		// return printable cmd name
						uint8_t cmd);

/*
**  Initialization.
*/

EP_STAT			_gdp_start_event_loop_thread(
						EP_THR *thr,
						struct event_base *evb,
						const char *where);

void			_gdp_newname(gdp_name_t gname,
						gdp_gclmd_t *gmd);


/*
**  Advertising.
*/

EP_STAT			_gdp_advertise(				// advertise resources (generic)
						EP_STAT (*func)(gdp_buf_t *, void *, int),
						void *ctx,
						int cmd);

EP_STAT			_gdp_advertise_me(			// advertise me only
						int cmd);

/*
**  Subscriptions.
*/

void			_gdp_subscr_lost(			// subscription disappeared
						gdp_req_t *req);

void			_gdp_subscr_poke(			// test subscriptions still alive
						gdp_chan_t *chan);

/*
**  Cryptography support
*/

EP_CRYPTO_KEY	*_gdp_crypto_skey_read(		// read a secret key
						const char *basename,
						const char *ext);
void			_gdp_sign_md(				// sign the metadata
						gdp_gcl_t *gcl);

/*
**  Libevent support
*/

// callback info passed into event loops
struct event_loop_info
{
	struct event_base	*evb;		// the event base for this loop
	const char			*where;		// a name to convey in errors
};


EP_STAT			_gdp_evloop_init(void);		// start event loop


/*
**  Convenience macros
*/

#define MICROSECONDS	* INT64_C(1000)
#define MILLISECONDS	* INT64_C(1000000)
#define SECONDS			* INT64_C(1000000000)


#endif // _GDP_PRIV_H_
