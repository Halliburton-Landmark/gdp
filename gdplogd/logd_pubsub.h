/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  Headers for publish/subscribe model
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

#ifndef _GDPD_PUBSUB_H_
#define _GDPD_PUBSUB_H_

#define SUB_DEFAULT_TIMEOUT		600		// default subscription timeout (10m)

// notify all subscribers that new data is available (or shutdown required)
void			sub_notify_all_subscribers(gdp_req_t *pubreq, int cmd);

// terminate a subscription
void			sub_end_subscription(gdp_req_t *req);

// terminate all subscriptions for a given {gcl, client} pair
EP_STAT			sub_end_all_subscriptions(gdp_gcl_t *gcl, gdp_name_t dest);

// reclaim subscription resources
void			sub_reclaim_resources(gdp_chan_t *chan);

#endif // _GDPD_PUBSUB_H_
