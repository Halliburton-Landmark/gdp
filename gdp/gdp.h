/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP.H --- public headers for use of the Swarm Global Data Plane
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

#ifndef _GDP_H_
#define _GDP_H_

#include <gdp/gdp_buf.h>
#include <gdp/gdp_stat.h>

#include <ep/ep.h>
#include <ep/ep_crypto.h>
#include <ep/ep_mem.h>
#include <ep/ep_stat.h>
#include <ep/ep_time.h>

#include <event2/event.h>
#include <event2/bufferevent.h>

#include <inttypes.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <sys/types.h>

/**********************************************************************
**	Opaque structures
*/

// an open handle on a GCL object instance
typedef struct gdp_gin		gdp_gin_t;
#define gdp_gcl_t			gdp_gin_t		// back compat

// GCL metadata
typedef struct gdp_gclmd	gdp_gclmd_t;
typedef uint32_t			gdp_gclmd_id_t;

// hash functions and signatures
typedef struct gdp_buf		gdp_hash_t;		//XXX is this right?
typedef struct gdp_buf		gdp_sig_t;		//XXX is this right?

// additional information when opening logs (e.g., keys, qos, hints)
typedef struct gdp_gcl_open_info	gdp_gcl_open_info_t;

// quality of service information for subscriptions
typedef struct gdp_sub_qos			gdp_sub_qos_t;

/**********************************************************************
**	Other data types
*/

// the internal name of a GCL (256 bits)
typedef uint8_t				gdp_name_t[32];

#define GDP_NAME_SAME(a, b)	(memcmp((a), (b), sizeof (gdp_name_t)) == 0)

// the printable name of a GCL
#define GDP_GCL_PNAME_LEN	43			// length of an encoded pname
typedef char				gdp_pname_t[GDP_GCL_PNAME_LEN + 1];

// a GCL record number
typedef int64_t				gdp_recno_t;
#define PRIgdp_recno		PRId64

/*
**	I/O modes
**
**		A GCL can only be open for read or write (that is, append)
**		in any given instance.	There are no r/w instances.	 The other
**		modes here are for caching; ANY means to return NULL if
**		no GCLs are in the cache for a given connection.
*/

typedef enum
{
	GDP_MODE_ANY =		0x0003,			// no mode specified (= RA)
	GDP_MODE_RO =		0x0001,			// readable
	GDP_MODE_AO =		0x0002,			// appendable
	GDP_MODE_RA =		0x0003,			// read + append
	GDP_MODE_MASK =		0x0003,			// mask for primary mode bits

	// following are bit masks
	_GDP_MODE_PEEK =	0x0100,			// "peek": for stats, no ref counting
										//    in _gdp_gcl_cache_get
} gdp_iomode_t;


/*
**  GCL Metadata keys
**
**		Although defined as integers, by convention metadata keys are
**		four ASCII characters, kind of like a file extension.  By further
**		convention, those with fewer than four non-zero characters are
**		system defined and may have semantics built in.  Names that are
**		four characters are reserved for user applications.  Names with
**		zero bytes in the middle are probably a bad idea, albeit legal.
*/

#define GDP_GCLMD_XID		0x00584944	// XID (external id)
#define GDP_GCLMD_PUBKEY	0x00505542	// PUB (public key)
#define GDP_GCLMD_CTIME		0x0043544D	// CTM (creation time)
#define GDP_GCLMD_EXPIRE	0x0058544D	// XTM (expiration date/time)
#define GDP_GCLMD_CID		0x00434944	// CID (creator id)
#define GDP_GCLMD_SYNTAX	0x0053594E	// SYN (data syntax: json, xml, etc.)
#define GDP_GCLMD_LOCATION	0x004C4F43	// LOC (location: lat/long)
#define GDP_GCLMD_UUID		0x00554944	// UID (unique ID)


/*
**	 Datums
**		These are the underlying data unit that is passed through a GCL.
*/

typedef struct gdp_datum	gdp_datum_t;


/*
**	Events
**		gdp_event_t encodes an event.  Every event has a type and may
**		optionally have a GCL handle and/or a message.  For example,
**		data (from a subscription) has all three.
*/

typedef struct gdp_event	gdp_event_t;

// event types
#define _GDP_EVENT_FREE		0	// internal use: event is free
#define GDP_EVENT_DATA		1	// 205 returned data
#define GDP_EVENT_EOS		2	// 205 normal end of subscription
#define GDP_EVENT_SHUTDOWN	3	// 515 subscription terminating because of shutdown
#define GDP_EVENT_CREATED	4	// 201 successful append, create, or similar
#define GDP_EVENT_SUCCESS	5	// 200 generic asynchronous success status
#define GDP_EVENT_FAILURE	6	//     generic asynchronous failure status
#define GDP_EVENT_MISSING	7	// 430 record is missing

extern gdp_event_t		*gdp_event_next(		// get event (caller must free!)
							gdp_gcl_t *gcl,			// if set wait for this GCL only
							EP_TIME_SPEC *timeout);

extern EP_STAT			gdp_event_free(			// free event from gdp_event_next
							gdp_event_t *gev);		// event to free

extern void				gdp_event_print(		// print event (for debugging)
							const gdp_event_t *gev,	// event in question
							FILE *fp,				// output file
							int detail,				// how detailed?
							int indent);			// indentation level

extern int				gdp_event_gettype(		// get the type of the event
							gdp_event_t *gev);

extern EP_STAT			gdp_event_getstat(		// get status code
							gdp_event_t *gev);

extern gdp_gcl_t		*gdp_event_getgcl(		// get the GCL of the event
							gdp_event_t *gev);

extern gdp_datum_t		*gdp_event_getdatum(	// get the datum of the event
							gdp_event_t *gev);

extern void				*gdp_event_getudata(	// get user data (callback only)
							gdp_event_t *gev);

typedef void			(*gdp_event_cbfunc_t)(	// the callback function
							gdp_event_t *ev);		// the event triggering the call

/**********************************************************************
**	Public globals and functions
*/

//extern struct event_base		*GdpIoEventBase;	// the base for GDP I/O events

// initialize the library
extern EP_STAT	gdp_init(
					const char *gdpd_addr);	// address of gdpd

// pre-initialize the library (gdp_init does this -- rarely needed)
EP_STAT			gdp_lib_init(
					const char *my_routing_name);

// run event loop (normally run from gdp_init; never returns)
extern void		*gdp_run_accept_event_loop(
					void *);				// unused

// create a new GCL
extern EP_STAT	gdp_gcl_create(
					gdp_name_t gclname,
					gdp_name_t logdname,
					gdp_gclmd_t *,			// pointer to metadata object
					gdp_gcl_t **pgcl);

// open an existing GCL
extern EP_STAT	gdp_gcl_open(
					gdp_name_t name,		// GCL name to open
					gdp_iomode_t rw,		// read/write (append)
					gdp_gcl_open_info_t *info,	// additional open info
					gdp_gcl_t **gcl);		// pointer to result GCL handle

// close an open GCL
extern EP_STAT	gdp_gcl_close(
					gdp_gcl_t *gcl);		// GCL handle to close

// delete and close an open GCL
extern EP_STAT	gdp_gcl_delete(
					gdp_gcl_t *gcl);		// GCL handle to delete

// append to a writable GCL
extern EP_STAT	gdp_gcl_append(
					gdp_gcl_t *gcl,			// writable GCL handle
					gdp_datum_t *);			// message to write

// async version
extern EP_STAT gdp_gcl_append_async(
					gdp_gcl_t *gcl,			// writable GCL handle
					gdp_datum_t *,			// message to write
					gdp_event_cbfunc_t,		// callback function
					void *udata);

// synchronous read based on record number
extern EP_STAT gdp_gcl_read_by_recno(
					gdp_gcl_t *gcl,			// readable GCL handle
					gdp_recno_t recno,		// record number
					gdp_datum_t *datum);	// pointer to result

// async read based on record number
extern EP_STAT gdp_gcl_read_by_recno_async(
					gdp_gcl_t *gcl,			// readable GCL handle
					gdp_recno_t recno,		// starting record number
					int32_t nrecs,			// number of records to read
					gdp_event_cbfunc_t cbfunc,	// callback function
					void *cbarg);			// argument to cbfunc

// synchronous read based on timestamp
extern EP_STAT gdp_gcl_read_by_ts(
					gdp_gcl_t *gcl,			// readable GCL handle
					EP_TIME_SPEC *ts,		// timestamp
					gdp_datum_t *datum);	// pointer to result

// async read based on timestamp
extern EP_STAT gdp_gcl_read_by_ts_async(
					gdp_gcl_t *gcl,			// readable GCL handle
					EP_TIME_SPEC *ts,		// starting record number
					int32_t nrecs,			// number of records to read
					gdp_event_cbfunc_t cbfunc,	// callback function
					void *cbarg);			// argument to cbfunc

// synchronous read based on hash
extern EP_STAT gdp_gcl_read_by_hash(
					gdp_gcl_t *gcl,			// readable GCL handle
					gdp_hash_t *hash,		// hash of desired record
					gdp_datum_t *datum);	// pointer to result

// async read based on timestamp
extern EP_STAT gdp_gcl_read_by_hash_async(
					gdp_gcl_t *gcl,			// readable GCL handle
					gdp_hash_t *hash,		// starting record hash
					int32_t nrecs,			// number of records to read
					gdp_event_cbfunc_t cbfunc,	// callback function
					void *cbarg);			// argument to cbfunc

// subscribe based on record number
extern EP_STAT	gdp_gcl_subscribe_by_recno(
					gdp_gcl_t *gcl,			// readable GCL handle
					gdp_recno_t start,		// starting record number
					int32_t nrecs,			// number of records to retrieve
					gdp_sub_qos_t *qos,		// quality of service info
					gdp_event_cbfunc_t cbfunc,
											// callback function for next datum
					void *cbarg);			// argument passed to callback

// subscribe based on timestamp
extern EP_STAT	gdp_gcl_subscribe_by_ts(
					gdp_gcl_t *gcl,			// readable GCL handle
					EP_TIME_SPEC *ts,		// starting timestamp
					int32_t nrecs,			// number of records to retrieve
					gdp_sub_qos_t *qos,		// quality of service info
					gdp_event_cbfunc_t cbfunc,
											// callback function for next datum
					void *cbarg);			// argument passed to callback

// subscribe based on hash
extern EP_STAT	gdp_gcl_subscribe_by_hash(
					gdp_gcl_t *gcl,			// readable GCL handle
					gdp_hash_t *hash,		// starting record hash
					int32_t nrecs,			// number of records to retrieve
					EP_TIME_SPEC *timeout,	// timeout
					gdp_event_cbfunc_t cbfunc,
											// callback function for next datum
					void *cbarg);			// argument passed to callback


// unsubscribe from a GCL
extern EP_STAT	gdp_gcl_unsubscribe(
					gdp_gcl_t *gcl,			// GCL handle
					gdp_event_cbfunc_t cbfunc,
											// callback func (to make unique)
					void *cbarg);			// callback arg (to make unique)
// read metadata
extern EP_STAT	gdp_gcl_getmetadata(
					gdp_gcl_t *gcl,			// GCL handle
					gdp_gclmd_t **gmdp);	// out-param for metadata

// create new log segment
extern EP_STAT	gdp_gcl_newsegment(
					gdp_gcl_t *gcl);		// GCL handle

// set append filter
extern EP_STAT	gdp_gcl_set_append_filter(
					gdp_gcl_t *gcl,			// GCL handle
					EP_STAT (*readfilter)(gdp_datum_t *, void *),
					void *filterdata);

// set read filter
extern EP_STAT	gdp_gcl_set_read_filter(
					gdp_gcl_t *gcl,			// GCL handle
					EP_STAT (*readfilter)(gdp_datum_t *, void *),
					void *filterdata);

// return the name of a GCL
//		XXX: should this be in a more generic "getstat" function?
extern const gdp_name_t *gdp_gcl_getname(
					const gdp_gcl_t *gcl);	// open GCL handle

// check to see if a GDP object name is valid
extern bool		gdp_name_is_valid(
					const gdp_name_t);

// print a GCL (for debugging)
extern void		gdp_gcl_print(
					const gdp_gcl_t *gcl,	// GCL handle to print
					FILE *fp);

// make a printable GDP object name from a binary version
char			*gdp_printable_name(
					const gdp_name_t internal,
					gdp_pname_t external);

// print an internal name for human use
void			gdp_print_name(
					const gdp_name_t internal,
					FILE *fp);

// make a binary GDP object name from a printable version
EP_STAT			gdp_internal_name(
					const gdp_pname_t external,
					gdp_name_t internal);

// parse a (possibly human-friendly) GDP object name
EP_STAT			gdp_parse_name(
					const char *ext,
					gdp_name_t internal);

// get the number of records in the log
extern gdp_recno_t	gdp_gcl_getnrecs(
					const gdp_gcl_t *gcl);	// open GCL handle

/*
**  Following are for back compatibility
*/

typedef gdp_event_cbfunc_t	gdp_gcl_sub_cbfunc_t;	// back compat

// read from a readable GCL based on record number
#define gdp_gcl_read		gdp_gcl_read_by_recno

// read from a readable GCL based on timestamp
#define gdp_gcl_read_ts			gdp_gcl_read_by_ts

// read asynchronously from a GCL based on record number
extern EP_STAT gdp_gcl_read_async(
					gdp_gcl_t *gcl,			// readable GCL handle
					gdp_recno_t recno,		// starting record number
					gdp_event_cbfunc_t cbfunc,	// callback function
					void *cbarg);			// argument to cbfunc

// subscribe to a readable GCL
#define gdp_gcl_subscribe		gdp_gcl_subscribe_by_recno

// subscribe by timestamp
#define gdp_gcl_subscribe_ts	gdp_gcl_subscribe_by_ts

// read multiple records (no subscriptions)
#if 0	//XXX OBSOLETE (???)
extern EP_STAT	gdp_gcl_multiread(
					gdp_gcl_t *gcl,			// readable GCL handle
					gdp_recno_t start,		// first record to retrieve
					int32_t nrecs,			// number of records to retrieve
					gdp_event_cbfunc_t cbfunc,
											// callback function for next datum
					void *cbarg);			// argument passed to callback
#endif //XXX
#define gdp_gcl_multiread		gdp_gcl_read_by_recno_async	// BACK COMPAT

// read multiple records starting from timestamp (no subscriptions)
#define gdp_gcl_multiread_ts	gdp_gcl_read_by_ts_async



/*
**  GCL Open Information
*/

// get a new open information structure
gdp_gcl_open_info_t	*gdp_gcl_open_info_new(void);

// free that structure
void				gdp_gcl_open_info_free(
						gdp_gcl_open_info_t *info);

// set the signing key
EP_STAT				gdp_gcl_open_info_set_signing_key(
						gdp_gcl_open_info_t *info,
						EP_CRYPTO_KEY *skey);

// set the signing callback function
EP_STAT				gdp_gcl_open_info_set_signkey_cb(
						gdp_gcl_open_info_t *info,
						EP_STAT (*signkey_cb)(
							gdp_name_t gname,
							void *signkey_udata,
							EP_CRYPTO_KEY **skey),
						void *signkey_udata);

// set the caching behavior
EP_STAT				gdp_gcl_open_info_set_caching(
						gdp_gcl_open_info_t *info,
						bool keep_in_cache);

/*
**  Metadata handling
*/

// create a new metadata set
gdp_gclmd_t		*gdp_gclmd_new(
					int entries);

// free a metadata set
void			gdp_gclmd_free(gdp_gclmd_t *gmd);

// add an entry to a metadata set
EP_STAT			gdp_gclmd_add(
					gdp_gclmd_t *gmd,
					gdp_gclmd_id_t id,
					size_t len,
					const void *data);

// get an entry from a metadata set by index
EP_STAT			gdp_gclmd_get(
					gdp_gclmd_t *gmd,
					int indx,
					gdp_gclmd_id_t *id,
					size_t *len,
					const void **data);

// get an entry from a metadata set by id
EP_STAT			gdp_gclmd_find(
					gdp_gclmd_t *gmd,
					gdp_gclmd_id_t id,
					size_t *len,
					const void **data);

// print metadata set (for debugging)
void			gdp_gclmd_print(
					const gdp_gclmd_t *gmd,
					FILE *fp,
					int detail,
					int indent);

/*
**  Datum handling
*/

// allocate a new message datum
gdp_datum_t		*gdp_datum_new(const gdp_gin_t *gin);

// free a message datum
void			gdp_datum_free(gdp_datum_t *);

// reset a datum to clean state
void			gdp_datum_reset(gdp_datum_t *);

// copy contents of one datum into another
extern void		gdp_datum_copy(
					gdp_datum_t *to,
					const gdp_datum_t *from);

// compute hash of a datum
gdp_hash_t		*gdp_datum_hash(
						gdp_datum_t *datum);

// print out data record
extern void		gdp_datum_print(
					const gdp_datum_t *datum,	// message to print
					FILE *fp,					// file to print it to
					uint32_t flags);			// formatting options

#define GDP_DATUM_PRTEXT		0x00000001		// print data as text
#define GDP_DATUM_PRDEBUG		0x00000002		// print debugging info
#define GDP_DATUM_PRSIG			0x00000004		// print the signature
#define GDP_DATUM_PRQUIET		0x00000008		// don't print any metadata
#define GDP_DATUM_PRMETAONLY	0x00000010		// only print metadata

// get the record number from a datum
extern gdp_recno_t	gdp_datum_getrecno(
					const gdp_datum_t *datum);

// get the timestamp from a datum
extern void		gdp_datum_getts(
					const gdp_datum_t *datum,
					EP_TIME_SPEC *ts);

// get the data length from a datum
extern size_t	gdp_datum_getdlen(
					const gdp_datum_t *datum);

// get the data buffer from a datum
extern gdp_buf_t *gdp_datum_getbuf(
					const gdp_datum_t *datum);

// get the signature from a datum
extern gdp_sig_t *gdp_datum_getsig(
					const gdp_datum_t *datum);

// get the message digest algorithm from a datum
extern short	gdp_datum_getmdalg(
					const gdp_datum_t *datum);


/*
**  Hashes
*/

// create a new empty hash structure
extern gdp_hash_t	*gdp_hash_new(
						int alg);

// free a hash structure
extern void			gdp_hash_free(
						gdp_hash_t *hash);

// reset a hash structure
extern void			gdp_hash_reset(
						gdp_hash_t *hash);

extern void			gdp_hash_set(
						gdp_hash_t *hash,
						void *hashbytes,
						size_t hashlen);

// get the length of a hash
extern size_t		gdp_hash_getlength(
						gdp_hash_t *hash);

// get the actual hash value (and length)
extern void			*gdp_hash_getptr(
						gdp_hash_t *hash,
						size_t *hashlen_ptr);

// compare two hash structures
extern bool			gdp_hash_equal(
						gdp_hash_t *a,
						gdp_hash_t *b);


/*
**  Signatures
*/

// create a new empty signature structure
extern gdp_sig_t	*gdp_sig_new(
						int alg);

// free a signature structure
extern void			gdp_sig_free(
						gdp_sig_t *sig);

// reset a signature structure
extern void			gdp_sig_reset(
						gdp_sig_t *sig);

// set a signature
extern void			gdp_sig_set(
						gdp_sig_t *sig,
						void *sigbuf,
						size_t siglen);

// copy on signature into another
extern void			gdp_sig_copy(
						gdp_sig_t *from,
						gdp_sig_t *to);

// duplicate a signature into a new structure
extern gdp_sig_t	*gdp_sig_dup(
						gdp_sig_t *sig);

// get the length of a signature (not the number of key bits!)
extern size_t		gdp_sig_getlength(
						gdp_sig_t *sig);

// get the actual signature (and length)
extern void			*gdp_sig_getptr(
						gdp_sig_t *sig,
						size_t *siglen_ptr);




#endif // _GDP_H_
