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
**  KSD_DATA_MANAGER - Manage the Key Service Data 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 



#include <stdio.h>
#include <string.h>

#include <gdp/gdp.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_crypto.h>
#include <hs/gcl_helper.h>
#include <hs/hs_hashchain.h>

#include "ksd_data_manager.h"
#include "ksd_key_gen.h"
#include "kds_pubsub.h"
#include "session_manager.h"


// Current Version. 
// Temporarily the list of logs managed in this key distribution service is 
#define		KS_OBJ_INFO	"/home/hsmoon/etc/kso_list.txt"
#define		DBG_DETAIL		
// Debug level value for Detailed debugging message
#define		TDLV		32			
#define		HT_SIZE		32			// LATER : INCREASE the number

#define	AC_LOG_SVR		"kr.re.etri.hsm.ld1"
#define	AC_LOG_KDIR		"/home/hsmoon/etc/keys/ac"


static EP_DBG	Dbg	= EP_DBG_INIT("kdist.ksdmanager",
							"KS Object Info Manager " );


FILE			*ksdFp = NULL;
bool			isChanged  = false;
bool			needUpdate = false;

static EP_THR_MUTEX		ksdMutex 
		EP_THR_MUTEX_INITIALIZER2( GDP_MUTEX_LORDER_LEAF );


hs_hashchain	*kslTable	= NULL;
hs_lnode		*aclist		= NULL;     
hs_lnode		*kloglist	= NULL;
// LATER :: treat in cb func registered in main. 


STAILQ_HEAD( slisthead, ac_log_data )	
		reWorkacl = STAILQ_HEAD_INITIALIZER( reWorkacl );


//
// INTERNAL  functions. 
// 
int	hash_folding1( int a_Tsize, int a_keysize, void *a_key ) 
{
	int			hashval = 0;
	int			ti;
	uint8_t		t_val;

	for( ti=0; ti<a_keysize; ti++ ) {
		t_val   = ((uint8_t*)a_key)[ti];
		hashval =  ( (hashval<<1) ^ ( t_val & 0xff ) ) % a_Tsize; 
	}

	return hashval;
}


KSD_info* get_new_ks_data( size_t a_len, char *a_name )
{
	KSD_info		*newInfo = NULL;

	
	if( a_len > GDP_GCL_PNAME_LEN ) {
		ep_dbg_printf( "[ERROR] Invalid Log pname %s \n", a_name ); 
		ep_dbg_printf( "   >>> length(%zu) must be shorter than %d\n", 
						a_len, GDP_GCL_PNAME_LEN ); 
		return newInfo; 
	}  


	newInfo = (KSD_info *)ep_mem_zalloc( sizeof(KSD_info) );
	if( newInfo == NULL ) {
		if( a_name == NULL ) 
			 ep_dbg_printf( "[ERROR] Cannot get memory for KDS_info\n"); 
		else ep_dbg_printf( "[ERROR] Cannot get memory for KDS_info %s\n",
							a_name ); 
		return newInfo; 
	}


	// related data log name... 
	if( a_len != 0 ) {
		strncpy( newInfo->dlog_pname, a_name, a_len );
		newInfo->dlog_pname[a_len] = '\0'; 
	}
	//newInfo->ac_data = NULL;
	//newInfo->key_data = NULL;

	newInfo->state = NEED_INIT;
	newInfo->isDuplicated = false;

	return newInfo;
}



ACL_info* get_new_ac_data( size_t a_len, char *a_name )
{
	ACL_info		*newInfo = NULL;

	
	if( a_len > GDP_GCL_PNAME_LEN ) {
		ep_dbg_printf( "[ERROR] Invalid AC Log pname %s \n", a_name ); 
		ep_dbg_printf( "   >>> length(%zu) must be shorter than %d\n", 
						a_len, GDP_GCL_PNAME_LEN ); 
		return newInfo; 
	}
	
	newInfo = (ACL_info *)ep_mem_malloc( sizeof(ACL_info) );
	if( newInfo == NULL ) {
		ep_dbg_printf( "[ERROR] Cannot get memory for ACL_info of %s \n", 
							a_name ); 
		return newInfo; 
	}

/*
	strncpy( newInfo->aclog_pname, a_name, a_len );
	newInfo->aclog_pname[a_len] = '\0'; 
	gdp_parse_name( newInfo->aclog_pname, newInfo->aclog_iname );
*/
	gdp_parse_name( a_name, newInfo->aclog_iname );

	newInfo->ref_count	= 1;
	newInfo->acr_type	= 0;

	newInfo->first_recn	= 0;
	newInfo->next_recn	= 1;
	newInfo->reInit_recn	= 0;
	newInfo->state			= NEED_INIT;
	newInfo->ref_time		= 0;

	newInfo->isAvailable	= false;
//	newInfo->reSubscription = false;
//	newInfo->retry_count = 0;


	newInfo->gcl		= NULL;
	newInfo->head		= NULL;
	newInfo->acrules	= NULL;

	LIST_INIT( &(newInfo->skeys) );

	// LATER update
	return newInfo;
}


void free_ksdinfo( void *a_val ) 
{
	KSD_info	*ksData = (KSD_info *)a_val;


	if( ksData->gcl != NULL ) gdp_gcl_close( ksData->gcl );
	if( ksData->writer_sub != NULL ) _gdp_req_free( &ksData->writer_sub );
	close_allSession( ksData );


	ep_mem_free( ksData );
}


void free_aclinfo( void *a_val ) 
{
	ACL_info			*t_val = (ACL_info *)a_val;
	struct gdp_datum	*t_datum = t_val->head;


	if( t_val->gcl != NULL ) gdp_gcl_close( t_val->gcl );

	// Free gdp_datum buffer list
	while( t_datum != NULL ) {
		t_val->head = t_datum->next;
		gdp_datum_free( t_datum );
		t_datum = t_val->head; 
	}

	free_ac_rule( t_val->acr_type, t_val->acrules );

	ep_mem_free( t_val );
}


void free_light_aclinfo( void *a_val ) 
{
	ACL_info			*acInfo = (ACL_info *)a_val;
	struct gdp_datum	*t_datum = acInfo->head;


	if( acInfo->gcl != NULL ) {
		gdp_gcl_close( acInfo->gcl );
		acInfo->gcl = NULL;
	}

	acInfo->reInit_recn = acInfo->next_recn;
	acInfo->state		= NEED_AC_SUBSCRIBE;
	acInfo->ref_time	= 0;

	// remove the buffered entry 
	while( t_datum != NULL ) {
		acInfo->head = t_datum->next;
		gdp_datum_free( t_datum );
		t_datum = acInfo->head; 
	}

}


int refresh_ks_info_file( ) 
{
	int				ti = 0;
	char			trw_mode; 
	hs_lnode		*t_cur = NULL;
	KSD_info		*t_info = NULL;
	LKEY_info		*lkey	= NULL;

	if( kslTable == NULL ) return EX_NOINPUT;

	if( ksdFp != NULL ) fclose( ksdFp );

	ksdFp = fopen( KS_OBJ_INFO, "w" );
	if( ksdFp == NULL ) {
		ep_dbg_printf( "[ERROR] Cannot Open the FILE %s \n", KS_OBJ_INFO ); 
		return EX_NOINPUT;
	}


	for( ti=0; ti<kslTable->hash_size; ti++ ) {
		t_cur = kslTable->htable[ti];

		while( t_cur != NULL ) {
			t_info = (KSD_info *)(t_cur->nval);
			lkey   = (LKEY_info *)(t_info->key_data->nval);
			if( lkey->rw_mode=='w' || lkey->rw_mode=='x' ) trw_mode = 'w';
			else trw_mode = 'r';

			fprintf( ksdFp, "%c\t%s\t%s\t%s\t%s\t%s\t%c\t%d\n", '+',  
				t_info->dlog_pname,	t_info->ac_data->idname, t_cur->idname, 
				t_info->key_data->idname, t_info->wdid_pname, 
				trw_mode, lkey->kgen_inx );

			t_cur = t_cur->next;	
		}

	}

	
	needUpdate = false; 

	return EX_OK;

}


EP_STAT advertise_all_ksd(gdp_buf_t *dbuf, void *ctx, int cmd) 
{
	int				ti      = 0;
	hs_lnode		*t_cur  = NULL;
	gdp_name_t		ad_name;
	gdp_pname_t		pname;
	bool			isAd    = false;
	EP_STAT			estat; 


	// no data 
	if( kslTable == NULL ) return EP_STAT_OK;

	for( ti=0; ti<kslTable->hash_size; ti++ ) {
		t_cur = kslTable->htable[ti];

		while( t_cur != NULL ) {
			estat = gdp_parse_name( t_cur->idname, ad_name );
			if( EP_STAT_ISOK( estat ) ) {
				gdp_buf_write( dbuf, ad_name, sizeof(gdp_name_t) ); 
				isAd = true; 
			} else isAd = false;

			ep_dbg_printf( "[AD:%c] (%zd)%s : %s \n", isAd?'S':'F', 
						t_cur->idlen, t_cur->idname, 
						gdp_printable_name( ad_name, pname ) );

			t_cur = t_cur->next;	
		}

	}


	return EP_STAT_OK;
}




// external api 
int update_ac_node( KSD_info *ksData, char *aname, int alen )
{
	hs_lnode		*ac_node = NULL;
	ACL_info		*ac_data = NULL;


	// ALL ALLOW 
	if( alen == 0 ) {
		ksData->ac_data = NULL;
		return EX_OK; 
	}

	// treat ac rule log info. 
	ac_node = insert_inlist( &aclist, alen, aname );
	if( ac_node->nval == NULL ) {
		ac_data = get_new_ac_data( alen, aname );
		if( ac_data == NULL ) {
			ep_dbg_printf("[ERROR] Fail to manage the ac data"
							" for %s \n",aname); 
			return	EX_MEMERR;
		} 
		ac_node->nval = ac_data;

	} else {
		ac_data = (ACL_info *)(ac_node->nval);
		ac_data->ref_count += 1; 
	}

	ksData->ac_data = ac_node;

	return EX_OK;
}

// 
// external api 
//

/*
** Prepare the ACL_info : update ACL_info & load rules. 
** This function is called at three cases. 
**	  First, Service init time: when preparing the key distribution service
**    Second, dynamic Key service request when the ac rule is used firstly 
**	  Third, restoration after gcl connection error with the related ac log  
**	Return value: EX_OK / EN_NOINPUT (gcl open error, null input)
*/
int load_ac_rules( ACL_info *ac_info ) 
{
	gdp_pname_t			pname;
	EP_STAT				estat;
	gdp_gcl_t			*gcl = NULL;

	
	if( ac_info == NULL ) return EX_NOINPUT;

	gdp_printable_name( ac_info->aclog_iname, pname );

	// When error occurrs, gcl is closed and set to NULL
	if( ac_info->gcl != NULL ) {
		// Already loaded or is loading. 
		ep_dbg_printf( "--- Already loaded / loading AC info %s \n", pname );
		return EX_OK; 
	}

	ep_thr_mutex_lock( &ac_info->mutex );

	// Set the ACR type info. 
	if( ac_info->state == NEED_INIT ) {
		find_gdp_gclmd_uint( gcl, GDP_GCLMD_ACTYPE, &(ac_info->acr_type) );
		if( isSupportedACR( ac_info->acr_type ) == false ) {
			// delete or flag. 
			ac_info->isAvailable = false;
			ep_thr_mutex_unlock( &ac_info->mutex );
			return EX_OK;
		}
		ac_info->isAvailable = true;
	}

	// Open the GCL to the AC log 
	estat = gdp_gcl_open( ac_info->aclog_iname, GDP_MODE_RO, NULL, &gcl );
	if( !EP_STAT_ISOK( estat ) ) {
		ep_dbg_printf( "[FAIL] Cannot Open GCL %s \n", pname );
		ep_thr_mutex_unlock( &ac_info->mutex );
		return EX_NOINPUT;
	}
	ac_info->gcl		= gcl;
	ac_info->last_recn  = gdp_gcl_getnrecs( gcl );

	ep_thr_mutex_unlock( &ac_info->mutex );

	// Asynchronously read & subscribe  
	// Because this service can receive the ac data by subscription 
	//		while it requests the read all ac data from the other gcl. 
	read_all_ac_data(gcl, ac_info); 

	return EX_OK;
}



// external api 
int update_key_node( KSD_info *ksData, char *kname, int klen, char krw_mode ) 
{
	hs_lnode		*key_node = NULL;
	LKEY_info		*key_data = NULL;


	if( klen == 0 ) return EX_INVALIDDATA;

	key_node = insert_inlist( &kloglist, klen, kname );
	if( key_node->nval == NULL ) {
		key_data = get_new_klinfo( klen, kname, krw_mode, -1 );

		if( key_data == NULL ) {
			ep_dbg_printf("[ERROR] Fail to manage the kl data"
									" for %s \n",kname);
			return	EX_MEMERR;
		} 
		key_node->nval = key_data;

		LIST_INSERT_HEAD( &(key_data->shlogs), ksData, loglist); 

	} else {
		key_data = key_node->nval;
		key_data->ref_count += 1; 

		LIST_INSERT_HEAD( &(key_data->shlogs), ksData, loglist); 
	}

	return EX_OK; 
}

// Extract information from the metadata  of key log 
// Store the extracted info (ctime, pubkey) in LKEY_info->gmd 
int update_kl_gmd( LKEY_info *keylog, gdp_gcl_t *gcl )
{
	int					tti; 
	int					tt_num = 0;
	EP_STAT				estat;
	gdp_gclmd_t			*gmd   = NULL;
//	bool				isSubKlog = false;


	if( keylog == NULL ) return EX_NOINPUT;
	if( gcl    == NULL ) return EX_NOINPUT;

	// LATER: check the necessary field 
	if( keylog->gmd != NULL ) return EX_OK;

	estat = gdp_gcl_getmetadata( gcl, &gmd );
	if( !EP_STAT_ISOK( estat ) )  {
		return EX_FAILGETDATA; 
	}


	keylog->gmd = gdp_gclmd_new( 0 );

	for( tti=0; tti<(gmd->nused); tti++ ) {

		if( gmd->mds[tti].md_id == GDP_GCLMD_CTIME ) {
			
			gdp_gclmd_add( keylog->gmd, GDP_GCLMD_CTIME, 
							gmd->mds[tti].md_len, 
							gmd->mds[tti].md_data );

//			ep_time_parse( md_data, &(keylog->ctime), EP_TIME_FMT_DEFAULT );
			tt_num++;
/*
		} else if( gmd->mds[tti].md_id == GDP_GCLMD_SUBKLOG ) {
			// init long_period : 1 is default ...  
			isSubKlog = true;
			memcpy( &(keylog->sub_iname), gmd->mds[tti].md_data, 
						gmd->mds[tti].md_len );
			tt_num++;
*/					
		} else if( gmd->mds[tti].md_id == GDP_GCLMD_PUBKEY ) {
			gdp_gclmd_add( keylog->gmd, GDP_GCLMD_PUBKEY, 
							gmd->mds[tti].md_len, 
							gmd->mds[tti].md_data );
			tt_num++;

		} else continue; 

		if( tt_num == 2 ) break; 
	}

	if( tt_num != 2 ) {
		ep_app_error("Wrong Metadata of key log ");
		return EX_INVALIDDATA;
	}

	return EX_OK;
}

// external api 
int load_key_data( KSD_info	*ksData, int inNum , LKEY_info *inKeylog ) 
{
	EP_STAT					estat;
	gdp_gcl_t				*gcl = NULL;
	gdp_gcl_open_info_t		*open_info = NULL;
	LKEY_info				*keylog = NULL;


	// currnet : NULL check before calling.. 
	if( ksData == NULL && inKeylog == NULL ) return EX_NOINPUT;

	if( ksData == NULL ) keylog = inKeylog; 
	else	keylog = (LKEY_info *)(ksData->key_data->nval);

	if( keylog->gcl != NULL ) goto step1;

	// Open the GCL to the Key log 
	if( keylog->rw_mode != 'w' && keylog->rw_mode != 'x' ) {
		estat = gdp_gcl_open( keylog->klog_iname, GDP_MODE_RO, NULL, &gcl );
	} else {
		open_info = gdp_gcl_open_info_new();
		estat = gdp_gcl_open( keylog->klog_iname, GDP_MODE_AO, 
				open_info, &gcl );
	}

	if( !EP_STAT_ISOK( estat ) ) {
		gdp_pname_t			pname;

		gdp_printable_name( keylog->klog_iname, pname );
		ep_dbg_printf( "[FAIL] Cannot Open GCL %s at mode %c \n",  
				pname, keylog->rw_mode );
		if( open_info != NULL ) gdp_gcl_open_info_free( open_info );

		if( keylog->rw_mode == 'w' ) keylog->rw_mode = 'x';
		if( keylog->rw_mode == 'r' ) keylog->rw_mode = 't';
		return EX_NOINPUT;

	} else {
		if( keylog->rw_mode == 'x' ) keylog->rw_mode = 'w';
		if( keylog->rw_mode == 't' ) keylog->rw_mode = 'r';
	}

	// ctime is really necessary?
	// IF necessary, later update ctime from metadata . (check load_ init )
	update_kl_gmd( keylog, gcl );
	keylog->last_recn = gdp_gcl_getnrecs( gcl );
	keylog->gcl = gcl;


step1:
	// Asynchronously read & subscribe  
	// Because this service can receive the ac data by subscription 
	//		while it requests the read all ac data from the other gcl. 
	prepare_keylogdata( keylog, inNum );
	// -1 of inNum is called at creation time . 
	// no key log. This service creates the key 

	return EX_OK;

}


// external api 
int create_key_log( KSD_info	*ksData ) 
{
	int						exit_status = EX_OK;
	EP_STAT					estat;
	LKEY_info				*keylog = NULL;
	gdp_gcl_t				*gcl = NULL;
	gdp_name_t				logdiname;
	const char				*logdxname = NULL;
	gdp_gclmd_t				*gmd =  NULL;

	int						keylen		= -1;
	int						keytype     = EP_CRYPTO_KEYTYPE_EC;
	int						key_enc_alg = EP_CRYPTO_SYMKEY_AES192;
	const char				*keydir		= NULL;
	EP_CRYPTO_KEY			*wKey       = NULL;

	char					*tempkeyfile = NULL;
	char					*finalkeyfile = NULL;



	// Handling for writing key 
	if (key_enc_alg < 0)
	{
		const char *p = ep_adm_getstrparam("swarm.gdp.crypto.keyenc.alg",
							"aes192");
		key_enc_alg = ep_crypto_keyenc_byname(p);
	}

	wKey = make_new_asym_key( keytype, &keylen );
	if( wKey == NULL ) {
		ep_app_error("Cannot create the new asym key for writing \n");  
		return  EX_CANTCREAT;
	}

	// Write secret key in the file. 
	keydir = get_keydir( AC_LOG_KDIR );
	tempkeyfile = write_secret_key( wKey, keydir, key_enc_alg );
	if( tempkeyfile == NULL ) {
		ep_app_error("Couldn't write secret key");
		ep_crypto_key_free( wKey );
		return EX_IOERR;
	}


	// currnet : NULL check before calling.. 
	keylog = (LKEY_info *)(ksData->key_data->nval);

	logdxname = select_logd_name( AC_LOG_SVR );
	gdp_parse_name( logdxname, logdiname );

	gmd = gdp_gclmd_new(0);
	add_time_in_gclmd( gmd );
	add_creator_in_gclmd( gmd );
	gdp_gclmd_add( gmd, GDP_GCLMD_TYP, 2, "KL" );
	gdp_gclmd_add( gmd, GDP_GCLMD_XID, strlen(ksData->key_pname), 
							ksData->key_pname );
	// can be multiple
//	gdp_gclmd_add( gmd, GDP_GCLMD_DLOG, strlen(ksData->dlog_pname), 
//							ksData->dlog_pname );
	gdp_gclmd_add( gmd, GDP_GCLMD_ACLOG, strlen(ksData->ac_pname), 
							ksData->ac_pname );

	exit_status = add_pubkey_in_gclmd( gmd, wKey, keytype, keylen, key_enc_alg );
	if( exit_status != 0 ) {
		ep_app_error("Couldn't add pub key \n");
		ep_crypto_key_free( wKey );
		ep_mem_free( tempkeyfile );
		return EX_FAILURE;
	}


	// create a GCL with the provided name
	estat = gdp_gcl_create( keylog->klog_iname, logdiname, gmd, &gcl );
	if( !EP_STAT_ISOK(estat) ) {
		ep_app_error("Couldn't create GCL for %s at %s\n", 
							ksData->key_pname, logdxname );
		ep_crypto_key_free( wKey );
		ep_mem_free( tempkeyfile );
		return EX_FAILURE;
	}

	finalkeyfile = rename_secret_key( gcl, keydir, tempkeyfile );
	if( finalkeyfile == NULL ) 
	{
		ep_mem_free( tempkeyfile );
		ep_crypto_key_free( wKey );
		gdp_gcl_close(gcl);
		return  EX_FILERENAME;
	} 


	ep_mem_free( tempkeyfile );
	ep_mem_free( finalkeyfile );
	ep_crypto_key_free( wKey );
	gdp_gcl_close(gcl);

	load_key_data( ksData, -1 , NULL );

	return EX_OK;

}


// external api 
int get_ksd_handle( gdp_name_t exkl_iname, KSD_info **outData, bool isNew, 
					char *dname, int dlen )
{
	hs_lnode		*cur_ent = NULL;
	gdp_pname_t		exkl_pname;
	KSD_info		*ksData = NULL;


	// extract the pname  from iname 
	gdp_printable_name( exkl_iname, exkl_pname);


	if( isNew == false )  {
		// find the KSD node. 
		ep_thr_mutex_lock( &ksdMutex  );
		cur_ent = lookup_entry_in_htable( kslTable, 
					strlen(exkl_pname), exkl_pname ); 
		ep_thr_mutex_unlock( &ksdMutex  );

		if( cur_ent == NULL ) return EX_NOTFOUND;
		if( cur_ent->nval == NULL ) return EX_UNAVAILABLE;

		*outData = (KSD_info *)(cur_ent->nval);
		return EX_OK;

	}

	//
	// isNew true case 

	// find the KSD node. If there is no KSD node related with pname, 
	//		make & initialize the node 
	//
	ep_thr_mutex_lock( &ksdMutex  );
	cur_ent = insert_entry_in_htable( kslTable, 
					strlen(exkl_pname), exkl_pname ); 
	ep_thr_mutex_unlock( &ksdMutex  );


	if( cur_ent==NULL  ) { 
		// No existing node / fail to make the new node  
		ep_dbg_printf("[ERROR] Fail to manage the key service data"
						" for %s\n", exkl_pname );
		return	EX_MEMERR;
	}


	// Key distribution object already exists  
	//	previously, this service received the service request  
	//		ongoing request --> internal store --> init 
	if( cur_ent->nval != NULL ) {
		// Check whether or not this is  duplicated request 
		*outData = (KSD_info *)(cur_ent->nval);

		if( strncmp( (*outData)->dlog_pname, dname, dlen ) == 0 ) {
			ep_dbg_printf("Duplicatd Key service request for %s\n", 
							(*outData)->dlog_pname );
			return EX_OK; 

		} else { 
			ep_dbg_printf("Service Conflict at %s : %s vs. %s \n",  
						exkl_pname, (*outData)->dlog_pname, dname );
			return EX_CONFLICT; 
		}
	}


	// make & assign nval
	ksData = get_new_ks_data( dlen, dname );
	if( ksData == NULL ) return EX_MEMERR; 

	cur_ent->nval = ksData;
	*outData      = ksData;

	return EX_OK; 
}



// external api
void cancel_ksd_handle( KSD_info *ksData, bool full_deletion )
{
	LKEY_info	*kinfo = (LKEY_info *)(ksData->key_data->nval);
	ACL_info	*ainfo = (ACL_info  *)(ksData->ac_data->nval);


	// refcount related debugging 
	printf("[DBG] cancel_ksd_handle is called for %s \n", 
					ksData->dlog_pname);

	if( kinfo != NULL ) {
		LIST_REMOVE( ksData, loglist );	
		if( ainfo!= NULL ) LIST_REMOVE( kinfo, klist ); 
	}


	if( ainfo!= NULL ) {
		if( ainfo->ref_count == 1 ) {
			if( full_deletion ) {
				// delete in list 
				free_aclinfo( (void *)ainfo ); 
			} else free_light_aclinfo( (void *)ainfo );

		}  else ainfo->ref_count--;
	}

	if( kinfo!= NULL ) {
		if( kinfo->ref_count == 1 ) {
			if( full_deletion ) {
				//delete in list 
				delete_node_inlist( &kloglist, ksData->key_data ); 
				free_lkinfo( (void *)kinfo ); 
			}  else free_light_lkinfo( (void *)kinfo ) ;

		} else kinfo->ref_count--; 
	}

	if( full_deletion ) free_ksdinfo( ksData );
	else { 
		ksData->gcl = NULL;
	}

}


// wch_start
// Read the file and initialize hash table, ac list, and key log list.  
// Return value : success or error_num 
int init_ks_info_from_file(  )
{
	int			tval;

	char		cmd; 	
	int			kgenfun;
	char		krw_mode;
	char		lbuf[1024];
	char		logn[CBUF_LEN];
	char		acln[CBUF_LEN];
	char		exkl[CBUF_LEN];
	char		ksln[CBUF_LEN];
	char		wdid[CBUF_LEN];

	int			line_num = 0;
	int			active_kso_count = 0;
	bool		reWrite = false;

	hs_lnode	*cur_ent;
	KSD_info	*ksData;
	hs_lnode	*ac_node;
	ACL_info	*ac_data; 
	hs_lnode	*key_node;
	LKEY_info	*kl_data;



	ksdFp = fopen( KS_OBJ_INFO, "a+" );
	if( ksdFp == NULL ) {
		ep_dbg_printf( "[ERROR] Cannot Open the FILE %s \n", KS_OBJ_INFO ); 
		return EX_NOINPUT;
	}



	// LATER: this file info must be managed securely. 
	//	 ver 1. plain text [current version]
	//	 ver 2. encrypted  [line by encryption / file or 
	//											log (direct use of diskImpl)] 
	//	 ver 3. secure container

	// Read the key service data for each log handled in this service from the file 
	while( feof( ksdFp ) == 0 ) {
		if( fgets( lbuf, 1024, ksdFp )  == NULL ) {
			ep_dbg_cprintf( Dbg, 3, "No existing AC Rule \n" ); 
			break;
		} 

		tval = sscanf( lbuf, "%c\t%s\t%s\t%s\t%s\t%s\t%c\t%d\n", 
					&cmd, logn, acln, exkl, ksln, wdid, &krw_mode, &kgenfun);
		if( tval != 8 ) {
			ep_dbg_cprintf( Dbg, 3, "Fail to Read key service log info"
							" at [%zu]%s\n", sizeof(lbuf), lbuf );
			return	EX_INVALIDDATA;
		}

#ifdef DBG_DETAIL
		ep_dbg_cprintf( Dbg, TDLV, "%d> [%c] [%zu]%s [%zu]%s [%zu]%s "
					"[%zu]%s [%zu]%s %c %d \n", 
					line_num, cmd, strlen(logn), logn, 
					strlen(acln), acln, strlen(exkl), exkl, 
					strlen(ksln), ksln, strlen(wdid), wdid, krw_mode, kgenfun);

		// At current version, [exkl] : KS_[logn] 
		// on current routing model, [ksln]: KEY_[logn] 
		// Because of key sharing, in some log,  [ksln] = its [ksln] 
		//	In other log, [ksln] = the other log's [ksln]
#endif

		// CHECK: ENTRY VALUE :  (gdp_pname_t vs. gdp_name_t)
		if( cmd == '+' ) {
			// Init routine so NOT necessary
			//ep_thr_mutex_lock( &ksdMutex  );
			cur_ent = insert_entry_in_htable( kslTable,	
										strlen(exkl), exkl ); 
			//ep_thr_mutex_unlock( &ksdMutex  );

			if( cur_ent==NULL  ) { 
				// error 1
				ep_dbg_printf("[ERROR] Fail to manage the key service"
								" data for %s \n", exkl );
				ep_dbg_printf( "   >>> %s \n", lbuf ); 
				return	EX_MEMERR;
			}

			if( cur_ent->nval != NULL ) {
				// error 2 
				ep_dbg_printf("[ERROR] Conflict the key service "
								"data for %s \n", exkl); 
				ep_dbg_printf( "   >>> %s \n", lbuf ); 
				return	EX_INVALIDDATA;
			}


			// make & assign nval
			ksData = get_new_ks_data( strlen(logn), logn );
			cur_ent->nval = ksData;

			strncpy( ksData->ac_pname, acln, strlen(acln) );
			strncpy( ksData->key_pname, ksln, strlen(ksln) );
			strncpy( ksData->wdid_pname, wdid, strlen(wdid) );
			ksData->ac_pname[strlen(acln)]  = '\0';
			ksData->key_pname[strlen(ksln)] = '\0';
			ksData->wdid_pname[strlen(wdid)] = '\0';

			// treat ac rule log info. 
			ac_node = insert_inlist( &aclist, strlen(acln), acln );
			if( ac_node->nval == NULL ) {
				ac_data = get_new_ac_data( strlen(acln), acln );
				if( ac_data == NULL ) {
					ep_dbg_printf("[ERROR] Fail to manage the ac data"
									" for %s \n",acln); 
					return	EX_MEMERR;
				} 
				ac_node->nval = ac_data;

			} else {
				ac_data = ac_node->nval;
				ac_data->ref_count += 1; 
			}
			ksData->ac_data = ac_node;


			// treat key log info 
			// Assumption :: On creating this info, assumption is checked. 
			//		So we don't check here)
			key_node = insert_inlist( &kloglist, strlen(ksln), ksln );
			if( key_node->nval == NULL ) {
				kl_data = get_new_klinfo( strlen(ksln), ksln, 
									krw_mode, kgenfun );

				if( kl_data == NULL ) {
					ep_dbg_printf("[ERROR] Fail to manage the kl data"
									" for %s \n",ksln);
					return	EX_MEMERR;
				} 
				key_node->nval = kl_data;

				// each key has only one related ac node... 
				LIST_INSERT_HEAD( &(ac_data->skeys), kl_data, klist); 
				LIST_INSERT_HEAD( &(kl_data->shlogs), ksData, loglist); 

			} else {
				kl_data = key_node->nval;
				kl_data->ref_count += 1; 

				LIST_INSERT_HEAD( &(kl_data->shlogs), ksData, loglist); 

				// we can check assumption with additional debugging info 
				//		(previous ac log name) 
			}
			ksData->key_data = key_node;

			active_kso_count++;


		} else if( cmd == '-' ) {
			reWrite = true; 
			
			cur_ent = delete_entry_in_htable( kslTable, 
							strlen(exkl), exkl ); 

			if( cur_ent==NULL  || cur_ent->nval==NULL ) { 
				ep_dbg_printf( "[ERROR] Fail to find the key service"
								" data for %s \n", exkl );
				ep_dbg_printf( "   >>> %s \n", lbuf ); 
				return	EX_INVALIDDATA;

				// LATER: print warning and continue... 
			}
			ksData = cur_ent->nval;

			// update ac & key log info 
			// if necessary, delete ac info (free memory : LATER: pool )
			// if necessary, delete key log info (free memory : LATER: pool )
			ac_node = ksData->ac_data;
			ac_data = (ACL_info *)(ac_node->nval);
			ac_data->ref_count -= 1;

			key_node = ksData->key_data;
			kl_data  = (LKEY_info *)(key_node->nval);
			kl_data->ref_count -= 1;
			LIST_REMOVE( ksData, loglist );	

			if( kl_data->ref_count ==  0 ) {
				key_node = delete_node_inlist( &kloglist, key_node );
				if( key_node == NULL ) {
					ep_dbg_printf( "[ERROR] Fail to delete the key"
							" node for %s \n", ksData->key_data->idname);
					return	EX_INVALIDDATA;

					// LATER: print warning and continue... 
				}
				LIST_REMOVE( kl_data, klist); 

				// free memory
				free_lkinfo( key_node->nval );
				ep_mem_free( key_node->idname );
				ep_mem_free( key_node );
			}

			if( ac_data->ref_count ==  0 ) {
				ac_node = delete_node_inlist( &aclist, ac_node );
				if( ac_node == NULL ) {
					ep_dbg_printf( "[ERROR] Fail to delete the ac rule"
							" node for %s \n", ksData->ac_data->idname );
					return	EX_INVALIDDATA;

					// LATER: print warning and continue... 
				}

				// free memory
				free_aclinfo( ac_node->nval );
				ep_mem_free( ac_node->idname );
				ep_mem_free( ac_node );
			}


			// delete the key service data 
			free_ksdinfo( ksData );
			ep_mem_free( cur_ent->idname );
			ep_mem_free( cur_ent );

			active_kso_count--;

		} else {
			ep_dbg_printf( "[ERROR] Invalid data format in %s \n", 
							KS_OBJ_INFO ); 
			ep_dbg_printf( "   >>> %s \n", lbuf ); 
			continue;
		}
		line_num++;

	}


	// if necessary, re write the file (with only + cmd) 
	if( reWrite ) {
		ep_dbg_printf( "[INFO] Rewrite File : line %d, active count %d \n", 
						line_num, active_kso_count ); 
		refresh_ks_info_file( ); 
	}


	return EX_OK;

}


//
// EXTERNAL  functions
// 

// MUST LATER UPDATE
void exit_ks_info_manager()
{
	if( isChanged ) printf("[INFO] KS service object is changed \n" );


	// file update... 
	if( needUpdate )  refresh_ks_info_file(); 
	if( ksdFp != NULL ) fclose( ksdFp );

	
	// free list 
	free_list( aclist, free_aclinfo );
	free_list( kloglist, free_lkinfo );


	// free hash table.. 
	free_hashtable( kslTable, free_ksdinfo );

	exit_key_data();
	exit_session_manager( );

}


//
// Initialize log list info assinged to this key distribution service. 
// Currently, this info is managed in the file (LATER: change) 
int init_ks_info_before_chopen() 
{
	int			exit_stat = EX_OK;

	kslTable = get_new_hashtable( HT_SIZE, hash_folding1 ); 
	if( kslTable == NULL ) {
		return EX_MEMERR;
	}

	// Read the file and initialize hash table, ac list, and key log list.  
	exit_stat = init_ks_info_from_file();

	init_session_manager( );

	return exit_stat;
}


/*
void request_retry_read_ac_data( ACL_info *a_acInfo )
{
	STAILQ_INSERT_TAIL( &reWorkacl, a_acInfo, dolist );
}
*/



// check_start
/*
** Load the AC rule info & Key data already set. 
** This function is called at initialization time of the service. 
**		Right after channel open & before the advertisement 
** (Need not to consider multi-thread problem)
*/
int load_ks_info()
{
	int				exit_stat = EX_OK;
	EP_STAT			estat = EP_STAT_OK;

	gdp_pname_t		pname;

	hs_lnode		*t_cur   = NULL;
	ACL_info		*ac_info = NULL;
	LKEY_info		*keylog  = NULL;


	//
	// Read & Subscribe the AC log info 
	// 
	t_cur = aclist;
	while( t_cur != NULL ) {
		ac_info =  (ACL_info *)(t_cur->nval); 

#ifdef DBG_DETAIL
		ep_dbg_printf( "--- Init AC info %s : %s \n",  t_cur->idname, 
							gdp_printable_name( ac_info->aclog_iname, pname ) );
#endif
		exit_stat = load_ac_rules( ac_info ); 
		if( exit_stat != EX_OK ) {
			ep_dbg_printf( "[FAIL] Cannot Open GCL %s \n",  t_cur->idname );
			return EX_NOINPUT;
		}

	}
// check_end 


	//
	// Read & Subscribe the Key log info  
	//
	t_cur = kloglist;
	while( t_cur != NULL ) {
		gdp_gcl_t				*gcl = NULL;
		gdp_gcl_open_info_t		*open_info = NULL;

		keylog =  (LKEY_info *)(t_cur->nval); 

#ifdef DBG_DETAIL
		ep_dbg_printf( "--- Init Key info %s : %s \n",  t_cur->idname, 
							gdp_printable_name( keylog->klog_iname, pname ) );
#endif

		// Open the GCL to the Key log 
		if( keylog->rw_mode != 'w' ) {
			estat = gdp_gcl_open( keylog->klog_iname, GDP_MODE_RO, NULL, &gcl );
		} else {
			open_info = gdp_gcl_open_info_new();
			estat = gdp_gcl_open( keylog->klog_iname, GDP_MODE_AO, 
					open_info, &gcl );
		}

		if( !EP_STAT_ISOK( estat ) ) {
			ep_dbg_printf( "[FAIL] Cannot Open GCL %s at mode %c \n",  
					t_cur->idname, keylog->rw_mode );
			if( open_info != NULL ) gdp_gcl_open_info_free( open_info );

			if( keylog->rw_mode == 'w' ) keylog->rw_mode = 'x';
			if( keylog->rw_mode == 'r' ) keylog->rw_mode = 't';
			return EX_NOINPUT;
		}

		update_kl_gmd( keylog, gcl );

		keylog->last_recn = gdp_gcl_getnrecs( gcl );
		keylog->gcl = gcl;

		// Asynchronously read & subscribe  
		// Because this service can receive the ac data by subscription 
		//		while it requests the read all ac data from the other gcl. 
		prepare_keylogdata( keylog, 0);

	}

	return exit_stat;
}



//EP_STAT process_ac_event(gdp_event_t *gev, bool subscribe, char event_call)
EP_STAT process_event(gdp_event_t *gev, bool subscribe, char event_call)
{
	bool			isBuffered = false;
	EP_STAT			estat	= gdp_event_getstat(gev);
	gdp_datum_t		*datum  = gdp_event_getdatum( gev );
	ACL_info		*acInfo = NULL; 
	LKEY_info		*keyInfo = NULL; 



	if( event_call == 'a' ) { // AC_EVENT 
		acInfo = (ACL_info *)gdp_event_getudata( gev );
	} else if( event_call == 'k' ) { // KEY_SUBSCRIBE_EVENT 
		keyInfo = (LKEY_info *)gdp_event_getudata( gev );
	}

	// decode it
	switch (gdp_event_gettype(gev))
	{
	  case GDP_EVENT_DATA:  
			// this event contains a data return
			if( event_call == 'a' ) 
				isBuffered = update_ac_data( acInfo, datum, false ); 
			else if( event_call == 'k' ) 
				update_rcv_key_datum( keyInfo, datum, false );
			break;


	  case GDP_EVENT_EOS:    
		// "end of subscription": no more data will be returned
		ep_app_info("End of %s",
				subscribe ? "Subscription" : "Multiread");
		estat = EP_STAT_END_OF_FILE;
		// Case: On receiving all requested log entries 
		//				(according to the entry count)  
		if( event_call == 'a' ) {
//			ep_thr_mutex_lock( &acInfo->mutex );
			gdp_gcl_close( acInfo->gcl );
			acInfo->gcl = NULL;
			acInfo->state = NEED_AC_SUBSCRIBE;
			acInfo->reInit_recn = acInfo->next_recn; 
//			ep_thr_mutex_unlock( &acInfo->mutex );

		} else if( event_call == 'k' ) {
			if( keyInfo->rw_mode != 's' ) {
				ep_dbg_printf("[ERROR] Not expected mode %d at EVENT_EOS\n", 
								keyInfo->rw_mode );
			}
			keyInfo->rw_mode = 'r'; // previous mode : 's'
		}

		break;

	  case GDP_EVENT_SHUTDOWN:  // here  
		// log daemon has shut down, meaning we lose our subscription
		estat = GDP_STAT_DEAD_DAEMON;
		ep_app_message(estat, "%s terminating because of log daemon shutdown",
				subscribe ? "Subscription" : "Multiread"); 
		if( event_call == 'a' ) 
				reflect_ac_shutdown( acInfo );
		else if( event_call == 'k' ) 
				reflect_klog_shutdown( keyInfo );
		break;

	  case GDP_EVENT_CREATED:
		ep_app_warn("[WARN] Unexpected Event at %c event : EVENT CREATED ", 
					event_call );
		break;

	  case GDP_EVENT_MISSING:  
		if( event_call == 'a' ) 
				isBuffered = update_ac_data( acInfo, datum, true ); 
		else if( event_call == 'k' ) 
			update_rcv_key_datum( keyInfo, datum, true );
		break;


	  default:
		// let the library handle this
		gdp_event_print(gev, stderr, 1);
		break;
	}

	if( isBuffered ) {
		gev->datum = NULL;
	}

	if (EP_STAT_ISFAIL(estat))			// ERROR or higher severity
	{
		char ebuf[100];
		fprintf(stderr, "    STATUS: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/* LATER
// Before read the KSD_info data, check the loaded data status. 
// Prepare the data status if it is not prepared yet. 
int check_data_status( KSD_info *ksData ) 
{
	int	stat;

	if( ksData == NULL ) return EX_UNAVAILABLE; 

	ep_time_now( &(ksData->latest_time) );  

	// Because of resource reclaim, 
	// KSD_info may not have additional ac/key info. 
	if( ksData->ac_data == NULL ) {
		stat = update_ac_node( ksData, ksData->ac_pname, 
								strlen( ksData->ac_pname ) ) ;
		if( stat != EX_OK ) return EX_TEMPFAIL;
	}


	if( ksData->key_data == NULL ) {
		// reload key data with the latest key. 
	}
}
*/




bool isAllowedAccess( KSD_info *ksData, gdp_gclmd_t *token, char mode )
{
	char			right   = 0;
	bool			isAllowed = false;
	ACL_info		*acInfo = NULL;


	if( ksData == NULL || token==NULL ) return false;
	if( ksData->ac_data == NULL       ) return false;

	acInfo = (ACL_info *)(ksData->ac_data->nval);

	if( acInfo == NULL ) return false;

	if( acInfo->state  != DONE_INIT ) {
		ep_app_warn( "Loading the ac rules. "
					" So we cannot determine now at %d mode", 
						acInfo->state ); 
		return false;
	}


	if( mode == 'r')  right = 4;
	else if( mode == 'd')  right = 1;
	else if( mode == 'w')  right = 2;

	isAllowed = checkRequestedRight_wtoken( acInfo->acr_type, right, 
												acInfo->acrules, token );

	ep_mem_free( acInfo );

	return isAllowed; 
}



// is already locked rdatum in calling function
// MUST Update the datum->recno : lastest recno. 
// MUST update the datum->dbuf / sig with the latest key info 
int request_key_latest( KSD_info *ksData, gdp_datum_t *datum, 
												gdp_gclmd_t *token ) 
{
	LKEY_info			*klogdata   = NULL;
	RKEY_1				*rKey		= NULL;



/*	
	// LATER 0: Check key / ac data  prepared & loaded status 
	stat = check_data_status( ksData );
	if( stat != EX_OK ) return stat;
*/

	//
	// 1. Re-authorization due to the dynamic ac revocation 
	// 
	if( !isAllowedAccess( ksData, token, 'r' ) ) {
		// detailed print
		ep_dbg_printf("[DBG] Change auth allow -> deny \n" ); 
		return EX_NOPERM;
	}

	klogdata = (LKEY_info *)(ksData->key_data->nval);
	rKey	 = get_latest_key( klogdata );

	if( rKey != NULL ) {
		datum->recno = klogdata->last_recn;
		fill_dbuf_withKey( datum, rKey, klogdata->param->sym_key_alg, 
										klogdata->param->sym_key_mode );

	} else return EX_NOTFOUND; // unavailable 
		

	return EX_OK;
}

// is already locked rdatum in calling function
int request_key_rec( KSD_info *ksData, gdp_datum_t *rdatum, 
												gdp_gclmd_t *token ) 
{
	LKEY_info			*klogdata   = NULL;
	RKEY_1				*rKey		= NULL;
	int					recn		= -1;

	//
	// 1. Re-authorization due to the dynamic ac revocation 
	// 
	if( !isAllowedAccess( ksData, token, 'r' ) ) {
		// detailed print
		ep_dbg_printf("[DBG] Change auth allow -> deny \n" ); 
		return EX_NOPERM;
	}

	klogdata = (LKEY_info *)(ksData->key_data->nval);
	recn     = rdatum->recno; 
	rKey	 = get_key_wrecno( klogdata, recn );

	if( rKey != NULL ) {
		fill_dbuf_withKey( rdatum, rKey, klogdata->param->sym_key_alg, 
										klogdata->param->sym_key_mode );

		if( rKey->ctime.tv_sec == 0 ) {
			// Remove the Rkey due to lact of ctime 
			return_RKEY1( rKey );
		}
	} else return EX_NOTFOUND; // unavailable 

	return EX_OK;
}


// is already locked rdatum in calling function
int request_key_ts( KSD_info *ksData, gdp_datum_t *rdatum, 
												gdp_gclmd_t *token ) 
{
	int					req_recn    = -1;
	RKEY_1				*rKey		= NULL;
	LKEY_info			*klogdata   = NULL;


	klogdata = (LKEY_info *)(ksData->key_data->nval);
	req_recn = find_proper_keyrecnum( klogdata, &(rdatum->ts), &rKey );

	if( rKey != NULL ) {
		rdatum->recno      = req_recn; 
		rdatum->ts.tv_sec  = rKey->ctime.tv_sec;
		rdatum->ts.tv_nsec = rKey->ctime.tv_nsec;

		fill_dbuf_withKey( rdatum, rKey, klogdata->param->sym_key_alg, 
										klogdata->param->sym_key_mode );
		return EX_OK; 	
	}

	if( req_recn > 0 ) {
		// NOT CALLED 
		ep_dbg_printf("[ERR] request_key_ts: req_recn routine \n" );
		rdatum->recno = req_recn; 
		return request_key_rec( ksData, rdatum, token );
	} 

	return EX_NOTFOUND;
}


// NAIVE version for subscribe 
// is already locked rdatum in calling function
int request_recno_for_ts( KSD_info *ksData, gdp_datum_t *rdatum  )
{
	int					req_recn    = -1;
	RKEY_1				*rKey		= NULL;
	LKEY_info			*klogdata   = NULL;


	klogdata = (LKEY_info *)(ksData->key_data->nval);
	req_recn = find_proper_keyrecnum( klogdata, &(rdatum->ts), &rKey );

	if( req_recn == -2 ) {
		// future time  : from next generated key
		req_recn = get_last_keyrec( ksData ) + 1; 

	} else if( req_recn == -3 ) {
		// before starting time.. 
		req_recn = 1;
	}
	// -1: INTERNAL ERROR 

	return req_recn;
}


int get_last_keyrec( KSD_info *ksData )
{
	LKEY_info			*klogdata   = NULL;


	klogdata = (LKEY_info *)(ksData->key_data->nval);

	return klogdata->last_recn; 
}


void notify_rule_change_toKey( ACL_info *acInfo, bool isDenyRule ) 
{
	LKEY_info			*kinfo;



	LIST_FOREACH( kinfo, &(acInfo->skeys), klist )
	{
		// notify ac rule change kinfo.. 
		notify_rule_change_toKS( kinfo, acInfo, isDenyRule );
	}
}



gdp_session* find_subSession( KSD_info *ksData, gdp_req_t *subReq )
{
	gdp_session					*curSession = NULL;


	curSession = subReq->udata;

	if( curSession != NULL ) return curSession;

	return lookup_session( ksData, subReq ); 

}



void send_keyInfo_to_subscriber( KSD_info *ksData, RKEY_1 *newKey, 
									gdp_req_t *subReq, gdp_session *curSession )
{
	gdp_pname_t					pname;
	LKEY_info					*klogdata = NULL;



	if( subReq->state != GDP_REQ_FREE ) {
		ep_dbg_printf("[WARN] subReq state : %u \n", subReq->state ); 
	}


	ep_thr_mutex_lock( &subReq->mutex ); 

	if( subReq->rpdu == NULL ) subReq->rpdu = _gdp_pdu_new(); 
	subReq->rpdu->cmd = GDP_ACK_CONTENT; 

	//
	// 1. Fill the response datum buf with new Key 
	// 
	klogdata = (LKEY_info *)(ksData->key_data->nval);
	fill_dbuf_withKey( subReq->rpdu->datum, newKey, 
								klogdata->param->sym_key_alg, 
								klogdata->param->sym_key_mode );

	if( newKey->ctime.tv_sec == 0 ) {
		// Remove the Rkey due to lact of ctime 
		ep_app_warn("New Key Ctime is 0 in send for subscription \n");
		return_RKEY1( newKey );
	}


	ep_thr_mutex_lock( &curSession->mutex );  // close & sub simulatneously?

	//
	// 3. Encrypt the response datum buf on the session  
	// 
	if( update_smsg_onsession( subReq->rpdu, curSession, 'S', false ) != EX_OK ) {
		ep_dbg_printf("[ERROR] update_response in send_for sub \n"
							"from %s to %s \n", 
							gdp_printable_name(subReq->cpdu->src, pname), 
							gdp_printable_name(subReq->cpdu->dst, pname)  );

		// close subscription & gdp_session 
		sub_end_subscription( subReq, ksData );  // ACK_DELETE 
		close_relatedSession( ksData, subReq );	
		ep_thr_mutex_unlock( &subReq->mutex ); 
		return;
	}

	//
	// 3. Send the PDU out & post-processing   
	// 
	ep_dbg_printf( "Pre info of req: next_rec: %zu | %zu , numrecs: %u \n", 
					subReq->nextrec, subReq->rpdu->datum->recno,
					subReq->numrecs );
	// LATER: need to check
	if( subReq->numrecs > 0 ) subReq->numrecs--;
	subReq->nextrec++;
	subReq->stat = _gdp_pdu_out( subReq->rpdu, subReq->chan, NULL );

	gdp_buf_reset( subReq->rpdu->datum->dbuf );
	if( subReq->rpdu->datum->sig != NULL ) {
		gdp_buf_reset( subReq->rpdu->datum->sig ); 
	}
	subReq->rpdu->datum->siglen = 0;



	ep_thr_mutex_unlock( &curSession->mutex ); 
	ep_thr_mutex_unlock( &subReq->mutex ); 
}


void notify_change_info_toKS( KSD_info *ksData, bool isDenyRule, 
													RKEY_1	*newKey )
{
	bool				isAllowed = true;
	uint32_t			pre_numrec = 0;
	gdp_req_t			*subReq;
	gdp_session			*curSession = NULL;
	gdp_pname_t				pname;



	if( ksData == NULL ) {
		ep_app_error( "NULL info(ksData) in notify change \n" );
		return;
	}

	if( newKey != NULL && ksData->writer_sub != NULL ) {
		// first send the new Key to the writer device. 
		subReq = ksData->writer_sub;
		curSession = find_subSession( ksData, subReq );
		if( curSession == NULL ) {
			ep_app_error("Lose session for subscription \n"
							"from %s to %s \n", 
							gdp_printable_name(subReq->cpdu->src, pname), 
							gdp_printable_name(subReq->cpdu->dst, pname)  );
			sub_end_subscription( subReq, ksData );

		} else send_keyInfo_to_subscriber( ksData, newKey, 
											ksData->writer_sub, curSession );

	}


	LIST_FOREACH( subReq, &(ksData->gcl->reqs), gcllist ) 
	{
		// already done for the log writer device
		if( subReq == ksData->writer_sub ) continue; 

		// For reader devices 
		curSession = find_subSession( ksData, subReq );
		if( curSession == NULL ) {
			ep_app_error("Lose session for subscription \n"
							"from %s to %s \n", 
							gdp_printable_name(subReq->cpdu->src, pname), 
							gdp_printable_name(subReq->cpdu->dst, pname) );
			isAllowed = false;
		}

		if( isDenyRule && isAllowed ) {
			// reCheck the subReq authorization  : update isAllowed variables  
			isAllowed =  isAllowedAccess( ksData, curSession->ac_info, 'r' );
		}


		if( isAllowed && newKey != NULL ) {
			// send the new Key to the related reader devices 
			pre_numrec = subReq->numrecs;
			send_keyInfo_to_subscriber( ksData, newKey, 
											subReq, curSession );

			// check the subscription end condition : if so, isAllowed =false; 
			if( pre_numrec==1 && subReq->numrecs==0 ) 
				isAllowed = false; 
		}

		if( !isAllowed ) { // end of subscription . 
			// send the sub end  / remove the request in sub list
			sub_end_subscription( subReq, ksData );

			// close the session. 
			// LATER: more consider explicit close by client vs. immediate close 
			close_relatedSession( ksData, subReq );	
		}
	}


}


void notify_elapse_time( ) 
{
	char			keygen_flag = 0;
	RKEY_1			*newKey		= NULL;
	hs_lnode		*curKeyNode = NULL;
	LKEY_info		*keyInfo	= NULL;
	KSD_info		*ksData		= NULL;
	EP_TIME_SPEC	tnow;



	ep_time_now( &tnow );	
	ep_time_print( &tnow, stdout, EP_TIME_FMT_HUMAN);

	curKeyNode = kloglist;
	while( curKeyNode != NULL ) {
		keyInfo = (LKEY_info *)( curKeyNode->nval );

		if( keyInfo == NULL ) {
			ep_dbg_printf("[ERROR] NULL key info for %s \n", 
								curKeyNode->idname );
			goto step1;
		}


		// skip in 'x' mode : because cannot store the generated log) 
		if( keyInfo->rw_mode != 'w' ) goto step1;
		
		keygen_flag = isNecessaryNewKey( keyInfo, NULL, tnow );
		if( keygen_flag == 0 ) goto step1;

		newKey = generate_next_key( keyInfo, &tnow, keygen_flag );

		if( newKey == NULL ) goto step1;

		store_new_generated_key( keyInfo, newKey );

		LIST_FOREACH( ksData, &(keyInfo->shlogs), loglist ) 
		{
			notify_change_info_toKS( ksData, false, newKey );
		}


step1:   
		curKeyNode = curKeyNode->next;
	}


}


void check_info_state( )
{
	int					pre_recn = 0;
	hs_lnode			*curNode = NULL;
	ACL_info			*acInfo  = NULL;	
	LKEY_info			*keyInfo  = NULL;	
	EP_TIME_SPEC		tnow;



	ep_time_now( &tnow );

	// CHECK ac info 
	curNode = aclist;
	while( curNode != NULL ) {
		acInfo = (ACL_info *)(curNode->nval);		

		if( acInfo == NULL ) {
			ep_dbg_printf("[CHECK] NULL ac info for %s \n", 
											curNode->idname ); 
			goto step1;
		}

		if( acInfo->isAvailable == false ) {
			ep_dbg_printf("[CHECK] Unavailable ac info for %s \n", 
													curNode->idname ); 
			goto step1;
		}


		if( acInfo->state == CHECK_BUF_ENTRY ) {
			// retry to update ac data 
			pre_recn = gdp_datum_getrecno( acInfo->head );
			update_ac_data( acInfo, NULL, false );

			if( acInfo->head != NULL ) {
				if( pre_recn == gdp_datum_getrecno( acInfo->head ) ) {
					ep_dbg_printf("[DEBUG] the same error in buffered ac\n"
									"Do we need to reclaim resource?\n" );
				}
			}

		} else if( acInfo->state == WAIT_NEXT_ENTRY ) {
			// CHECK TIME OUT : ELAPSE ? THEN RE TRY...  
			if( tnow.tv_sec < acInfo->ref_time  ) {
				ep_dbg_printf("[DEBUG] Retry to read %zu on "
								"elapsed WAIT_NEXT_ENTRY time\n", 
								acInfo->next_recn );

				acInfo->ref_time = tnow.tv_sec + WAIT_TIME_SEC;
				request_acgcl_read_async( acInfo, acInfo->next_recn );
			}

		} else if( acInfo->state == NEED_AC_SUBSCRIBE ) {
			// case 1: failure on gcl_subscribe (acInfo->gcl = NULL)
			// case 2: SHUTDOWN of aclog server (acInfo->gcl = NULL) 
			// case 3: end of subscription event (acInfo->gcl = NULL)
			load_ac_rules( acInfo ); 

		} else if( acInfo->state == NEED_AC_SUBSCRIBE ) {
			// case 1: failure on first_gcl_open 
			load_ac_rules( acInfo ); 
		}

step1:
		curNode = curNode->next;
	}


	// CHECK ac info 
	curNode = kloglist;
	while( curNode != NULL ) {
		keyInfo = (LKEY_info *)(curNode->nval);		

		// w / x : writer 
		if( keyInfo->rw_mode == 'x' || keyInfo->rw_mode == 't' ) {
			// retry to connect to the key log server.. 
			load_key_data( NULL, 0, keyInfo ); 

		} else if( keyInfo->rw_mode == 'r' ) {  
			// r, s, t : reader
			if( keyInfo->gcl != NULL ) {
				gdp_gcl_close( keyInfo->gcl );
				keyInfo->gcl = NULL;
				load_key_data( NULL, 0, keyInfo ); 
			}
		}

		curNode = curNode->next;
	}


}


void reflect_lost_channel( )
{
	int					ti		= 0;
	hs_lnode			*curNode = NULL;
	ACL_info			*acInfo  = NULL;	
	LKEY_info			*keyInfo = NULL;	
	KSD_info			*ksData  = NULL;



	// reflect on kloglist  
	curNode =  kloglist;
	while( curNode != NULL ) {
		keyInfo = (LKEY_info *)(curNode->nval);		

		if( keyInfo->rw_mode == 'w' ) keyInfo->rw_mode = 'x';
		if( keyInfo->rw_mode == 'r' || keyInfo->rw_mode == 's' ) 
									keyInfo->rw_mode = 't';

		keyInfo->gcl = NULL; 

		curNode = curNode->next;
	}


	curNode =  aclist;
	while( curNode != NULL ) {
		acInfo = (ACL_info *)(curNode->nval);		

		acInfo->state = NEED_AC_SUBSCRIBE;
		acInfo->ref_time = 0;
		acInfo->gcl = NULL;
		// think more buffered entries: head 

		curNode = curNode->next;
	}


	for( ti=0; ti<kslTable->hash_size; ti++ ) {

		curNode = kslTable->htable[ti];
		while( curNode != NULL ) {
			ksData = (KSD_info *)(curNode->nval);

			ksData->gcl			= NULL;
			ksData->writer_sub	= NULL;
//			ksData->state		= NEED_INIT; // LATER : think more 
			close_allSession( ksData );

			curNode = curNode->next;
		}

	}
}



