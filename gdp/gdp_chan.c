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
#include <ep/ep_hexdump.h>
#include <ep/ep_log.h>
#include <ep/ep_prflags.h>
#include <ep/ep_string.h>

#include <errno.h>
#include <string.h>
#include <sys/queue.h>

#include <netinet/tcp.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.chan", "GDP channel processing");


// protocol version number in layer 4 (transport) PDU
#if PROTOCOL_L4_V3
#define GDP_CHAN_PROTO_VERSION	3
#else
#define GDP_CHAN_PROTO_VERSION	4
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
	gdp_chan_recv_cb_t		*recv_cb;		// receive callback
	gdp_chan_send_cb_t		*send_cb;		// send callback
	gdp_chan_ioevent_cb_t	*ioevent_cb;	// close/error/eof callback
	gdp_chan_router_cb_t	*router_cb;		// router event callback
	gdp_chan_advert_func_t	*advert_cb;		// advertising function
};

/* Channel states */
#define GDP_CHAN_UNCONNECTED	0		// channel is not connected yet
#define GDP_CHAN_CONNECTING		1		// connection being initiated
#define GDP_CHAN_CONNECTED		2		// channel is connected and active
#define GDP_CHAN_ERROR			3		// channel has had error
#define GDP_CHAN_CLOSING		4		// channel is closing


#if PROTOCOL_L4_V3
// Running over a Version 3 Transport Layer
// PDU layout shown in gdp_pdu.h
#define MIN_HEADER_LENGTH	(1+1+1+1+32+32+4+1+1+1+1+4)
#else
/*
**  On-the-Wire PDU Format
**
**		This is for client layer to routing layer communications.
**		It may, in some modified form, also be used for router to
**		router communications, but that's beyond the scope of this
**		header file.
**
**		off	len	meaning
**		---	---	-------
**		0	1	version (must be 4) [1]
**		1	1	header length in units of 32 bits (= H / 4)
**		2	1	flags / type of service [2]
**		4	4	payload (SDU) length (= P)
**		8	4	flow id
**		8	32	destination address
**		40	32	source address
**		?	?	for future use (probably options)
**		H	P	payload (SDU) (starts at offset given in octet 1)
**
**		[1] If the high order bit of the version is set, this is
**			reserved for router-to-router communication.  When the
**			client generates or sees a PDU, the high order bit must
**			be zero.  The remainder of a router-to-router PDU is not
**			defined here.
**
**		[2] The low-order three bits define the address fields.  If
**			zero, there are two 32-byte (256-bit) addresses for
**			destination and source respectively.  Other values are
**			reserved.
**
**			If the high order bit of flags/type of service is set,
**			this is a client-to-router interaction (e.g.,
**			advertise) and the low order bits are a specific
**			command (see below).
**
**			It is likely that router-to-router commands will want
**			to re-use this field as a command.
**
**		Special flag values (masked with 0xF8) are:
**			0x80	Forward this PDU to the destination, strip off the
**					header, and re-interpret the payload as a new
**					PDU.
**			0x90	Payload contains an advertisement.
**			0x98	Payload contains a withdrawal.
**			0xF0	(Router-to-client) Indicates a "name not found"
**					(or "no route") error.
**		Question: should the PDU have a "protocol" field (a la IPv4
**			packets) with a special value of GDP_in_GDP, by analogy
**			with IP's IP_in_IP, rather than using a FORWARD bit?
*/

// values for flags / router control / type of service field
#define GDP_TOS_ADDR_FMT	0x07	// indicates structure of addresses
#define GDP_TOS_ROUTER		0x80	// router should interpret this PDU
#define GDP_TOS_ROUTERMASK	0xf8	// mask for the router command
#define GDP_TOS_FORWARD		0x80	// forward to another address
#define GDP_TOS_ADVERTISE	0x90	// name advertisement
#define GDP_TOS_WITHDRAW	0x98	// name withdrawal
#define GDP_TOS_NOROUTE		0xF0	// no route / name unknown

//XXX following needs to be changed if ADDR_FMT != 0
// magic, hdrlen, tos, rsvd, paylen, dst, src, pad
#define MIN_HEADER_LENGTH	(1 + 1 + 1 + 1 + 2 + 32 + 32 + 2)
#endif	// PROTOCOL_L4_V3
#define MAX_HEADER_LENGTH	(255 * 4)

//XXX obsolete
#if PROTOCOL_L4_V3
static uint8_t	RoutingLayerAddr[32] =
	{
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
	};
#endif // PROTOCOL_L4_V3


/*
**  _GDP_CHAN_INIT --- initialize channel subsystem
*/

EP_STAT
_gdp_chan_init(
		struct event_base *evbase,
		void *options_unused)
{
	if (evbase == NULL)
		return EP_STAT_ABORT;
	EventBase = evbase;
	return EP_STAT_OK;
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
**		On return, the header has been consumed from the input but
**			the complete payload is still in the input buffer.
**			The payload length is returned through plenp.
**		Returns GDP_STAT_KEEP_READING and leaves the header in the
**			input buffer if the entire payload is not yet in memory.
**		Returns GDP_STAT_NAK_NOROUTE if the router cannot find a
**			path to the destination.
*/

static EP_STAT
read_header(gdp_chan_t *chan,
		gdp_buf_t *ibuf,
		gdp_name_t *src,
		gdp_name_t *dst,
		size_t *plenp)
{
	uint8_t *pbp = gdp_buf_getptr(ibuf, MIN_HEADER_LENGTH);
	int b;
	size_t hdr_len;
	size_t payload_len = 0;
	EP_STAT estat = EP_STAT_OK;

	if (pbp == NULL)
	{
		// fewer than MIN_HEADER_LENGTH bytes in buffer
		ep_dbg_cprintf(Dbg, 11, "read_header: pbp == NULL\n");
		estat = GDP_STAT_KEEP_READING;
		goto done;
	}

	if (ep_dbg_test(Dbg, 66))
	{
		ep_dbg_printf("read_header: initial header:\n");
		ep_hexdump(pbp, MIN_HEADER_LENGTH, ep_dbg_getfile(), EP_HEXDUMP_HEX, 0);
	}

	GET8(b);				// PDU version number
#if PROTOCOL_L4_V3
	if (b != 2 && b != 3)
#else
	if (b != GDP_CHAN_PROTO_VERSION)
#endif
	{
		ep_dbg_cprintf(Dbg, 1, "wrong protocol version %d (%d expected)\n",
				b, GDP_CHAN_PROTO_VERSION);
		estat = GDP_STAT_PDU_VERSION_MISMATCH;

		// for lack of anything better, flush the entire input buffer
		gdp_buf_drain(ibuf, gdp_buf_getlength(ibuf));
		goto done;
	}
#if PROTOCOL_L4_V3
	GET8(b);				// time to live (ignored by non-routing layer)
	int rnak;
	GET8(b);					// reserved (ignored)
	GET8(rnak);					// command (ignored except for router naks)
	memcpy(dst, pbp, sizeof (gdp_name_t));	// destination
	pbp += sizeof (gdp_name_t);
	memcpy(src, pbp, sizeof (gdp_name_t));	// source
	pbp += sizeof (gdp_name_t);
	GET32(b);					// rid (ignored)
	GET16(b);					// signature size and type (ignored)
	GET8(b);					// option length / 4
	hdr_len = MIN_HEADER_LENGTH + (b * 4);
	GET8(b);					// flags (ignored)
	GET32(payload_len);			// data length
#else
	GET8(hdr_len);			// header length / 4
	hdr_len *= 4;
	if (hdr_len < MIN_HEADER_LENGTH)
	{
		ep_dbg_cprintf(Dbg, 1,
				"read_header: short header, need %d got %zd\n",
				MIN_HEADER_LENGTH, hdr_len);
		estat = GDP_STAT_PDU_CORRUPT;
		goto done;
	}

	// if we don't yet have the whole header, wait until we do
	if (gdp_buf_getlength(ibuf) < hdr_len)
		return GDP_STAT_KEEP_READING;

	int flags;
	GET8(flags);			// flags/type of service (ignored)
	GET8(b);				// reserved (MBZ)
	if (b != 0)				//DEBUG: really shouldn't test for zero here
	{
		ep_dbg_cprintf(Dbg, 1, "read_header: reserved (MBZ) = 0x%02x\n", b);
		estat = GDP_STAT_PDU_CORRUPT;
		goto done;
	}
	GET16(payload_len);
	if ((flags & GDP_TOS_ADDR_FMT) == 0)
	{
		memcpy(dst, pbp, sizeof (gdp_name_t));
		pbp += sizeof (gdp_name_t);
		memcpy(src, pbp, sizeof (gdp_name_t));
		pbp += sizeof (gdp_name_t);
	}
	else
	{
		ep_dbg_cprintf(Dbg, 1,
				"read_header: unknown address format 0x%02x\n",
				flags & GDP_TOS_ADDR_FMT);
		estat = GDP_STAT_PDU_CORRUPT;
		goto done;
	}

	if (ep_dbg_test(Dbg, 55))
	{
		gdp_pname_t src_p, dst_p;
		ep_dbg_printf("read_header(%zd): flags 0x%02x, paylen %zd\n"
					"    src %s\n"
					"    dst %s\n",
				hdr_len, flags, payload_len,
				gdp_printable_name(*src, src_p),
				gdp_printable_name(*dst, dst_p));
	}

	// check for router meta-commands (tos)
	if (EP_UT_BITSET(GDP_TOS_ROUTER, flags))
	{
		if ((flags & GDP_TOS_ROUTERMASK) == GDP_TOS_NOROUTE)
		{
			estat = GDP_STAT_NAK_NOROUTE;
			goto done;
		}
		else
		{
			ep_dbg_cprintf(Dbg, 1, "read_header: PDU router tos = %0xd\n",
					flags & GDP_TOS_ROUTERMASK);
			estat = GDP_STAT_PDU_CORRUPT;
			goto done;
		}
	}
#endif	// PROTOCOL_L4_V3

	// XXX check for rational payload_len here? XXX

	// make sure entire PDU is in memory
	if (gdp_buf_getlength(ibuf) < hdr_len + payload_len)
	{
		estat = GDP_STAT_KEEP_READING;
		goto done;
	}

	// consume the header, but leave the payload
	gdp_buf_drain(ibuf, hdr_len);
#if PROTOCOL_L4_V3
	if (rnak == GDP_NAK_R_NOROUTE)
		estat = GDP_STAT_NAK_NOROUTE;
#endif

done:
	if (EP_STAT_ISOK(estat))
		estat = EP_STAT_FROM_INT(payload_len);
	else
	{
		ep_dbg_cprintf(Dbg, 19, "read_header: draining %zd on error\n",
						hdr_len + payload_len);
		gdp_buf_drain(ibuf, hdr_len + payload_len);
		payload_len = 0;
	}

	{
		char ebuf[100];
		ep_dbg_cprintf(Dbg, 32, "read_header: hdr %zd pay %zd stat %s\n",
				hdr_len, payload_len,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	*plenp = payload_len;
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
	gdp_chan_t *chan = (gdp_chan_t *) ctx;
	gdp_name_t src, dst;

	ep_dbg_cprintf(Dbg, 50, "chan_read_cb: fd %d, %zd bytes\n",
			bufferevent_getfd(bev), gdp_buf_getlength(ibuf));

	EP_ASSERT(bev == chan->bev);

	while (gdp_buf_getlength(ibuf) >= MIN_HEADER_LENGTH)
	{
		// get the transport layer header
		size_t payload_len;
		estat = read_header(chan, ibuf, &src, &dst, &payload_len);

		// if we don't have enough input, wait for more (we'll be called again)
		if (EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
			break;

		if (!EP_STAT_ISOK(estat))
		{
			// deliver routing error to upper level
			ep_dbg_cprintf(Dbg, 27, "chan_read_cb: sending to router_cb %p\n",
						chan->router_cb);
			if (chan->router_cb != NULL)
			{
				estat = (*chan->router_cb)(chan, src, dst, payload_len, estat);
			}
			else
			{
				ep_dbg_cprintf(Dbg, 1, "chan_read_cb: NULL router_cb\n");
				estat = GDP_STAT_NOT_IMPLEMENTED;
				gdp_buf_drain(ibuf, payload_len);
			}
		}

		// pass it to the L5 callback
		// note that if the callback is not set, the PDU is thrown away
		if (EP_STAT_ISOK(estat))
		{
			if (chan->recv_cb != NULL)
			{
				// call upper level processing
				estat = (*chan->recv_cb)(chan, src, dst, ibuf, payload_len);
			}
			else
			{
				// discard input
				ep_dbg_cprintf(Dbg, 1, "chan_read_cb: NULL recv_cb\n");
				estat = GDP_STAT_NOT_IMPLEMENTED;
				gdp_buf_drain(ibuf, payload_len);
			}
		}
		char ebuf[100];
		ep_dbg_cprintf(Dbg, 32, "chan_read_cb: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
}


/*
**	CHAN_EVENT_CB --- events or errors occur on network socket
*/

static EP_PRFLAGS_DESC	EventWhatFlags[] =
{
	{ BEV_EVENT_READING,	BEV_EVENT_READING,		"READING"			},
	{ BEV_EVENT_WRITING,	BEV_EVENT_WRITING,		"WRITING"			},
	{ BEV_EVENT_EOF,		BEV_EVENT_EOF,			"EOF"				},
	{ BEV_EVENT_ERROR,		BEV_EVENT_ERROR,		"ERROR"				},
	{ BEV_EVENT_TIMEOUT,	BEV_EVENT_TIMEOUT,		"TIMEOUT"			},
	{ BEV_EVENT_CONNECTED,	BEV_EVENT_CONNECTED,	"CONNECTED"			},
	{ 0, 0, NULL }
};

static void
chan_event_cb(struct bufferevent *bev, short events, void *ctx)
{
	bool restart_connection = false;
	gdp_chan_t *chan = (gdp_chan_t *) ctx;
	uint32_t cbflags = 0;

	if (ep_dbg_test(Dbg, 10))
	{
		int sockerr = EVUTIL_SOCKET_ERROR();
		ep_dbg_printf("chan_event_cb[%d]: ", getpid());
		ep_prflags(events, EventWhatFlags, ep_dbg_getfile());
		ep_dbg_printf(", fd=%d , errno=%d %s\n",
				bufferevent_getfd(bev),
				sockerr, evutil_socket_error_to_string(sockerr));
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

		ep_dbg_cprintf(Dbg, 1, "chan_event_cb[%d]: got EOF, %zu bytes left\n",
					getpid(), l);
		cbflags |= GDP_IOEVENT_EOF;
		restart_connection = true;
	}
	if (EP_UT_BITSET(BEV_EVENT_ERROR, events))
	{
		int sockerr = EVUTIL_SOCKET_ERROR();

		ep_dbg_cprintf(Dbg, 1, "chan_event_cb[%d]: error: %s\n",
				getpid(), evutil_socket_error_to_string(sockerr));
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

		do
		{
			long delay = ep_adm_getlongparam("swarm.gdp.reconnect.delay", 1000L);
			if (delay > 0)
				ep_time_nanosleep(delay * INT64_C(1000000));
			estat = chan_reopen(chan);
		} while (!EP_STAT_ISOK(estat));
	}

	if (chan->state == GDP_CHAN_CONNECTED)
		(*chan->advert_cb)(chan, GDP_CMD_ADVERTISE, ctx);
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
		gdp_chan_t *chan,
		void *adata)
{
	EP_STAT estat = EP_STAT_OK;
	char abuf[500] = "";
	char *port = NULL;		// keep gcc happy

	// attach to a socket
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

	ep_dbg_cprintf(Dbg, 28, "chan_open_helper(%s)\n", abuf);

	// strip off addresses and try them
	estat = GDP_STAT_NOTFOUND;				// anything that is not OK
	{
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

			ep_dbg_cprintf(Dbg, 20, "chan_open_helper: trying host %s port %s\n",
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
						"chan_open_helper: getaddrinfo(%s, %s) =>\n"
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
					ep_log(estat, "chan_open_helper: cannot create socket");
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
						ep_log(estat, "chan_open_helper: cannot set TCP_NODELAY");
						// error not fatal, let's just go on
					}
				}
				if (connect(sock, a->ai_addr, a->ai_addrlen) < 0)
				{
					// connection failure
					estat = ep_stat_from_errno(errno);
					ep_dbg_cprintf(Dbg, 38,
							"chan_open_helper[%d]: connect failed: %s\n",
							getpid(), strerror(errno));
					close(sock);
					continue;
				}

				// success!  Make it non-blocking and associate with bufferevent
				ep_dbg_cprintf(Dbg, 39, "successful connect\n");
				estat = EP_STAT_OK;

				// set up the bufferevent
				evutil_make_socket_nonblocking(sock);
				chan->bev = bufferevent_socket_new(EventBase, sock,
								BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE |
								BEV_OPT_DEFER_CALLBACKS |
								BEV_OPT_UNLOCK_CALLBACKS);
				bufferevent_setcb(chan->bev,
								chan_read_cb, NULL, chan_event_cb, chan);
				bufferevent_setwatermark(chan->bev,
								EV_READ, MIN_HEADER_LENGTH, 0);
				bufferevent_enable(chan->bev, EV_READ | EV_WRITE);
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
	}

	// error cleanup and return
	if (!EP_STAT_ISOK(estat))
	{
fail0:
		if (ep_dbg_test(Dbg, 2))
		{
			char ebuf[80];
			ep_dbg_printf("chan_open_helper[%d]: could not open channel: %s\n",
					getpid(), ep_stat_tostr(estat, ebuf, sizeof ebuf));
			//ep_log(estat, "chan_open_helper: could not open channel");
		}
	}
	else
	{
		ep_dbg_cprintf(Dbg, 1,
					"chan_open_helper[%d]: talking to router at %s:%s\n",
					getpid(), host, port);
		(*chan->advert_cb)(chan, GDP_CMD_ADVERTISE, adata);
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
		gdp_chan_recv_cb_t *recv_cb,
		gdp_chan_send_cb_t *send_cb,
		gdp_chan_ioevent_cb_t *ioevent_cb,
		gdp_chan_router_cb_t *router_cb,
		gdp_chan_advert_func_t *advert_func,
		gdp_chan_x_t *cdata,
		gdp_chan_t **pchan)
{
	EP_STAT estat;
	gdp_chan_t *chan;

	ep_dbg_cprintf(Dbg, 11, "_gdp_chan_open(%s)\n", router_addr);

	// allocate a new channel structure
	chan = (gdp_chan_t *) ep_mem_zalloc(sizeof *chan);
	ep_thr_mutex_init(&chan->mutex, EP_THR_MUTEX_DEFAULT);
	ep_thr_mutex_setorder(&chan->mutex, GDP_MUTEX_LORDER_CHAN);
	ep_thr_cond_init(&chan->cond);
	chan->state = GDP_CHAN_CONNECTING;
	chan->recv_cb = recv_cb;
	chan->send_cb = send_cb;			//XXX unused at this time
	chan->ioevent_cb = ioevent_cb;
	chan->router_cb = router_cb;
	chan->advert_cb = advert_func;
	chan->cdata = cdata;
	if (router_addr != NULL)
		chan->router_addr = ep_mem_strdup(router_addr);

	estat = chan_open_helper(chan, NULL);

	if (EP_STAT_ISOK(estat))
		*pchan = chan;
	else
		chan_do_close(chan, BEV_EVENT_ERROR);
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

	// close the (now dead) bufferevent
	if (chan->bev != NULL)
		bufferevent_free(chan->bev);
	chan->bev = NULL;
	estat = chan_open_helper(chan, NULL);
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

static EP_STAT
send_helper(gdp_chan_t *chan,
			gdp_target_t *target,
			gdp_name_t src,
			gdp_name_t dst,
			gdp_buf_t *payload,
			int tos)
{
	EP_STAT estat = EP_STAT_OK;
	int i;
	size_t payload_len = 0;

	if (payload != NULL)
		payload_len = gdp_buf_getlength(payload);

	if (ep_dbg_test(Dbg, 51))
	{
		gdp_pname_t src_printable;
		gdp_pname_t dst_printable;
		ep_dbg_printf("send_helper:\n\tsrc %s\n\tdst %s\n\tpayload %p ",
				gdp_printable_name(src, src_printable),
				gdp_printable_name(dst, dst_printable),
				payload);
		if (payload == NULL)
			ep_dbg_printf("(no payload)\n");
		else
		{
			ep_dbg_printf("len %zd\n", payload_len);
			ep_hexdump(gdp_buf_getptr(payload, payload_len), payload_len,
					ep_dbg_getfile(), EP_HEXDUMP_ASCII, 0);
		}
	}

	// build the header in memory
	char pb[MAX_HEADER_LENGTH];
	char *pbp = pb;

#if PROTOCOL_L4_V3
	PUT8(GDP_CHAN_PROTO_VERSION);		// version number (3)
	PUT8(GDP_TTL_DEFAULT);				// time to live
	PUT8(0);							// reserved
	PUT8(tos);							// command (overloaded)
	memcpy(pbp, dst, sizeof (gdp_name_t));	// dst
	pbp += sizeof (gdp_name_t);
	memcpy(pbp, src, sizeof (gdp_name_t));	// src
	pbp += sizeof (gdp_name_t);
	PUT32(0);							// request ID (unused)
	PUT16(0);							// signature length and MD algorithm
	PUT8(0);							// optionals length
	PUT8(0);							// flags
	PUT32(payload_len);					// SDU payload length
#else
	PUT8(GDP_CHAN_PROTO_VERSION);		// version number
	PUT8(MIN_HEADER_LENGTH / 4);		// header length (= 72 / 4)
	PUT8(tos);							// flags / type of service
	PUT8(0);							// reserved
	PUT16(payload_len);
	memcpy(pbp, dst, sizeof (gdp_name_t));
	pbp += sizeof (gdp_name_t);
	memcpy(pbp, src, sizeof (gdp_name_t));
	pbp += sizeof (gdp_name_t);
	PUT16(0);							// padding
#endif

	// now write header to the socket
	bufferevent_lock(chan->bev);
	EP_ASSERT((pbp - pb) == MIN_HEADER_LENGTH);
	if (ep_dbg_test(Dbg, 33))
	{
		ep_dbg_printf("send_helper: sending %zd octets:\n",
					payload_len + (pbp - pb));
		ep_hexdump(pb, pbp - pb, ep_dbg_getfile(), 0, 0);
		if (payload_len > 0)
		{
			ep_hexdump(gdp_buf_getptr(payload, payload_len), payload_len,
					ep_dbg_getfile(), EP_HEXDUMP_ASCII, pbp - pb);
		}
	}

	i = bufferevent_write(chan->bev, pb, pbp - pb);
	if (i < 0)
	{
		estat = GDP_STAT_PDU_WRITE_FAIL;
		goto fail0;
	}

	// and the payload
	if (payload_len > 0)
	{
		i = bufferevent_write_buffer(chan->bev, payload);
		if (i < 0)
		{
			estat = GDP_STAT_PDU_WRITE_FAIL;
			goto fail0;
		}
	}

fail0:
	bufferevent_unlock(chan->bev);
	return estat;
}

EP_STAT
_gdp_chan_send(gdp_chan_t *chan,
			gdp_target_t *target,
			gdp_name_t src,
			gdp_name_t dst,
			gdp_buf_t *payload)
{
	if (ep_dbg_test(Dbg, 32))
	{
		size_t l = evbuffer_get_length(payload);
		uint8_t *p = evbuffer_pullup(payload, l);
		ep_dbg_printf("_gdp_chan_send: sending PDU:\n");
		ep_hexdump(p, l, ep_dbg_getfile(), EP_HEXDUMP_ASCII, 0);
	}
	return send_helper(chan, target, src, dst, payload, 0);
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
	gdp_pname_t pname;

	ep_dbg_cprintf(Dbg, 39, "_gdp_chan_advertise(%s):\n",
			gdp_printable_name(gname, pname));

	gdp_buf_t *payload = gdp_buf_new();

	// might batch several adverts into one PDU
	gdp_buf_write(payload, gname, sizeof (gdp_name_t));

#if PROTOCOL_L4_V3
	estat = send_helper(chan, NULL, _GdpMyRoutingName, RoutingLayerAddr,
						payload, GDP_CMD_ADVERTISE);
#else
//	gdp_name_t null_name = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//							 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	estat = send_helper(chan, NULL, _GdpMyRoutingName, gname,
						NULL, GDP_TOS_ADVERTISE);
#endif

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

	ep_dbg_cprintf(Dbg, 39, "_gdp_chan_withdraw(%s)\n",
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


/* vim: set noexpandtab : */
