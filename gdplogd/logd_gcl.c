/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**	----- END LICENSE BLOCK -----
*/

#include "logd.h"
#include "logd_physlog.h"

static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.gcl", "GDP Log Daemon GCL handling");


/*
**  GCL_ALLOC --- allocate a new GCL handle in memory
*/

EP_STAT
gcl_alloc(gdp_name_t gcl_name, gdp_iomode_t iomode, gdp_gcl_t **pgcl)
{
	EP_STAT estat;
	gdp_gcl_t *gcl;
	extern void gcl_close(gdp_gcl_t *gcl);

	// get the standard handle
	estat = _gdp_gcl_newhandle(gcl_name, &gcl);
	EP_STAT_CHECK(estat, goto fail0);
	gcl->iomode = GDP_MODE_ANY;		// might change mode later: be permissive

	// add the gdpd-specific information
	gcl->x = ep_mem_zalloc(sizeof *gcl->x);
	if (gcl->x == NULL)
	{
		estat = EP_STAT_OUT_OF_MEMORY;
		goto fail0;
	}
	gcl->x->gcl = gcl;

	// make sure that if this is freed it gets removed from GclsByUse
	gcl->freefunc = gcl_close;

	// OK, return the value
	*pgcl = gcl;

fail0:
	return estat;
}


/*
**  GCL_OPEN --- open an existing GCL
*/

EP_STAT
gcl_open(gdp_name_t gcl_name, gdp_iomode_t iomode, gdp_gcl_t **pgcl)
{
	EP_STAT estat;
	gdp_gcl_t *gcl;

	estat = gcl_alloc(gcl_name, iomode, &gcl);
	EP_STAT_CHECK(estat, goto fail0);

	// so far, so good...  do the physical open
	estat = gcl_physopen(gcl);
	EP_STAT_CHECK(estat, goto fail1);

	// success!
	*pgcl = gcl;
	return estat;

fail1:
	_gdp_gcl_freehandle(gcl);
fail0:
	return estat;
}


/*
**  GCL_CLOSE --- close a GDP version of a GCL handle
**
**		Called from _gdp_gcl_freehandle, generally when the reference
**		count drops to zero and the GCL is reclaimed.
*/

void
gcl_close(gdp_gcl_t *gcl)
{
	if (gcl->x == NULL)
		return;

	// close the underlying files
	if (gcl->x->fp != NULL)
		gcl_physclose(gcl);

	ep_mem_free(gcl->x);
	gcl->x = NULL;
}


EP_STAT
get_open_handle(gdp_req_t *req, gdp_iomode_t iomode)
{
	EP_STAT estat;

	// if we already got this (e.g., in _gdp_pdu_process or in cache)
	//		just let it be
	if (req->gcl != NULL ||
		(req->gcl = _gdp_gcl_cache_get(req->pdu->dst, iomode)) != NULL)
	{
		if (ep_dbg_test(Dbg, 40))
		{
			gdp_pname_t pname;

			gdp_printable_name(req->pdu->dst, pname);
			ep_dbg_printf("get_open_handle: using existing GCL:\n\t%s => %p\n",
					pname, req->gcl);
		}
		return EP_STAT_OK;
	}

	// not in cache?  create a new one.
	if (ep_dbg_test(Dbg, 10))
	{
		gdp_pname_t pname;

		gdp_printable_name(req->pdu->dst, pname);
		ep_dbg_printf("get_open_handle: opening %s\n", pname);
	}
	estat = gcl_open(req->pdu->dst, iomode, &req->gcl);
	if (EP_STAT_ISOK(estat))
		_gdp_gcl_cache_add(req->gcl, iomode);
	req->gcl->flags |= GCLF_DEFER_FREE;

	if (ep_dbg_test(Dbg, 40))
	{
		gdp_pname_t pname;
		char ebuf[60];

		gdp_printable_name(req->pdu->dst, pname);
		ep_stat_tostr(estat, ebuf, sizeof ebuf);
		ep_dbg_printf("get_open_handle: %s:\n\t@%p: %s\n",
				pname, req->gcl, ebuf);
	}
	return estat;
}


/*
**  GCL_RECLAIM_RESOURCES --- find unused GCL resources and reclaim them
**
**		This should really also have a maximum number of GCLs to leave
**		open so we don't run out of file descriptors under high load.
**
**		This implementation locks the GclsByUse list during the
**		entire operation.  That's probably not the best idea.
*/

void
gcl_reclaim_resources(void)
{
	// how long to leave GCLs open before reclaiming (default: 5 minutes)
	time_t reclaim_age = ep_adm_getlongparam("swarm.gdplogd.reclaim.age",
								300L);
	_gdp_gcl_cache_reclaim(reclaim_age);
}
