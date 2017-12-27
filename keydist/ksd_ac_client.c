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
**  KSD_AC_CLIENT -- client functions to interact with external log servers      
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <hs/hs_errno.h>
#include "ksd_data_manager.h"




//
// INTERNAL functions
//

/*
** Callback functions to process the received ac log
*/
void acread_cb(gdp_event_t *gev)
{
	(void) process_event(gev, true, 'a'); 
	gdp_event_free(gev);
}


/*
** Return the number of datum 
**		which arrived out of order and are stored in buffer    
*/
int get_bufferednum( ACL_info *acInfo )
{
	int			count = 0;
	gdp_datum_t		*t_cur = acInfo->head;


	while( t_cur != NULL ) {
		count++;
		t_cur = t_cur->next;
	}

	return count;
}


/*
** Store the datum which arrived out of order in buffer 
** [NOTE] ASSUME that acInfo is already locked. 
** [NOTE] after calling, caller must set inData = NULL  
*/
void insert_acdata_inbuf( ACL_info *acInfo, gdp_recno_t	inRecno, 
							gdp_datum_t *inData )
{
	gdp_recno_t		t_rec = 0;
	gdp_datum_t		*t_cur = acInfo->head;
	gdp_datum_t		*t_pre = NULL;


	while( t_cur != NULL ) {

		t_rec = gdp_datum_getrecno( t_cur );

		if( t_rec < inRecno )  {
			t_pre = t_cur;
			t_cur = t_cur->next;

		} else if( t_rec == inRecno ) {
			ep_app_warn( "Repeated rcv data at %" PRIgdp_recno, t_rec ); 
			// skip this message  
			gdp_datum_free( inData );
			return; 

		} else  break;

	}


	if( t_pre == NULL ) {
		inData->next = acInfo->head;
		acInfo->head = inData;

	} else {
		t_pre->next = inData;
		inData->next = t_cur; 
	}	

}


/*
** Delete the header datum in buffer  
** [NOTE] ASSUME that acInfo is already locked. 
*/
void delete_ac_buf_head( ACL_info *acInfo )
{
	gdp_datum_t		*del_datum = acInfo->head;

	acInfo->head = acInfo->head->next;

	gdp_datum_free( del_datum );
}



//
// EXTERNAL functions
//


/*
**  Read all ac data from AC LOG [Asynchronously, use callback] 
**  [INFO] this functions is called while loading the ac rules 
**		  at key service prepartion time  or restoration time 
*/ 
void read_all_ac_data(gdp_gcl_t *gcl, ACL_info *acInfo ) 
{ 
	EP_STAT						estat = EP_STAT_OK;
	gdp_event_cbfunc_t			cbfunc = acread_cb;


	if( gcl == NULL || acInfo == NULL ) {
		ep_app_error("[DEBUG] %s, %s in read all ac", 
					gcl==NULL?"NULL gcl":"", acInfo==NULL?"NULL info":"");
	}

	ep_thr_mutex_lock( &acInfo->mutex );

	// Check first recno (request the new api such as gdp_gcl_firstrecno(gcl) ) 
	if( acInfo->reInit_recn == 0 ) 
		acInfo->first_recn = 1; // LATER UPDATE 
	else acInfo->first_recn = acInfo->reInit_recn; 	 
	acInfo->next_recn  = acInfo->first_recn;


	// LATER: in the case that the first recn is not 1. 
	// Can API to get the first or last recn be supported ? 
	estat = gdp_gcl_subscribe( gcl, acInfo->first_recn, 0, NULL, 
									cbfunc, (void *)(acInfo) ); 

	if( !EP_STAT_ISOK(estat) ) {
		char ebuf[100];
		ep_app_error("ASYNC_read_all: Cannot subscribe at start_recn %" 
						PRIgdp_recno ", error\n\t%s", 
				acInfo->first_recn, ep_stat_tostr(estat, ebuf, sizeof ebuf) );

		// LATER: we must read all data.. retry reinit... & subscribe.. 
		// Retry periodically ... 
		acInfo->reInit_recn = acInfo->first_recn;
		acInfo->state = NEED_AC_SUBSCRIBE;

		// There may be problem in gcl. so re-open later. 
		gdp_gcl_close( gcl ); 
		acInfo->gcl = NULL;

	} else if (  gdp_gcl_getnrecs( gcl ) < acInfo->next_recn ) {
		// no privious data... 
		acInfo->state = DONE_INIT;	

	} else acInfo->state = DOING_INIT;

	ep_thr_mutex_unlock( &acInfo->mutex );

	// wait to treat existing ac rules. 
	sleep( 10 );
}


void request_acgcl_read_async( ACL_info  *acInfo, gdp_recno_t a_recno )
{
	gdp_event_cbfunc_t		cbfunc = acread_cb;
	gdp_gcl_read_async( acInfo->gcl, a_recno, cbfunc, (void *)(acInfo) );
}


/*
** When we get the data from AC LOG server through subscribe/async read request, 
**	this function is called. 
**  [Called  gdp_event : GDP_EVENT_DATA / _MISSING]  
** This fucntion processes the received AC RULE info 
** 
** Second argu (datum): received datum 
** Return value: If any data arrives out of order, the data is buffered  and 
**					this function returns true 
**				 In order case, return false
*/
bool update_ac_data( ACL_info *acInfo, gdp_datum_t *datum, bool isMissing )
{
	int					dlen; 
	bool				isBuffered = false;
	unsigned char		*data;
	gdp_recno_t			rd_recn = 0;
	char				pre_state; 
	char				rule_mode  = 0;
	bool				apply_deny = false;



	if( acInfo == NULL ) {
		ep_app_warn("NULL acInfo in update_ac_data" );
		return isBuffered;
	}

	if( (datum!=NULL) && (datum->dbuf==NULL) ) {
		ep_app_warn("No ac data in update_ac_data" );
		return isBuffered;
	}

	if( datum == NULL )  {
		ep_thr_mutex_lock( &acInfo->mutex );
		acInfo->last_recn   = gdp_gcl_getnrecs( acInfo->gcl );
		pre_state			= acInfo->state;
		goto step1; 
	}

	rd_recn =  gdp_datum_getrecno( datum );

	if( rd_recn < acInfo->next_recn ) {
		ep_app_warn("Previous data at %" PRIgdp_recno "at expected %"
					PRIgdp_recno, rd_recn, acInfo->next_recn );
		return isBuffered;
	}


	ep_thr_mutex_lock( &acInfo->mutex );

	acInfo->last_recn   = gdp_gcl_getnrecs( acInfo->gcl );
	pre_state			= acInfo->state;

	//print current state  / next state 

	//
	// GDP_EVENT_MISSING CASE 
	// 
	if( isMissing ) {
		// process the Missed recno info 

		if( acInfo->state == WAIT_NEXT_ENTRY ) {
			if( rd_recn == acInfo->next_recn ) {
				acInfo->state = DOING_INIT;
				acInfo->ref_time = 0;
			}
		}


		if( rd_recn == acInfo->first_recn ) {
			// 
			// To handle the case : first recno != 1 
			//
			if( acInfo->first_recn != acInfo->next_recn ) {
				// Un expected  error 
				ep_app_error("[UN] Rcv MISSING at %" PRIgdp_recno "recn (first %" 
								PRIgdp_recno", next %" PRIgdp_recno ")", 
					acInfo->first_recn, acInfo->first_recn, acInfo->next_recn );
				ep_thr_mutex_unlock( &acInfo->mutex );
				return isBuffered;
			}
			acInfo->first_recn +=1;
			acInfo->next_recn +=1;

		} else if( acInfo->first_recn == acInfo->next_recn ) {
			// 
			// To handle the Missing in datum which arrives out of order  
			//	when any available rule is not received 
			//
			if(  acInfo->head == NULL ) {
				// 
				// This is the first received pkt. 
				// ASSUME that the all available entry has the larger recno.  
				//		
				acInfo->first_recn = rd_recn+1; 
				acInfo->next_recn  = rd_recn+1; 
				// state : DOING_INIT

			} else {
				//
				// Store the the received datum for later process
				//
				 
				gdp_recno_t			bh_recn;
				
				bh_recn = gdp_datum_getrecno( acInfo->head ) ;

				if( bh_recn > rd_recn ) {
					// ASSUME the same as the case of acInfo->head == NULL
					acInfo->first_recn = rd_recn+1; 
					acInfo->next_recn  = rd_recn+1; 

				} else if( bh_recn == rd_recn ) {
					// Un expected... 
					ep_app_error("[UN] Rcv MISSING at %" PRIgdp_recno 
									" recn : Conflicting pre-recived result", 
									bh_recn );

					acInfo->first_recn = rd_recn; 
					acInfo->next_recn  = rd_recn; 

				} else {
					//  LATER... (missing entry) 
					ep_app_warn("[LATER] need to treat missing entry at %"  
									PRIgdp_recno " recn with the data at %" 
									PRIgdp_recno " recn ", 
									rd_recn, bh_recn );

					// retry the read request at rd_recn ? 
					//		or assume no entry at rd_
				}

				// state : DOING_INIT
			}

		} else {
			// 
			// To handle the Missing in datum which arrives out of order  
			//		after any available rule is applied 
			//
			if( acInfo->next_recn == rd_recn ) {
				acInfo->next_recn += 1;

			}  else {
				// Ignore : case: acInfo->next_recn <  rd_recn 
				ep_app_warn("[LATER] need to treat missing entry at %"  
								PRIgdp_recno " recn with the data at %" 
								PRIgdp_recno " recn ", 
							rd_recn, acInfo->first_recn );
			}

		}

		if( acInfo->state == DONE_INIT ) {
			ep_app_error("[CHECK] after initialization, receive MISSING"
							" record at %" PRIgdp_recno 
							"\n --- first:%" PRIgdp_recno ", next: %" 
							PRIgdp_recno , 
							rd_recn, acInfo->first_recn, acInfo->next_recn );

		} else if( acInfo->state == NEED_AC_SUBSCRIBE ) {
			ep_app_warn("[CHECK] In NEED_AC_SUBSCRIBE state, receive MISSING"
							" record at %" PRIgdp_recno 
							"\n --- first:%" PRIgdp_recno ", next: %" 
							PRIgdp_recno , 
							rd_recn, acInfo->first_recn, acInfo->next_recn );
		}

		// we also check the buffered entry below... 
	}  // end of isMissing 
	
	// process received data 
	dlen = gdp_buf_getlength(datum->dbuf);
	data = gdp_buf_getptr( datum->dbuf, dlen );

	if( !isMissing && dlen > 0 ) {
		// 
		// Treat the received data...  : applied or buffered 
		// 

		ep_thr_mutex_lock( &datum->mutex );
		if( rd_recn == acInfo->next_recn ) { // applied 
			int		rval;
		
			if( acInfo->state == WAIT_NEXT_ENTRY ) {
				acInfo->state = DOING_INIT;
				acInfo->ref_time = 0;
			}

			rval = reflect_ac_rule( acInfo->acr_type, &(acInfo->acrules), 	
										dlen, data, &rule_mode );

			if( rval == EX_OK ) {
				acInfo->next_recn++;
				if( rule_mode == 'd' ) apply_deny = true;

			} else if( rval == EX_INVALIDDATA ) {
				// Worng AC data : re- request /read 
				struct timeval		tv;
				
				request_acgcl_read_async( acInfo, rd_recn );
				gettimeofday( &tv, NULL );
				acInfo->ref_time = tv.tv_sec + WAIT_TIME_SEC;
				acInfo->state = WAIT_NEXT_ENTRY; 

			} else { 
				// why ? internal error : EX_MEMERR 
				ep_app_error("[CHECK] reflect_rule_erro : %d\n", rval );

				insert_acdata_inbuf( acInfo, rd_recn, datum );
				isBuffered = true;
			}

		} else if( rd_recn  > acInfo->next_recn ) {
			insert_acdata_inbuf( acInfo, rd_recn, datum );
			isBuffered = true;
		} else {
			// ignore this packet... 
		}

		ep_thr_mutex_unlock( &datum->mutex );

	}


step1: 
	// 
	// Theat the ac data in buffer 
	// 
	while( acInfo->head != NULL ) {
		gdp_recno_t		b_recn;

		b_recn = gdp_datum_getrecno( acInfo->head );

		if( b_recn < acInfo->next_recn ) {
			// error case 
			ep_app_error("[CHECK] record(%" PRIgdp_recno ") in buffer head"
							" has smaller recn than" 
							 " the next_recn (%" PRIgdp_recno ")", 
							b_recn, acInfo->next_recn );
			delete_ac_buf_head( acInfo );

		} else if( b_recn == acInfo->next_recn ) {
			int			rval;
	
			dlen = gdp_buf_getlength(acInfo->head->dbuf);
			data = gdp_buf_getptr( acInfo->head->dbuf, dlen );

			rval = reflect_ac_rule( acInfo->acr_type, &(acInfo->acrules), 
										dlen, data, &rule_mode);
			if( rval == EX_OK ) {
				delete_ac_buf_head( acInfo );
				acInfo->next_recn++;
				if( rule_mode == 'd' ) apply_deny = true;

				if( acInfo->state == CHECK_BUF_ENTRY ) 
					acInfo->state = DOING_INIT;

			} else if( rval == EX_INVALIDDATA ) {
				// Worng AC data : re- request /read 
				struct timeval		tv;
				
				request_acgcl_read_async( acInfo, b_recn );
				gettimeofday( &tv, NULL );
				acInfo->ref_time = tv.tv_sec + WAIT_TIME_SEC;
				acInfo->state = WAIT_NEXT_ENTRY; 
				delete_ac_buf_head( acInfo );

			} else { // INTERNAL ERROR : EX_MEM
				acInfo->state = CHECK_BUF_ENTRY; 
				break;
			}

		} else {
			// check the number of  buffered entry. 
			if( get_bufferednum( acInfo ) >  NUM_WAIT_ENTRY ) {
				// Missed AC data : re- request /read 
				struct timeval		tv;
				
				request_acgcl_read_async( acInfo, acInfo->next_recn );
				gettimeofday( &tv, NULL );
				acInfo->ref_time = tv.tv_sec + WAIT_TIME_SEC;
				acInfo->state = WAIT_NEXT_ENTRY; 
			}

			break;
		}

	}

	if( acInfo->next_recn > acInfo->last_recn ) acInfo->state = DONE_INIT;

	if( acInfo->state == DONE_INIT ) {
		if( acInfo->head != NULL ) {
			ep_app_info("[CHECK] In DONE_INIT state, process record %" 
							PRIgdp_recno "\n --- first:%" PRIgdp_recno 
							", next: %" PRIgdp_recno ", head: %" PRIgdp_recno,  
							rd_recn, acInfo->first_recn, acInfo->next_recn, 
							gdp_datum_getrecno( acInfo->head ) );
			acInfo->state = DOING_INIT;
		}

	} 
	

	ep_dbg_printf("[ac info state change] %d -> %d \n", 
										pre_state, acInfo->state );
	ep_thr_mutex_unlock( &acInfo->mutex );


	if( pre_state == DONE_INIT ) {
		if( rule_mode != 0 ) notify_rule_change_toKey( acInfo, apply_deny );

		// changed rule: apply key gen 
		// apply_deny : re check access rule on existing subscription 
		//	 noti :acinfo->skeys  list
		//		noti keylog->shlogs list 
	}

	return isBuffered;
}


/*
** Called when AC Log server is shut down 
** Retry to access the AC Log server (periodically)
** While AC Los server is not working, we cannot change any ac rule. 
** So, if we read all exsiting ac rules, we can continue the service.  
*/
void reflect_ac_shutdown( ACL_info *a_acInfo )
{
	if( a_acInfo->isAvailable ) {

		a_acInfo->reInit_recn    = a_acInfo->next_recn;
		a_acInfo->state			 = NEED_AC_SUBSCRIBE;

		// check the position where this funcion is called.. 
		gdp_gcl_close( a_acInfo->gcl );
		a_acInfo->gcl = NULL; 
		//STAILQ_INSERT_TAIL( &reWorkacl, a_acInfo, dolist );
	}

}

