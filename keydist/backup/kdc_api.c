/* vim: set ai sw=4 sts=4 ts=4 : */

/*
** 
**	----- BEGIN LICENSE BLOCK -----
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
**  KDS_API --- not include yet..  
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.12.12 
*/ 

#include <string.h>

#include <ep/ep.h>
#include <ep/ep_dbg.h>

#include <hs/hs_errno.h>
#include <hs/gdp_extension.h>

#include "kdc_api.h"
#include "session_manager.h"


#define	KSD_SERVICE		"kr.re.etri.ksd_service1" 


static EP_DBG	Dbg = EP_DBG_INIT("kds.api", "Client API for KDS" );

static EP_THR_MUTEX	OpenMutex	EP_THR_MUTEX_INITIALIZER;


//
//  Internal Functions 
//
 
bool isAllowedReqMode( char inMode ) 
{
	if( inMode != KS_MODE_GEN_DIST && inMode != KS_MODE_ONLY_DIST )
			return false;

	return true;
}

void kdc_prstat( EP_STAT estat, const gdp_gcl_t *gcl, const char *where ) 
{
	int			dbgLev = 2;
	char		ebuf[100];

	if( EP_STAT_ISOK(estat) ) dbgLev = 39;
	else if( EP_STAT_ISWARN(estat) ) dbgLev = 11;

	if( gcl == NULL )  {
		ep_dbg_cprintf( Dbg, dbgLev, "<<< %s : %s \n", 
				where, ep_stat_tostr(estat, ebuf, sizeof ebuf) );
	} else {
		ep_dbg_cprintf( Dbg, dbgLev, "<<< %s(%s) : %s \n", 
				where, gcl->pname, ep_stat_tostr(estat, ebuf, sizeof ebuf) );
	}

}


EP_STAT check_bad_kdc_gcl(const gdp_gcl_t *gcl, const char *where, 
														const char mode)
{
	if( gcl == NULL ) {
		ep_dbg_cprintf( Dbg, 2, "<<< %s: NULL GCL \n", where );
		return GDP_STAT_NULL_GCL;
	} 

	if( !EP_UT_BITSET(GCLF_INUSE, (gcl)->flags ) ) {
		ep_dbg_cprintf( Dbg, 2, "<<< %s: Not opened GCL \n", where ); 
		return GDP_STAT_GCL_NOT_OPEN; 
	}

	if( mode == 'S' ) {
		if( gcl->apndfpriv == NULL ) {
			ep_dbg_cprintf( Dbg, 2, "<<< %s: NULL SESSION \n", where );
			return KDS_STAT_NULL_SESSION;
		}

		if( ((sapnd_dt *)(gcl->apndfpriv))->curSession == NULL ) {
			ep_dbg_cprintf( Dbg, 2, "<<< %s: NULL SESSION \n", where );
			return KDS_STAT_NULL_SESSION;
		}

	}

	return EP_STAT_OK;
}


//
// External Functions 
//

gdp_gclmd_t* set_ksinfo( gdp_pname_t dlogname, gdp_pname_t alogname, 
					gdp_pname_t klogname, char rw_mode, gdp_pname_t wdid) 
{
	gdp_gclmd_t			*sInfo = NULL;
	size_t				dlen, alen, klen, wlen;
	EP_STAT				estat; 



	// Error check 
	if( isAllowedReqMode( rw_mode ) == false )  return NULL;

	dlen = strlen( dlogname );
	alen = strlen( alogname );
	klen = strlen( klogname );
	wlen = strlen( wdid     );
		
	if( dlen<=0 || alen<=0 || klen<=0 || wlen<=0 ) return NULL;


	sInfo = gdp_gclmd_new( 0 );
	if( sInfo == NULL ) return NULL;


	// Fill the key service information in sInfo 
	estat =	gdp_gclmd_add( sInfo, GDP_GCLMD_DLOG, dlen, dlogname );
	if( !EP_STAT_ISOK( estat ) ) goto fail0;

	estat =	gdp_gclmd_add( sInfo, GDP_GCLMD_ACLOG, alen, alogname );
	if( !EP_STAT_ISOK( estat ) ) goto fail0;

	estat =	gdp_gclmd_add( sInfo, GDP_GCLMD_KLOG, klen, klogname );
	if( !EP_STAT_ISOK( estat ) ) goto fail0;

	estat =	gdp_gclmd_add( sInfo, GDP_GCLMD_WDID, wlen, wdid );
	if( !EP_STAT_ISOK( estat ) ) goto fail0;

	if( rw_mode == KS_MODE_GEN_DIST ) 
			estat =	gdp_gclmd_add( sInfo, GDP_GCLMD_KGEN, 1, "w" );
	else	estat =	gdp_gclmd_add( sInfo, GDP_GCLMD_KGEN, 1, "r" );
	if( !EP_STAT_ISOK( estat ) ) goto fail0;


	return sInfo;

fail0:
	if( sInfo != NULL ) gdp_gclmd_free( sInfo );
	return NULL;

}



// LATER 
// return value :exit_status 
// gmd already includes the dname, aname, kname, mode 
// start / close / change   
EP_STAT kds_service_request( gdp_pname_t ksname, char cmd, 
							gdp_gclmd_t *sinfo, KGEN_param	*kinfo )
{
	EP_STAT					estat;
	const char				*pname	= NULL;
	gdp_name_t				ksl_iname;
	gdp_name_t				ksdname;
	gdp_gcl_t				*gcl	= NULL;



	ep_dbg_cprintf( Dbg, 19, "\n>>> [%d] kds_service_request for %s \n", 
								cmd, ksname==NULL?"NULL":ksname );
	
	estat = GDP_CHECK_INITIALIZED;
	EP_STAT_CHECK( estat, goto fail0 );

	if( cmd == KS_SERVICE_CANCLE ) {
		// LATER: not implemented yet. 
		return KDS_STAT_NOT_SUPPORT;
	}

	if( cmd != KS_SERVICE_START ) goto step1; 

	//
	// Action requesting to start the KS service with CMD_CREATE command 
	//
	 
	if( ksname == NULL )  return KDS_STAT_WRONG_INPUT;

	pname = ep_adm_getstrparam( "swarm.kdistd.gdpname", KSD_SERVICE );
	gdp_parse_name( ksname, ksl_iname );
	gdp_parse_name( pname, ksdname );

	estat = _kdc_gcl_create( ksl_iname, ksdname, sinfo, _GdpChannel, 
					GDP_REQ_ALLOC_RID, &gcl );
	if( EP_STAT_ISOK( estat ) == false ) goto fail0;

	// close the session 

/*

	//
	// PART B. Proceed the response of CMD_CREATE 
	// 
	// S responses. GDP_ACK_CREATED  (new) / GDP_ACK_SUCCESS (already)
	// F responses: GDP_NAK_C_BADREQ, GDP_NAC_C_UNAUTH, GDP_NAK_S_INTERNAL 

		// 2. gcl_append 
			// make kgen_param info & write it in buf
			// send append pdu. 
			// if success returned -> close 
			// else fail. close 
	


	// response. GDP_ACK_CREATED 

	// put_kgenparam_to_buf( )
step1:
	if( kinfo != NULL ) {
	}
	// cmd_open
	// cmd_append 
	// cmd_close 
*/

step1:
	// open. append close 


fail0: 

	kdc_prstat( estat, gcl, "kdc_service_request" );

	return estat;
	
}


EP_STAT kdc_gcl_open( gdp_name_t ks_name, gdp_iomode_t io_mode, 
						gdp_gcl_t **pgcl, char s_mode )
{
	int						cmd			= GDP_CMD_OPEN_RO;
	EP_STAT					estat;
	gdp_gcl_t				*gcl		= NULL;



	estat = GDP_CHECK_INITIALIZED;
	EP_STAT_CHECK( estat, return estat );

	if( io_mode == GDP_MODE_RO )	  cmd = GDP_CMD_OPEN_RO; 
	else if( io_mode == GDP_MODE_AO ) cmd = GDP_CMD_OPEN_AO; 
	else if( io_mode == GDP_MODE_RA ) cmd = GDP_CMD_OPEN_RA; 
	else {
		ep_dbg_printf("[ERROR] kdc_open: illegal io-mode %d \n", 
						io_mode );
		return GDP_STAT_BAD_IOMODE;
	}


	if( !gdp_name_is_valid( ks_name ) ) {
		// illegal GCL name
		ep_dbg_cprintf( Dbg, 6, "kdc_gcl_open: illegal gcl name \n" );
		return GDP_STAT_NULL_GCL;
	}

	// lock this operation to keep the GCL cache consistent
	ep_thr_mutex_lock(&OpenMutex);

	
	// See if we already have this open 
	gcl = _gdp_gcl_cache_get( ks_name, io_mode );
	if( gcl != NULL ) {
		// reference count++ & gcl lock in _gdp_gcl_cache_get 
		ep_dbg_cprintf( Dbg, 10, "kdc_gcl_open(%s): using existing GCL "
						" @ %p \n", gcl->pname, gcl );
		gcl->iomode |= io_mode;
		estat = EP_STAT_OK;

	} else {
		// No existing gcl. Create a new one. 
		estat = _gdp_gcl_newhandle( ks_name, &gcl );
		EP_STAT_CHECK( estat, goto fail0 );

		_gdp_gcl_lock( gcl );
		gcl->iomode = io_mode; 

		estat = _kdc_gcl_open( gcl, cmd, s_mode, _GdpChannel, 
											GDP_REQ_ALLOC_RID );
		EP_THR_MUTEX_ASSERT_ISLOCKED( &gcl->mutex );
	}

	if( EP_STAT_ISOK( estat ) ) *pgcl = gcl;


fail0:
	if( !EP_STAT_ISOK( estat ) ) {
		char		tebuf[100]; 

		ep_dbg_cprintf( Dbg, 2, "<<< kdc_gcl_open: %s \n", 
						ep_stat_tostr( estat, tebuf, sizeof tebuf) ); 
	}

	if( gcl == NULL ) {  // do nothing
	}else if( EP_STAT_ISOK( estat ) ) {
		_gdp_gcl_unlock( gcl );
	} else {
		_kdc_gcl_freehandle( gcl, true );
	}

	// unlock this operation to keep the GCL cache consistent
	ep_thr_mutex_unlock(&OpenMutex);

	return estat;
}


EP_STAT kdc_gcl_read( gdp_gcl_t *gcl, gdp_recno_t	recno, 
									gdp_datum_t *datum, char mode )
{
	EP_STAT						estat;

	ep_dbg_cprintf( Dbg, 39, "\n>>> kdc_read \n" );

	EP_ASSERT_POINTER_VALID( datum );
	estat = check_bad_kdc_gcl( gcl, "kdc_read", mode );
	if( !EP_STAT_ISOK( estat ) ) return estat;

	gdp_datum_reset( datum );
	datum->recno = recno ;

	_gdp_gcl_lock( gcl );
	estat = _kdc_gcl_read( gcl, datum, _GdpChannel, 0, mode ); 
	_gdp_gcl_unlock( gcl );

	kdc_prstat( estat, gcl, "kdc_read" );

	return estat;

}


EP_STAT kdc_gcl_read_ts( gdp_gcl_t *gcl, EP_TIME_SPEC *ts, 
									gdp_datum_t *datum, char mode )
{
	EP_STAT						estat;


	ep_dbg_cprintf( Dbg, 39, "\n>>> kdc_read_ts \n" );

	EP_ASSERT_POINTER_VALID( datum );
	estat = check_bad_kdc_gcl( gcl, "kdc_read_ts", mode );
	if( !EP_STAT_ISOK( estat ) ) return estat;

	datum->recno = GDP_PDU_NO_RECNO;
	memcpy( &datum->ts, ts, sizeof datum->ts );

	_gdp_gcl_lock( gcl );
	estat = _kdc_gcl_read( gcl, datum, _GdpChannel, 0, mode ); 
	_gdp_gcl_unlock( gcl );

	kdc_prstat( estat, gcl, "kdc_read_ts" );

	return estat;

}



EP_STAT kdc_gcl_read_async( gdp_gcl_t *gcl, gdp_recno_t recno, 
										gdp_event_cbfunc_t	cbfunc, 
										void *udata, char mode )
{
	EP_STAT						estat;


	ep_dbg_cprintf( Dbg, 39, "\n>>> kdc_read_ts \n" );

	estat = check_bad_kdc_gcl( gcl, "kdc_read_ts", mode );
	if( !EP_STAT_ISOK( estat ) ) return estat;

	_gdp_gcl_lock( gcl );
	estat = _kdc_gcl_read_async( gcl, recno, cbfunc, udata, 
											_GdpChannel, mode ); 
	_gdp_gcl_unlock( gcl );

	kdc_prstat( estat, gcl, "kdc_read_ts" );

	return estat;

}



EP_STAT kdc_gcl_subscribe( gdp_gcl_t *gcl, gdp_recno_t start, 
									int32_t	numrecs, EP_TIME_SPEC *timeout, 
									gdp_event_cbfunc_t	cbfunc, 
									void *udata, char mode )
{
	int							exit_status; 
	EP_STAT						estat;
	gdp_req_t					*req	= NULL;


	ep_dbg_cprintf( Dbg, 39, "\n>>> kdc_subscribe \n" );

	estat = check_bad_kdc_gcl( gcl, "kdc_subscribe", mode );
	if( !EP_STAT_ISOK( estat ) ) return estat;

	_gdp_gcl_lock( gcl );

	estat = _gdp_req_new( GDP_CMD_SUBSCRIBE, gcl, _GdpChannel, NULL, 
				GDP_REQ_PERSIST | GDP_REQ_CLT_SUBSCR | GDP_REQ_ALLOC_RID, 
							&req );
	EP_STAT_CHECK( estat, goto fail0 );

	req->cpdu->flags |= GDP_PDU_ASYNC_FLAG;
	req->cpdu->datum->recno = start;
	req->numrecs = numrecs;
	gdp_buf_put_uint32( req->cpdu->datum->dbuf, numrecs );

	if( timeout != NULL ) {
		gdp_buf_put_timespec( req->cpdu->datum->dbuf, timeout );
	}

	if( mode == 'S' ) {
		gdp_session		*curSession = NULL;

		curSession = ((sapnd_dt *)gcl->apndfpriv)->curSession;
		exit_status = update_smsg_onsession( req->cpdu, 
											curSession, mode, true );
		if( exit_status != EX_OK ) {
			ep_dbg_printf("[ERROR-S] Fail to handle smsg on session"
							" in kdc_subscribe\n %d: %s \n", 
							exit_status, str_errinfo( exit_status ) );
			estat = KDS_STAT_FAIL_SMSG;
			_gdp_req_free( &req );
			goto fail0;
		}
	}

	estat = _kdc_gcl_subscribe( req, cbfunc, udata, mode );


fail0:
	_gdp_gcl_unlock( gcl );

	kdc_prstat( estat, gcl, "kdc_subscribe" );

	return estat;

}


EP_STAT kdc_gcl_subscribe_ts( gdp_gcl_t *gcl, EP_TIME_SPEC *start, 
									int32_t	numrecs, EP_TIME_SPEC *timeout, 
									gdp_event_cbfunc_t	cbfunc, 
									void *udata, char mode )
{
	int							exit_status; 
	EP_STAT						estat;
	gdp_req_t					*req	= NULL;


	ep_dbg_cprintf( Dbg, 39, "\n>>> kdc_subscribe \n" );

	estat = check_bad_kdc_gcl( gcl, "kdc_subscribe", mode );
	if( !EP_STAT_ISOK( estat ) ) return estat;

	_gdp_gcl_lock( gcl );

	estat = _gdp_req_new( GDP_CMD_SUBSCRIBE, gcl, _GdpChannel, NULL, 
				GDP_REQ_PERSIST | GDP_REQ_CLT_SUBSCR | GDP_REQ_ALLOC_RID, 
							&req );
	EP_STAT_CHECK( estat, goto fail0 );

	req->cpdu->flags |= GDP_PDU_ASYNC_FLAG;
	memcpy( &req->cpdu->datum->ts, start, sizeof req->cpdu->datum->ts );
	req->numrecs = numrecs;
	gdp_buf_put_uint32( req->cpdu->datum->dbuf, numrecs );

	if( timeout != NULL ) {
		gdp_buf_put_timespec( req->cpdu->datum->dbuf, timeout );
	}

	if( mode == 'S' ) {
		gdp_session		*curSession = NULL;

		curSession = ((sapnd_dt *)gcl->apndfpriv)->curSession;
		exit_status = update_smsg_onsession( req->cpdu, 
											curSession, mode, true );
		if( exit_status != EX_OK ) {
			ep_dbg_printf("[ERROR-S] Fail to handle smsg on session"
							" in kdc_subscribe\n %d: %s \n", 
							exit_status, str_errinfo( exit_status ) );
			estat = KDS_STAT_FAIL_SMSG;
			_gdp_req_free( &req );
			goto fail0;
		}
	}

	estat = _kdc_gcl_subscribe( req, cbfunc, udata, mode );


fail0:
	_gdp_gcl_unlock( gcl );

	kdc_prstat( estat, gcl, "kdc_subscribe" );

	return estat;

}


// this function is called on Session mode 
// pre-processing rmsg on session 
// verify hmac & decrypt the rmsg 
int kdc_cb_preprocessor( gdp_event_t *gev)
{
	int				exit_status	= EX_OK;
	uint8_t			mode;
	gdp_gcl_t		*gcl		= NULL;
	gdp_session		*curSession	= NULL;


	gcl = gev->gcl;
	if( gcl == NULL )				return EX_NULL_GCL;
	if( gcl->apndfpriv == NULL )	return EX_NULL_SESSION;


	mode		= ((sapnd_dt*)gcl->apndfpriv)->mode;
	curSession	= ((sapnd_dt*)gcl->apndfpriv)->curSession;
	if( curSession == NULL	)		return EX_NULL_SESSION;

	exit_status = update_async_rmsg_onsession( gcl, curSession, 
												gev->datum, mode );

	if( exit_status != EX_OK ) {
		ep_dbg_printf("[ERROR-S] Fail to process rmsg on session in cb \n" 
						"[%d] %s \n", 
						exit_status, str_errinfo( exit_status ) );
	}

	return exit_status; 
}


EP_STAT kdc_gcl_append( gdp_gcl_t *gcl, gdp_datum_t *datum, char mode )
{
	EP_STAT						estat;


	estat = check_bad_kdc_gcl( gcl, "kdc_append", mode );
	if( !EP_STAT_ISOK( estat ) ) return estat;

	_gdp_gcl_lock( gcl );
	estat = _kdc_gcl_append( gcl, datum, _GdpChannel, 0, mode ); 
	_gdp_gcl_unlock( gcl );

	return estat;

}


EP_STAT kdc_gcl_append_async( gdp_gcl_t *gcl, gdp_datum_t *datum,
								gdp_event_cbfunc_t	cbfunc, 
								void *udata, char mode )
{
	EP_STAT						estat;


	estat = check_bad_kdc_gcl( gcl, "kdc_append_async", mode );
	if( !EP_STAT_ISOK( estat ) ) return estat;

	_gdp_gcl_lock( gcl );
	estat = _kdc_gcl_append_async( gcl, datum, cbfunc, udata,
												_GdpChannel, 0, mode ); 
	_gdp_gcl_unlock( gcl );

	return estat;

}





EP_STAT kdc_gcl_close( gdp_gcl_t *gcl, char mode )
{
	EP_STAT			estat;


	estat = check_bad_kdc_gcl( gcl, "kdc_close", mode );
	if( !EP_STAT_ISOK( estat ) ) return estat;


	ep_dbg_cprintf( Dbg, 19, "\n>>> kdc_close: %s \n", gcl->pname );
	estat = _kdc_gcl_close( gcl, mode, _GdpChannel, 0 );


	if( !EP_STAT_ISOK( estat ) ) {
		char		tebuf[100]; 

		ep_dbg_cprintf( Dbg, 2, "<<< kdc_close: %s \n", 
						ep_stat_tostr( estat, tebuf, sizeof tebuf) ); 
	}

	return estat;
}


// LATER ??? 
int kds_getmdtadata( )
{
	return EX_OK;
}


