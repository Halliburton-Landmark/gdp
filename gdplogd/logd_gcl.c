/* vim: set ai sw=4 sts=4 ts=4 : */

/*
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
#include "logd_pubsub.h"

#if !LOG_CHECK
static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.gcl", "GDP Log Daemon GCL handling");
#endif


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

	//XXX for now, assume all GCLs are on disk
	gcl->x->physimpl = &GdpDiskImpl;

	// make sure that if this is freed it gets removed from GclsByUse
	gcl->freefunc = gcl_close;

	// OK, return the value
	*pgcl = gcl;

fail0:
	return estat;
}


/*
**  GCL_OPEN --- open an existing GCL
**
**		Returns the GCL locked.
*/

EP_STAT
gcl_open(gdp_name_t gcl_name, gdp_iomode_t iomode, gdp_gcl_t **pgcl)
{
	EP_STAT estat;
	gdp_gcl_t *gcl;

	estat = gcl_alloc(gcl_name, iomode, &gcl);
	EP_STAT_CHECK(estat, goto fail0);

	// so far, so good...  do the physical open
	_gdp_gcl_lock(gcl);
	estat = gcl->x->physimpl->open(gcl);
	EP_STAT_CHECK(estat, goto fail1);

	// success!
	*pgcl = gcl;
	return estat;

fail1:
	_gdp_gcl_decref(&gcl);
fail0:
	// if this isn't a "not found" error, mark it as an internal error
	if (!EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOTFOUND))
		estat = GDP_STAT_NAK_INTERNAL;
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

	// close the underlying files and free memory as needed
	if (gcl->x->physimpl->close != NULL)
		gcl->x->physimpl->close(gcl);

	ep_mem_free(gcl->x);
	gcl->x = NULL;
}

#if !LOG_CHECK

/*
**  Get an open instance of the GCL in the request.
**
**		This maps the GCL name to the internal GCL instance.
**		That open instance is returned in the request passed in.
**		The GCL will have it's reference count bumped, so the
**		caller must call _gdp_gcl_decref when done with it.
**
**		GCL is returned locked.
*/

EP_STAT
get_open_handle(gdp_req_t *req, gdp_iomode_t iomode)
{
	EP_STAT estat;

	// if we already got this (e.g., in _gdp_pdu_process or in cache)
	//		just let it be
	if (req->gcl != NULL ||
		(req->gcl = _gdp_gcl_cache_get(req->cpdu->dst, iomode)) != NULL)
	{
		if (ep_dbg_test(Dbg, 40))
		{
			gdp_pname_t pname;

			gdp_printable_name(req->cpdu->dst, pname);
			ep_dbg_printf("get_open_handle: using existing GCL:\n\t%s => %p\n",
					pname, req->gcl);
		}
		return EP_STAT_OK;
	}

	// not in cache?  create a new one.
	if (ep_dbg_test(Dbg, 11))
	{
		gdp_pname_t pname;

		gdp_printable_name(req->cpdu->dst, pname);
		ep_dbg_printf("get_open_handle: opening %s\n", pname);
	}

	estat = gcl_open(req->cpdu->dst, iomode, &req->gcl);
	if (EP_STAT_ISOK(estat))
		_gdp_gcl_cache_add(req->gcl, iomode);
	if (req->gcl != NULL)
		req->gcl->flags |= GCLF_DEFER_FREE;

	if (ep_dbg_test(Dbg, 40))
	{
		gdp_pname_t pname;
		char ebuf[60];

		gdp_printable_name(req->cpdu->dst, pname);
		ep_stat_tostr(estat, ebuf, sizeof ebuf);
		ep_dbg_printf("get_open_handle: %s => %p: %s\n",
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
gcl_reclaim_resources(void *null)
{
	// how long to leave GCLs open before reclaiming (default: 5 minutes)
	time_t reclaim_age = ep_adm_getlongparam("swarm.gdplogd.reclaim.age",
								300L);
	_gdp_gcl_cache_reclaim(reclaim_age);
	sub_reclaim_resources(_GdpChannel);
}
# endif // LOG_CHECK
