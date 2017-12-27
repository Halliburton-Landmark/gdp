/* vim: set ai sw=4 sts=4 ts=4 : */

/*
** 
**	----- BEGIN LICENSE BLOCK -----
**	KEY Generation / Distribution Service Daemon 
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

// comment handling 
/*
**  KDS_PROTO -- ACTIONS for each command to Key distribution Service   
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 


/*
#include "logd_admin.h"

#include <gdp/gdp_gclmd.h>
#include <gdp/gdp_priv.h>
*/

#include <gdp/gdp_buf.h> 
#include <hs/hs_errno.h> 
#include <hs/gcl_helper.h> 
#include <hs/gdp_extension.h> 
#include "keydistd.h"
#include "ksd_data_manager.h"
#include "kds_pubsub.h"
#include "session_manager.h"



static EP_DBG	Dbg = EP_DBG_INIT("kds.proto", 
							"Key Distribution Service protocol");

/*
**	helper routine for returning errors
*/
EP_STAT
kds_gcl_error(gdp_name_t gcl_name, char *msg, EP_STAT logstat, EP_STAT estat)
{
	gdp_pname_t pname;

	gdp_printable_name(gcl_name, pname);
	if (EP_STAT_ISSEVERE(logstat))
	{
		// server error (rather than client error)
		ep_log(logstat, "%s: %s", msg, pname);
		if (!GDP_STAT_IS_S_NAK(logstat))
			logstat = estat;
	}
	else
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 1, "%s: %s: %s\n", msg, pname,
				ep_stat_tostr(logstat, ebuf, sizeof ebuf));
		if (!GDP_STAT_IS_C_NAK(logstat))
			logstat = estat;
	}
	return logstat;
}


/*
**  Flush information from an incoming datum
**
**		All commands should do this /before/ they write any return
**		values into the datum.  If where == NULL then data was expected
**		and will not be flagged.
*/
void flush_input_data(gdp_req_t *req, char *where)
{
	int i;
	gdp_datum_t *datum = req->cpdu->datum;


	if (datum == NULL)
		return;
	if (datum->dbuf != NULL && (i = gdp_buf_getlength(datum->dbuf)) > 0)
	{
		if (where != NULL)
			ep_dbg_cprintf(Dbg, 4,
			   "flush_input_data: %s: flushing %d bytes of unexpected input\n",
					where, i);
		gdp_buf_reset(datum->dbuf);
	}

	if (datum->sig != NULL)
	{
		i = gdp_buf_getlength(datum->sig);
		if (i > 0 && ep_dbg_test(Dbg, 4) && where != NULL)
			ep_dbg_printf( "flush_input_data: %s : "
						"flushing %d bytes of unexpected signature\n",
						where, i);
		if (datum->siglen != i)
				ep_dbg_cprintf(Dbg, 4, "    Warning: siglen = %d\n",
						datum->siglen);
		gdp_buf_reset(datum->sig);
	}
	else if (datum->siglen > 0)
		ep_dbg_cprintf(Dbg, 4, "flush_input_datum: no sig, but siglen = %d\n",
				datum->siglen);
	datum->siglen = 0;

}


/*
**  GET_STARTING_POINT --- get the starting point for a read or subscribe
*/
// multiread_ts/subscribe_ts:
//		starting time(req->cpdu->datum->ts), rec #(req->numrecs)
// multiread/subscribe: (0 req->numrecs ?)
//		starting rec(req->cpdu->datum->recno), rec #(req->numrecs)
// special record: datum->recno (0 recno means latest key) 
// special record_ts: datum->ts (the closest key after datum->ts) 

int get_starting_point(gdp_req_t *req, KSD_info *ksData)
{
	gdp_datum_t			*datum	= req->cpdu->datum;


	if (EP_TIME_IS_VALID(&datum->ts) && datum->recno <= 0)
	{
		// read by timestamp instead of record number
		datum->recno = request_recno_for_ts( ksData, datum );
	}
	else
	{
		// handle record numbers relative to the end
		if (datum->recno <= 0)
		{
			datum->recno += get_last_keyrec( ksData ) + 1;
			if (datum->recno <= 0)
			{
				// can't read before the beginning
				datum->recno = 1;
			}
		}
	}
	req->nextrec = req->cpdu->datum->recno;
	
	return datum->recno;
} 


/*
**	Command implementations
*/

EP_STAT
implement_me(char *s)
{
	ep_app_error("Not implemented: %s", s);
	return GDP_STAT_NOT_IMPLEMENTED;
}



int get_ksdinfo_from_md( gdp_gclmd_t *gmd, char *dName, int *dLen, 
				char *aName, int *aLen, char *kName, int *kLen, char *rw, 
											char *wid, int *widlen )
{
	int			exit_status = EX_OK;
	EP_STAT		estat;


	// data Log name to be served 
	estat = gdp_gclmd_find( gmd, GDP_GCLMD_DLOG, (size_t *)dLen, 
							(const void **)&dName );
	if( !EP_STAT_ISOK( estat ) ) return EX_UNAVAILABLE;

	// AC Log name to be used 
	estat = gdp_gclmd_find( gmd, GDP_GCLMD_ACLOG, (size_t *)aLen, 
				(const void **)&aName );
	if( !EP_STAT_ISOK( estat ) ) return EX_UNAVAILABLE;

	// Key Log name to be accessed 
	estat = gdp_gclmd_find( gmd, GDP_GCLMD_KLOG, (size_t *)kLen, 
				(const void **)&kName );
	if( !EP_STAT_ISOK( estat ) ) return EX_UNAVAILABLE;

	// Key Log name to be accessed 
	estat = gdp_gclmd_find( gmd, GDP_GCLMD_WDID, (size_t *)widlen, 
				(const void **)&wid );
	if( !EP_STAT_ISOK( estat ) ) return EX_UNAVAILABLE;

	// Key gen&distribution vs. Key distribution  
	exit_status = find_gdp_gclmd_char( gmd, GDP_GCLMD_KGEN, rw ); 

	return exit_status;
}



/***********************************************************************
**  GDP command implementations
**
**		Each of these takes a request as the argument.
**
**		These routines should set req->rpdu->cmd to the "ACK" reply
**		code, which will be used if the command succeeds (i.e.,
**		returns EP_STAT_OK).  Otherwise the return status is decoded
**		to produce a NAK code.  A specific NAK code can be sent
**		using GDP_STAT_FROM_NAK(nak).
**
**		All routines are expected to consume all their input from
**		the channel and to write any output to the same channel.
**		They can consume any unexpected input using flush_input_data.
**
***********************************************************************/


// print trace info about a command
#define CMD_TRACE(cmd, msg, ...)											\
			if (ep_dbg_test(Dbg, 20))										\
			{																\
				flockfile(ep_dbg_getfile());								\
				ep_dbg_printf("%s [%d]: ", _gdp_proto_cmd_name(cmd), cmd);	\
				ep_dbg_printf(msg, __VA_ARGS__);							\
				ep_dbg_printf("\n");										\
				funlockfile(ep_dbg_getfile());								\
			}


/*
**  CMD_PING --- just return an OK response to indicate that we are alive.
**
**		If this is addressed to a GCL (instead of the daemon itself),
**		it is really a test to see if the subscription is still alive.
*/

EP_STAT
cmd_ping(gdp_req_t *req)
{
	gdp_gcl_t *gcl;
	EP_STAT estat = EP_STAT_OK;

	req->rpdu->cmd = GDP_ACK_SUCCESS;
	flush_input_data(req, "cmd_ping");

	if (GDP_NAME_SAME(req->cpdu->dst, _GdpMyRoutingName))
		return EP_STAT_OK;

	gcl = _gdp_gcl_cache_get(req->rpdu->dst, GDP_MODE_RO);
	if (gcl != NULL)
	{
		// We know about the GCL.  How about the subscription?
		gdp_req_t *sub;

		LIST_FOREACH(sub, &req->gcl->reqs, gcllist)
		{
			if (GDP_NAME_SAME(sub->rpdu->dst, req->rpdu->dst) &&
					EP_UT_BITSET(GDP_REQ_SRV_SUBSCR, sub->flags))
			{
				// Yes, we have a subscription!
				goto done;
			}
		}
	}

	req->rpdu->cmd = GDP_NAK_S_LOSTSUB;		// lost subscription
	estat =  GDP_STAT_NAK_NOTFOUND;

done:
	_gdp_gcl_decref(&gcl);
	return estat;
}


struct ac_token* check_actoken( gdp_buf_t *inBuf ) 
{
	struct ac_token		*newToken = NULL;


	newToken = read_token_from_buf( inBuf ); 
	if(newToken == NULL ) return NULL;

	if( verify_actoken( newToken ) ) {
		// Pass the verification 
		newToken->info = _gdp_gclmd_deserialize( newToken->dbuf );
		gdp_buf_free( newToken->dbuf );
		ep_mem_free( newToken->sigbuf );
		newToken->dbuf = NULL;
		newToken->sigbuf = NULL;
		newToken->dlen = 0;
		newToken->siglen = 0;

		return newToken;

	} else {
		// Fail the verification 
		if( newToken != NULL ) free_token( newToken );
		return NULL;
	}

}


/*
**  CMD_CREATE --- Create Key Distribution Service(KDS) 
**						and new GCL for the KDS 
**
**	 Follow the existing GDP_CMD_CREATE policy. 
**		Like gdplog daemon, the pdu is addressed to the key service daemon. 
**		Also, the reponse indicates the name of KDS rather than the daemon. 
*/
EP_STAT
cmd_create(gdp_req_t *req)
{
	int					rLen; 
	int					exit_status;
	EP_STAT				estat = EP_STAT_OK;
	gdp_name_t			gclname;
	gdp_gcl_t			*gcl = NULL;
	gdp_gclmd_t			*gmd = NULL;
	struct ac_token		*token = NULL;


	// error handling 1
	if (!GDP_NAME_SAME(req->cpdu->dst, _GdpMyRoutingName))
	{
		// this is directed to a GCL, not to the daemon
		return kds_gcl_error(req->cpdu->dst, 
							"cmd_create: kds gcl name required",
							GDP_STAT_NAK_CONFLICT, GDP_STAT_NAK_BADREQ);
	}
	req->rpdu->cmd = GDP_ACK_CREATED;

	ep_thr_mutex_lock(&req->cpdu->datum->mutex);


	// 
	// 1. get the name of the new GCL
	// 
	rLen = gdp_buf_read(req->cpdu->datum->dbuf, gclname, sizeof gclname);
	if (rLen < sizeof gclname)
	{
		ep_dbg_cprintf(Dbg, 2, "cmd_create: kds gcl name required\n");
		estat = GDP_STAT_GCL_NAME_INVALID;
		req->rpdu->cmd = GDP_NAK_C_BADREQ;
		goto fail0;
	}

	// make sure we aren't creating a log with our name
	if (GDP_NAME_SAME(gclname, _GdpMyRoutingName))
	{
		estat = kds_gcl_error(gclname,
				"cmd_create: cannot create a kds with same name as daemon",
				GDP_STAT_GCL_NAME_INVALID, GDP_STAT_NAK_FORBIDDEN);
		req->rpdu->cmd = GDP_NAK_C_BADREQ;
		goto fail0;
	}

	{
		gdp_pname_t		tbuf;

		ep_dbg_cprintf(Dbg, 14, "cmd_create: creating KDS GCL %s\n",
				gdp_printable_name(gclname, tbuf));
	}

	if (GDP_PROTO_MIN_VERSION <= 2 && req->cpdu->ver == 2)
	{
		// change the request to seem to come from this GCL
		memcpy(req->rpdu->dst, gclname, sizeof req->rpdu->dst);
	}


	// 
	// 2. Check & Verify the access token 
	// 
	token = check_actoken( req->cpdu->datum->dbuf ); 
	if( token == NULL ) {
		gdp_pname_t		tname;

		// LATER: check error in using tname twice 
		ep_dbg_printf("[ERROR] cmd_create: Fail to check access token \n"
					  "\t from %s \n\t to %s \n", 
					  gdp_printable_name(req->cpdu->src, tname),  
					  gdp_printable_name(gclname, tname) ); 
		estat = GDP_STAT_NAK_FORBIDDEN;
		req->rpdu->cmd = GDP_NAK_C_UNAUTH;
		goto fail0;
	}


	// 
	// 3. Get the basic info for key distribution service 
	//      using the metadata format 
	gmd = _gdp_gclmd_deserialize(req->cpdu->datum->dbuf);

	char		rw_mode;
	char		*dname, *aname, *kname, *wid;
	int			dlen, alen, klen, widlen;
	KSD_info	*tksData = NULL; 


	dname = aname = kname = wid    = NULL;
	dlen  = alen  = klen  = widlen = 0;
	exit_status = get_ksdinfo_from_md( gmd, dname, &dlen, aname, &alen, 
						kname, &klen, &rw_mode, wid, &widlen );
	if( exit_status != EX_OK ) {
		estat = kds_gcl_error(gclname,
				"cmd_create: cannot create a kds without enough metadata ",
				GDP_STAT_PROTOCOL_FAIL, GDP_STAT_NAK_BADREQ);
		req->rpdu->cmd = GDP_NAK_C_BADREQ;
		goto fail0;
	}


	// 
	// 4. Prepare the data objects to handle the requested service 
	// 
	// get the KSD data object to serve the log. 
	exit_status = get_ksd_handle( gclname, &tksData, true, dname, dlen ); 	
	if( exit_status != EX_OK ) {
		estat = kds_gcl_error(gclname,
				"cmd_create: cannot prepare kds  ",
				GDP_STAT_INTERNAL_ERROR, GDP_STAT_NAK_INTERNAL);
		req->rpdu->cmd = GDP_NAK_S_INTERNAL;
		goto fail0;
	}



	// update the KSD data object 
	if( tksData->key_data == NULL ) {
		ACL_info		*ac_data  = NULL;
		LKEY_info		*key_data = NULL;


		strncpy( tksData->ac_pname,	 aname, alen );
		strncpy( tksData->key_pname, kname, klen );
		strncpy( tksData->wdid_pname, wid, widlen );
		tksData->ac_pname[alen] = '\0';
		tksData->key_pname[klen] = '\0';
		tksData->wdid_pname[widlen] = '\0';


		if( update_ac_node(tksData, aname, alen) != EX_OK ) {
			cancel_ksd_handle( tksData, true );
			estat = kds_gcl_error(gclname,
					"cmd_create: cannot prepare kds  ",
					GDP_STAT_INTERNAL_ERROR, GDP_STAT_NAK_INTERNAL);
			req->rpdu->cmd = GDP_NAK_S_INTERNAL;
			goto fail0;
		}
		if( update_key_node(tksData, kname, klen, rw_mode) != EX_OK ) {
			cancel_ksd_handle( tksData, true );
			estat = kds_gcl_error(gclname,
					"cmd_create: cannot prepare kds  ",
					GDP_STAT_INTERNAL_ERROR, GDP_STAT_NAK_INTERNAL);
			req->rpdu->cmd = GDP_NAK_S_INTERNAL;
			goto fail0;
		}

		if( tksData->ac_data != NULL ) {
			LIST_INSERT_HEAD( &(((ACL_info *)(tksData->ac_data->nval))->skeys), 
						(LKEY_info *)(tksData->key_data->nval), klist ); 
		}


		exit_status = EX_OK; 

		if( exit_status != EX_OK ) {
			cancel_ksd_handle( tksData, true );
			estat = GDP_STAT_INTERNAL_ERROR;
			req->rpdu->cmd = GDP_NAK_S_INTERNAL;
			goto fail0;
		} 


		// Error check for log names for ac & Load the AC rules if necessary  
		ac_data = (ACL_info *)(tksData->ac_data->nval);
		exit_status = load_ac_rules( ac_data ); 
		if( exit_status == EX_NOINPUT ) {
			//Not avaiable AC log 
			cancel_ksd_handle( tksData, true );
			estat = GDP_STAT_GCL_NOT_OPEN;
			req->rpdu->cmd = GDP_NAK_C_BADREQ;
			goto fail0;
		}

		tksData->state = DONE_INIT;

		// Error check for log names for key 
		// if necessary, make the key log  
		key_data = (LKEY_info *)(tksData->key_data->nval);
		exit_status = check_keylogname( key_data );
		if( exit_status == EX_NOTFOUND ) {
			// MUST create the key log 
			exit_status = create_key_log( tksData );
			tksData->state = DOING_INIT;

		} else if( exit_status != EX_OK ) {
			//Not avaiable key log 
			cancel_ksd_handle( tksData, true );
			estat = GDP_STAT_GCL_NOT_OPEN;
			req->rpdu->cmd = GDP_NAK_C_BADREQ;
			goto fail0;

		} else load_key_data( tksData, 0 ); 

		refresh_ks_info_file();

	} else tksData->isDuplicated = true;


	// 
	// At this point, key_data->gcl != NULL 
	//		load the ac data & key data ... 
	// 

	if( tksData->gcl != NULL ) {
		gcl = tksData->gcl;
		req->rpdu->cmd = GDP_ACK_SUCCESS;
		req->gcl = gcl;
		gcl->flags |= GCLF_DEFER_FREE;
		goto fail0;
		// don't need advertise.. 
	}


	// get the memory space for the GCL itself
	estat = kds_gcl_alloc(gclname, GDP_MODE_AO, &gcl, tksData);
	EP_STAT_CHECK(estat, goto fail0);
	tksData->gcl =gcl;  // LATER check NOT NULL pre gcl case. 

	_gdp_gcl_lock(gcl);
	req->gcl = gcl;			// for debugging

	// assume both read and write modes
	gcl->iomode = GDP_MODE_RA;

	// no further input, so we can reset the buffer just to be safe
	flush_input_data(req, "cmd_create");
	
	//key_data = (LKEY_info *)(tksData->key_data->nval);
	//gcl->gmd = key_data->gmd;
	//gcl->nrecs = key_data->last_recn;
	//update_ksd_gcl( gcl, tksData );


	// advertise this new GCL :: delay 
	kds_advertise_one(gcl->name, GDP_CMD_ADVERTISE);

	// cache the open GCL Handle for possible future use
	EP_ASSERT(gdp_name_is_valid(gcl->name));
	_gdp_gcl_cache_add(gcl, gcl->iomode);

	// pass any creation info back to the caller
	// (none at this point)

	// leave this in the cache
	gcl->flags |= GCLF_DEFER_FREE;
	_gdp_gcl_decref(&req->gcl);


	// exit routine of this function 
fail0:
	ep_thr_mutex_unlock(&req->cpdu->datum->mutex);

	if( gcl==NULL && gmd!=NULL ) gdp_gclmd_free( gmd );

	if( req->rpdu->cmd != GDP_ACK_CREATED ) {
		// error case 
		free_token( token );

	}

	if( ep_dbg_test(Dbg, 9) )
	{
		char ebuf[100];

		if( gcl != NULL ) 
			ep_dbg_printf("<<< KDS_cmd_create(%s): %s\n", gcl->pname,
							ep_stat_tostr(estat, ebuf, sizeof ebuf));
		else 
			ep_dbg_printf("<<< KDS_cmd_create(name-err): %s\n", 
							ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}


	return estat;
}



/*
**  CMD_OPEN --- open for read-only, append-only, or read-append
*/
EP_STAT cmd_open(gdp_req_t *req)
{
	EP_STAT				estat  = EP_STAT_OK;
	gdp_gcl_t			*gcl   = NULL;
	struct ac_token		*token = NULL;
	KSD_info			*ksData = NULL;
	gdp_pname_t			tname;


	req->rpdu->cmd = GDP_ACK_SUCCESS;

	ep_thr_mutex_lock(&req->rpdu->datum->mutex);

	// LATER: can be changed the session crypto context with session->epoch 

	// 
	// 1. Check & Verify the access token 
	// 
	token = check_actoken( req->cpdu->datum->dbuf ); 
	if( token == NULL ) {
		// LATER: check error in using tname twice 
		ep_dbg_printf("[ERROR] cmd_open: Fail to check access token \n"
					  "\t from %s \n\t to %s \n", 
					  gdp_printable_name(req->cpdu->src, tname),  
					  gdp_printable_name(req->cpdu->dst, tname) ); 
		estat = GDP_STAT_NAK_FORBIDDEN;
		req->rpdu->cmd = GDP_NAK_C_UNAUTH;
		goto fail0;
	}


	// 
	// 2. Find the Key Service handle to be requested 
	// 
	estat = get_open_handle(req, GDP_MODE_ANY);
	if( !EP_STAT_ISOK(estat) )
	{
		estat = kds_gcl_error(req->cpdu->dst, "cmd_open: could not open GCL",
							estat, GDP_STAT_NAK_INTERNAL);
		req->rpdu->cmd = GDP_NAK_S_INTERNAL;
		goto fail0;
	}
	gcl = req->gcl;
	ksData = (KSD_info *)(gcl->apndfpriv); 


	// 
	// 3. Authorization  based on access control policy 
	//	LATER: consider mode 
	// 
	if( !isAllowedAccess( ksData, token->info, 'r' ) ) {
		// LATER: check error in using tname twice 
		ep_dbg_printf("[ERROR] cmd_open: Not Allowed Access \n"
					  "\t from %s \n\t to %s \n", 
					  gdp_printable_name(req->cpdu->src, tname),  
					  gdp_printable_name(req->cpdu->dst, tname) ); 
		estat = GDP_STAT_NAK_FORBIDDEN;
		req->rpdu->cmd = GDP_NAK_C_UNAUTH;
		goto fail0;
	}


	// 
	// 4. Create the session info   
	// 
	gdp_session		*sinfo = NULL;
	sinfo = create_session_info( ksData, token, req ); 
	if( sinfo == NULL ) {
		estat = GDP_STAT_NAK_INTERNAL; 
		req->rpdu->cmd = GDP_NAK_S_INTERNAL;
		goto fail0;
	}

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_open");


	// 
	// 5. Manage the internal data in GCL & ksdata 
	// 
	gcl->flags |= GCLF_DEFER_FREE;
	gcl->iomode = GDP_MODE_RA;

	
	// 
	// 6. Make Response Data 
	// 
	LKEY_info		*keylog = (LKEY_info *)(ksData->key_data->nval);
	if( keylog->gmd != NULL)
	{
		// send metadata as payload
		_gdp_gclmd_serialize( keylog->gmd, req->rpdu->datum->dbuf);
	}
	req->rpdu->datum->recno = keylog->last_recn;

	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];
		ep_dbg_printf("<<< cmd_open(%s): gcl %p nrecs %" PRIgdp_recno ": %s\n",
					gcl->pname, gcl, gcl->nrecs,
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}


	// 
	// 7. Encrypt the data based according to the session status  
	// 
	if( update_response_onsession( req->rpdu, sinfo )  != EX_OK ) {
		ep_dbg_printf("[ERROR] update_response in cmd_open \n");
		estat = GDP_STAT_INTERNAL_ERROR;
		req->rpdu->cmd = GDP_NAK_S_INTERNAL;
	}

fail0:
	ep_thr_mutex_unlock(&req->rpdu->datum->mutex);
	_gdp_gcl_decref(&req->gcl);

/*
	if( req->rpdu->cmd != GDP_ACK_SUCCESS ) {
		if( token != NULL ) free_token( token );
	}
*/
	if( token != NULL ) free_token( token );

	return estat;
}


/*
**  CMD_CLOSE --- close an open GCL
**
**		XXX	Since GCLs are shared between clients you really can't just
**			close things willy-nilly.  Thus, this is currently a no-op
**			until such time as reference counting works.
**
**		XXX	We need to have a way of expiring unused GCLs that are not
**			closed.
*/

EP_STAT
cmd_close(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;

	req->rpdu->cmd = GDP_ACK_SUCCESS;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_close");

	// a bit wierd to open the GCL only to close it again....
	estat = get_open_handle(req, GDP_MODE_ANY);
	if (!EP_STAT_ISOK(estat))
	{
		return kds_gcl_error(req->cpdu->dst, "cmd_close: GCL not open",
							estat, GDP_STAT_NAK_BADREQ);
	}
/*
	// remove any subscriptions
	sub_end_all_subscriptions(req->gcl, req->cpdu->src);
*/
	//return number of records
	req->rpdu->datum->recno = req->gcl->nrecs;

	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];
		ep_dbg_printf("<<< cmd_close(%s): %s\n", req->gcl->pname,
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}


	// drop reference
	_gdp_gcl_decref(&req->gcl);

	return estat;
}


#define PUT64(v) \
		{ \
			*pbp++ = ((v) >> 56) & 0xff; \
			*pbp++ = ((v) >> 48) & 0xff; \
			*pbp++ = ((v) >> 40) & 0xff; \
			*pbp++ = ((v) >> 32) & 0xff; \
			*pbp++ = ((v) >> 24) & 0xff; \
			*pbp++ = ((v) >> 16) & 0xff; \
			*pbp++ = ((v) >> 8) & 0xff; \
			*pbp++ = ((v) & 0xff); \
		}

//static EP_STAT init_sig_digest(gdp_gcl_t *gcl);




/*
**  CMD_READ --- read a single record from a GCL
**
**		This returns the data as part of the response.  To get multiple
**		values in one call, see cmd_subscribe.
*/
EP_STAT
cmd_read(gdp_req_t *req)
{
	int					stat;
	int					dlen		= 0;
	EP_STAT				estat		= EP_STAT_OK;
	KSD_info			*ksData		= NULL;
	gdp_session			*curSession = NULL;
	gdp_datum_t			*rdatum		= NULL;



	req->rpdu->cmd = GDP_ACK_CONTENT;


	//
	// 1. Find the related GCL handle 
	//
	estat = get_open_handle(req, GDP_MODE_RO);
	if( !EP_STAT_ISOK(estat) )
	{
		if( EP_STAT_IS_SAME( estat, GDP_STAT_NOTFOUND ) ) goto fail0;
		else goto fail1; 
	}
	ksData = (KSD_info *)(req->gcl->apndfpriv);


	//
	// 2. Find the related session info  / Check basic session info
	//
	curSession = lookup_session( ksData, req);
	if( curSession == NULL ) {
		req->rpdu->cmd = GDP_NAK_C_BADREQ; // C_NOTFOUND 
		_gdp_gcl_decref(&req->gcl);
		return kds_gcl_error(req->cpdu->dst, "cmd_read: Session not open",
							GDP_STAT_NOTFOUND, GDP_STAT_NAK_BADREQ);
	}


/*  where: here or sub-functions
	// 
	// 3. Recheck the access control rule because of time difference 
	//	LATER: consider mode 
	// 
	if( !isAllowedAccess( ksData, curSession->ac_info->info, 'r' ) ) {
		// LATER: check error in using tname twice 
		ep_dbg_printf("[ERROR] cmd_open: Not Allowed Access \n"
					  "\t from %s \n\t to %s \n", 
					  gdp_printable_name(req->cpdu->src, tname),  
					  gdp_printable_name(req->cpdu->dst, tname) ); 
		req->rpdu->cmd = GDP_NAK_C_UNAUTH;
		_gdp_gcl_decref(&req->gcl);
		return GDP_STAT_NAK_FORBIDDEN;
	}
*/

	// recno: datum->recno (recno==0: latest key)  
	// read_ts : datum->ts  (starting time / rec num) 
	//		both of them does not have any data in datum->buf 

	rdatum	= req->cpdu->datum;
	dlen	= gdp_buf_getlength( rdatum->dbuf ); 
	if( dlen > 0 ) {
		// check whether this request is sent from the writer.. 
		ep_dbg_printf("RCV msg (%d) in cmd_read \n", dlen ); 
	} 

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_read");


	//
	// 3. Find the requested key info 
	//
// special record: datum->recno (0 recno means latest key) 
// special record_ts: datum->ts (the closest key after datum->ts) 
	ep_thr_mutex_lock(&req->rpdu->datum->mutex);
	if( rdatum->recno<0 )  {
		if( EP_TIME_IS_VALID(&rdatum->ts) ) {
			// next rec = req->rpdu->datum->recno + 1;
			stat = request_key_ts( ksData, req->rpdu->datum, 
									curSession->ac_info ); 
		} else {
			stat = EX_INVALIDDATA;
		}
	} else if( rdatum->recno==0 ) {
		// next rec = req->rpdu->datum->recno + 1;
		stat = request_key_latest( ksData, req->rpdu->datum,  
									curSession->ac_info ); 

	} else { // sepcific exit stat 
		req->rpdu->datum->recno = req->cpdu->datum->recno;
		stat = request_key_rec( ksData, req->rpdu->datum,   
									curSession->ac_info ); 
	}
//	CMD_TRACE(req->cpdu->cmd, "%s %" PRIgdp_recno,
//			req->gcl->pname, req->cpdu->datum->recno);

	// 
	// 4. Encrypt the data based according to the session status  
	// 
	if( stat == EX_OK ) {
		// rpdu->datum->recno has to be set already. 
		// data is set in above request_key_* functions 
		if( update_response_onsession( req->rpdu, curSession )  != EX_OK ) {
			ep_dbg_printf("[ERROR] update_response in cmd_read \n");
			estat = GDP_STAT_INTERNAL_ERROR;
			req->rpdu->cmd = GDP_NAK_S_INTERNAL;
		}

	} else {
		req->rpdu->datum->recno = req->cpdu->datum->recno;

		// according to the stat, set rpdu->cmd, estat  
		if( stat == EX_UNAVAILABLE ) {
			req->rpdu->cmd = GDP_NAK_C_NOTACCEPTABLE;
			estat = GDP_STAT_NAK_NOTACCEPTABLE; 
		} 
	}
	ep_thr_mutex_unlock(&req->rpdu->datum->mutex);

/*
	// deliver "record expired" as "gone" and "record missing" as "not found"
	if (EP_STAT_IS_SAME(estat, GDP_STAT_RECORD_EXPIRED))
		estat = GDP_STAT_NAK_GONE;
	if (EP_STAT_IS_SAME(estat, GDP_STAT_RECORD_MISSING))
		estat = GDP_STAT_NAK_NOTFOUND;
*/
	_gdp_gcl_decref(&req->gcl);
	return estat;

fail0:
	req->rpdu->cmd = GDP_NAK_C_NOTFOUND;
	return kds_gcl_error(req->cpdu->dst, "cmd_read: GCL open failture", 
							estat, GDP_STAT_NOTFOUND );

fail1:
	req->rpdu->cmd = GDP_NAK_S_INTERNAL;
	return kds_gcl_error(req->cpdu->dst, "cmd_read: GCL open failture", 
							estat, GDP_STAT_NAK_INTERNAL);
}


/*
**  Initialize the digest field
**
**		This needs to be done during the append rather than the open
**		so if gdplogd is restarted, existing connections will heal.
*/
// LATER check (secure channel ) 
/*
static EP_STAT
init_sig_digest(gdp_gcl_t *gcl)
{ 
	EP_STAT estat;
	size_t pklen;
	uint8_t *pkbuf;
	int pktype;
	int mdtype;
	EP_CRYPTO_KEY *key;

	if (gcl->digest != NULL)
		return EP_STAT_OK;

	// assuming we have a public key, set up the message digest context
	if (gcl->gclmd == NULL)
		goto nopubkey;
	estat = gdp_gclmd_find(gcl->gclmd, GDP_GCLMD_PUBKEY, &pklen,
					(const void **) &pkbuf);
	if (!EP_STAT_ISOK(estat) || pklen < 5)
		goto nopubkey;

	mdtype = pkbuf[0];
	pktype = pkbuf[1];
	//pkbits = (pkbuf[2] << 8) | pkbuf[3];
	ep_dbg_cprintf(Dbg, 40, 
			"init_sig_data: mdtype=%d, pktype=%d, pklen=%zd\n",
			mdtype, pktype, pklen);
	key = ep_crypto_key_read_mem(pkbuf + 4, pklen - 4,
			EP_CRYPTO_KEYFORM_DER, EP_CRYPTO_F_PUBLIC);
	if (key == NULL)
		goto nopubkey;

	gcl->digest = ep_crypto_vrfy_new(key, mdtype);

	// include the GCL name
	ep_crypto_vrfy_update(gcl->digest, gcl->name, sizeof gcl->name);

	// and the metadata (re-serialized)
	gdp_buf_t *evb = gdp_buf_new();
	_gdp_gclmd_serialize(gcl->gclmd, evb);
	size_t evblen = gdp_buf_getlength(evb);
	ep_crypto_vrfy_update(gcl->digest, 
					gdp_buf_getptr(evb, evblen), evblen);
	gdp_buf_free(evb);

	if (false)
	{
nopubkey:
		if (EP_UT_BITSET(GDP_SIG_PUBKEYREQ, GdpSignatureStrictness))
		{
			ep_dbg_cprintf(Dbg, 1, "ERROR: no public key for %s\n",
						gcl->pname);
			estat = GDP_STAT_CRYPTO_SIGFAIL;
		}
		else
		{
			ep_dbg_cprintf(Dbg, 52, "WARNING: no public key for %s\n",
						gcl->pname);
			estat = EP_STAT_OK;
		}
	}

	return estat; 
}
*/


void cb_acload(int fd, short what, void *data)
{
	gdp_pname_t		pname;
	ACL_info		*acinfo = (ACL_info *)data;

	if( acinfo == NULL ) {
		ep_app_error("Timeout Call : cb_acload BUT no data ");
		return ;
	}

	gdp_printable_name( acinfo->aclog_iname, pname);
	ep_dbg_printf("Timeout Call: cb_acload for %s \n", pname );

	if( load_ac_rules( acinfo ) != EX_OK ) {
		long	retry_intvl = 300;  // 5 min ? 

		struct event *retryTimer = event_new( GdpIoEventBase, -1, 
					EV_TIMEOUT, &cb_acload, (void *)acinfo);
		struct timeval tv  = { retry_intvl, 0 };
		event_add( retryTimer, &tv );
	}

}

EP_STAT load_ksdata( gdp_gcl_t *gcl , int inNum) 
{
	KSD_info			*ksData = NULL;
	ACL_info			*acinfo  = NULL;
	LKEY_info			*kinfo  = NULL;

	ksData = (KSD_info *)(gcl->apndfpriv);
	if( ksData == NULL || ksData->key_data == NULL || 
										ksData->ac_data == NULL ) {
		ep_dbg_printf("ERROR: Do Not Exist the related ks data "
						"on loading data for %s \n", gcl->pname );
		return GDP_STAT_NOTFOUND;
	}

	kinfo  = (LKEY_info *)(ksData->key_data->nval);
	acinfo = (ACL_info  *)(ksData->ac_data->nval);
	if( kinfo == NULL || acinfo == NULL ) {
		ep_dbg_printf("ERROR: Do Not load the related key data "
						"on loading data for %s \n", gcl->pname );
		return GDP_STAT_NOTFOUND;
	}

	if( acinfo->gcl == NULL ) {
		// retry 
		if( load_ac_rules( acinfo ) != EX_OK ) {
			long	retry_intvl = 300;  // 5 min ? 

			struct event *retryTimer = event_new( GdpIoEventBase, -1, 
						EV_TIMEOUT, &cb_acload, (void *)acinfo);
			struct timeval tv  = { retry_intvl, 0 };
			event_add( retryTimer, &tv );

		}
	}


	if( kinfo->gcl == NULL ) load_key_data( ksData, inNum ) ;


	return EP_STAT_OK;

}


EP_STAT reflect_apnd_msg( gdp_gcl_t *gcl, gdp_datum_t *rDatum )
{
	int					dlen    = 0;
	unsigned char		*rdata  = NULL;
	KSD_info			*ksData = NULL;
	KGEN_param			*kparam = NULL;	
	LKEY_info			*kinfo  = NULL;


	ksData = (KSD_info *)(gcl->apndfpriv);
	if( ksData == NULL || ksData->key_data == NULL ) {
		ep_dbg_printf("ERROR: Do Not Exist the related ks data "
						"on reflecting param for %s \n", gcl->pname );
		return GDP_STAT_NOTFOUND;
	}
	kinfo = (LKEY_info *)(ksData->key_data->nval);
	if( kinfo == NULL ) {
		ep_dbg_printf("ERROR: Do Not Exist the related key data "
						"on reflecting param for %s \n", gcl->pname );
		return GDP_STAT_NOTFOUND;
	}


	dlen = gdp_buf_getlength( rDatum->dbuf ); 
	rdata = gdp_buf_getptr( rDatum->dbuf, dlen );
	if( dlen == 5 ) {
		// LATER: close command 
		if( strncmp( (const char*)rdata, "close", 5 ) == 0 ) { 
			// service close request 
			return GDP_STAT_NAK_GONE;
		}
	}
	if( rdata == NULL || dlen != 28 ) {
		ep_dbg_printf("ERROR: Apended kparam data (%d) \n", dlen );
		return GDP_STAT_DATUM_REQUIRED;
	}


	kparam = convert_to_kgen_param( rdata );
	if( kparam == NULL ) {
		ep_dbg_printf("ERROR: Fail to manage kparam \n"  );
		return GDP_STAT_INTERNAL_ERROR;
	}

	kinfo->param = kparam;
	dlen = store_kgen_param( kinfo->kgen_inx, kparam );
	if( dlen != EX_OK ) {
		ep_dbg_printf("ERROR: Fail to store kparam at %d\n", 
						kinfo->kgen_inx );
		return GDP_STAT_INTERNAL_ERROR;
	}


	return EP_STAT_OK;
}



/*
**  CMD_APPEND --- send the kgen param info (new / change)
**		can be sent from service with creation capability 
*/


EP_STAT cmd_append(gdp_req_t *req)
{ 
	int					rval;
	EP_STAT				estat		= EP_STAT_OK;
	KSD_info			*ksData		= NULL;
	gdp_gcl_t			*gcl		= NULL;
	gdp_session			*curSession	= NULL;


	req->rpdu->cmd = GDP_ACK_SUCCESS;

	//
	// 1. Find the related GCL handle 
	//
	estat = get_open_handle(req, GDP_MODE_AO);
	if (!EP_STAT_ISOK(estat))
	{
		req->rpdu->cmd = GDP_NAK_C_BADREQ; // C_NOTFOUND 
		return kds_gcl_error(req->cpdu->dst, "cmd_append: GCL not open",
							estat, GDP_STAT_NAK_BADREQ);
	}
//	CMD_TRACE(req->cpdu->cmd, "%s %" PRIgdp_recno,
//			req->gcl->pname, req->cpdu->datum->recno);

	gcl = req->gcl;
	ksData = (KSD_info *)(gcl->apndfpriv); 


	//
	// 2. Find the related session info  / Check basic session info
	//
	curSession = lookup_session( ksData, req);
	if( curSession == NULL ) {
		req->rpdu->cmd = GDP_NAK_C_BADREQ; // C_NOTFOUND 
		_gdp_gcl_decref(&req->gcl);
		return kds_gcl_error(req->cpdu->dst, "cmd_append: Session not open",
							GDP_STAT_NOTFOUND, GDP_STAT_NAK_BADREQ);
	}
	if( curSession->mode != 'c' || curSession->mode != 'b' ) {
		req->rpdu->cmd = GDP_NAK_C_UNAUTH;
		_gdp_gcl_decref(&req->gcl);
		return GDP_STAT_NAK_FORBIDDEN;
	}

	ep_thr_mutex_lock(&req->cpdu->datum->mutex);

	//
	// 3. Check the rcv msg on session 
	//    - verify HMAC / DECRYPT data  --> cpdu->datum->dbuf 
	//		- session state 
	rval = check_reqdbuf_onsession( req->cpdu,  curSession );
	if( rval != EX_OK ) {
		req->rpdu->cmd = GDP_NAK_C_UNAUTH;
		ep_thr_mutex_unlock(&req->cpdu->datum->mutex);
		_gdp_gcl_decref(&req->gcl);
		return GDP_STAT_NAK_FORBIDDEN;
	}


	// 
	// 4. Reflect the received msg 
	// 
	estat = reflect_apnd_msg( req->gcl, req->cpdu->datum );
	if( EP_STAT_ISOK(estat ) ) estat = load_ksdata( req->gcl, -1 ); 

	if( !EP_STAT_ISOK(estat) ) {
		if( EP_STAT_IS_SAME( estat, GDP_STAT_NOTFOUND ) )
			req->rpdu->cmd = GDP_NAK_C_NOTFOUND;
		else if( EP_STAT_IS_SAME( estat, GDP_STAT_NOTFOUND )  )
			req->rpdu->cmd = GDP_NAK_S_INTERNAL;
		else
			req->rpdu->cmd = GDP_NAK_C_NOTACCEPTABLE;

	} else ksData->state = DONE_INIT; // change 
	// if ksData->doing_init && rw_mode && acinfo --> done_init 
	// later:  need_all need_ac need_ks done_init  


	// return the actual last record number (even on error)
	// req->rpdu->datum->recno = req->gcl->nrecs; 

	// we can now drop the data and signature in the command request
	gdp_buf_reset(req->cpdu->datum->dbuf);
	if (req->cpdu->datum->sig != NULL)
		gdp_buf_reset(req->cpdu->datum->sig);
	req->cpdu->datum->siglen = 0;

	// we're no longer using this handle
	ep_thr_mutex_unlock(&req->cpdu->datum->mutex);
	_gdp_gcl_decref(&req->gcl);


	// No response data 

	return estat;  

}


/*
**  POST_SUBSCRIBE --- do subscription work after initial ACK
**
**		Assuming the subscribe worked we are now going to deliver any
**		previously existing records.  Once those are all sent we can
**		convert this to an ordinary subscription.  If the subscribe
**		request is satisified, we remove it.
**
**		This code is also the core of multiread.
*/
void post_subscribe(gdp_req_t *req)
{ 
	int					last_rec, stat;
	EP_STAT				estat;
	KSD_info			*ksData		= NULL;
	gdp_session			*curSession	= NULL;



	EP_ASSERT_ELSE(req != NULL, return);
	EP_ASSERT_ELSE(req->state != GDP_REQ_FREE, return);
	ep_dbg_printf(
			"post_subscribe: numrecs = %d, nextrec = %"PRIgdp_recno"\n",
			req->numrecs, req->nextrec);

	if (req->rpdu == NULL)
		req->rpdu = _gdp_pdu_new();

	// make sure the request has the right responses code
	req->rpdu->cmd = GDP_ACK_CONTENT;

	ksData		= (KSD_info *)(req->gcl->apndfpriv);
	curSession	= req->udata;


	while (req->numrecs >= 0)
	{

		EP_ASSERT_ELSE(req->gcl != NULL, break);

		// see if data pre-exists in the GCL
		last_rec = get_last_keyrec( ksData );
		if( req->nextrec > last_rec )
		{
			// no, it doesn't; convert to long-term subscription
			break;
		}

		// 
		// 1. Find the key info and put the info in req->datum  
		// get the next record and return it as an event
		req->rpdu->datum->recno = req->nextrec;
		stat = request_key_rec( ksData, req->rpdu->datum, 
									curSession->ac_info );

		// 
		// 2. Encrypt the data based according to the session status  
		// 
		if( stat == EX_OK ) {
			if( update_response_onsession( req->rpdu, curSession )  != EX_OK ) {
				ep_dbg_printf("[ERROR] update_response in post_sub \n");
				estat = GDP_STAT_INTERNAL_ERROR;
				req->rpdu->cmd = GDP_NAK_S_INTERNAL;
				req->numrecs = -1;	// terminate subscription

				// break ? or not... 
			} else req->rpdu->cmd = GDP_ACK_CONTENT;

		} else if( stat == EX_NOTFOUND) {
			req->rpdu->cmd = GDP_NAK_C_REC_MISSING;
			estat = EP_STAT_OK;
		} 

		// send the PDU out
		req->stat = estat = _gdp_pdu_out(req->rpdu, req->chan, NULL);

		// have to clear the old data and signature
		gdp_buf_reset(req->rpdu->datum->dbuf);
		if (req->rpdu->datum->sig != NULL)
			gdp_buf_reset(req->rpdu->datum->sig);
		req->rpdu->datum->siglen = 0;


		// advance to the next record
		if (req->numrecs > 0 && --req->numrecs == 0)
		{ // LATER : check: --rq->numrecs 
			// numrecs was positive, now zero, but zero means infinity
			req->numrecs--;
		}
		req->nextrec++;

		// if we didn't successfully send a record, terminate
		EP_STAT_CHECK(estat, break);

		// DEBUG: force records to be interspersed
		if (ep_dbg_test(Dbg, 101))
			ep_time_nanosleep(1 MILLISECONDS);
	}


	if (req->numrecs < 0 || !EP_UT_BITSET(GDP_REQ_SUBUPGRADE, req->flags))
	{
		// no more to read: do cleanup & send termination notice
		// where is lockec req->mutex, req->gcl->mutex 
		//	: in gdp_main.c  (proc_cmd)
		sub_end_subscription(req);
	}
	else
	{
		ep_dbg_cprintf(Dbg, 24, "post_subscribe: converting to subscription\n");
		req->flags |= GDP_REQ_SRV_SUBSCR;

		// link this request into the GCL so the subscription can be found
		if (!EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags))
		{
			IF_LIST_CHECK_OK(&req->gcl->reqs, req, gcllist, gdp_req_t)
			{
				_gdp_gcl_incref(req->gcl);		//DEBUG: is this appropriate?
				LIST_INSERT_HEAD(&req->gcl->reqs, req, gcllist);
				req->flags |= GDP_REQ_ON_GCL_LIST;
			}
		}
	} 


}


/*
**  CMD_SUBSCRIBE --- subscribe command
**
**		Arranges to return existing data (if any) after the response
**		is sent, and non-existing data (if any) as a side-effect of
**		append.
**
**		XXX	Race Condition: if records are written between the time
**			the subscription and the completion of the first half of
**			this process, some records may be lost.  For example,
**			if the GCL has 20 records (1-20) and you ask for 20
**			records starting at record 11, you probably want records
**			11-30.  But if during the return of records 11-20 another
**			record (21) is written, then the second half of the
**			subscription will actually return records 22-31.
**
**		XXX	Does not implement timeouts.
*/

// multiread_ts/subscribe_ts: ==> cmd_subscribe
//		starting time(req->cpdu->datum->ts), rec #(req->numrecs)
// multiread/subscribe: (0 req->numrecs ?)
//		starting rec(req->cpdu->datum->recno), rec #(req->numrecs)
// req->numrecs is stored in datum->dbuf. 
// last time can be stored in datum->dbuf (also) 
EP_STAT
cmd_subscribe(gdp_req_t *req)
{ 
	int					stat;
	EP_STAT				estat;
	KSD_info			*ksData		= NULL;
	gdp_pname_t			tname;
	gdp_session			*curSession	= NULL;
//	gdp_datum_t			*rdatum		= NULL;
	EP_TIME_SPEC		timeout;



	// LATER CHECK:  at debugging 
	if (req->gcl != NULL)
		EP_THR_MUTEX_ASSERT_ISLOCKED(&req->gcl->mutex);

	req->rpdu->cmd = GDP_ACK_SUCCESS;


	//
	// 1. Find the related GCL handle 
	//
	estat = get_open_handle(req, GDP_MODE_RO);
	if( !EP_STAT_ISOK(estat) )
	{
		if( EP_STAT_IS_SAME( estat, GDP_STAT_NOTFOUND ) ) goto fail0;
		else goto fail1; 
	}
	ksData = (KSD_info *)(req->gcl->apndfpriv);


	//
	// 2. Find the related session info  / Check basic session info
	//
	curSession = lookup_session( ksData, req);
	if( curSession == NULL ) {
		req->rpdu->cmd = GDP_NAK_C_BADREQ; // C_NOTFOUND 
		_gdp_gcl_decref(&req->gcl);
		return kds_gcl_error(req->cpdu->dst, "cmd_subscribe: Session not open",
							GDP_STAT_NOTFOUND, GDP_STAT_NAK_BADREQ);
	}

	
	// where: here or another position? 
	// 
	// 3. Recheck the access control rule because of time difference 
	// 
	if( !isAllowedAccess( ksData, curSession->ac_info, 'r' ) ) {
		// LATER: check error in using tname twice 
		ep_dbg_printf("[ERROR] cmd_subscribe: Not Allowed Access \n"
					  "\t from %s \n\t to %s \n", 
					  gdp_printable_name(req->cpdu->src, tname),  
					  gdp_printable_name(req->cpdu->dst, tname) ); 
		req->rpdu->cmd = GDP_NAK_C_UNAUTH;
		_gdp_gcl_decref(&req->gcl);
		return GDP_STAT_NAK_FORBIDDEN;
	}


	// get the additional parameters: number of records and timeout
	ep_thr_mutex_lock(&req->cpdu->datum->mutex);


	//
	// 4. Check the rcv msg on session 
	//    - verify HMAC / DECRYPT data  --> cpdu->datum->dbuf 
	//		- session state 
	stat = check_reqdbuf_onsession( req->cpdu,  curSession );
	if( stat != EX_OK ) {
		req->rpdu->cmd = GDP_NAK_C_UNAUTH;
		ep_thr_mutex_unlock(&req->cpdu->datum->mutex);
		_gdp_gcl_decref(&req->gcl);
		ep_dbg_printf("[ERROR] cmd_subscribe: Fail to check msg on session \n"
					  "\t from %s \n\t to %s \n", 
					  gdp_printable_name(req->cpdu->src, tname),  
					  gdp_printable_name(req->cpdu->dst, tname) ); 
		return GDP_STAT_NAK_FORBIDDEN;
	}


	// LATER CHECK: ALWAYS 2FIELDs
	// sub_ts (starting-time): req->cpdu->datum->ts   
	// sub    (starting-rec ): req->cpdu->datum->recno  
	// Req number of entries: req->numrecs :<- cpdu->datum->dbuf
	// Req last time: can be stored in datum->dbuf (also) 
	//rdatum	= req->cpdu->datum;
	req->numrecs = (int) gdp_buf_get_uint32(req->cpdu->datum->dbuf);
	gdp_buf_get_timespec(req->cpdu->datum->dbuf, &timeout);
//	if (ep_dbg_test(Dbg, 14))
	{
		ep_dbg_printf("cmd_subscribe: first = %" PRIgdp_recno ", numrecs = %d\n  ",
				req->cpdu->datum->recno, req->numrecs);
		_gdp_gcl_dump(req->gcl, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_subscribe");
	ep_thr_mutex_unlock(&req->cpdu->datum->mutex);


	if (req->numrecs < 0)
	{
		_gdp_gcl_decref(&req->gcl);
		req->rpdu->cmd = GDP_NAK_C_BADOPT;
		return GDP_STAT_NAK_BADOPT;
	}

	// get our starting point, which may be relative to the end
	stat = get_starting_point(req, ksData);
	if( stat == -1 ) {
		_gdp_gcl_decref(&req->gcl);
		goto fail1;
	}

	ep_dbg_printf(
			"cmd_subscribe: starting from %" PRIgdp_recno ", %d records\n",
			req->nextrec, req->numrecs);


	// see if this is refreshing an existing subscription
	{
		gdp_req_t *r1;

		for (r1 = LIST_FIRST(&req->gcl->reqs); r1 != NULL;
				r1 = LIST_NEXT(r1, gcllist))
		{
	/*		if (ep_dbg_test(Dbg, 50))
			{
				ep_dbg_printf("cmd_subscribe: comparing to ");
				_gdp_req_dump(r1, ep_dbg_getfile(), 0, 0);
			} 
	*/
			if (GDP_NAME_SAME(r1->rpdu->dst, req->cpdu->src) &&
					r1->rpdu->rid == req->cpdu->rid)
			{
				ep_dbg_cprintf(Dbg, 20, "cmd_subscribe: refreshing sub\n");
				break;
			}
		}
		if (r1 != NULL)
		{
			// make sure we don't send data already sent
			req->nextrec = r1->nextrec;

			// abandon old request, we'll overwrite it with new request
			// (but keep the GCL around)
//			ep_dbg_cprintf(Dbg, 20, "cmd_subscribe: removing old request\n");
			LIST_REMOVE(r1, gcllist);
			r1->flags &= ~GDP_REQ_ON_GCL_LIST;
			_gdp_gcl_decref(&r1->gcl);
			_gdp_req_lock(r1);
			_gdp_req_free(&r1);
		}
	}

	// mark this as persistent and upgradable
	req->flags |= GDP_REQ_PERSIST | GDP_REQ_SUBUPGRADE;

	// note that the subscription is active
	ep_time_now(&req->act_ts);

	stat = get_last_keyrec( ksData );

	// if some of the records already exist, arrange to return them
	if (req->nextrec <= stat)
	{
//		ep_dbg_cprintf(Dbg, 24, "cmd_subscribe: doing post processing\n");
		req->flags &= ~GDP_REQ_SRV_SUBSCR;
		req->postproc = &post_subscribe;
		req->udata    = curSession;
	}
	else
	{
		// this is a pure "future" subscription
//		ep_dbg_cprintf(Dbg, 24, "cmd_subscribe: enabling subscription\n");
		req->flags |= GDP_REQ_SRV_SUBSCR;

		// link this request into the GCL so the subscription can be found
		if (!EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags))
		{
			IF_LIST_CHECK_OK(&req->gcl->reqs, req, gcllist, gdp_req_t)
			{
				LIST_INSERT_HEAD(&req->gcl->reqs, req, gcllist);
				req->flags |= GDP_REQ_ON_GCL_LIST;
			}
			else
			{
				estat = EP_STAT_ASSERT_ABORT;
			}
		}
	}

	// we don't drop the GCL reference until the subscription is satisified

	return EP_STAT_OK;


fail0:
	req->rpdu->cmd = GDP_NAK_C_NOTFOUND;
	return kds_gcl_error(req->cpdu->dst, "cmd_subscribe: GCLopen failture", 
							estat, GDP_STAT_NOTFOUND );

fail1:
	req->rpdu->cmd = GDP_NAK_S_INTERNAL;
	return kds_gcl_error(req->cpdu->dst, "cmd_subscribe: failture", 
							estat, GDP_STAT_NAK_INTERNAL);


}


/*
**  CMD_MULTIREAD --- read multiple records
**
**		Arranges to return existing data (if any) after the response
**		is sent.  No long-term subscription will ever be created, but
**		much of the infrastructure is reused.
*/

EP_STAT
cmd_multiread(gdp_req_t *req)
{ 

	return kds_gcl_error(req->cpdu->dst,
						"cmd_multiread: not supported yet",
							GDP_STAT_NAK_BADREQ,
							GDP_STAT_NAK_BADREQ);

}


/*
**  CMD_UNSUBSCRIBE --- terminate a subscription
**
**		XXX not implemented yet
*/


/*
**  CMD_GETMETADATA --- get metadata for a GCL
*/

EP_STAT
cmd_getmetadata(gdp_req_t *req)
{ 
	return kds_gcl_error(req->cpdu->dst,
						"cmd_getmetadata: not supported yet",
							GDP_STAT_NAK_BADREQ,
							GDP_STAT_NAK_BADREQ);

}


EP_STAT
cmd_newsegment(gdp_req_t *req)
{ 
	// This service (log server) doesn't support the fwd append 
	return kds_gcl_error(req->cpdu->dst,
						"cmd_newsegment: not supported",
							GDP_STAT_NAK_BADREQ,
							GDP_STAT_NAK_BADREQ);
}



/*
**  CMD_FWD_APPEND --- NOT Supported command
*/

EP_STAT
cmd_fwd_append(gdp_req_t *req)
{ 

	// This service (log server) doesn't support the fwd append 
	return kds_gcl_error(req->cpdu->dst,
						"cmd_fwd_append: not supported",
							GDP_STAT_NAK_BADREQ,
							GDP_STAT_NAK_BADREQ);

}


/**************** END OF COMMAND IMPLEMENTATIONS ****************/



/*
**  GDPD_PROTO_INIT --- initialize protocol module
*/

// CREATE: request the Key-distribution-service.  
//	From:  writer / manager 
//  t1: generate / distribution 
//	t2: distribution 
static struct cmdfuncs	CmdFuncs[] =
{
	{ GDP_CMD_PING,			cmd_ping				},  
	{ GDP_CMD_CREATE,		cmd_create				},  
	{ GDP_CMD_OPEN_AO,		cmd_open				}, 
	{ GDP_CMD_OPEN_RO,		cmd_open				}, 
	{ GDP_CMD_CLOSE,		cmd_close				}, //
	{ GDP_CMD_READ,			cmd_read				}, 
	{ GDP_CMD_APPEND,		cmd_append				},  
	{ GDP_CMD_SUBSCRIBE,	cmd_subscribe			}, 
	{ GDP_CMD_MULTIREAD,	cmd_multiread			}, 
	{ GDP_CMD_GETMETADATA,	cmd_getmetadata			}, 
	{ GDP_CMD_OPEN_RA,		cmd_open				}, 
	{ GDP_CMD_NEWSEGMENT,	cmd_newsegment			}, 
	{ GDP_CMD_FWD_APPEND,	cmd_fwd_append			}, 
	{ 0,					NULL					}
};
// LATER: we can consider two options 
//	1. add the two command for session init / close for cmd_create 
//  2. cmd_open (for cmd_create & other commands) for cmd_create: dst of cmd_open == that of cmd_create 


EP_STAT
ksd_proto_init(void)
{
	// register the commands we implement
	_gdp_register_cmdfuncs(CmdFuncs);
	return EP_STAT_OK;
}
