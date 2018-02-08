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
**  KSD_KEY_MANAGER - functions to support the data structure for key handling  
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 


#include <string.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include "ksd_key_gen.h"
#include "ksd_data_manager.h"


int		kmanage_policy		= SKELETON_LATEST;
int		latest_key_count    = 24;  // 1day 
int		total_thval			= 300;  // regulate.. 
int		del_num				= 10; 
int		del_period			= 5; 

int		max_inx				= 0; // LATER (KEY CREATION ++)


// 
// Internal Functions 
// 
int check_key_chain_type( RKEY_1 *inKey ) 
{
	int			kType;


	if( inKey->seqn == inKey->pre_seqn ) kType = 1; // no chain 
	else if( inKey->pre_seqn == 0 ) kType = 2;  // one chain 
	else kType = 0; // multi chain 

	return kType;
}


int check_key_chain_type2( LKEY_info *keyInfo )
{	
	int				inx;
	KGEN_param		*kparam =  NULL;


	if( keyInfo == NULL ) return -1;

	kparam = keyInfo->param;
	if( kparam == NULL ) return -1;

	inx = kparam->security_level;
	if(  inx < 30 )      return 2; // one chain 
	else if( inx < 90 )  return 0; // multi chain 
	else if( inx < 120 ) return 1; // no chain 
	else                 return -1; 

}



// trim in not multi chain case
void  trim_key2_memory( struct hdr_rkey2 *rHdr, LKEY_info *oKeyInfo, 
						int kType ) 	
{
	int						count_d = 0;	
	int						count_p = 0;	
	int						time_th = 300; // 5 min 
	bool					deleted = false;					
	EP_TIME_SPEC			now;
	struct	hdr_rkey2		*tHdr, *hHdr, *curHdr;


	// the number of entries is over 3. 
	tHdr = (struct hdr_rkey2 *)(oKeyInfo->k_tail);
	hHdr   = (struct hdr_rkey2 *)(oKeyInfo->keys);
	curHdr = tHdr;
	ep_time_now( &now );


	curHdr = tHdr;
	while( curHdr != hHdr ) {
		if( curHdr != rHdr ) { 
			if( now.tv_sec > curHdr->latest_time.tv_sec + time_th ) {

				if ( kType == 1 ) deleted = true;
				else { 
					if( count_p > del_period ) {
						deleted = true;
						count_p = 0;
					}
				}

				if( deleted ) {
					// deletion
					return_RKEY1( curHdr->key_entry ); 
					// alwasy curHdr->pre != NULL 
					curHdr->pre->next = curHdr->next;
					if( curHdr == tHdr ) {
						oKeyInfo->k_tail = (void *)(curHdr->pre);
					} else curHdr->next->pre = curHdr->pre; 

					ep_mem_free( curHdr ); 

					count_d++;
					if( count_d > del_num ) break;
				} 

			} else  curHdr = curHdr->pre ;


		} else curHdr = curHdr->pre;

		count_p++;	
	}	

}


// trim in multi chain case
void  trim_key1_memory( struct hdr_rkey1 *rHdr, LKEY_info *oKeyInfo ) 	
{
	struct	hdr_rkey1		*tHdr, *hHdr, *delHdr, *curHdr;


	tHdr = (struct hdr_rkey1 *)(oKeyInfo->k_tail);
	hHdr   = (struct hdr_rkey1 *)(oKeyInfo->keys);

	if( tHdr == hHdr ) {  // one chain
		ep_app_warn( "[CHECK] one chain with %d entries above %d \n", 
				tHdr->child_num, total_thval );
		return ; 
	} 

	if( hHdr->next == tHdr ) { // 2 chain 
		if( tHdr == rHdr ) {
			ep_app_warn( "[CHECK] 2 chain with latest tail access %d \n", 
							total_thval );
		} else {
			if( tHdr->child_num > 1 ) {
				// remove the oldest keys except header in tail 
				return_RKEY1s( tHdr->childh->next ); 
				tHdr->childh->next = NULL;
				tHdr->childt = tHdr->childh;
				oKeyInfo->count = oKeyInfo->count - tHdr->child_num + 1;
				tHdr->child_num = 1;
			}
		}
		return ; 
	}


	// 3 or more chain
	// the most oldest  or sub oldest : current sub oldest... 
	//   find the keys whose latesst access time is older 
	//		than the acess time of keys in tail 
	delHdr = tHdr;
	curHdr = tHdr;
	while( curHdr != hHdr ) {
		if( curHdr == rHdr ) curHdr = curHdr->pre; 
		else {
			// cannot be equal 
			if( ep_time_before( &(curHdr->latest_time), 
									&(delHdr->latest_time) ) ) {
				if( curHdr->child_num == 1 ) {
					if( delHdr->child_num == 1 ) {
						delHdr = curHdr;
						curHdr = curHdr->pre; 
					} else curHdr = curHdr->pre;

				} else {
					// delete keys pointed by curHdr except header... 
					return_RKEY1s( curHdr->childh->next);
					curHdr->childh->next = NULL;
					curHdr->childt = curHdr->childh;
					oKeyInfo->count = oKeyInfo->count - curHdr->child_num + 1;
					curHdr->child_num = 1;
					return ; 
				} 

			} else curHdr = curHdr->pre;  
		} 

	} // end of while 


	// remove keys pointed by delHdr  (not hHdr, rHdr) 
	if( delHdr->child_num == 1 ) {
		oKeyInfo->count--;
		return_RKEY1( delHdr->childh);

		delHdr->pre->next = delHdr->next;
		if( delHdr->next != NULL ) {
			delHdr->next->pre = delHdr->pre;
		}
		ep_mem_free( delHdr );

	} else { // maybe delHdr == tail. 
		return_RKEY1s( delHdr->childh->next);
		delHdr->childh->next = NULL;
		delHdr->childt = delHdr->childh;
		oKeyInfo->count = oKeyInfo->count - delHdr->child_num + 1;
		delHdr->child_num = 1;
	}

}



bool insert_klog_insublist( struct hdr_rkey1 *curHdr, RKEY_1 *inKey )
{
	RKEY_1		*t_cur, *t_pre;


	t_pre = NULL;
	t_cur = curHdr->childh;

	while( t_cur != NULL ) {
		if(  t_cur->seqn > inKey->seqn ) {
			t_pre = t_cur;
			t_cur = t_cur->next;
		} else if(  t_cur->seqn ==  inKey->seqn ) {
			// Unexpected... 
			return false;
		} else break; 

	}

	if( t_pre == NULL ) {
		inKey->next = curHdr->childh;
		curHdr->childh = inKey; 

	} else {
		inKey->next = t_cur;
		t_pre->next = inKey;

		if( t_cur == NULL ) curHdr->childt = inKey;  
	}

	return true;
}



// Reflect the received previous key info in first argu 
//	to the LKEY_info data structure identified by second argu. 
// This function is called in initialization process. 
// This function returns the  key log record number for us to read next 
//	based on the manage policy. 
// The return value 0 means the error or the end of initiazliation 
gdp_recno_t	update_key_during_init( gdp_datum_t *rDatum, LKEY_info *oKeyInfo ) 
{
	gdp_recno_t			cur_recn;
	gdp_recno_t			another_recn;
	RKEY_1				*rKey = NULL;



	// error check 
	if( rDatum == NULL || oKeyInfo == NULL ) {
		ep_app_error( "NULL data for key history init" );
		return 0;
	}

	//
	// Parse the recived key log info 
	cur_recn = gdp_datum_getrecno( rDatum ); 

	if( oKeyInfo->param->sym_key_alg == 0 ) {
		char	tVal1, tVal2;
		rKey	 = convert_klog_to_RKEY( rDatum, &tVal1, &tVal2 ); 

		oKeyInfo->param->sym_key_alg  = (int)tVal1;
		oKeyInfo->param->sym_key_mode = (int)tVal2;

	} else	rKey = convert_klog_to_RKEY( rDatum, NULL, NULL ); 
	if( rKey == NULL ) {
		ep_app_error( "Fail to parse/convert the received key log" );
		return 0;
	}


	insert_key_log( rKey, oKeyInfo, BACKWARD_IN );

	switch( kmanage_policy ) {
		case ALL_KEY: another_recn = cur_recn -1;
				break;

		case LATEST_KEY: 
				if( oKeyInfo->count < latest_key_count ) 
						another_recn = cur_recn -1;
				else another_recn = 0;
				break;

		case SKELETON_LATEST: 
				if( oKeyInfo->count < latest_key_count ) {
						another_recn = rKey->pre_seqn; 
				}else another_recn = 0;
				break;

		default:
			ep_app_error( "Wrong Key manage policy: %d \n", kmanage_policy );
			return 0;
	}

	return another_recn;
}


// Get and Manage the key histories. 
// According to the policy, this function can obtain all key histories 
//		or some snapshot of key histories. 
// Process: from the latest key histroy to the older key histories 
// Respond of gcl_read can affect the next record num to read: Synchronous read
void load_keyhistories(LKEY_info *keyInfo, int inNum )
{
	int					try = 0;
	int					lcount; 
	EP_STAT				estat;
	gdp_gcl_t			*gcl	= keyInfo->gcl;
	gdp_recno_t			nrecno	= keyInfo->last_recn;
	gdp_datum_t			*datum  = gdp_datum_new(); 



	if( inNum == 0 ) lcount = total_thval;
	else			 lcount = inNum;

	while( nrecno>0	) {	 
		// if loaded key number is above total_thval  
		if( keyInfo->count > lcount ) break;

		estat = gdp_gcl_read( gcl, nrecno, datum );

		if( EP_STAT_ISOK( estat )  ) {
			// reflect the received key info... 
			nrecno = update_key_during_init(  datum, keyInfo );
			
			// flush any left over data 
			if( gdp_buf_reset(gdp_datum_getbuf(datum))  < 0 ) {
				char	ttbuf[40];
				strerror_r( errno, ttbuf, sizeof ttbuf );
				ep_app_warn("buffer reset failed: %s", ttbuf );
			}

			try = 0;
				
		} else {
			if( EP_STAT_IS_SAME( estat, GDP_STAT_NAK_NOTFOUND ) ) {
				// no read entry 
				break; 
			} else  if( try > 3 ) break;
			else try++;
		}

	}
}



void klogread_cb( gdp_event_t *gev )
{
	(void) process_event( gev, true, 'k' ); // k - key 
	gdp_event_free( gev );
}


//
// EXTERNAL FUNCTIONS 
//
void free_lkinfo( void *a_val ) 
{
	int			kType;	 
	LKEY_info	*keyInfo = (LKEY_info *)a_val;


	kType = check_key_chain_type2( keyInfo );

	if( keyInfo->param != NULL ) ep_mem_free( keyInfo->param );
	keyInfo->param = NULL;

	if( keyInfo->gcl != NULL ) gdp_gcl_close( keyInfo->gcl );
	keyInfo->gcl = NULL;

	if( keyInfo->gmd != NULL ) gdp_gclmd_free( keyInfo->gmd );
	keyInfo->gmd = NULL; 

	// remove keys
	if( kType < 0 ) {  // unknown type 
		ep_app_error( "[UNknown] K-chain type at mode %c \n", 
				keyInfo->rw_mode );
		if( keyInfo->keys != NULL ) 
			ep_app_error("Fail to remove keys (%d)", keyInfo->count );

	} else if( kType == 0 ) {  // multi chain 
		struct hdr_rkey1	*khead;
		khead =  (struct hdr_rkey1 *)(keyInfo->keys);
		free_rkeyHdr1_keys( khead );

	} else { // 1 (no chain) , 2 (one chain) 
		struct hdr_rkey2	*khead;
		khead =  (struct hdr_rkey2 *)(keyInfo->keys);
		free_rkeyHdr2_keys( khead );
	}
	keyInfo->k_tail = NULL; 

	if( keyInfo->sub_keys != NULL ) 
		return_RKEY1s( keyInfo->sub_keys );

	ep_mem_free( keyInfo );
}


void free_light_lkinfo( void *a_val ) 
{
	LKEY_info	*keyInfo = (LKEY_info *)a_val;

	if( keyInfo->gcl != NULL ) gdp_gcl_close( keyInfo->gcl );
	keyInfo->gcl = NULL;
 
}


// -1 of a_kgeninx means the external key distribution service request.
LKEY_info* get_new_klinfo( size_t a_len, char *a_name, 
							char a_rwmode, char	a_kgeninx )
{
	LKEY_info		*newInfo = NULL;

	
	if( a_len > GDP_GCL_PNAME_LEN ) {
		ep_dbg_printf( "[ERROR] Invalid Key Log pname %s \n", a_name ); 
		ep_dbg_printf( "   >>> length(%zu) must be shorter than %d\n", 
							a_len, GDP_GCL_PNAME_LEN ); 
		return newInfo; 
	}
	
	newInfo = (LKEY_info *)ep_mem_zalloc( sizeof(LKEY_info) );
	if( newInfo == NULL ) {
		ep_dbg_printf( "[ERROR] Cannot get memory for LKEY_info"
						" of %s \n", a_name ); 
		return newInfo; 
	}

/*
	strncpy( newInfo->klog_pname, a_name, a_len );
	newInfo->klog_pname[a_len] = '\0'; 
	gdp_parse_name( newInfo->klog_pname, newInfo->klog_iname );
*/
	gdp_parse_name( a_name, newInfo->klog_iname );
	newInfo->ref_count	= 1;

	if( a_kgeninx == -1 ) {
		max_inx++; 
		newInfo->kgen_inx = max_inx ; 

		if( a_rwmode == 'w' ) newInfo->rw_mode = a_rwmode;
		else newInfo->rw_mode = 'r';

	} else {
		// during init 
		if( a_kgeninx > max_inx ) max_inx = a_kgeninx;
		newInfo->kgen_inx   = a_kgeninx;

		if( a_rwmode == 'w' ) newInfo->rw_mode = a_rwmode;
		else newInfo->rw_mode = 'r';

		newInfo->param	= load_kgen_param( a_kgeninx, newInfo->rw_mode ); 
		if( newInfo->param == NULL ) {
			ep_dbg_printf( "[ERROR] Fail to load the kgen "
							"param for %s \n", a_name );
			// memory free 
			ep_mem_free( newInfo );
			return NULL;
		}
	}

	LIST_INIT( &(newInfo->shlogs) );


	ep_thr_mutex_init( &newInfo->mutex, EP_THR_MUTEX_DEFAULT );
	ep_thr_mutex_setorder( &newInfo->mutex, GDP_MUTEX_LORDER_KSD );


	return newInfo;
}



// can be called by key generator & key distributor w/o generation. 
// mode (0: backward, 1: random, 2: latest insert) 
void insert_key_log( RKEY_1 *inKey, LKEY_info *oKeyInfo, char mode )
{
	int					kType = check_key_chain_type( inKey ); 


	if( kType == 0 ) { // multi chain. 
		struct hdr_rkey1	*khead, *ktail, *newHdr;
		
		khead =  (struct hdr_rkey1 *)(oKeyInfo->keys);
		ktail =  (struct hdr_rkey1 *)(oKeyInfo->k_tail); 


		// First entry 
		if( ktail == NULL ) {
			newHdr = get_new_rkeyHdr1( inKey );
			if( newHdr == NULL ) return;

			oKeyInfo->k_tail = (void *)newHdr;
			oKeyInfo->keys   = (void *)newHdr;
			oKeyInfo->count++;

		} else if( mode == BACKWARD_IN ) {
			// check whether new previous chain is necessary  
			if( ktail->childt->pre_seqn == inKey->pre_seqn ) {
				ktail->childt->next = inKey;
				ktail->childt = inKey;

				ktail->child_num++;
				oKeyInfo->count++; // not be called in SKELETON  mode

			} else {
				if( ktail->childh->seqn !=  inKey->pre_seqn ) {
					ep_app_warn("[DBG CHECK] BACKward insert pre(%d %d)"
								" new(%d %d)\n", 
							ktail->childh->seqn, ktail->childh->pre_seqn, 
							inKey->seqn, inKey->pre_seqn ); 
				}
				newHdr = get_new_rkeyHdr1( inKey );
				if( newHdr == NULL ) return;

				newHdr->pre = ktail; 
				ktail->next = newHdr; 
				oKeyInfo->k_tail = (void *)newHdr;
				oKeyInfo->count++;
			}

		} else if( mode == FORWARD_IN ) { // ktail != NULL, khead != NULL 

			// check whether new  chain is necessary  
			if( khead->childh->pre_seqn == inKey->pre_seqn ) {
				// MUST: inKey->seqn > khead->childh->seqn
				inKey->next = khead->childh; 
				khead->child_num++;
				khead->childh = inKey; 

				oKeyInfo->count++;

			} else {  // On checking error, need to consider 
				// Make new chain and Insert header. 

				newHdr = get_new_rkeyHdr1( inKey );
				if( newHdr == NULL ) return;

				newHdr->next = khead; 
				khead->pre   = newHdr;

				oKeyInfo->keys = (void *)newHdr;
				oKeyInfo->count++;
			}

			if( oKeyInfo->count > total_thval ) {
				trim_key1_memory( khead, oKeyInfo ); 	
			}

		} else { // mode == RANDOM_IN  
			// find the store position  
			if( khead->childh->seqn < inKey->seqn ) 
					insert_key_log( inKey, oKeyInfo, FORWARD_IN ) ; 
			else if( ktail->childt->seqn > inKey->seqn ) 
					insert_key_log( inKey, oKeyInfo, BACKWARD_IN ) ; 
			else {
				struct hdr_rkey1	*curHdr;
				bool	stored     = false;
				bool	newEntry   = false;
				// hval : alwasy positive, tval: positive or negative 
				int		hval = khead->childh->seqn - inKey->seqn;
				int		tval = inKey->seqn - ktail->childh->seqn; 

				if( hval < tval ) {
					curHdr = khead;

					while( curHdr != NULL ) {
						if( curHdr->childh->pre_seqn == inKey->pre_seqn ) {
							// insert this key in curHdr list.. 
							newEntry = insert_klog_insublist( curHdr, inKey ); 
							stored = true;
							newHdr = curHdr;
							break; 
						} else if(curHdr->childh->pre_seqn <  inKey->pre_seqn) {
							// make new hdr between curHdr->pre & curHdr 
							newHdr = get_new_rkeyHdr1( inKey );
							if( newHdr == NULL ) return;
						
							newHdr->next = curHdr; 
							newHdr->pre  = curHdr->pre; 
							curHdr->pre->next = newHdr;
							curHdr->pre = newHdr; 

							newEntry = true;
							stored = true;
							break; 
						} else curHdr = curHdr->next;  // check next hdr.. 
					}

					if( stored == false ) {
						ep_app_error("[NEED DEBUG] random insert err head \n");
						return;
					}

				} else {
					curHdr = ktail; 

					while( curHdr != NULL ) {
						if( curHdr->childh->pre_seqn == inKey->pre_seqn ) {
							// insert this key in curHdr list.. 
							newEntry = insert_klog_insublist( curHdr, inKey ); 
							stored = true;
							newHdr = curHdr;
							break; 

						} else if(curHdr->childh->pre_seqn >  inKey->pre_seqn) {
							// make new hdr between curHdr & (curHdr->next)
							newHdr = get_new_rkeyHdr1( inKey );
							if( newHdr == NULL ) return;
						
							newHdr->next = curHdr->next; 
							newHdr->pre  = curHdr; 
							curHdr->next->pre = newHdr;
							curHdr->next      = newHdr; 

							newEntry = true;
							stored = true;
							break; 
						} else curHdr = curHdr->pre;  // check previous hdr.. 
					}

					if( stored == false ) {
						ep_app_error("[NEED DEBUG] random insert err tail \n");
						return;
					}
				}

				if( newEntry ) {
					oKeyInfo->count++;
					if( oKeyInfo->count > total_thval ) 
						trim_key1_memory( newHdr, oKeyInfo ); 	
				}
			} // 245 
			
		} // random_in 

	} else { // another key gen type (no or one chain) 
		struct hdr_rkey2	*khead, *ktail, *newHdr;
		
		khead =  (struct hdr_rkey2 *)(oKeyInfo->keys);
		ktail =  (struct hdr_rkey2 *)(oKeyInfo->k_tail); 


		// First entry 
		if( ktail == NULL ) {
			newHdr = get_new_rkeyHdr2( inKey );
			if( newHdr == NULL ) return;

			oKeyInfo->k_tail = (void *)newHdr;
			oKeyInfo->keys   = (void *)newHdr;
			oKeyInfo->count++;

		} else if( mode == BACKWARD_IN ) {
			newHdr = get_new_rkeyHdr2( inKey );
			if( newHdr == NULL ) return;
		
			ktail->next = newHdr;
			newHdr->pre = ktail;
			oKeyInfo->k_tail = (void *)newHdr;

			oKeyInfo->count++;

		} else if( mode == FORWARD_IN ) {
			newHdr = get_new_rkeyHdr2( inKey );
			if( newHdr == NULL ) return;
		
			khead->pre   = newHdr;
			newHdr->next = khead;
			oKeyInfo->keys = (void *)newHdr;

			oKeyInfo->count++;

			if( oKeyInfo->count > total_thval ) {
				trim_key2_memory( khead, oKeyInfo, kType ); 	
			}

		} else { // RANDOM 
			struct hdr_rkey2	*curHdr;

			curHdr = khead;
			while( curHdr != NULL ) {
				if( curHdr->key_entry->seqn == inKey->seqn ) {
					// Existing entry  
					return ; 
				} else if( curHdr->key_entry->seqn > inKey->seqn ) {
					curHdr = curHdr->next; 
				} else break;   
			}

			newHdr = get_new_rkeyHdr2( inKey );
			if( newHdr == NULL ) return;
		
			if( curHdr == NULL ) { // insert tail 
				ktail->next = newHdr;
				newHdr->pre = ktail;
				oKeyInfo->k_tail = (void *)newHdr;
			} else if( curHdr == khead ) {
				khead->pre   = newHdr;
				newHdr->next = khead;
				oKeyInfo->keys = (void *)newHdr;
			} else {
				curHdr->pre->next = newHdr;
				newHdr->pre		  = curHdr->pre; 
				newHdr->next	  = curHdr;
				curHdr->pre		  = newHdr; 
			}

			oKeyInfo->count++;
			if( oKeyInfo->count > total_thval ) {
				trim_key2_memory( khead, oKeyInfo, kType ); 	
			}

		} // end of RANDOM 

	}

}



void prepare_keylogdata( LKEY_info *keyInfo, int inNum )
{
	// Read the key logs & Manage the key history 
	//		to support key distribution service 

	// -1 : no key log
	if( inNum != -1 ) load_keyhistories( keyInfo, inNum );


	if( keyInfo->rw_mode == 'r' ) {
		// subscribe the new generated key 
		EP_STAT					testat; 
		gdp_event_cbfunc_t		cbfunc = klogread_cb;

		testat =  gdp_gcl_subscribe( keyInfo->gcl, keyInfo->last_recn+1, 
						0, NULL, cbfunc, (void *)keyInfo );

		// error handling 
		if( EP_STAT_ISOK( testat ) ) 
					keyInfo->rw_mode = 's'; 
		//else; later try to subscribe again. (at another request handle time)  

	} else {
		// prepare key generation : currently no action 
		// prepare_key_generation( keyInfo );
	}

}


void reflect_klog_shutdown( LKEY_info *keyInfo )
{
	if( keyInfo->rw_mode == 'w' ) keyInfo->rw_mode = 'x';
	else if( keyInfo->rw_mode == 'r' || keyInfo->rw_mode == 's' ) 
		keyInfo->rw_mode = 't';

	gdp_gcl_close( keyInfo->gcl );
	keyInfo->gcl = NULL;

}


void update_rcv_key_datum( LKEY_info *keyInfo, gdp_datum_t *rDatum, 
							bool isMissing ) 
{

	// error check 
	if( rDatum == NULL || keyInfo == NULL ) {
		ep_app_error( "NULL data for process rcv key datum" );
		return ;
	}


	// Insert the rcv key info 
	if( isMissing == false ) {
		RKEY_1				*rKey	= NULL;
		KSD_info			*ksData	= NULL;



		//
		// Parse the recived key log info 
		if( keyInfo->param->sym_key_alg == 0 ) {
			char	tVal1, tVal2;
			rKey	 = convert_klog_to_RKEY( rDatum, &tVal1, &tVal2 ); 

			keyInfo->param->sym_key_alg  = (int)tVal1;
			keyInfo->param->sym_key_mode = (int)tVal2;

		} else	rKey = convert_klog_to_RKEY( rDatum, NULL, NULL ); 

		if( rKey == NULL ) {
			// Wrong Data if necessary re-try to read this key Info.. 
			// LATER :  at the case that this server requests the specific key  
			ep_app_error( "Fail to parse/convert the received key log" );
			return ; 
		}

		insert_key_log( rKey, keyInfo, RANDOM_IN );

		LIST_FOREACH( ksData, &(keyInfo->shlogs), loglist )
		{
			notify_change_info_toKS( ksData, false, rKey );	
		}

	}

	if( keyInfo->rw_mode == 'r' || keyInfo->rw_mode == 's' || 
											keyInfo->rw_mode == 't' ) {
		keyInfo->last_recn = gdp_gcl_getnrecs( keyInfo->gcl );

	} else ep_app_error( "Writer mode called in update_rcv_key_datum" );

}


RKEY_1* get_latest_key( LKEY_info *keyInfo ) 
{
	int			kType; 


	kType = check_key_chain_type2( keyInfo );
	if( kType < 0 ) {  // unknown type 
		ep_app_error( "[UNknown] K-chain type at mode %c \n", 
				keyInfo->rw_mode );
		return NULL;

	} else if( kType == 0 ) {  // multi chain 
		struct hdr_rkey1	*khead;
		
		khead =  (struct hdr_rkey1 *)(keyInfo->keys);
		if( khead == NULL ) return NULL;

		return khead->childh;

	} else { // 1 (no chain) , 2 (one chain) 
		struct hdr_rkey2	*khead;
		
		khead =  (struct hdr_rkey2 *)(keyInfo->keys);
		if( khead == NULL ) return NULL;
		
		return khead->key_entry;

	}

	return NULL; 
}


// called at key service creation time : cmd_create 
// valid request (with valid key log name) : return OK or NOTFOUND 
// distrubion request : valid request -> keep the gcl to key log  
//						invalid request -> deny request (no object) 
// gen/dist request : valid request -> return NOTFOUND (caller create to key log)
//					  invalid request -> check writer : me : OK keep gcl 
//												other: deny request  
int check_keylogname( LKEY_info *keylog ) 
{
	EP_STAT					estat;
	gdp_pname_t				pname;
	gdp_gcl_t				*gcl = NULL;



	if( keylog == NULL )		return EX_NOINPUT; 
	if( keylog->gcl != NULL )	return EX_OK;


	gdp_printable_name( keylog->klog_iname, pname );
	estat = gdp_gcl_open( keylog->klog_iname, GDP_MODE_RO, NULL, &gcl );

	// if key distribution request (r mode): must succeed 
	// if key gen & distribution request (w mode): must fail 
	if( !EP_STAT_ISOK( estat ) ) {
		// No existing log :: LATER GDP_NAK_R_NOROUTE (more detaied check) 
		if( keylog->rw_mode != 'w' ) {
			// NO key log 
			ep_dbg_printf( "[FAIL] Cannot Open GCL %s at mode %c \n",  
				pname, keylog->rw_mode );
			return EX_INVALIDDATA;

		} else return EX_NOTFOUND; 
	}

	// Open the GCL to the Key log 
	if( keylog->rw_mode != 'w' ) keylog->gcl = gcl; 
	else {
		// check whether the log is created by myself 
		gdp_gcl_open_info_t		*open_info = NULL;

		gdp_gcl_close( gcl ); 

		open_info = gdp_gcl_open_info_new();
		estat = gdp_gcl_open( keylog->klog_iname, GDP_MODE_AO, 
				open_info, &gcl );

		if( !EP_STAT_ISOK( estat ) ) {
			ep_dbg_printf( "[FAIL] Cannot Open GCL %s at append mode \n", pname); 
			if( open_info != NULL ) gdp_gcl_open_info_free( open_info );

			// LATER: more detailed error check 
			// Cur: assume it is not created by myself 
			return EX_INVALIDDATA;

		} else keylog->gcl = gcl; 

	} 
	
	return EX_OK;

}




int find_proper_keyrecnum( LKEY_info *keylog, EP_TIME_SPEC *rtime, 
														RKEY_1 **oKey ) 
{
	int			kType;
	RKEY_1		*rcvKey  = NULL;



	// Latest key is always managed in key histories (with time delay)

	kType = check_key_chain_type2( keylog ); 

	if( kType == 0 ) { // Multi Chain 
		struct hdr_rkey1	*khead, *ktail, *curHdr, *preHdr;
		
		khead =  (struct hdr_rkey1 *)(keylog->keys);
		ktail =  (struct hdr_rkey1 *)(keylog->k_tail); 

		// NO key? or Not loaded? 
		if( ktail == NULL ) {
			return -1;
			// LATER: retry load_key_data( keylog, -1 ); 
			//		However, periodic retry... 
		}

		
		if( ep_time_before( &(khead->latest_time), rtime ) ) {
			// No requested key 
			return -2;

		} else if( ep_time_before( rtime, &(ktail->childt->ctime) ) ) {
			// Isn't the loaded range 
			rcvKey = request_key_with_ts( keylog, rtime );

			if( rcvKey == NULL ) return -3; 
			else {
				insert_key_log( rcvKey, keylog, BACKWARD_IN );
				*oKey = rcvKey;
				return rcvKey->seqn; 
			}

		} else { // Find the chain including the the requested time 
			curHdr = khead;
			preHdr = NULL;
			while( curHdr != NULL ) {
				if( ep_time_before( &(curHdr->latest_time), rtime ) ) {
					break; 
				}
				preHdr = curHdr;
				curHdr = curHdr->next;
			}

			// cannot be NULL preHdr, curHdr (other chase) 
			if( preHdr==NULL || curHdr==NULL ) {
				ep_dbg_printf("[ERROR] find the chain : NULL %s \n",
										preHdr==NULL?"preHdr":"curHdr" );
				return -1; 
			}

			if( preHdr->childh->pre_seqn != curHdr->childh->seqn ) {
				// skipped hdr or skipped entry in curHdr 
				rcvKey = request_key_with_ts( keylog, rtime );

				if( rcvKey == NULL ) return -1; 
				else {
					insert_key_log( rcvKey, keylog, RANDOM_IN );
					*oKey = rcvKey;
					return rcvKey->seqn; 
				}

			} else {
				// find the closest key in preHdr 
				RKEY_1		*cKey, *pKey;

				pKey = NULL;
				cKey = preHdr->childh;

				while( cKey != NULL ) {
					if( ep_time_before( &(cKey->ctime), rtime ) ) {
						pKey = cKey;
						cKey = cKey->next; 
					} else break; 
				}

				if( cKey==NULL || pKey==NULL ) {
					ep_dbg_printf("[ERROR] find the chain : NULL in hdr %s \n",
										pKey==NULL?"pKey":"cKey" );
					return -1; 
				}

				*oKey = pKey; 
				return pKey->seqn;
			}

		}

	} else { // another key gen type (no or one chain) 
		struct hdr_rkey2	*khead, *ktail;
		
		khead =  (struct hdr_rkey2 *)(keylog->keys);
		ktail =  (struct hdr_rkey2 *)(keylog->k_tail); 

		// NO key? or Not loaded? 
		if( ktail == NULL ) {
			return -1;
			// LATER: retry load_key_data( keylog, -1 ); 
			//		However, periodic retry... 
		}

		
		if( ep_time_before( &(khead->latest_time), rtime ) ) {
			// No requested key 
			return -2;

		} else if( ep_time_before( rtime, &(ktail->latest_time) ) ) {

			// no entry 
			if( ktail->key_entry->seqn == 1 ) return -3; 

			// Isn't the loaded range 
			rcvKey = request_key_with_ts( keylog, rtime );

			if( rcvKey == NULL ) return -1; 
			else {
				insert_key_log( rcvKey, keylog, BACKWARD_IN );
				*oKey = rcvKey;
				return rcvKey->seqn; 
			}

		} else { // Find the entry  
			struct hdr_rkey2	*curHdr, *preHdr;

			preHdr = NULL;
			curHdr = khead;
			while( curHdr != NULL ) {
				if( ep_time_before( &(curHdr->latest_time), rtime ) ) {
					break;
				} 
				preHdr = curHdr;
				curHdr = curHdr->next;
			}

	
			// if preHdr time == rtime 
			if( preHdr->latest_time.tv_sec == rtime->tv_sec &&
					preHdr->latest_time.tv_nsec == rtime->tv_nsec ) {
				*oKey = preHdr->key_entry;
				return (*oKey)->seqn;

			} else if( curHdr->key_entry->seqn + 1 == 
											preHdr->key_entry->seqn ) {
				*oKey = preHdr->key_entry;
				return (*oKey)->seqn;

			} else {
				// skipped entry. 
				//LATER within delta time delay, one chain case : return preHdr
				rcvKey = request_key_with_ts( keylog, rtime );

				if( rcvKey == NULL ) return -1; 
				else {
					insert_key_log( rcvKey, keylog, RANDOM_IN );
					*oKey = rcvKey;
					return rcvKey->seqn; 
				}
			}


		} 

	}


	return -1;

}

RKEY_1* common_afterkeyread( LKEY_info *keylog, gdp_datum_t *rdatum, 
													EP_STAT	estat )
{
	if( EP_STAT_ISOK( estat )  ) {
		RKEY_1				*rKey = NULL;

		// Parse the recived key log info 
		if( keylog->param->sym_key_alg == 0 ) {
			char	tVal1, tVal2;
			rKey	 = convert_klog_to_RKEY( rdatum, &tVal1, &tVal2 ); 

			keylog->param->sym_key_alg  = (int)tVal1;
			keylog->param->sym_key_mode = (int)tVal2;

		} else	rKey = convert_klog_to_RKEY( rdatum, NULL, NULL ); 

		return rKey; 

	} else {
		//LATER: error check : when we need to retry gcl/close -> open 
		char		ebuf[100];

		ep_dbg_printf("GCL_read_key_err: %s \n", 
						ep_stat_tostr( estat, ebuf, sizeof ebuf) );

	}

	return NULL;
}


RKEY_1* request_key_with_ts( LKEY_info *keylog, EP_TIME_SPEC * refTime )
{
	EP_STAT				estat;
	gdp_gcl_t			*gcl	= keylog->gcl;
	gdp_datum_t			*datum  = gdp_datum_new(); 


	// GDP_STAT_GCL_NOT_OPEN : 
	estat = gdp_gcl_read_ts( gcl, refTime, datum );

	return common_afterkeyread( keylog, datum, estat );
}


RKEY_1* request_key_with_rec( LKEY_info *keylog, gdp_recno_t reqNum )
{
	EP_STAT				estat;
	gdp_gcl_t			*gcl	= keylog->gcl;
	gdp_datum_t			*datum  = gdp_datum_new(); 


	// GDP_STAT_GCL_NOT_OPEN : 
	estat = gdp_gcl_read( gcl, reqNum, datum );

	return common_afterkeyread( keylog, datum, estat );
}


RKEY_1* get_key_wrecno( LKEY_info *keylog,  gdp_recno_t reqNum )
{
	int			kType;
	RKEY_1		*rcvKey  = NULL;



	// Latest key is always managed in key histories (with time delay)

	kType = check_key_chain_type2( keylog ); 

	if( kType == 0 ) { // Multi Chain 
		struct hdr_rkey1	*khead, *ktail;
		
		khead =  (struct hdr_rkey1 *)(keylog->keys);
		ktail =  (struct hdr_rkey1 *)(keylog->k_tail); 

		// NO key? or Not loaded? 
		if( ktail == NULL ) {
			return NULL;
			// LATER: retry load_key_data( keylog, -1 ); 
			//		However, periodic retry... 
		}

		if( keylog->last_recn < reqNum ) return NULL;
		if( ktail->childt->seqn > reqNum ) {	

			// Isn't the loaded range 
			rcvKey = request_key_with_rec( keylog, reqNum );

			if( rcvKey == NULL ) return NULL; 
			else {
				insert_key_log( rcvKey, keylog, BACKWARD_IN );
				return rcvKey; 
			}

		} 
		
		// Find the chain including the the requested time 
		{
			struct hdr_rkey1	*curHdr;

			curHdr = khead;
			// Current NAVIE search
			while( curHdr != NULL ) {
				if( curHdr->childh->seqn > reqNum ) {
					if( curHdr->childh->pre_seqn >= reqNum ) {
						curHdr = curHdr->next;

					} else {
						// in this chain 
						RKEY_1		*curKey, *preKey; 

						preKey = curHdr->childh;
						curKey = curHdr->childh->next; 
						while( curKey != NULL ) {
							if( curKey->seqn > reqNum) {
								preKey = curKey;
								curKey = curKey->next;
							} else if( curKey->seqn == reqNum) return curKey;
							else break; 
						}

						// extract key from preKey value.. 
						// No insert : No ctime 
						rcvKey = extract_prev_key( keylog, preKey, reqNum);
						return rcvKey;
					}

				} else if( curHdr->childh->seqn == reqNum ) {
					// this entry... 
					rcvKey = curHdr->childh;
					return rcvKey;

				} else break; 
			}

			// skipped chain 
			rcvKey = request_key_with_rec( keylog, reqNum );

			if( rcvKey == NULL ) return NULL; 
			else {
				insert_key_log( rcvKey, keylog, BACKWARD_IN );
				return rcvKey; 
			}

		}

	} else { // another key gen type (no or one chain) 
		struct hdr_rkey2	*khead, *ktail;
		
		khead =  (struct hdr_rkey2 *)(keylog->keys);
		ktail =  (struct hdr_rkey2 *)(keylog->k_tail); 

		// NO key? or Not loaded? 
		if( ktail == NULL ) {
			return NULL;
			// LATER: retry load_key_data( keylog, -1 ); 
			//		However, periodic retry... 
		}


		if( keylog->last_recn < reqNum ) return NULL;
		if( ktail->key_entry->seqn > reqNum ) {	

			// Isn't the loaded range  (right thrshold ? check 5) LATER
			if( kType == 1 || (ktail->key_entry->seqn - reqNum) > 5 ) {
				rcvKey = request_key_with_rec( keylog, reqNum );

				if( rcvKey == NULL ) return NULL; 
				else {
					insert_key_log( rcvKey, keylog, BACKWARD_IN );
					return rcvKey; 
				}

			}

			// No insert : No ctime 
			rcvKey = extract_prev_key( keylog, ktail->key_entry, reqNum);
			return rcvKey;
		} 
		
		{ // Find the entry  
			struct hdr_rkey2	*curHdr, *preHdr;

			curHdr = khead->next;
			while( curHdr != NULL ) {
				if( curHdr->key_entry->seqn > reqNum ) curHdr = curHdr->next;
				else if( curHdr->key_entry->seqn == reqNum ) {
					return curHdr->key_entry;
				} else  break;
					
			}

			// Isn't the loaded range 
			preHdr = curHdr->pre;
			if( kType == 1 || (preHdr->key_entry->seqn - reqNum) > 5 ) {
				rcvKey = request_key_with_rec( keylog, reqNum );

				if( rcvKey == NULL ) return NULL; 
				else {
					insert_key_log( rcvKey, keylog, RANDOM_IN );
					return rcvKey; 
				}

			}

			// No insert : No ctime 
			rcvKey = extract_prev_key( keylog, preHdr->key_entry, reqNum);
			return rcvKey;
		} 

	}


	return NULL;  // non-reachable

}


// synchronous store 
// LATER: on failing to store the key, buffer & try to store later.  
int	store_new_generated_key( LKEY_info *keylog, RKEY_1 *newKey )
{
	int				exit_status = EX_OK; 
	EP_STAT			estat;
	gdp_datum_t		*datum = NULL;

	
	// 1. check : keylog->gcl status. if necessary, reopen 


	// 2. fill the newKey info in the request 
	datum = gdp_datum_new( );
	fill_dbuf_withKey( datum, newKey, keylog->param->sym_key_alg, 
										keylog->param->sym_key_mode ); 

	estat = gdp_gcl_append( keylog->gcl, datum );
	if( !EP_STAT_ISOK( estat ) ) {
		char		ebuf[100];

		ep_app_error("Append error: %s", 
						ep_stat_tostr( estat, ebuf, sizeof ebuf ) );

		// LATER : we must store the generated key info &  
		//			forward the key to the other key dist service. 
		// check more detailed error info to assign rw_mode 
		keylog->rw_mode = 'x';
		gdp_gcl_close( keylog->gcl );
		keylog->gcl = NULL;

		return EX_FAILURE;
	}
	keylog->last_recn++;

	return exit_status;
}




// NOTIFICATION is occurred the following cases. 
// 1. AC rule is changed  (deny: revocation) (both )  --- 
// 2. the time for key change elpased  (generation including case)
// 3. When receiving the changed key (distribution only case) ---  

/*
** 1. AC rule is changed  
*/
void notify_rule_change_toKS( LKEY_info *keylog, ACL_info *acInfo, 
									bool isDenyRule )
{
	KSD_info			*ksData;
	gdp_pname_t			pname;
	RKEY_1				*newKey = NULL;

	
	ep_dbg_printf( "[Rule %s] on acLog %s : Noti KeyLog %s \n", 
					isDenyRule?"REV":"ADD", 
					gdp_printable_name( acInfo->aclog_iname, pname ),
					gdp_printable_name( keylog->klog_iname,  pname ) );


	if( isDenyRule == false ) return;
	// 	One of key change is occurred on revocation (deny) 
	//  Existing subscription is affected on revocation 

	// KEY gen 
//	if( keylog->rw_mode == 'w' || keylog->rw_mode == 'x' ) {
	if( keylog->rw_mode == 'w' ) {
		newKey = alarm_rule_change( keylog  );
	}

	if( newKey != NULL ) {
		// store the new Key in key log 
		store_new_generated_key( keylog, newKey );
	}

	LIST_FOREACH( ksData, &(keylog->shlogs), loglist )
	{
		notify_change_info_toKS( ksData, isDenyRule, newKey );	
	}

}


