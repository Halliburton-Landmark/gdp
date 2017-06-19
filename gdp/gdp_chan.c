/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	I/O CHANNEL HANDLING
**		This communicates between the client and the routing layer.
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

#include "gdp.h"
#include "gdp_chan.h"
#include "gdp_priv.h"
#include "gdp_zc_client.h"

#include <ep/ep_dbg.h>
#include <ep/ep_log.h>
#include <ep/ep_prflags.h>
#include <ep/ep_string.h>

#include <errno.h>
#include <string.h>
#include <sys/queue.h>

#include <netinet/tcp.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.chan", "GDP channel processing");


#if ___OLD_ROUTER___
#define GDP_CHAN_PROTO_VERSION	3			// protocol version number in PDU
#else
#define GDP_CHAN_PROTO_VERSION	4			// protocol version number in PDU
#endif

static struct event_base	*EventBase;
static EP_STAT				chan_reopen(gdp_chan_t *);

/*
**  Channel internal structure
*/

struct gdp_chan
{
	EP_THR_MUTEX			mutex;			// data structure lock
	EP_THR_COND				cond;			// wake up after state change
	int16_t					state;			// current state of channel
	uint16_t				flags;			// status flags
	struct bufferevent		*bev;			// associated bufferevent (socket)
	char					*router_addr;	// text version of router address
	gdp_chan_x_t			*cdata;			// arbitrary user data

	// callbacks
	gdp_cursor_recv_cb_t	*recv_cb;		// receive callback
	gdp_chan_send_cb_t		*send_cb;		// send callback
	gdp_chan_advert_func_t	*advert_cb;		// advertising function
	gdp_chan_ioevent_cb_t	*ioevent_cb;	// close/error/eof callback
};

/* Channel states */
#define GDP_CHAN_UNCONNECTED	0		// channel is not connected yet
#define GDP_CHAN_CONNECTING		1		// connection being initiated
#define GDP_CHAN_CONNECTED		2		// channel is connected and active
#define GDP_CHAN_ERROR			3		// channel has had error
#define GDP_CHAN_CLOSING		4		// channel is closing


/*
**  Cursor internal structure
*/

struct gdp_cursor
{
	SLIST_ENTRY(gdp_cursor) next;		// linked list pointer
	gdp_chan_t			*chan;			// associated channel
	uint32_t			payload_len;	// total payload length
	gdp_name_t			src;			// source name for this cursor
	gdp_name_t			dst;			// destination name for this cursor
	gdp_buf_t			*ibuf;			// input buffer
	gdp_cursor_x_t		*udata;			// arbitrary user defined data
	EP_STAT				estat;			// status of last operation
};


/*
**  On-the-Wire PDU Format
**
**		Currently with no attempt at compression.
**
**		off	len	meaning
**		---	---	-------
**		0	1	version (must be 4)
**		1	1	time to live in hops
**		2	1	type of service (for now, must be zero)
**		3	1	header length in units of 32 bits (= H / 4)
**		4	4	payload (SDU) length
**		8	32	destination address
**		40	32	source address
**		72	?	for future use (probably options)
**		H	N	payload (SDU) (starts at offset given in octet 3)
**
**		Type of Service is intended for future expansion, and for now
**		clients should always send this as zero.  However, if the
**		high order bit is set it indicates router-to-router traffic
**		with low order bits indicating the router command.
*/

#if ___OLD_ROUTER___
#define MIN_HEADER_LENGTH	(1+1+1+1+32+32+4+1+1+1+1+4)
#else
#define MIN_HEADER_LENGTH	(1 + 1 + 1 + 1 + 4 + 32 + 32)
#endif
#define MAX_HEADER_LENGTH	(255 * 4)

static uint8_t	RoutingLayerAddr[32] =
	{
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
	};


/*
**  _GDP_CHAN_INIT --- initialize channel subsystem
*/

EP_STAT
_gdp_chan_init(
		struct event_base *evbase,
		void *unused)
{
	if (evbase == NULL)
		return EP_STAT_ABORT;
	EventBase = evbase;
	return EP_STAT_OK;
}


/*
**  Create and destroy cursors
*/

// free list (to speed up allocation and avoid memory fragmentation)
static SLIST_HEAD(cursor_list, gdp_cursor)
						CursorFreeList = SLIST_HEAD_INITIALIZER(CursorFreeList);
static EP_THR_MUTEX		CursorFreeListMutex		EP_THR_MUTEX_INITIALIZER;

static gdp_cursor_t *
cursor_new(gdp_buf_t *ibuf)
{
	gdp_cursor_t *cursor;

	ep_thr_mutex_lock(&CursorFreeListMutex);
	cursor = SLIST_FIRST(&CursorFreeList);
	if (cursor != NULL)
		SLIST_REMOVE_HEAD(&CursorFreeList, next);
	ep_thr_mutex_unlock(&CursorFreeListMutex);

	if (cursor == NULL)
	{
		cursor = ep_mem_zalloc(sizeof *cursor);
	}

	// save this buffer: note: reference, not copy
	cursor->ibuf = ibuf;

	return cursor;
}

static void
cursor_free(gdp_cursor_t *cursor)
{
	cursor->ibuf = NULL;					// owned by channel
	cursor->estat = EP_STAT_OK;
	cursor->udata = NULL;
	SLIST_INSERT_HEAD(&CursorFreeList, cursor, next);
}


/*
**  Lock and Unlock a channel
*/

void
_gdp_chan_lock(gdp_chan_t *chan)
{
	ep_thr_mutex_lock(&chan->mutex);
}

void
_gdp_chan_unlock(gdp_chan_t *chan)
{
	ep_thr_mutex_unlock(&chan->mutex);
}


/*
**  Read and decode fixed PDU header
*/

static EP_STAT
read_header(gdp_cursor_t *cursor)
{
	gdp_buf_t *ibuf = GDP_BUF_FROM_EVBUFFER(
								bufferevent_get_input(cursor->chan->bev));
	uint8_t *pbp = gdp_buf_getptr(ibuf, MIN_HEADER_LENGTH);
	int b;
	int hdr_len;
	EP_STAT estat = EP_STAT_OK;

	if (pbp == NULL)
	{
		// fewer than MIN_HEADER_LENGTH bytes in buffer
		estat = GDP_STAT_KEEP_READING;
		goto done;
	}

	GET8(b);				// PDU version number
	if (b != GDP_CHAN_PROTO_VERSION)
	{
		ep_dbg_cprintf(Dbg, 1, "wrong protocol version %d (%d expected)\n",
				b, GDP_CHAN_PROTO_VERSION);
		estat = GDP_STAT_PDU_VERSION_MISMATCH;
		goto done;
	}

	GET8(b);				// time to live
	GET8(b);				// type of service
	GET8(hdr_len);			// header length / 4
	if (hdr_len < 0)
	{
		estat = GDP_STAT_PDU_CORRUPT;
		goto done;
	}
	hdr_len *= 4;

	// if we don't yet have the whole header, wait until we do
	if (gdp_buf_getlength(ibuf) < hdr_len)
		return GDP_STAT_KEEP_READING;

	GET32(cursor->payload_len);
	memcpy(cursor->src, pbp, sizeof cursor->src);
	pbp += sizeof cursor->src;
	memcpy(cursor->dst, pbp, sizeof cursor->dst);
	pbp += sizeof cursor->dst;

	// XXX hack: only return entire PDU
	if (gdp_buf_getlength(ibuf) < hdr_len + cursor->payload_len)
		return GDP_STAT_KEEP_READING;

	// consume entire header
	gdp_buf_drain(ibuf, hdr_len);

done: {
		char ebuf[100];
		ep_dbg_cprintf(Dbg, 32, "read_header: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**	CHAN_READ_CB --- data is available for reading from network socket
**
**		Minimal implementation: read in PDU and hand it to
**		processing routine.  If that processing is going to be
**		lengthy it should use a thread.
**
**		We insist that the entire PDU be in memory before passing
**		the cursor up.  To fix that we would need to associate the
**		cursor with a {src, dst, seqno} tuple, but our naive
**		implementation will never intersperse portions of messages,
**		so this is safe.
*/

static void
chan_read_cb(struct bufferevent *bev, void *ctx)
{
	EP_STAT estat;
	gdp_buf_t *ibuf = GDP_BUF_FROM_EVBUFFER(bufferevent_get_input(bev));
	gdp_chan_t *chan = ctx;

	ep_dbg_cprintf(Dbg, 50, "chan_read_cb: fd %d, %zd bytes\n",
			bufferevent_getfd(bev), gdp_buf_getlength(ibuf));

	EP_ASSERT(bev == chan->bev);

	// we store the header data in a cursor
	gdp_cursor_t *cursor = cursor_new(ibuf);

	while (gdp_buf_getlength(ibuf) > MIN_HEADER_LENGTH)
	{
		// get the transport layer header
		estat = read_header(cursor);

		// pass it to the L5 callback
		// note that if the callback is not set, the PDU is thrown away
		if (EP_STAT_ISOK(estat) && chan->recv_cb != NULL)
			estat = (*chan->recv_cb)(cursor, 0);
		char ebuf[100];
		ep_dbg_cprintf(Dbg, 32, "chan_read_cb: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		EP_STAT_CHECK(estat, break);
	}

	cursor_free(cursor);
}


/*
**	CHAN_EVENT_CB --- events or errors occur on network socket
*/

static EP_PRFLAGS_DESC	EventWhatFlags[] =
{
	{ BEV_EVENT_READING,	BEV_EVENT_READING,		"BEV_EVENT_READING"		},
	{ BEV_EVENT_WRITING,	BEV_EVENT_WRITING,		"BEV_EVENT_WRITING"		},
	{ BEV_EVENT_EOF,		BEV_EVENT_EOF,			"BEV_EVENT_EOF"			},
	{ BEV_EVENT_ERROR,		BEV_EVENT_ERROR,		"BEV_EVENT_ERROR"		},
	{ BEV_EVENT_TIMEOUT,	BEV_EVENT_TIMEOUT,		"BEV_EVENT_TIMEOUT"		},
	{ BEV_EVENT_CONNECTED,	BEV_EVENT_CONNECTED,	"BEV_EVENT_CONNECTED"	},
	{ 0, 0, NULL }
};

static void
chan_event_cb(struct bufferevent *bev, short events, void *ctx)
{
	bool restart_connection = false;
	gdp_chan_t *chan = ctx;
	uint32_t cbflags = 0;

	if (ep_dbg_test(Dbg, 25))
	{
		ep_dbg_printf("chan_event_cb: ");
		ep_prflags(events, EventWhatFlags, ep_dbg_getfile());
		ep_dbg_printf(", fd=%d , errno=%d\n",
				bufferevent_getfd(bev), EVUTIL_SOCKET_ERROR());
	}

	EP_ASSERT(bev == chan->bev);

	if (EP_UT_BITSET(BEV_EVENT_CONNECTED, events))
	{
		// sometimes libevent says we're connected when we're not
		if (EVUTIL_SOCKET_ERROR() == ECONNREFUSED)
		{
			chan->state = GDP_CHAN_ERROR;
			cbflags |= GDP_IOEVENT_ERROR;
		}
		else
		{
			chan->state = GDP_CHAN_CONNECTED;
			cbflags |= GDP_IOEVENT_CONNECTED;
		}
		ep_thr_cond_broadcast(&chan->cond);
	}
	if (EP_UT_BITSET(BEV_EVENT_EOF, events))
	{
		gdp_buf_t *ibuf = GDP_BUF_FROM_EVBUFFER(bufferevent_get_input(bev));
		size_t l = gdp_buf_getlength(ibuf);

		ep_dbg_cprintf(Dbg, 1, "chan_event_cb: got EOF, %zu bytes left\n", l);
		cbflags |= GDP_IOEVENT_EOF;
		restart_connection = true;
	}
	if (EP_UT_BITSET(BEV_EVENT_ERROR, events))
	{
		int sockerr = EVUTIL_SOCKET_ERROR();

		ep_dbg_cprintf(Dbg, 1, "chan_event_cb: error: %s\n",
				evutil_socket_error_to_string(sockerr));
		cbflags |= GDP_IOEVENT_ERROR;
		restart_connection = true;
	}

	if (chan->ioevent_cb != NULL)
		(*chan->ioevent_cb)(chan, cbflags);

	// if we need to restart, let it run
	if (restart_connection)
	{
		EP_STAT estat;

		chan->state = GDP_CHAN_ERROR;
		ep_thr_cond_broadcast(&chan->cond);

		// close the (now dead) socket file descriptor
		close(bufferevent_getfd(chan->bev));
		bufferevent_setfd(chan->bev, -1);

		do
		{
			long delay = ep_adm_getlongparam("swarm.gdp.reconnect.delay", 1000L);
			if (delay > 0)
				ep_time_nanosleep(delay * INT64_C(1000000));
			estat = chan_reopen(chan);
		} while (!EP_STAT_ISOK(estat));
		(*chan->advert_cb)(chan, GDP_CMD_ADVERTISE, ctx);
	}
}


/*
**  Helper for close, error, and eof handlers
*/

static EP_STAT
chan_do_close(gdp_chan_t *chan, int what)
{
	if (chan == NULL)
	{
		ep_dbg_cprintf(Dbg, 7, "chan_do_close: null channel\n");
		return EP_STAT_ERROR;
	}

	chan->state = GDP_CHAN_CLOSING;
	ep_thr_cond_broadcast(&chan->cond);
	if (chan->ioevent_cb != NULL)
		(*chan->ioevent_cb)(chan, what);
	bufferevent_free(chan->bev);
	chan->bev = NULL;
	if (chan->router_addr != NULL)
		ep_mem_free(chan->router_addr);
	ep_thr_cond_destroy(&chan->cond);
	ep_thr_mutex_destroy(&chan->mutex);
	ep_mem_free(chan);
	return EP_STAT_OK;
}


/*
**	_GDP_CHAN_OPEN_HELPER --- open channel to the routing layer
*/

static EP_STAT
chan_open_helper(
		gdp_chan_t *chan)
{
	EP_STAT estat = EP_STAT_OK;

	if (chan->bev == NULL)
	{
		estat = ep_stat_from_errno(errno);
		ep_dbg_cprintf(Dbg, 18, "_gdp_chan_open: no bufferevent\n");
		goto fail0;
	}

	// attach to a socket
	char abuf[500] = "";
	char *port = NULL;		// keep gcc happy
	char *host;

	// get the host:port info into abuf
	if (chan->router_addr != NULL && chan->router_addr[0] != '\0')
	{
		strlcpy(abuf, chan->router_addr, sizeof abuf);
	}
	else
	{
#if GDP_OSCF_USE_ZEROCONF
		if (ep_adm_getboolparam("swarm.gdp.zeroconf.enable", true))
		{
			ep_dbg_cprintf(Dbg, 1, "Trying Zeroconf:\n");

			if (gdp_zc_scan())
			{
				ep_dbg_cprintf(Dbg, 20, "... after gdp_zc_scan\n");
				zcinfo_t **list = gdp_zc_get_infolist();
				ep_dbg_cprintf(Dbg, 20, "... after gdp_zc_get_infolist: %p\n",
						list);
				if (list != NULL)
				{
					char *info = gdp_zc_addr_str(list);
					ep_dbg_cprintf(Dbg, 20, "... after gdp_zc_addr_str: %p\n",
							info);
					gdp_zc_free_infolist(list);
					ep_dbg_cprintf(Dbg, 20, "... after gdp_zc_free_infolist\n");
					if (info != NULL)
					{
						if (info[0] != '\0')
						{
							ep_dbg_cprintf(Dbg, 1, "Zeroconf found %s\n",
									info);
							strlcpy(abuf, info, sizeof abuf);
							strlcat(abuf, ";", sizeof abuf);
						}
						free(info);
					}
				}
			}
			else
				ep_dbg_cprintf(Dbg, 20, "gdp_zc_scan failed\n");
		}
#endif // GDP_OSCF_USE_ZEROCONF
		strlcat(abuf,
				ep_adm_getstrparam("swarm.gdp.routers", "127.0.0.1"),
				sizeof abuf);
	}

	ep_dbg_cprintf(Dbg, 8, "_gdp_chan_open(%s)\n", abuf);

	// strip off addresses and try them
	estat = GDP_STAT_NOTFOUND;				// anything that is not OK
	char *delim = abuf;
	do
	{
		char pbuf[10];

		host = delim;						// beginning of address spec
		delim = strchr(delim, ';');			// end of address spec
		if (delim != NULL)
			*delim++ = '\0';

		host = &host[strspn(host, " \t")];	// strip early spaces
		if (*host == '\0')
			continue;						// empty spec

		ep_dbg_cprintf(Dbg, 1, "Trying %s\n", host);

		port = host;
		if (*host == '[')
		{
			// IPv6 literal
			host++;						// strip [] to satisfy getaddrinfo
			port = strchr(host, ']');
			if (port != NULL)
				*port++ = '\0';
		}

		// see if we have a port number
		if (port != NULL)
		{
			// use strrchr so IPv6 addr:port without [] will work
			port = strrchr(port, ':');
			if (port != NULL)
				*port++ = '\0';
		}
		if (port == NULL || *port == '\0')
		{
			int portno;

			portno = ep_adm_getintparam("swarm.gdp.router.port",
							GDP_PORT_DEFAULT);
			snprintf(pbuf, sizeof pbuf, "%d", portno);
			port = pbuf;
		}

		ep_dbg_cprintf(Dbg, 20, "_gdp_chan_open: trying host %s port %s\n",
				host, port);

		// parsing done....  let's try the lookup
		struct addrinfo *res, *a;
		struct addrinfo hints;
		int r;

		memset(&hints, '\0', sizeof hints);
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		r = getaddrinfo(host, port, &hints, &res);
		if (r != 0)
		{
			// address resolution failed; try the next one
			switch (r)
			{
			case EAI_SYSTEM:
				estat = ep_stat_from_errno(errno);
				if (!EP_STAT_ISOK(estat))
					break;
				// ... fall through

			case EAI_NONAME:
				estat = EP_STAT_DNS_NOTFOUND;
				break;

			default:
				estat = EP_STAT_DNS_FAILURE;
			}
			ep_dbg_cprintf(Dbg, 1,
					"_gdp_chan_open: getaddrinfo(%s, %s) =>\n"
					"    %s\n",
					host, port, gai_strerror(r));
			continue;
		}

		// attempt connects on all available addresses
		_gdp_chan_lock(chan);
		for (a = res; a != NULL; a = a->ai_next)
		{
			// make the actual connection
			// it would be nice to have a private timeout here...
			evutil_socket_t sock = socket(a->ai_family, SOCK_STREAM, 0);
			if (sock < 0)
			{
				// bad news, but keep trying
				estat = ep_stat_from_errno(errno);
				ep_log(estat, "_gdp_chan_open: cannot create socket");
				continue;
			}

			// shall we disable Nagle algorithm?
			if (ep_adm_getboolparam("swarm.gdp.tcp.nodelay", false))
			{
				int enable = 1;
				if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
							(void *) &enable, sizeof enable) != 0)
				{
					estat = ep_stat_from_errno(errno);
					ep_log(estat, "_gdp_chan_open: cannot set TCP_NODELAY");
					// error not fatal, let's just go on
				}
			}
			if (connect(sock, a->ai_addr, a->ai_addrlen) < 0)
			{
				// connection failure
				estat = ep_stat_from_errno(errno);
				ep_dbg_cprintf(Dbg, 38,
						"_gdp_chan_open: connect failed: %s\n",
						strerror(errno));
				close(sock);
				continue;
			}

			// success!  Make it non-blocking and associate with bufferevent
			ep_dbg_cprintf(Dbg, 39, "successful connect\n");
			estat = EP_STAT_OK;
			evutil_make_socket_nonblocking(sock);
			bufferevent_setfd(chan->bev, sock);
			break;
		}

		_gdp_chan_unlock(chan);
		freeaddrinfo(res);

		if (EP_STAT_ISOK(estat))
		{
			// success
			break;
		}
	} while (delim != NULL);

	// error cleanup and return
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[80];
fail0:
		ep_dbg_cprintf(Dbg, 2,
				"_gdp_chan_open: could not open channel: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		//ep_log(estat, "_gdp_chan_open: could not open channel");
	}
	else
	{
		ep_dbg_cprintf(Dbg, 1, "_gdp_chan_open: talking to router at %s:%s\n",
					host, port);
	}
	return estat;
}


/*
**  _GDP_CHAN_OPEN --- open a channel
*/

EP_STAT
_gdp_chan_open(
		const char *router_addr,
		void *qos,
		gdp_cursor_recv_cb_t *recv_cb,
		gdp_chan_send_cb_t *send_cb,
		gdp_chan_ioevent_cb_t *ioevent_cb,
		gdp_chan_advert_func_t *advert_func,
		gdp_chan_x_t *cdata,
		gdp_chan_t **pchan)
{
	EP_STAT estat;
	gdp_chan_t *chan;

	// allocate a new channel structure
	chan = ep_mem_zalloc(sizeof *chan);
	ep_thr_mutex_init(&chan->mutex, EP_THR_MUTEX_DEFAULT);
	ep_thr_mutex_setorder(&chan->mutex, GDP_MUTEX_LORDER_CHAN);
	ep_thr_cond_init(&chan->cond);
	chan->state = GDP_CHAN_CONNECTING;
	chan->recv_cb = recv_cb;
	//chan->send_cb = send_cb;
	chan->advert_cb = advert_func;
	chan->ioevent_cb = ioevent_cb;
	chan->cdata = cdata;
	if (router_addr != NULL)
		chan->router_addr = ep_mem_strdup(router_addr);

	// set up the bufferevent
	chan->bev = bufferevent_socket_new(EventBase,
					-1,
					BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE |
					BEV_OPT_DEFER_CALLBACKS | BEV_OPT_UNLOCK_CALLBACKS);
	bufferevent_setcb(chan->bev, chan_read_cb, NULL, chan_event_cb, chan);
	bufferevent_enable(chan->bev, EV_READ | EV_WRITE);
	*pchan = chan;

	estat = chan_open_helper(chan);

	if (!EP_STAT_ISOK(estat))
	{
		*pchan = NULL;
		chan_do_close(chan, BEV_EVENT_ERROR);
	}
	return estat;
}


/*
**  CHAN_REOPEN --- re-open a channel (e.g., on router failure)
*/

static EP_STAT
chan_reopen(gdp_chan_t *chan)
{
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 12, "chan_reopen: %p\n	 advert_cb = %p\n",
			chan, chan->advert_cb);
	estat = chan_open_helper(chan);
	return estat;
}


/*
**	_GDP_CHAN_CLOSE --- close a channel (user-driven)
*/

EP_STAT
_gdp_chan_close(gdp_chan_t *chan)
{
	return chan_do_close(chan, GDP_IOEVENT_USER_CLOSE);
}


/*
**  _GDP_CHAN_SEND --- send a message to a channel
*/

EP_STAT
_gdp_chan_send(gdp_chan_t *chan,
			gdp_target_t *target,
			gdp_name_t src,
			gdp_name_t dst,
			gdp_buf_t *payload)
{
	char pb[MAX_HEADER_LENGTH];
	char *pbp = pb;
	EP_STAT estat = EP_STAT_OK;
	int i;

	// build the header in memory
	PUT8(GDP_CHAN_PROTO_VERSION);		// version number
	PUT8(15);							// time to live
	PUT8(0);							// type of service
	PUT8(18);							// header length (= 72 / 4)
	PUT32(gdp_buf_getlength(payload));
	memcpy(pbp, dst, sizeof (gdp_name_t));
	pbp += sizeof (gdp_name_t);
	memcpy(pbp, src, sizeof (gdp_name_t));
	pbp += sizeof (gdp_name_t);

	// now write it to the socket
	i = bufferevent_write(chan->bev, pb, pbp - pb);
	if (i < 0)
	{
		estat = GDP_STAT_PDU_WRITE_FAIL;
		goto fail0;
	}

	// and the payload
	i = bufferevent_write_buffer(chan->bev, payload);
	if (i < 0)
	{
		estat = GDP_STAT_PDU_WRITE_FAIL;
		goto fail0;
	}

fail0:
	return estat;
}


/*
**  Advertising primitives
*/

EP_STAT
_gdp_chan_advertise(
			gdp_chan_t *chan,
			gdp_name_t gname,
			gdp_adcert_t *adcert,
			gdp_chan_advert_cr_t *challenge_cb,
			void *adata)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;
	uint32_t reqflags = 0;
	gdp_pname_t pname;

	ep_dbg_cprintf(Dbg, 39, "_gdp_chan_advertise(%s):",
			gdp_printable_name(gname, pname));

	// create a new request and point it at the routing layer
	estat = _gdp_req_new(GDP_CMD_ADVERTISE, NULL, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);
	memcpy(req->cpdu->dst, RoutingLayerAddr, sizeof req->cpdu->dst);

	// send the request
	estat = _gdp_req_send(req);

	// there is no reply
	_gdp_req_free(&req);

fail0:
	if (ep_dbg_test(Dbg, 21))
	{
		char ebuf[100];

		ep_dbg_printf("_gdp_advertise => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	return estat;
}


EP_STAT
_gdp_chan_withdraw(
			gdp_chan_t *chan,
			gdp_name_t gname,
			void *adata)
{
	gdp_pname_t pname;

	ep_dbg_cprintf(Dbg, 39, "_gdp_chan_withdraw(%s):",
			gdp_printable_name(gname, pname));
	return GDP_STAT_NOT_IMPLEMENTED;
}


/*
**  _GDP_CHAN_GET_UDATA --- get user data from channel
*/

gdp_chan_x_t *
_gdp_chan_get_cdata(gdp_chan_t *chan)
{
	return chan->cdata;
}


/*
**  Get/Set functions for cursors.
*/

gdp_chan_t *
_gdp_cursor_get_chan(gdp_cursor_t *cursor)
{
	return cursor->chan;
}


gdp_buf_t *
_gdp_cursor_get_buf(gdp_cursor_t *cursor)
{
	return cursor->ibuf;
}


void
_gdp_cursor_get_endpoints(
				gdp_cursor_t *cursor,
				gdp_name_t *src,
				gdp_name_t *dst)
{
	memcpy(*src, cursor->src, sizeof *src);
	memcpy(*dst, cursor->dst, sizeof *dst);
}


size_t
_gdp_cursor_get_payload_len(gdp_cursor_t *cursor)
{
	return cursor->payload_len;
}


EP_STAT
_gdp_cursor_get_estat(gdp_cursor_t *cursor)
{
	return cursor->estat;
}


void
_gdp_cursor_set_udata(gdp_cursor_t *cursor, gdp_cursor_x_t *udata)
{
	cursor->udata = udata;
}


gdp_cursor_x_t *
_gdp_cursor_get_udata(gdp_cursor_t *cursor)
{
	return cursor->udata;
}


/* vim: set noexpandtab : */
