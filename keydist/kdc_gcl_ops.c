/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	This implements GDP Connection Log (GCL) utilities.
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

#include <string.h>
#include <sys/errno.h>

#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>
#include <gdp/gdp_gclmd.h>

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_log.h>
#include <hs/hs_errno.h>

#include "session_manager.h"

//#include <ep/ep_app.h>
//#include <ep/ep_hash.h>
//#include <ep/ep_prflags.h>


/*
#include "gdp_event.h"
#include "gdp_gclmd.h"

#include <event2/event.h>

*/

static EP_DBG	Dbg = EP_DBG_INIT("kds.gcl.ops", "GCL operations for KDS ");


/*
**	CREATE_GCL_NAME -- create a name for a new GCL
*/
/*
EP_STAT
_gdp_gcl_newname(gdp_gcl_t *gcl)
{
	if (!GDP_GCL_ISGOOD(gcl))
		return GDP_STAT_GCL_NOT_OPEN;
	_gdp_newname(gcl->name, gcl->gclmd);
	gdp_printable_name(gcl->name, gcl->pname);
	return EP_STAT_OK;
}
*/

/*
**	_GDP_GCL_CREATE --- create a new GCL
**
**		Creation is a bit tricky, since we don't start with an existing
**		GCL, and we address the message to the desired daemon instead
**		of to the GCL itself.  Some magic needs to occur.
*/

EP_STAT
_kdc_gcl_create(gdp_name_t kslname, gdp_name_t ksdname, gdp_gclmd_t *sInfo, 
				gdp_chan_t *chan, uint32_t reqflags, gdp_gcl_t **pgcl )
{
	int						rval;
	gdp_req_t				*req	= NULL;
	gdp_gcl_t				*gcl	= NULL;
	EP_STAT					estat	= EP_STAT_OK;



	errno = 0;				// avoid spurious messages

	{
		gdp_pname_t gxname, dxname;

		ep_dbg_cprintf(Dbg, 17,
				"_kdc_gcl_create: gcl=%s\n\tlogd=%s\n",
				kslname == NULL ? "none" : gdp_printable_name(kslname, gxname),
				gdp_printable_name(ksdname, dxname));
	}

	// create a new pseudo-GCL for the daemon so we can correlate the results
	estat = _gdp_gcl_newhandle( ksdname, &gcl );
	EP_STAT_CHECK(estat, goto fail0);

	// create the request
	_gdp_gcl_lock(gcl);

	estat = _gdp_req_new(GDP_CMD_CREATE, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);


	// 
	//  Part A. Fill the CMD_CREATE pdu 
	// 
	//	src: my GUID / dst: KDS_SERVICE 
	memcpy(req->cpdu->src, _GdpMyRoutingName, sizeof req->cpdu->src);
	memcpy(req->cpdu->dst, ksdname, sizeof req->cpdu->dst);

	//	buf 1>  ksname ... 
	gdp_buf_write(req->cpdu->datum->dbuf, kslname, sizeof (gdp_name_t));

	//	buf 2> access token in this device  
	rval = insert_mytoken( req->cpdu->datum->dbuf );
	if( rval != EX_OK ) {
		estat =  KDS_STAT_TOKEN_FAIL;
		goto fail0;
	}

	//	buf 3> basic info for requested service : sinfo (name is pname) 
	//			data log name, access log name, key log name, 
	//			mode (w/gen or only dist), writer device name in access token

	// add the metadata to the output stream
	_gdp_gclmd_serialize( sInfo,  req->cpdu->datum->dbuf);


	estat = _gdp_invoke(req);
	EP_STAT_CHECK(estat, goto fail1);

	// success --- change the GCL name to the true name
	_gdp_gcl_cache_changename(gcl, kslname);

	// free resources and return results
	_gdp_req_free(&req);
	*pgcl = gcl;
	return estat;

fail0:
	if (gcl != NULL)
		_gdp_gcl_decref(&gcl);  // including unlock
fail1:
	if (req != NULL)
		_gdp_req_free(&req);

	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 8, "Could not create GCL: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}



void _kdc_gcl_freehandle( gdp_gcl_t *gcl, bool isFull )
{
	if( gcl->apndfpriv != NULL ) {
		free_session_apnddata( gcl );
	}

	if( isFull ) _gdp_gcl_freehandle( gcl );
}

/*
**	_KDC_GCL_OPEN --- open a GCL for requesting decryption key
*/
EP_STAT
_kdc_gcl_open(gdp_gcl_t *gcl,
			int cmd, char mode, 
//			char	*myToken, 
//			gdp_gcl_open_info_t *info,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	int					exit_status = EX_OK;
	EP_STAT				estat		= EP_STAT_OK;
	gdp_req_t			*req		= NULL;
	gdp_session			*curSession	= NULL;
	uint8_t				rCmd		= 0; 


	EP_ASSERT_ELSE(GDP_GCL_ISGOOD(gcl),
					return EP_STAT_ASSERT_ABORT);

	errno = 0;				// avoid spurious messages
	reqflags |= GDP_REQ_ROUTEFAIL;			// don't retry on router errors
	EP_THR_MUTEX_ASSERT_ISLOCKED(&gcl->mutex);


	estat = _gdp_req_new(cmd, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// fill the datum on session 
	curSession	= request_session( req, mode ); 
	exit_status	= update_smsg_onsession( req->cpdu, curSession, mode, true );
	if( exit_status != EX_OK ) {
		ep_dbg_printf("[ERROR-S] Fail to handle smsg on session in _kdc_open\n"
						"%d: %s\n", 
						exit_status, str_errinfo( exit_status ) );
		//estat = GDP_STAT_INTERNAL_ERROR;
		estat = KDS_STAT_FAIL_SMSG;
		goto fail0;
	}

	estat = _gdp_invoke(req);
	rCmd = req->rpdu->cmd;
	// if error on response (rpdu->cmd != OK ), goto fail0: CHECK LATER
	EP_STAT_CHECK(estat, goto fail0);

	// success
	exit_status	= update_rmsg_onsession( req->rpdu, curSession, mode );
	if( exit_status != EX_OK ) {
		ep_dbg_printf("[ERROR-S] Fail to handle rmsg on session in _kdc_open\n"
						"[cmd:%d] %d: %s\n", 
						req->rpdu->cmd,  
						exit_status, str_errinfo( exit_status ) );

		if( rCmd != GDP_ACK_SUCCESS && rCmd != GDP_ACK_CREATED ) {
			//estat = GDP_STAT_INTERNAL_ERROR;
			estat = KDS_STAT_FAIL_RMSG;
			goto fail0;
		}
	}
	
	// save the number of records
	gcl->nrecs = req->rpdu->datum->recno;

	// read in the metadata to internal format
	gcl->gclmd = _gdp_gclmd_deserialize(req->rpdu->datum->dbuf);
	estat = EP_STAT_OK;

fail0:
	if (req != NULL)
	{
		req->gcl = NULL;		// owned by caller
		_gdp_req_free(&req);
	}

	// log failure
	if (EP_STAT_ISOK(estat))
	{
		// success!
		if (ep_dbg_test(Dbg, 30))
		{
			ep_dbg_printf("Opened [%u]", rCmd);
			_gdp_gcl_dump(gcl, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
		}
		else
		{
			ep_dbg_cprintf(Dbg, 10, "Opened[%u] GCL %s\n", rCmd, gcl->pname);
		}
	}
	else
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 9,
				"Couldn't open GCL [%d] %s:\n\t%s\n", rCmd, 
				gcl->pname, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;

}


/*
**	_KDC_GCL_CLOSE --- share operation for closing a GCL handle & data
*/
EP_STAT
_kdc_gcl_close(gdp_gcl_t *gcl, char mode, gdp_chan_t *chan, uint32_t reqflags ) 
{
	EP_STAT					estat		= EP_STAT_OK;
	gdp_req_t				*req		= NULL;
	gdp_session				*curSession	= NULL;
	int						nrefs		= 0;
	int						exit_status	= EX_OK;


	errno = 0;				// avoid spurious messages
	if (!GDP_GCL_ISGOOD(gcl))
		return GDP_STAT_GCL_NOT_OPEN;

	if( mode == 'S' ) {
		if( gcl->apndfpriv == NULL )	return KDS_STAT_NULL_SESSION;
		curSession = ((sapnd_dt *)(gcl->apndfpriv))->curSession;
		if( curSession == NULL )		return KDS_STAT_NULL_SESSION;
	}


	if (ep_dbg_test(Dbg, 38))
	{
		ep_dbg_printf("_kdc_gcl_close: ");
		_gdp_gcl_dump(gcl, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
	}

	_gdp_gcl_lock(gcl);

	// need to count the number of references /excluding/ subscriptions
	nrefs = gcl->refcnt;
	req = LIST_FIRST(&gcl->reqs);
	while (req != NULL)
	{
		if (EP_UT_BITSET(GDP_REQ_CLT_SUBSCR, req->flags))
			nrefs--;
		req = LIST_NEXT(req, gcllist);
	}

	if (nrefs > 1)
	{
		// nothing more to do
		goto finis;
	}

	estat = _gdp_req_new(GDP_CMD_CLOSE, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);


	exit_status = update_smsg_onsession( req->cpdu, curSession, mode, true ); 
	if( exit_status != EX_OK ) {
		ep_dbg_printf("[ERROR-S] Fail to handle smsg on session in kdc_close\n"
						"%d: %s\n", 
						exit_status, str_errinfo( exit_status ) );
		estat = KDS_STAT_FAIL_SMSG;
		goto fail0;
	}


	// tell the daemon to close it
	estat = _gdp_invoke(req);

	//XXX should probably check status (and do what with it?)
	// No session process on close response.  

	// release resources held by this handle
	_gdp_req_free(&req);

finis:
fail0:
	_kdc_gcl_freehandle( gcl, false ); 
	_gdp_gcl_decref(&gcl);
	return estat;
}



/*
**  APPEND_COMMON --- common code for sync and async appends
**
**		datum should be locked when called.
**		req will be locked upon return.
*/

static EP_STAT
kdc_append_common(gdp_gcl_t *gcl,
		gdp_datum_t *datum,
		gdp_chan_t *chan,
		uint32_t reqflags,
		gdp_req_t **reqp, char mode)
{
	int					exit_status;
	EP_STAT				estat	= GDP_STAT_BAD_IOMODE;
	gdp_req_t			*req	= NULL;
	gdp_session			*curSession = NULL;


	errno = 0;				// avoid spurious messages

	if( !GDP_DATUM_ISGOOD(datum) )
				return GDP_STAT_DATUM_REQUIRED;

	EP_ASSERT_POINTER_VALID(datum);
	if( !EP_UT_BITSET(GDP_MODE_AO, gcl->iomode) )
											goto fail0;

	curSession = ((sapnd_dt *)gcl->apndfpriv)->curSession;

	// create a new request structure
	estat = _gdp_req_new(GDP_CMD_APPEND, gcl, chan, NULL, reqflags, reqp);
	EP_STAT_CHECK(estat, goto fail0);
	req = *reqp;

	// if the assertion fails, we may be using an already freed datum
	EP_ASSERT_ELSE(datum->inuse, return EP_STAT_ASSERT_ABORT);

	// set up for signing (req->md will be updated with data part)
	req->md = gcl->digest;
	datum->recno = gcl->nrecs + 1;

	// Note that this is just a guess: the append may still fail,
	// but we need to do this if there are multiple threads appending
	// at the same time.
	// If the append fails, we'll be out of sync and all hell breaks loose.
	gcl->nrecs++;

	// if doing append filtering (e.g., encryption), call it now.
	if (gcl->apndfilter != NULL)
		estat = gcl->apndfilter(datum, gcl->apndfpriv);

	// caller owns datum
	gdp_datum_copy(req->cpdu->datum, datum);


	exit_status	= update_smsg_onsession( req->cpdu, curSession, mode, true );
	if( exit_status != EX_OK ) {
		ep_dbg_printf("[ERROR-S] Fail to handle smsg on session"
						" in _kdc_append\n %d: %s \n", 
						exit_status, str_errinfo( exit_status ) );
		//estat = GDP_STAT_INTERNAL_ERROR;
		estat = KDS_STAT_FAIL_SMSG;
	}

fail0:
	return estat;
}


/*
**  _GDP_GCL_APPEND --- shared operation for appending to a GCL
**
**		Used both in GDP client library and gdpd.
*/
EP_STAT
_kdc_gcl_append(gdp_gcl_t *gcl,
			gdp_datum_t *datum,
			gdp_chan_t *chan,
			uint32_t reqflags, char mode )
{
	int					exit_status; 
	uint8_t				rCmd;
	EP_STAT				estat	= GDP_STAT_BAD_IOMODE;
	gdp_req_t			*req	= NULL;
	gdp_session			*curSession = NULL;


	estat = kdc_append_common(gcl, datum, chan, reqflags, &req, mode);
	EP_STAT_CHECK(estat, goto fail0);

	// send the request to the log server
	estat = _gdp_invoke(req);
	if (EP_STAT_ISOK(estat))
		gcl->nrecs = datum->recno;

	// No data buf Need to check HMAC 
	rCmd = req->rpdu->cmd;
	curSession = ((sapnd_dt *)gcl->apndfpriv)->curSession;
	exit_status	= update_rmsg_onsession( req->rpdu, curSession, mode );
	if( exit_status != EX_OK ) {
		ep_dbg_printf("[ERROR-S] Fail to handle rmsg on session "
						"in _kdc_append \n [cmd:%d] %d: %s\n", 
						rCmd, exit_status, str_errinfo( exit_status ) );

		if( rCmd != GDP_ACK_SUCCESS ) {
			//estat = GDP_STAT_INTERNAL_ERROR;
			estat = KDS_STAT_FAIL_RMSG;
			goto fail1;
		}
	}


	gdp_buf_reset(datum->dbuf);
	if (datum->sig != NULL)
		gdp_buf_reset(datum->sig);
	gdp_datum_copy(datum, req->rpdu->datum);

fail1:
	_gdp_req_free(&req);

fail0:
	if (ep_dbg_test(Dbg, 42))
	{
		ep_dbg_printf("_gdp_gcl_append: returning ");
		gdp_datum_print(datum, ep_dbg_getfile(), GDP_DATUM_PRDEBUG);
	}


	return estat;
}


// LATER step1: for async  
//		Before calling req->sub_cb, process update_rmsg_onsession 
//			becaue HMAC use the pdu hdr information.. 
// LATER step2: update_rmsg_onsession can be called proc_resp . 
//					need to remove the call at each cmd_resp processing 
// Current: async mode, we use another simplified HMAC calculation except PDU. 
//			& the similar func to update_rmsg_ONSE is called in cb func. 
// callback function has to call update_async_rmsg~~ at first.  

/*
**  _GDP_GCL_APPEND_ASYNC --- asynchronous append
*/

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

EP_STAT
_kdc_gcl_append_async(
			gdp_gcl_t *gcl,
			gdp_datum_t *datum,
			gdp_event_cbfunc_t cbfunc,
			void *cbarg,
			gdp_chan_t *chan,
			uint32_t reqflags, char mode)
{
	EP_STAT				estat;
	gdp_req_t			*req = NULL;
	int i;


	// deliver results asynchronously
	reqflags |= GDP_REQ_ASYNCIO;
	estat = kdc_append_common(gcl, datum, chan, reqflags, &req, mode);
	EP_STAT_CHECK(estat, goto fail0);

	// arrange for responses to appear as events or callbacks
	req->cpdu->flags |= GDP_PDU_ASYNC_FLAG;

	_gdp_event_setcb(req, cbfunc, cbarg);

	estat = _gdp_req_send(req);

	// synchronous calls clear the data in the datum, so be consistent
	i = gdp_buf_drain(req->cpdu->datum->dbuf, SIZE_MAX);
	if (i < 0 && ep_dbg_test(Dbg, 1))
		ep_dbg_printf("_gdp_gcl_append_async: gdp_buf_drain failure\n");

	gdp_buf_reset(datum->dbuf);
	if (datum->sig != NULL)
		gdp_buf_reset(datum->sig);

	// cleanup and return
	if (!EP_STAT_ISOK(estat))
	{
		_gdp_req_free(&req);
	}
	else
	{
		req->state = GDP_REQ_IDLE;
		ep_thr_cond_signal(&req->cond);
		_gdp_req_unlock(req);
	}

fail0:
	if (ep_dbg_test(Dbg, 11))
	{
		char ebuf[100];
		ep_dbg_printf("_gdp_gcl_append_async => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**  _GDP_GCL_READ --- read a record from a GCL
**
**		Parameters:
**			gcl --- the gcl from which to read
**			datum --- the data buffer (to avoid dynamic memory)
**			chan --- the data channel used to contact the remote
**			reqflags --- flags for the request
**
**		This might be read by recno or read by timestamp based on
**		the command.  In any case the cmd is the defining factor.
*/

EP_STAT
_kdc_gcl_read(gdp_gcl_t *gcl,
			gdp_datum_t *datum,
			gdp_chan_t *chan,
			uint32_t reqflags, char mode)
{
	int				exit_status;
	uint8_t			rCmd;
	EP_STAT			estat		= GDP_STAT_BAD_IOMODE;
	gdp_req_t		*req		= NULL;
	gdp_session		*curSession	= NULL;


	errno = 0;				// avoid spurious messages

	if (!GDP_DATUM_ISGOOD(datum))
				return GDP_STAT_DATUM_REQUIRED;

	EP_ASSERT_ELSE(datum->inuse, return EP_STAT_ASSERT_ABORT);
	if (!EP_UT_BITSET(GDP_MODE_RO, gcl->iomode))
											goto fail0;

	// create and send a new request
	estat = _gdp_req_new(GDP_CMD_READ, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);
	gdp_datum_copy(req->cpdu->datum, datum);

	// before calling this function, caller has to check the related vaules
	//		with the function check_bad_kdc_gcl. 
	if( mode == 'S' ) {
		curSession = ((sapnd_dt *)gcl->apndfpriv)->curSession;
		exit_status	= update_smsg_onsession( req->cpdu, curSession, 
															mode, true );
		if( exit_status != EX_OK ) {
			ep_dbg_printf("[ERROR-S] Fail to handle smsg on session"
							" in _kdc_read\n %d: %s \n", 
							exit_status, str_errinfo( exit_status ) );
			estat = KDS_STAT_FAIL_SMSG;
			goto fail1;
		}
	}
	
	// send data 
	estat = _gdp_invoke(req);

	rCmd = req->rpdu->cmd;
	exit_status	= update_rmsg_onsession( req->rpdu, curSession, mode );
	if( exit_status != EX_OK ) {
		ep_dbg_printf("[ERROR-S] Fail to handle rmsg on session "
					"in _kdc_read \n [cmd:%d] %d: %s\n", 
					rCmd, exit_status, str_errinfo( exit_status ) );

		if( rCmd != GDP_ACK_CONTENT ) {
			//estat = GDP_STAT_INTERNAL_ERROR;
			estat = KDS_STAT_FAIL_RMSG;
			goto fail1;
		}
	}

	// ok, done!  pass the datum contents to the caller and free the request
	gdp_datum_copy(datum, req->rpdu->datum);

fail1:
	_gdp_req_free(&req);

fail0:
	return estat;
}



/*
**  _GDP_GCL_READ_ASYNC --- asynchronously read a record from a GCL
**
**		Parameters:
**			gcl --- the gcl from which to read
**			recno --- the record number to read
**			cbfunc --- the callback function (NULL => deliver as events)
**			cbarg --- user argument to cbfunc
**			chan --- the data channel used to contact the remote
**
**		This might be read by recno or read by timestamp based on
**		the command.  In any case the cmd is the defining factor.
*/
EP_STAT
_kdc_gcl_read_async(gdp_gcl_t *gcl,
			gdp_recno_t recno,
			gdp_event_cbfunc_t cbfunc,
			void *cbarg,
			gdp_chan_t *chan, char mode)
{
	int				exit_status;
	EP_STAT			estat;
	gdp_req_t		*req		= NULL;
	gdp_session		*curSession	= NULL;



	errno = 0;				// avoid spurious messages

	if (!EP_UT_BITSET(GDP_MODE_RO, gcl->iomode))
						return GDP_STAT_BAD_IOMODE;

	// create and send a new request
	estat = _gdp_req_new(GDP_CMD_READ, gcl, chan, NULL, GDP_REQ_ASYNCIO, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// arrange for responses to appear as events or callbacks
	req->cpdu->flags |= GDP_PDU_ASYNC_FLAG;
	req->cpdu->datum->recno = recno;

	_gdp_event_setcb(req, cbfunc, cbarg);

	// before calling this function, caller has to check the related vaules
	//		with the function check_bad_kdc_gcl. 
	if( mode == 'S' ) {
		curSession = ((sapnd_dt *)gcl->apndfpriv)->curSession;
		exit_status = update_smsg_onsession( req->cpdu, curSession, 
															mode, true ); 
		if( exit_status != EX_OK ) {
			ep_dbg_printf("[ERROR-S] Fail to handle smsg on session in"
							" _kdc_read_async \n %d: %s\n", 
							exit_status, str_errinfo( exit_status ) );
			estat = KDS_STAT_FAIL_SMSG;
			goto fail1;
		}
	}

	estat = _gdp_req_send(req);


	if (EP_STAT_ISOK(estat))
	{
		req->state = GDP_REQ_IDLE;
		_gdp_req_unlock(req);
	}
	else
	{
fail1:
		_gdp_req_free(&req);
	}

fail0:
	return estat;

}


/*
**  Subscription disappeared; remove it from list
*/
static void
kdc_subscr_lost(gdp_req_t *req)
{
	//TODO IMPLEMENT ME!
	if (ep_dbg_test(Dbg, 1))
	{
		ep_dbg_printf("subscr_lost: ");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	LIST_REMOVE( req, chanlist );

}


/*
**  Re-subscribe to a GCL
*/
static EP_STAT
kdc_subscr_resub(gdp_req_t *req)
{
	uint8_t			tmode = 'I';
	EP_STAT			estat;
	gdp_session		*curSession = NULL;


	ep_dbg_cprintf(Dbg, 39, "subscr_resub: refreshing req@%p\n", req);
	EP_ASSERT_ELSE(req != NULL, return EP_STAT_ASSERT_ABORT);
	EP_THR_MUTEX_ASSERT_ISLOCKED(&req->mutex);
	EP_ASSERT_ELSE(req->gcl != NULL, return EP_STAT_ASSERT_ABORT);
	EP_ASSERT_ELSE(req->cpdu != NULL, return EP_STAT_ASSERT_ABORT);

	//
	// Make another SUBSCRIBE request 
	//
	req->state = GDP_REQ_ACTIVE;
	req->cpdu->cmd = GDP_CMD_SUBSCRIBE;
	req->cpdu->flags |= GDP_PDU_ASYNC_FLAG;
	memcpy(req->cpdu->dst, req->gcl->name, sizeof req->cpdu->dst);
	memcpy(req->cpdu->src, _GdpMyRoutingName, sizeof req->cpdu->src);

	//XXX it seems like this should be in a known state
	if (req->cpdu->datum == NULL)
		req->cpdu->datum = gdp_datum_new();
	else if (req->cpdu->datum->dbuf != NULL)
		gdp_buf_reset(req->cpdu->datum->dbuf);
	req->cpdu->datum->recno = req->gcl->nrecs + 1;
	gdp_buf_put_uint32(req->cpdu->datum->dbuf, req->numrecs);


	if( req->gcl->apndfpriv != NULL ) {
		sapnd_dt	*tdata = (sapnd_dt *)req->gcl->apndfpriv;

		tmode		= tdata->mode;
		curSession	= tdata->curSession;
	}

	if( tmode=='S' && curSession != NULL ) {
		int			tval;

		tval = update_smsg_onsession( req->cpdu, curSession, tmode, true );
		if( tval != EX_OK ) {
			ep_dbg_printf("[ERROR-S] Fail to handle smsg on session"
							" in kdc_resub \n %d: %s \n",
							tval, str_errinfo( tval ) ); 
			return KDS_STAT_FAIL_SMSG;
		}
	}


	estat = _gdp_invoke(req);

	if (ep_dbg_test(Dbg, EP_STAT_ISOK(estat) ? 20 : 1))
	{
		char ebuf[200];

		ep_dbg_printf("subscr_resub(%s) ->\n\t%s\n",
				req->gcl == NULL ? "(no gcl)" : req->gcl->pname,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	// check HMAC of rcv pkt with update_rmsg : LATER 

	req->state = GDP_REQ_IDLE;
	if (req->rpdu->datum != NULL)
		gdp_datum_free(req->rpdu->datum);
	req->rpdu->datum = NULL;

	return estat;
}





/*
**  Periodically ping all open subscriptions to make sure they are
**  still happy.
*/

static void *
gdp_subscr_poker_thread(void *chan_)
{
	bool		removed = false;
	gdp_chan_t *chan = chan_;
	long delta_poke = ep_adm_getlongparam("swarm.gdp.subscr.pokeintvl",
							GDP_SUBSCR_REFRESH_DEF);
	long delta_dead = ep_adm_getlongparam("swarm.gdp.subscr.deadintvl",
							GDP_SUBSCR_TIMEOUT_DEF);

	ep_dbg_cprintf(Dbg, 10, "Starting subscription poker thread\n");
	chan->flags |= GDP_CHAN_HAS_SUB_THR;

	// loop forever poking subscriptions
	for (;;)
	{
		EP_STAT estat;
		gdp_req_t *req;
		gdp_req_t *nextreq;
		EP_TIME_SPEC now;
		EP_TIME_SPEC t_poke;	// poke if older than this
		EP_TIME_SPEC t_dead;	// abort if older than this

		// wait for a while to avoid hogging CPU
		ep_time_nanosleep(delta_poke / 2 SECONDS);
		ep_dbg_cprintf(Dbg, 40, "\nsubscr_poker_thread: poking\n");

		ep_time_now(&now);
		ep_time_from_nsec(-delta_poke SECONDS, &t_poke);
		ep_time_add_delta(&now, &t_poke);
		ep_time_from_nsec(-delta_dead SECONDS, &t_dead);
		ep_time_add_delta(&now, &t_dead);

		// do loop is in case _gdp_req_lock fails
		do
		{
			estat = EP_STAT_OK;
			for (req = LIST_FIRST(&chan->reqs); req != NULL; req = nextreq)
			{
				removed = false;

				estat = _gdp_req_lock(req);
				EP_STAT_CHECK(estat, break);

				nextreq = LIST_NEXT(req, chanlist);
				if (ep_dbg_test(Dbg, 51))
				{
					char tbuf[60];

					ep_time_format(&now, tbuf, sizeof tbuf, EP_TIME_FMT_HUMAN);
					ep_dbg_printf("subscr_poker_thread: at %s checking ", tbuf);
					_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
				}

				// if no GCL, it can't be a subscription
				gdp_gcl_t *gcl = req->gcl;
				_gdp_req_unlock(req);	// GCLs need to be locked before reqs
				if (gcl == NULL)
					continue;

				// lock GCL, then req, then validate req
				if (ep_thr_mutex_trylock(&gcl->mutex) != 0)
					continue;
				gcl->flags |= GCLF_ISLOCKED;
				_gdp_req_lock(req);

				if (!EP_UT_BITSET(GDP_REQ_CLT_SUBSCR, req->flags))
				{
					// not a subscription: skip this entry
				}
				else if (ep_time_before(&t_poke, &req->act_ts))
				{
					// we've seen activity recently, no need to poke
				}
				else if (ep_time_before(&req->act_ts, &t_dead))
				{
					// this subscription is dead
					//XXX should be impossible: subscription refreshed each time
					kdc_subscr_lost(req);
					removed = true;
				}
				else
				{
					// t_dead < act_ts <= t_poke: refresh this subscription
					(void) kdc_subscr_resub(req);
				}

				// if _gdp_invoke failed, try again at the next poke interval
				if( removed ) _gdp_req_free( &req );
				else		  _gdp_req_unlock(req);

				_gdp_gcl_unlock(gcl);
			}
		}
		while (!EP_STAT_ISOK(estat));
	}

	// not reached; keep gcc happy
	ep_log(EP_STAT_SEVERE, "subscr_poker_thread: fell out of loop");
	return NULL;
}



// response of subscribe : synchronouse
// async process: GDP_ACK_CONTNET by event 
EP_STAT
_kdc_gcl_subscribe( gdp_req_t *req,  
						gdp_event_cbfunc_t cbfunc,
						void *cbarg, char mode )
{
	int				exit_status;
	EP_STAT			estat = EP_STAT_OK;
	gdp_session		*curSession	= NULL;



	errno = 0;				// avoid spurious messages

	EP_ASSERT_POINTER_VALID( req );

	_gdp_event_setcb(req, cbfunc, cbarg);

	if( mode == 'S' ) 
		curSession = ((sapnd_dt *)(req->gcl->apndfpriv))->curSession;

	estat = _gdp_invoke( req );
	EP_ASSERT( req->state == GDP_REQ_ACTIVE );

	if( !EP_STAT_ISOK( estat ) ) {
		// Fail to send subscription 
		_gdp_req_free( &req );
		return estat; 
	}

	// Originally, response data is NULL. 
	// However, Need to check the HMAC 
	//	LATER:  need to synchrnize the status between two end points on error 
	if( mode == 'S' ) {
		uint8_t			rCmd = req->rpdu->cmd;

		exit_status	= update_rmsg_onsession( req->rpdu, curSession, mode );
		if( exit_status != EX_OK ) {
			ep_dbg_printf("[ERROR-S] Fail to handle rmsg on session "
					"in _kdc_read \n [cmd:%d] %d: %s\n", 
					rCmd, exit_status, str_errinfo( exit_status ) );

			if( rCmd == GDP_ACK_SUCCESS ) {
				//estat = GDP_STAT_INTERNAL_ERROR;
				estat = KDS_STAT_FAIL_RMSG;
				// LATER: re-subscribe in caller or sub-end 

				_gdp_req_free( &req );
				return estat;
			}
		}
	}


	// Clean the unnecessary data structure 
	req->flags |= GDP_REQ_ASYNCIO;
	req->state	= GDP_REQ_IDLE;
	if( req->rpdu != NULL ) _gdp_pdu_free( req->rpdu );
	req->rpdu	= NULL;
	if( req->cpdu->datum != NULL ) gdp_buf_reset( req->cpdu->datum->dbuf );

	// Wait asynchronously received datum  & process them with cb func
	ep_thr_cond_signal( &req->cond );
	_gdp_req_unlock( req ); // is lockec in gdp_req_new

	// the req is still on the channel list. 

	if( req->cpdu->cmd == GDP_CMD_SUBSCRIBE ) {
		bool	spawnthread	= false;
		long	poke = ep_adm_getlongparam("swarm.gdp.subscr.pokeintvl", 
													60L );

		ep_thr_mutex_lock( &req->chan->mutex );

		if( poke>0 && !EP_UT_BITSET(GDP_CHAN_HAS_SUB_THR, req->chan->flags) ) 
		{
			spawnthread = true;
			req->chan->flags |= GDP_CHAN_HAS_SUB_THR; 
		}

		ep_thr_mutex_unlock( &req->chan->mutex );

		if( spawnthread ) {
			int		tstat = ep_thr_spawn( &req->chan->sub_thr_id, 
						gdp_subscr_poker_thread, req->chan );

			if( tstat != 0 ) {
				EP_STAT		spawn_stat = ep_stat_from_errno( tstat );

				ep_log( spawn_stat, "_kdc_subscribe: thread spawn failure");
			}
		}
		
	}

	return estat;

}

/*
**  _GDP_GCL_GETMETADATA --- return metadata for a log
*/
// not yet implemented in server part. 
EP_STAT
_kdc_gcl_getmetadata(gdp_gcl_t *gcl,
		gdp_gclmd_t **gmdp,
		gdp_chan_t *chan,
		uint32_t reqflags, char mode)
{
	int						exit_status;
	EP_STAT					estat;
	gdp_req_t				*req		= NULL;
	gdp_session				*curSession = NULL;



	errno = 0;				// avoid spurious messages
	estat = _gdp_req_new(GDP_CMD_GETMETADATA, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);


	// before calling this function, caller has to check the related vaules
	//		with the function check_bad_kdc_gcl. 
	if( mode == 'S' ) {
		curSession = ((sapnd_dt *)gcl->apndfpriv)->curSession;
		exit_status = update_smsg_onsession( req->cpdu, curSession, 
															mode, true ); 
		if( exit_status != EX_OK ) {
			ep_dbg_printf("[ERROR-S] Fail to handle smsg on session in"
							" _kdc_read_async \n %d: %s\n", 
							exit_status, str_errinfo( exit_status ) );
			estat = KDS_STAT_FAIL_SMSG;
			goto fail1;
		}
	}

	estat = _gdp_invoke(req);
	EP_STAT_CHECK(estat, goto fail1);


	if( mode == 'S' ) {
		exit_status	= update_rmsg_onsession( req->rpdu, curSession, mode );

		if( exit_status != EX_OK ) {
			ep_dbg_printf("[ERROR-S] Fail to handle rmsg on session "
						"in _kdc_metadata \n [cmd:%d] %d: %s\n", 
						req->rpdu->cmd,  
						exit_status, str_errinfo( exit_status ) );

			estat = KDS_STAT_FAIL_RMSG;
			goto fail1;
		}
	}

	*gmdp = _gdp_gclmd_deserialize(req->rpdu->datum->dbuf);

fail1:
	_gdp_req_free(&req);

fail0:
	return estat;
}


// LATER: instead of CMD_CREATE, we can use NEWSEGMENT to request kds service

