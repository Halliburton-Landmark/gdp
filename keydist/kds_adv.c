/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  Log Advertisements
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


#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>
#include <ep/ep_dbg.h>

#include "ksd_data_manager.h"


static EP_DBG	Dbg = EP_DBG_INIT("kdist.advertise",
							"Key dist service Advertisements");


/*
**  Advertise all known GCLs
*/
EP_STAT
kds_advertise_all(int cmd)
{
	EP_STAT estat = _gdp_advertise(advertise_all_ksd, NULL, cmd);
	if (ep_dbg_test(Dbg, 21))
	{
		char ebuf[100];

		ep_dbg_printf("kds_advertise_all => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**  Advertise a new GCL
*/

static EP_STAT
advertise_one(gdp_buf_t *dbuf, void *ctx, int cmd)
{
	gdp_buf_write(dbuf, ctx, sizeof (gdp_name_t));
	return EP_STAT_OK;
}

void
kds_advertise_one(gdp_name_t gname, int cmd)
{
	EP_STAT estat = _gdp_advertise(advertise_one, gname, cmd);
	if (ep_dbg_test(Dbg, 11))
	{
		char ebuf[100];
		gdp_pname_t pname;

		ep_dbg_printf("kds_advertise_one(%s) => %s\n",
				gdp_printable_name(gname, pname),
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
}
