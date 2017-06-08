/* vim: set ai sw=4 sts=4 ts=4 : */

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

/*
**	These headers are not intended for external use.
*/

#ifndef _GDP_PRIV_H_
#define _GDP_PRIV_H_

#include <ep/ep.h>
#include <ep/ep_assert.h>
#include <ep/ep_crypto.h>
#include <ep/ep_thr.h>

#include <event2/buffer.h>

#if EP_OSCF_USE_VALGRIND
# include <valgrind/helgrind.h>
#else
# define VALGRIND_HG_CLEAN_MEMORY(a, b)
#endif

typedef struct gdp_chan		gdp_chan_t;
typedef struct gdp_cursor	gdp_cursor_t;
typedef struct gdp_req		gdp_req_t;
typedef struct gdp_gob		gdp_gob_t;
typedef struct gdp_gin		gdp_gin_t;
STAILQ_HEAD(gev_list, gdp_event);

extern EP_THR		_GdpIoEventLoopThread;
extern event_base_t	*_GdpIoEventBase;	// for all I/O events
extern gdp_chan_t	*_GdpChannel;		// our primary app-level protocol port
extern gdp_name_t	_GdpMyRoutingName;	// source name for PDUs
extern bool			_GdpLibInitialized;	// are we initialized?

#define GDP_CHECK_INITIALIZED											\
					(_GdpLibInitialized ? EP_STAT_OK					\
										: gdp_init(NULL))

#ifndef GDP_OPT_EXTENDED_CACHE_CHECK		//XXX DEBUG TEMPORARY
# define GDP_OPT_EXTENDED_CACHE_CHECK	1	//XXX DEBUG TEMPORARY
#endif										//XXX DEBUG TEMPORARY
#if GDP_OPT_EXTENDED_CACHE_CHECK
#define IF_LIST_CHECK_OK(list, item, chain, type)						\
	type *_x_ ## item;													\
	if (ep_dbg_test(Dbg, 10))											\
	{																	\
		LIST_FOREACH(_x_ ## item, list, chain)							\
		{																\
			EP_ASSERT_ELSE(_x_ ## item != item, break);					\
		}																\
	}																	\
	else																\
		_x_ ## item = NULL;												\
	if (_x_ ## item == NULL)
#else
#define IF_LIST_CHECK_OK(list, item, chain, type)						\
	if (true)
#endif

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

#define GDP_DATUM_ISGOOD(datum)											\
				((datum) != NULL &&										\
				 (datum)->dbuf != NULL &&								\
				 (datum)->inuse)

void			_gdp_datum_dump(		// dump data record (for debugging)
						const gdp_datum_t *datum,	// message to print
						FILE *fp);					// file to print it to

gdp_datum_t		*gdp_datum_dup(		// duplicate a datum
						const gdp_datum_t *datum);



/*
**  GDP Objects
**
**		There are two data structures around GCLs.
**
**		The gdp_gin is what a gdp_gcl_t represents.  It is
**		one-to-one with application open instances of a GCL.
**		Client-side subscriptions are associated with this so
**		that gdp_event_next can deliver the correct information.
**		Read and append filters are also considered "external".
**		It is not used at all by gdplogd.  It is called a
**		gdp_gcl_t in the public API for historic reasons.
**
**		The gdp_gob is the internal representation.  This is what
**		is in the cache.  It has the request list representing
**		server-side subscriptions.  It is used both by the
**		GDP library and by gdplogd.
*/

SLIST_HEAD(gcl_head, gdp_gcl);


// application per-open-instance information (unused in gdplogd)
struct gdp_gin
{
	EP_THR_MUTEX		mutex;			// lock on this data structure
	gdp_gob_t			*gob;			// internal GDP object
	SLIST_ENTRY(gdp_gin)
						next;			// chain for freelist
	uint16_t			flags;			// see below
	gdp_iomode_t		iomode;			// read only or append only
	void				(*closefunc)(gdp_gcl_t *);
										// called when this is closed
	EP_STAT				(*apndfilter)(	// append filter function
							gdp_datum_t *,
							void *);
	void				*apndfpriv;		// private data for apndfilter
	EP_STAT				(*readfilter)(	// read filter function
							gdp_datum_t *,
							void *);
	void				*readfpriv;		// private data for readfilter
};

// internal GDP object, shared between open instances, used in gdplogd
struct gdp_gob
{
	EP_THR_MUTEX		mutex;			// lock on this data structure
	time_t				utime;			// last time used (seconds only)
	LIST_ENTRY(gdp_gob)	ulist;			// list sorted by use time
	struct req_head		reqs;			// list of outstanding requests
	gdp_name_t			name;			// the internal name
	gdp_pname_t			pname;			// printable name (for debugging)
	uint16_t			flags;			// flag bits, see below
	int					refcnt;			// reference counter
	void				(*freefunc)(gdp_gob_t *);
										// called when this is freed
	gdp_recno_t			nrecs;			// # of records (actually last recno)
	gdp_gclmd_t			*gclmd;			// metadata
	EP_CRYPTO_MD		*digest;		// base crypto digest
	struct gdp_gob_xtra	*x;				// for use by gdplogd, gdp-rest
};

// flags for GDP objects (* means shared with gdp_gob_inst)
#define GCLF_DROPPING		0x0001		// handle is being deallocated
#define GCLF_INCACHE		0x0002		// handle is in cache
#define GCLF_ISLOCKED		0x0004		// gcl is locked *
#define GCLF_INUSE			0x0008		// handle is allocated *
#define GCLF_DEFER_FREE		0x0010		// defer actual free until reclaim
#define GCLF_KEEPLOCKED		0x0020		// don't unlock in _gdp_gcl_decref
#define GCLF_PENDING		0x0040		// not yet fully open

#define GDP_GOB_ISGOOD(gob)												\
				((gob) != NULL &&										\
				 EP_UT_BITSET(GCLF_INUSE, (gob)->flags))
#define GDP_GIN_ISGOOD(gin)												\
				((gin) != NULL &&										\
				 EP_UT_BITSET(GCLF_INUSE, (gin)->flags) &&				\
				 GDP_GOB_ISGOOD((gin)->gob))
#define GDP_GOB_ASSERT_ISLOCKED(gob)									\
			(															\
				EP_ASSERT(GDP_GOB_ISGOOD(gob)) &&						\
				EP_ASSERT(EP_UT_BITSET(GCLF_ISLOCKED, (gob)->flags)) &&	\
				EP_THR_MUTEX_ASSERT_ISLOCKED(&(gob)->mutex)				\
			)
#define GDP_GIN_ASSERT_ISLOCKED(gin)									\
			(															\
				EP_ASSERT(GDP_GIN_ISGOOD(gin)) &&						\
				EP_ASSERT(EP_UT_BITSET(GCLF_ISLOCKED, (gin)->flags)) &&	\
				EP_THR_MUTEX_ASSERT_ISLOCKED(&(gin)->mutex) &&			\
				GDP_GOB_ASSERT_ISLOCKED((gin)->gob)						\
			)
#define GDP_GIN_CHECK_RETURN_STAT(gin)									\
			do															\
			{															\
				if (!EP_ASSERT((gin) != NULL))							\
						return GDP_STAT_NULL_GCL;						\
				if (!EP_ASSERT(EP_UT_BITSET(GCLF_INUSE, (gin)->flags)))	\
						return GDP_STAT_GCL_NOT_OPEN;					\
			} while (false)
#define GDP_GIN_CHECK_RETURN_NULL(gin)									\
			do															\
			{															\
				if (!EP_ASSERT((gin) != NULL))							\
						return NULL;									\
				if (!EP_ASSERT(EP_UT_BITSET(GCLF_INUSE, (gin)->flags)))	\
						return NULL;									\
			} while (false)



/*
**  GOB cache.
**
**		Implemented in gdp/gdp_gob_cache.c.
*/

EP_STAT			_gdp_gob_cache_init(void);	// initialize cache

void			_gdp_gob_cache_dump(		// print cache (for debugging)
						int plev,
						FILE *fp);

typedef EP_STAT	gcl_open_func(
						gdp_gcl_t *gcl,
						void *open_info);

EP_STAT			_gdp_gob_cache_get(		// get entry from cache
						gdp_name_t gcl_name,
						uint32_t flags,
						gdp_gob_t **pgob);

#define GGCF_NOCREATE		0			// dummy
#define GGCF_CREATE			0x00000001	// create cache entry if non existent
#define GGCF_GET_PENDING	0x00000002	// return "pending" entries
#define GGCF_PEEK			0x00000004	// don't update cache usage time

void			_gdp_gob_cache_add(			// add entry to cache
						gdp_gob_t *gob);

void			_gdp_gob_cache_changename(	// update the name of a GCL
						gdp_gob_t *gob,
						gdp_name_t newname);

void			_gdp_gob_cache_drop(		// drop entry from cache
						gdp_gob_t *gob,			// GCL to drop
						bool cleanup);			// set if doing cache cleanup

void			_gdp_gob_cache_reclaim(		// flush old entries
						time_t maxage);

void			_gdp_gob_cache_shutdown(	// immediately shut down cache
						void (*shutdownfunc)(gdp_req_t *));

void			_gdp_gob_touch(				// move to front of LRU list
						gdp_gob_t *gob);

void			_gdp_gob_cache_foreach(		// run over all cached GCLs
						void (*f)(gdp_gob_t *));

#define _gdp_gob_lock(g)		_gdp_gob_lock_trace(g, __FILE__, __LINE__, #g)
#define _gdp_gob_unlock(g)		_gdp_gob_unlock_trace(g, __FILE__, __LINE__, #g)
#define _gdp_gob_decref(g, k)	_gdp_gob_decref_trace(g, k, __FILE__, __LINE__, #g)

void			_gdp_gob_lock_trace(		// lock the GCL mutex
						gdp_gob_t *gob,
						const char *file,
						int line,
						const char *id);

void			_gdp_gob_unlock_trace(		// unlock the GCL mutex
						gdp_gob_t *gob,
						const char *file,
						int line,
						const char *id);

void			_gdp_gob_incref(			// increase reference count
						gdp_gob_t *gob);

void			_gdp_gob_decref_trace(		// decrease reference count (trace)
						gdp_gob_t **gobp,
						bool keeplocked,
						const char *file,
						int line,
						const char *id);

#define _gdp_gin_lock(g)		_gdp_gin_lock_trace(g, __FILE__, __LINE__, #g)
#define _gdp_gin_unlock(g)		_gdp_gin_unlock_trace(g, __FILE__, __LINE__, #g)

void			_gdp_gin_lock_trace(		// lock the GCL mutex
						gdp_gin_t *gin,
						const char *file,
						int line,
						const char *id);


void			_gdp_gin_unlock_trace(		// unlock the GCL mutex
						gdp_gin_t *gin,
						const char *file,
						int line,
						const char *id);

void			_gdp_gob_pr_stats(			// print (debug) GOB statistics
						FILE *fp);

/*
**  Other GCL handling.  These are shared between client access
**  and the GDP daemon.
**
**  Implemented in gdp/gdp_gob_ops.c.
*/

EP_STAT			_gdp_gob_new(				// create new in-mem handle
						gdp_name_t name,
						gdp_gob_t **gobhp);

void			_gdp_gob_free(				// free in-memory handle
						gdp_gob_t **gob);		// GOB to free

EP_STAT			_gdp_gob_newname(			// create new name based on metadata
						gdp_gob_t *gob);

void			_gdp_gob_dump(				// dump for debugging
						const gdp_gob_t *gob,	// GOB to print
						FILE *fp,				// where to print it
						int detail,				// how much to print
						int indent);			// unused at this time

EP_STAT			_gdp_gob_create(			// create a new GDP object
						gdp_name_t gobname,
						gdp_name_t logdname,
						gdp_gclmd_t *gmd,
						gdp_chan_t *chan,
						uint32_t reqflags,
						gdp_gob_t **pgob);

EP_STAT			_gdp_gob_open(				// open a GCL
						gdp_gob_t *gob,
						int cmd,
						gdp_gcl_open_info_t *open_info,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gob_close(				// close a GCL (handle)
						gdp_gob_t *gob,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gob_delete(			// delete and close a GCL (handle)
						gdp_gob_t *gob,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gob_read(				// read GCL record based on datum
						gdp_gob_t *gob,
						gdp_datum_t *datum,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gob_read_async(		// read asynchronously
						gdp_gob_t *gob,
						gdp_recno_t recno,
						gdp_event_cbfunc_t cbfunc,
						void *cbarg,
						gdp_chan_t *chan);

EP_STAT			_gdp_gob_append(			// append a record (gdpd shared)
						gdp_gob_t *gob,
						gdp_datum_t *datum,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gob_append_async(		// append asynchronously
						gdp_gob_t *gob,
						gdp_datum_t *datum,
						gdp_event_cbfunc_t cbfunc,
						void *cbarg,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_subscribe(			// subscribe to data
						gdp_req_t *req,
						int32_t numrecs,
						EP_TIME_SPEC *timeout,
						gdp_event_cbfunc_t cbfunc,
						void *cbarg);

EP_STAT			_gdp_gcl_unsubscribe(		// delete subscriptions
						gdp_gin_t *gin,
						gdp_event_cbfunc_t cbfunc,
						void *cbarg,
						uint32_t reqflags);

EP_STAT			_gdp_gob_getmetadata(		// retrieve metadata
						gdp_gob_t *gob,
						gdp_gclmd_t **gmdp,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gob_newsegment(		// create a new physical segment
						gdp_gob_t *gob,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gob_fwd_append(		// forward APPEND (replication)
						gdp_gob_t *gob,
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
	EP_CRYPTO_KEY		*signkey;			// signing key
	EP_STAT				(*signkey_cb)(		// callback to get signing key
							gdp_name_t	name,
							void *signkey_udata,
							EP_CRYPTO_KEY **);
	void				*signkey_udata;
	bool				keep_in_cache:1;	// defer GCL free
};


/*
**  A Work Request (and associated Response)
**
**		A GDP request is packaged up in one of these things and
**		submitted.  Responses are returned in the same structure.
**
**		There are two PDU pointers:
**		* cpdu is the PDU with the command.  Generally this is
**			kept around until the response is read in case you
**			need to retransmit the command PDU.
**		* rpdu is the PDU with the response.
**
**		PDUs have an associated gdp_buf_t to store the actual
**		data.  That buffer does not have a write callback, so
**		it can be used without having any side effects.
**
**		The PDU includes the command/response code, the rid,
**		the record number, the timestamp, the data buffer,
**		and an optional signature buffer.
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
**		In some cases requests may have pending events.  This
**		occurs for commands such as SUBSCRIBE or MULTIREAD when
**		the first data return appears before the ack for the
**		initial command has finished processing.  To avoid confusing
**		applications you have to defer these events until the app
**		knows that the command succeeded.
**
**		Implemented in gdp_req.c.
*/

struct gdp_req
{
	EP_THR_MUTEX		mutex;		// lock on this data structure
	EP_THR_COND			cond;		// pthread wakeup condition variable
	uint16_t			state;		// see below
	LIST_ENTRY(gdp_req)	goblist;	// linked list for cache management
	LIST_ENTRY(gdp_req)	chanlist;	// reqs associated with a given channel
	gdp_gob_t			*gob;		// associated GDP Object handle
	gdp_pdu_t			*cpdu;		// PDU for commands
	gdp_pdu_t			*rpdu;		// PDU for ack/nak responses
	gdp_chan_t			*chan;		// the network channel for this req
	EP_STAT				stat;		// status code from last operation
	gdp_recno_t			nextrec;	// next record to return (subscriptions)
	int32_t				numrecs;	// remaining number of records to return
	uint32_t			flags;		// see below
	EP_TIME_SPEC		act_ts;		// timestamp of last successful activity
	void				(*postproc)(struct gdp_req *);
									// do post processing after ack sent
	gdp_event_cbfunc_t	sub_cbfunc;	// callback function (subscribe & async I/O)
	void				*sub_cbarg;	// user-supplied opaque data to cb
	EP_CRYPTO_MD		*md;		// message digest context

	// these are only of interest in clients, never in gdplogd
	gdp_gin_t			*gin;		// external GCL handle (client only)
	struct gev_list		events;		// pending events (see above)
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
#define GDP_REQ_ON_GOB_LIST		0x00000080	// this is on a GOB list
#define GDP_REQ_ON_CHAN_LIST	0x00000100	// this is on a channel list
#define GDP_REQ_CORE			0x00000200	// internal to the core code
#define GDP_REQ_ROUTEFAIL		0x00000400	// fail immediately on route failure

EP_STAT			_gdp_req_new(				// create new request
						int cmd,
						gdp_gob_t *gob,
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
						gdp_gob_t *gob, gdp_rid_t rid);

gdp_rid_t		_gdp_rid_new(				// create new request id
						gdp_gob_t *gob, gdp_chan_t *chan);

EP_STAT			_gdp_req_send(				// send request to daemon (async)
						gdp_req_t *req);

EP_STAT			_gdp_req_unsend(			// pull failed request off GCL list
						gdp_req_t *req);

EP_STAT			_gdp_req_dispatch(			// do local req processing
						gdp_req_t *req,
						int cmd);

EP_STAT			_gdp_invoke(				// send request to daemon (sync)
						gdp_req_t *req);

void			_gdp_req_freeall(			// free all requests in GCL list
						gdp_gob_t *gob,
						gdp_gin_t *gin,
						void (*shutdownfunc)(gdp_req_t *));

void			_gdp_req_dump(				// print (debug) request
						const gdp_req_t *req,
						FILE *fp,
						int detail,
						int indent);

void			_gdp_req_pr_stats(			// print (debug) statistics
						FILE *fp);


/*
**  Channel and I/O Event support
*/

// extended channel information (passed as channel "udata")
struct gdp_chan_x
{
	struct req_head		reqs;			// reqs associated with this channel
	EP_STAT				(*connect_cb)(	// called on connection established
							gdp_chan_t *chan);
};

// functions used internally related to channel I/O
EP_STAT			_gdp_io_recv(
						gdp_cursor_t *cursor,
						uint32_t flags);

EP_STAT			_gdp_io_event(
						gdp_chan_t *chan,
						uint32_t flags);

EP_STAT			_gdp_advertise_me(
						gdp_chan_t *chan,
						int cmd);

// I/O event handling
struct event_loop_info
{
	const char		*where;
};

void			*_gdp_run_event_loop(
						void *eli_);


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

#define GDP_RECLAIM_AGE_DEF		300L		// default reclaim age (sec)


/*
**  Advertising.
*/

typedef EP_STAT	chan_advert_cb_t(			// advertise any known names
						gdp_chan_t *chan,
						int cmd);

EP_STAT			_gdp_advertise(				// advertise resources (generic)
						gdp_chan_t *chan,
						EP_STAT (*func)(gdp_buf_t *, void *, int),
						void *ctx,
						int cmd);

EP_STAT			_gdp_advertise_me(			// advertise me only
						gdp_chan_t *chan,
						int cmd);

/*
**  Subscriptions.
*/

#define GDP_SUBSCR_REFRESH_DEF	60L			// default refresh interval (sec)
#define GDP_SUBSCR_TIMEOUT_DEF	180L		// default timeout (sec)

void			_gdp_subscr_lost(			// subscription disappeared
						gdp_req_t *req);

void			_gdp_subscr_poke(			// test subscriptions still alive
						gdp_chan_t *chan);

/*
**  Initialization and Maintenance.
*/

void			_gdp_newname(gdp_name_t gname,
						gdp_gclmd_t *gmd);

void			_gdp_reclaim_resources(		// reclaim system resources
						void *);				// unused

void			_gdp_reclaim_resources_init(
						void (*f)(int, short, void *));

void			_gdp_dump_state(int plev);

/*
**  Cryptography support
*/

EP_CRYPTO_KEY	*_gdp_crypto_skey_read(		// read a secret key
						const char *basename,
						const char *ext);
void			_gdp_sign_md(				// sign the metadata
						gdp_gcl_t *gcl);


/*
**  Orders for lock acquisition
**		Lower numbered locks should be acquired before higher numbered locks.
*/

#define GDP_MUTEX_LORDER_GIN		6
#define GDP_MUTEX_LORDER_GOBCACHE	8
#define GDP_MUTEX_LORDER_GOB		10
#define GDP_MUTEX_LORDER_REQ		12
#define GDP_MUTEX_LORDER_CHAN		14
#define GDP_MUTEX_LORDER_DATUM		18
#define GDP_MUTEX_LORDER_LEAF		31	// freelists, etc.


/*
**  Convenience macros
*/

#define MICROSECONDS	* INT64_C(1000)
#define MILLISECONDS	* INT64_C(1000000)
#define SECONDS			* INT64_C(1000000000)


#endif // _GDP_PRIV_H_
