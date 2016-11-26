/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	I/O CHANNEL HANDLING
**		This communicates between the client and the routing layer.
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

#include "gdp.h"
#include "gdp_priv.h"
#include "gdp_zc_client.h"

#include <ep/ep_dbg.h>
#include <ep/ep_log.h>
#include <ep/ep_prflags.h>
#include <ep/ep_string.h>

#include <netinet/tcp.h>
#include <errno.h>
#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.chan", "GDP channel processing");
static EP_DBG	DemoMode = EP_DBG_INIT("_demo", "Demo Mode");


/*
**	GDP_READ_CB --- data is available for reading from gdpd socket
**
**		Minimal implementation: read in PDU and hand it to
**		processing routine.  If that processing is going to be
**		lengthy it should use a thread.
*/

static void
gdp_read_cb(struct bufferevent *bev, void *ctx)
{
	EP_STAT estat;
	gdp_pdu_t *pdu;
	gdp_buf_t *ievb = bufferevent_get_input(bev);
	gdp_chan_t **pchan = ctx;
	gdp_chan_t *chan = *pchan;

	ep_dbg_cprintf(Dbg, 50, "gdp_read_cb: fd %d, %zd bytes\n",
			bufferevent_getfd(bev), gdp_buf_getlength(ievb));

	while (gdp_buf_getlength(ievb) > 0)
	{
		pdu = _gdp_pdu_new();
		estat = _gdp_pdu_in(pdu, chan);
		if (!EP_STAT_ISOK(estat))
		{
			// bad PDU (too short, version mismatch, whatever)
			_gdp_pdu_free(pdu);
			return;
		}

		ep_dbg_cprintf(Dbg, 11,
				"\n*** Processing %s %d=%s from socket %d\n",
				GDP_CMD_IS_COMMAND(pdu->cmd) ? "command" : "ack/nak",
				pdu->cmd, _gdp_proto_cmd_name(pdu->cmd),
				bufferevent_getfd(bev));
		(*chan->process)(pdu, chan);
	}
}


/*
**	GDP_EVENT_CB --- events or errors occur on gdpd socket
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
gdp_event_cb(struct bufferevent *bev, short events, void *ctx)
{
	bool restart_connection = false;
	gdp_chan_t **pchan = ctx;
	gdp_chan_t *chan = *pchan;

	if (ep_dbg_test(Dbg, 25))
	{
		ep_dbg_printf("gdp_event_cb: ");
		ep_prflags(events, EventWhatFlags, ep_dbg_getfile());
		ep_dbg_printf(", fd=%d , errno=%d\n",
				bufferevent_getfd(bev), EVUTIL_SOCKET_ERROR());
	}

	EP_ASSERT(bev == chan->bev);

	if (EP_UT_BITSET(BEV_EVENT_CONNECTED, events))
	{
		// sometimes libevent says we're connected when we're not
		if (EVUTIL_SOCKET_ERROR() == ECONNREFUSED)
			chan->state = GDP_CHAN_ERROR;
		else
			chan->state = GDP_CHAN_CONNECTED;
		ep_thr_cond_broadcast(&chan->cond);
	}
	if (EP_UT_BITSET(BEV_EVENT_EOF, events))
	{
		gdp_buf_t *ievb = bufferevent_get_input(bev);
		size_t l = gdp_buf_getlength(ievb);

		ep_dbg_cprintf(Dbg, 1, "_gdp_event_cb: got EOF, %zu bytes left\n", l);
		restart_connection = true;
	}
	if (EP_UT_BITSET(BEV_EVENT_ERROR, events))
	{
		int sockerr = EVUTIL_SOCKET_ERROR();

		ep_dbg_cprintf(Dbg, 1, "_gdp_event_cb: error: %s\n",
				evutil_socket_error_to_string(sockerr));
		restart_connection = true;
	}

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
			estat = _gdp_chan_open(NULL, NULL, pchan);
		} while (!EP_STAT_ISOK(estat));
		(*chan->advertise)(GDP_CMD_ADVERTISE);
	}
}


/*
**	_GDP_CHAN_OPEN --- open channel to the routing layer
*/

EP_STAT
_gdp_chan_open(const char *gdp_addr,
		void (*process)(gdp_pdu_t *, gdp_chan_t *),
		gdp_chan_t **pchan)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_chan_t *chan = *pchan;
	bool newchan = false;

	if (chan == NULL)
	{
		// allocate a new channel structure
		newchan = true;
		chan = ep_mem_zalloc(sizeof *chan);
		LIST_INIT(&chan->reqs);
		ep_thr_mutex_init(&chan->mutex, EP_THR_MUTEX_DEFAULT);
		ep_thr_cond_init(&chan->cond);
		chan->state = GDP_CHAN_CONNECTING;
		chan->process = process;

		// set up the bufferevent
		chan->bev = bufferevent_socket_new(GdpIoEventBase,
						-1,
						BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE |
						BEV_OPT_DEFER_CALLBACKS | BEV_OPT_UNLOCK_CALLBACKS);
		*pchan = chan;
	}
	else
	{
		ep_dbg_cprintf(Dbg, 12, "Re-using channel @ %p\n"
				"	 process = %p\n"
				"	 advertise = %p\n",
				chan, chan->process, chan->advertise);
	}
	if (chan->bev == NULL)
	{
		estat = ep_stat_from_errno(errno);
		ep_dbg_cprintf(Dbg, 18, "_gdp_chan_open: no bufferevent\n");
		goto fail0;
	}

	// speak of the devil
	bufferevent_setcb(chan->bev, gdp_read_cb, NULL, gdp_event_cb, pchan);
	bufferevent_enable(chan->bev, EV_READ | EV_WRITE);

	// attach it to a socket
	char abuf[500] = "";
	char *port = NULL;		// keep gcc happy
	char *host;

	// get the host:port info into abuf
	if (gdp_addr != NULL && gdp_addr[0] != '\0')
	{
		strlcpy(abuf, gdp_addr, sizeof abuf);
	}
	else
	{
#if GDP_OSCF_USE_ZEROCONF
		if (ep_adm_getboolparam("swarm.gdp.zeroconf.enable", true))
		{
			ep_dbg_cprintf(DemoMode, 1, "Trying Zeroconf:\n");

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
							ep_dbg_cprintf(DemoMode, 1, "Zeroconf found %s\n",
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

		ep_dbg_cprintf(DemoMode, 1, "Trying %s\n", host);

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
		ep_thr_mutex_lock(&chan->mutex);
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
			int enable = 1;
			if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable)))
			{
				// bad news, but keep trying
				estat = ep_stat_from_errno(errno);
				ep_log(estat, "_gdp_chan_open: cannot create socket");
				continue;
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

		ep_thr_mutex_unlock(&chan->mutex);
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
		if (chan != NULL && newchan)
		{
			if (chan->bev != NULL)
				bufferevent_free(chan->bev);
			chan->bev = NULL;
			ep_mem_free(chan);
			*pchan = NULL;
		}
		return estat;
	}
	ep_dbg_cprintf(Dbg, 1, "_gdp_chan_open: talking to router at %s:%s\n",
				host, port);
	return estat;
}


/*
**	_GDP_CHAN_CLOSE --- close a channel (e.g., on error)
*/

void
_gdp_chan_close(gdp_chan_t **pchan)
{
	EP_ASSERT(pchan != NULL);
	gdp_chan_t *chan = *pchan;
	if (chan == NULL)
	{
		ep_dbg_cprintf(Dbg, 7, "_gdp_chan_close: null channel\n");
		return;
	}

	chan->state = GDP_CHAN_CLOSING;
	ep_thr_cond_broadcast(&chan->cond);
	*pchan = NULL;
	if (chan->close_cb != NULL)
		(*chan->close_cb)(chan);
	bufferevent_free(chan->bev);
	chan->bev = NULL;
	ep_thr_cond_destroy(&chan->cond);
	ep_thr_mutex_destroy(&chan->mutex);
	ep_mem_free(chan);
}

/* vim: set noexpandtab : */
