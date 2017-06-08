/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	This implements the GDP I/O event loop
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


/*
**  Following is a list of actions that should be undertaken in
**  response to various events:
**
**	Event					Client Action				Gdplogd Action
**	
**	connection established	advertise one				advertise all
**							re-subscribe all
**
**	connection lost [1]		retry open					retry open
**
**	data available			process command/ack			process command/ack
**
**	write complete			anything needed?			anything needed?
**
**	advertise timeout		re-advertise me				re-advertise all
**
**	connection close		withdraw me					withdraw all
**
**	[1] Should be handled automatically by the channel layer, but should
**		generate a "connection established" event.
*/

#include "gdp.h"
#include "gdp_chan.h"
#include "gdp_priv.h"

#include <ep/ep_app.h>
#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>

#include <errno.h>
#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.ioloop", "GDP I/O event loop");

EP_THR_MUTEX		_GdpSubscriptionMutex	EP_THR_MUTEX_INITIALIZER;


/*
**  Data Ready (Receive) callback
**
**		Called whenever there is input from the channel.
**		It is up to this routine to actually read the data from the
**		chan level buffer into active memory.
*/

EP_STAT
_gdp_io_recv(
		gdp_cursor_t *cursor,
		uint32_t flags)
{
	gdp_buf_t *ibuf;
	size_t payload_len;
	EP_STAT estat;

	if (EP_UT_BITSET(GDP_CURSOR_PARTIAL, flags))
		return EP_STAT_OK;			// need entire payload

	ibuf = _gdp_cursor_get_buf(cursor);
	payload_len = _gdp_cursor_get_payload_len(cursor);
	EP_ASSERT_ELSE(gdp_buf_getlength(ibuf) >= payload_len, return EP_STAT_OK);

	gdp_pdu_t *pdu = _gdp_pdu_new();
	_gdp_cursor_get_endpoints(cursor, &pdu->src, &pdu->dst);
	//pdu->payload_len = payload_len;
	estat = _gdp_pdu_in(pdu, cursor);
	return estat;
}

EP_STAT
_gdp_io_event(
		gdp_chan_t *chan,
		uint32_t what)
{
	EP_STAT estat = EP_STAT_OK;

	if (EP_UT_BITSET(BEV_EVENT_CONNECTED, what))
	{
		// connection up; do advertising and resend subscriptions (if any)
		gdp_chan_x_t *cx = _gdp_chan_get_udata(chan);
		if (cx->connect_cb != NULL)
			estat = (*cx->connect_cb)(chan);
	}
	return estat;
}
