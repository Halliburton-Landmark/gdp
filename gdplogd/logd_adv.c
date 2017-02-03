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


#include "logd.h"

#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>

#include <ep/ep_dbg.h>


static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.advertise",
							"GDP GCL Advertisements");


/*
**  Advertise all known GCLs
*/

static void
adv_addone(gdp_name_t gname, void *ctx)
{
	gdp_buf_t *b = ctx;
	int i;

	if (ep_dbg_test(Dbg, 54))
	{
		gdp_pname_t pname;

		ep_dbg_printf("\tAdvertise %s\n", gdp_printable_name(gname, pname));
	}

	i = gdp_buf_write(b, gname, sizeof (gdp_name_t));
	if (i < 0)
		ep_dbg_cprintf(Dbg, 1, "logd_adv_addone: gdp_buf_write failure\n");
}


static EP_STAT
advertise_all(gdp_buf_t *dbuf, void *ctx, int cmd)
{
	GdpDiskImpl.foreach(adv_addone, dbuf);
	return EP_STAT_OK;
}


EP_STAT
logd_advertise_all(int cmd)
{
	EP_STAT estat = _gdp_advertise(advertise_all, NULL, cmd);
	if (ep_dbg_test(Dbg, 21))
	{
		char ebuf[100];

		ep_dbg_printf("logd_advertise_all => %s\n",
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
logd_advertise_one(gdp_name_t gname, int cmd)
{
	EP_STAT estat = _gdp_advertise(advertise_one, gname, cmd);
	if (ep_dbg_test(Dbg, 11))
	{
		char ebuf[100];
		gdp_pname_t pname;

		ep_dbg_printf("logd_advertise_one(%s) =>\n\t%s\n",
				gdp_printable_name(gname, pname),
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
}
