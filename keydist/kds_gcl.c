/* vim: set ai sw=4 sts=4 ts=4 : */

/*
** 
**	----- BEGIN LICENSE BLOCK -----
**  KEY Generation / Distribution Service Daemon
**
**	Copyright (c) 2015-2017, Electronics and Telecommunications 
**	Research Institute (ETRI). All rights reserved. 
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
**  KDS_gcl -- functions to handle GCL on this service    
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 


#include <hs/hs_errno.h>
#include "keydistd.h"


#if !LOG_CHECK
static EP_DBG	Dbg = EP_DBG_INIT("kds.gcl", 
					"Key Distribution Service data & GCL handling");
#endif


/*
**  GCL_ALLOC --- allocate a new GCL handle in memory
*/
EP_STAT kds_gcl_alloc(gdp_name_t gcl_name, gdp_iomode_t iomode, 
				gdp_gcl_t **pgcl, KSD_info *ksData )
{
	EP_STAT				estat;
	gdp_gcl_t			*gcl = NULL;
	extern void kds_gcl_close(gdp_gcl_t *gcl);


	// get the standard handle
	estat = _gdp_gcl_newhandle(gcl_name, &gcl);
	EP_STAT_CHECK(estat, goto fail0);

	gcl->iomode = GDP_MODE_ANY;		// might change mode later: be permissive

	// add the KSD-specific information
	gcl->x = NULL; 
	gcl->apndfpriv = (void *)ksData; 	
	ksData->gcl = gcl;

	// make sure that if this is freed it gets removed from GclsByUse
	gcl->freefunc = kds_gcl_close;

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
kds_gcl_open(gdp_name_t gcl_name, gdp_iomode_t iomode, gdp_gcl_t **pgcl)
{
	int					exit_status;
	EP_STAT				estat   = EP_STAT_OK;
	gdp_gcl_t			*gcl    = NULL;
	KSD_info			*ksData = NULL;



	// find ksData to be served in this service 
	exit_status = get_ksd_handle( gcl_name, &ksData, false, NULL, 0 );
	if( exit_status != EX_OK ) {
		ep_app_error("Check: fail to get ksd object for %s", gcl_name );
		return GDP_STAT_NOTFOUND;
	}

	estat = kds_gcl_alloc(gcl_name, iomode, &gcl, ksData );
	EP_STAT_CHECK(estat, goto fail0);

	// so far, so good...  
	_gdp_gcl_lock(gcl);

	// success!
	*pgcl = gcl;
	return estat;

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
kds_gcl_close(gdp_gcl_t *gcl)
{
	KSD_info	*ksData = (KSD_info *)(gcl->apndfpriv); 

	if( ksData == NULL ) return ;

	ksData->gcl = NULL;

	// LATER: remove below line & treat the resource reclaim... 
	cancel_ksd_handle( ksData, false );
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
	EP_STAT			estat = EP_STAT_OK;


	// if we already got this (e.g., in _gdp_pdu_process or in cache)
	//		just let it be
	if (req->gcl != NULL ||
		(req->gcl = _gdp_gcl_cache_get(req->cpdu->dst, iomode)) != NULL)
	{
		if (ep_dbg_test(Dbg, 40))
		{
			gdp_pname_t pname;

			gdp_printable_name(req->cpdu->dst, pname);
			ep_dbg_printf("get_open_handle: using existing GCL:\n"
							"\t%s => %p\n", pname, req->gcl );
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


	estat = kds_gcl_open(req->cpdu->dst, iomode, &req->gcl);
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

# endif // LOG_CHECK
