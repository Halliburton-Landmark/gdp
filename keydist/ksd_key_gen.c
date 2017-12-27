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
**  KSD_KEY_GEN - Generate the key: peridic, event-based or dual mode
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 


#include <string.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <ep/ep_app.h>
#include <hs/hs_util.h>

#include "ksd_key_gen.h"


void print_symkey( FILE *a_fp, RKEY_1 *a_insk, KGEN_param *inParam  );
RKEY_1* generate_randomsymkey( KGEN_param *inParam, RKEY_1 *outKey ); 
RKEY_1* generate_prehashkey( RKEY_1 *pRefKey ); 


//  3 Return valule 
//  not change : 0 / change in the same chain : 1 
//  change to other chain (including first key, no chain): 2
char isNecessaryNewKey( LKEY_info *keyInfo, RKEY_1 *latest_key, 
							EP_TIME_SPEC	tnow ) 
{
	int				ch_inx; 
	KGEN_param		*kparam = keyInfo->param;
//	EP_TIME_SPEC	tnow ; 


	if( latest_key == NULL )  
			latest_key = get_latest_key( keyInfo );
	if( latest_key == NULL ) return 2;

	// Check based on the key change type
	ch_inx = kparam->security_level / 10;

	if( ch_inx % 3 == 0 ) { // period based 
//		ep_time_now( &tnow ); 
		tnow.tv_sec -= kparam->max_key_period;

		if( ep_time_before( &(latest_key->ctime), &tnow ) )  return 0;
		else {
			if( ch_inx == 0 )  return 1;
			if( ch_inx == 9 ) return 2;
			
			if( ch_inx == 3 ) { // because of restarting this service... 
				if( keyInfo->sub_keys == NULL ) return 2;
				else                            return 1;
			}

			if( ch_inx == 6 )  {
				// header keys are loaded in memory always. 
				struct hdr_rkey1 *khead = NULL;

				khead = (struct hdr_rkey1 *)(keyInfo->keys); 
				if( khead->child_num < kparam->chain_length ) return 1;
				else return 2; 
			}

		}


	} else if( ch_inx % 3 == 1 ) { // revocation based 
		if( keyInfo->revocation > 0 ) {
			if( ch_inx == 1 )  return 1;
			if( ch_inx == 10 ) return 2;

			if( ch_inx == 4 ) {
				if( keyInfo->sub_keys == NULL ) return 2;
				else                            return 1;
			}

			if( ch_inx == 7 )  {
				// header keys are loaded in memory always. 
				struct hdr_rkey1 *khead = NULL;

				khead = (struct hdr_rkey1 *)(keyInfo->keys); 
				if( khead->child_num < kparam->chain_length ) return 1;
				else return 2; 
			}

		} else return 0;

	} else { //  ch_inx % 3 == 2 ::  period + revocation   
//		ep_time_now( &tnow ); 

		if( keyInfo->revocation <  kparam->change_thval1 ) {
			if( keyInfo->revocation > 0 ) {
				tnow.tv_sec -= kparam->min_key_period; 
				if( ep_time_before(&(latest_key->ctime), &tnow) )  return 0;
				else if( ch_inx == 2 )  return 1; 
				else if( ch_inx == 11 )	return 2; 
				else if( ch_inx == 5 )	{
					if( keyInfo->sub_keys == NULL ) return 2;
					else                            return 1;
				} else {
					// header keys are loaded in memory always. 
					struct hdr_rkey1 *khead = NULL;

					khead = (struct hdr_rkey1 *)(keyInfo->keys); 
					if( khead->child_num < kparam->chain_length ) return 1;
					else return 2; 
				}

			} else {
				tnow.tv_sec -= kparam->max_key_period; 
				if( ep_time_before(&(latest_key->ctime), &tnow) )  return 0;
				else if( ch_inx == 2 )	return 1; 
				else if( ch_inx == 11 )	return 2; 
				else if( ch_inx == 5 )	{
					if( keyInfo->sub_keys == NULL ) return 2;
					else                            return 1;
				} else {
					// header keys are loaded in memory always. 
					struct hdr_rkey1 *khead = NULL;

					khead = (struct hdr_rkey1 *)(keyInfo->keys); 
					if( khead->child_num < kparam->chain_length ) return 1;
					else return 2; 
				}
			}

		} else  {
			if( ch_inx == 2 )		return 1;
			else if( ch_inx == 11 )	return 2;
			else {
				// header keys are loaded in memory always. 
				struct hdr_rkey1 *khead = NULL;

				khead = (struct hdr_rkey1 *)(keyInfo->keys); 
				if( khead->child_num < kparam->chain_length ) return 1;
				else return 2; 
			}
		}

	} 


	return 0;
}



//
// External APIs
// 
RKEY_1* extract_prev_key( LKEY_info *keyInfo, RKEY_1 *refKey, 
												gdp_recno_t reqNum )
{
	int				ti, round;
	KGEN_param		*kparam = keyInfo->param;
	RKEY_1			*preKey = refKey; 
	RKEY_1			*ppKey  = NULL; 
	RKEY_1			*newKey = NULL; 




	// simple extraction (later :: more secure & efficient gen algorithm)  
	switch( kparam->keygenfunID ) {
		case RAND_GEN1: 
		case RO_RSA_GEN1: 
		case RE_RSA_GEN1: 
		case UP_RSA_GEN1: 
			ep_app_warn("NOT SUPPORTED FUNC: %d", kparam->keygenfunID);
			return NULL;

		case RO_HASH_GEN1: 
			round = refKey->seqn - reqNum ; 
			for( ti=0; ti<round; ti++ ) {
				newKey = generate_prehashkey( preKey ); 
				ppKey  = preKey;
				preKey = newKey;

				// We must not free the refKey
				if( ti!=0 ) {
					if( ppKey != NULL ) return_RKEY1( ppKey ); 
					ppKey = NULL;
				}
			}

			return newKey;

		case RE_HASH_GEN1: 
		case UP_HASH_GEN1: 
		default: 
			ep_app_warn("NOT SUPPORTED FUNC: %d", kparam->keygenfunID);
			return NULL;
	}

}


RKEY_1* alarm_rule_change( LKEY_info *keylog ) 
{
	keylog->revocation++;

	return generate_next_key( keylog, NULL, -1 );
}


// Return the new generated key on the right key change condition. 
// if the condition isn't met, return NULL
RKEY_1* generate_next_key( LKEY_info *keyInfo, EP_TIME_SPEC *curtime, 
												char change_type) 
{
	char			change  = 0;
	KGEN_param		*kparam = keyInfo->param;
	RKEY_1			*newKey = NULL; 
	RKEY_1			*rKey   = NULL; 
	RKEY_1			*latest_key; 
	EP_TIME_SPEC	tnow;


	ep_thr_mutex_lock( &keyInfo->mutex ); 
	latest_key = get_latest_key( keyInfo );

	if( curtime == NULL ) 
				ep_time_now( &tnow );
	else {
		tnow.tv_sec			= curtime->tv_sec;
		tnow.tv_nsec		= curtime->tv_nsec;
		tnow.tv_accuracy	= curtime->tv_accuracy;
	}

	if( change_type == -1 ) 
		change = isNecessaryNewKey( keyInfo, latest_key, tnow );
	else change = change_type;

	if( change == 0 )  {
		ep_thr_mutex_unlock( &keyInfo->mutex ); 
		return NULL;
	}

	if( keyInfo->sub_keys == NULL ) { 
		newKey = get_new_RKEY1(); 
		if( newKey == NULL ) {
			ep_app_warn("Fail to generate key becase of memory" );
			ep_thr_mutex_unlock( &keyInfo->mutex ); 
			return NULL;
		}
		newKey->seqn = keyInfo->last_recn + 1;
		if( change == 1 ) { // change in the same chain 
			if( kparam->security_level < 30 ) 
				newKey->pre_seqn = 0; // one chain 
			else newKey->pre_seqn = latest_key->pre_seqn; // multi chain 

		} else { // change other chain including first key & no chain 
			// first key in one chain so latest_key == NULL 
			if( kparam->security_level < 30 ) newKey->pre_seqn = 0;  
			else if( kparam->security_level < 90 ) {
				if( latest_key == NULL ) newKey->pre_seqn = -1; 
				else newKey->pre_seqn = latest_key->seqn ;  
			} else newKey->pre_seqn = newKey->seqn; 
		}
		newKey->ctime.tv_sec  = tnow.tv_sec;
		newKey->ctime.tv_nsec = tnow.tv_nsec;
	}


	// simple generation (later :: more secure & efficient gen algorithm)  
	switch( kparam->keygenfunID ) {
		case RAND_GEN1: 
			rKey = generate_randomsymkey( kparam, newKey );  
			break;

		case RO_RSA_GEN1: 
		case RE_RSA_GEN1: 
		case UP_RSA_GEN1: 
			ep_app_warn("NOT SUPPORTED FUNC: %d", kparam->keygenfunID);
			ep_thr_mutex_unlock( &keyInfo->mutex ); 
			return NULL;

		case RO_HASH_GEN1: 
			if( keyInfo->sub_keys != NULL ) {
				newKey = keyInfo->sub_keys;
				keyInfo->sub_keys = newKey->next;

				// preseqn is alread set. 
				newKey->seqn			= keyInfo->last_recn + 1;
				newKey->ctime.tv_sec	= tnow.tv_sec;
				newKey->ctime.tv_nsec	= tnow.tv_nsec;
				newKey->next			= NULL;

				rKey = newKey;

			} else {
				// gen key list : reverse direction 
				int				t_rnum = kparam->chain_length;
				RKEY_1			*preKey = NULL;	

				rKey = generate_randomsymkey( kparam, newKey );  
				while( rKey!= NULL && t_rnum > 1 ) {
					// insert the generated key in sub_keys list. 
					rKey->next			= keyInfo->sub_keys;
					keyInfo->sub_keys	= rKey; 

					// make newKey; 
					// newKey is pre-generated key / rKey is next key  
					preKey = rKey; 
					rKey = generate_prehashkey( preKey ); 
					t_rnum--;
				}

				if( rKey != NULL ) { // t_rnum == 1 
					// Make the keys with the number of chain length. 
					// rKey is the first key in the chain (but not list) 
					newKey = rKey;
					newKey->seqn			= keyInfo->last_recn + 1;
					newKey->ctime.tv_sec	= tnow.tv_sec;
					newKey->ctime.tv_nsec	= tnow.tv_nsec;
					newKey->next			= NULL;

				} else { // rKey == NULL (fail to create enough key)
					// Fail to create the chain len number of keys 
					rKey = keyInfo->sub_keys;

					if( rKey != NULL ) {
						newKey = keyInfo->sub_keys;
						keyInfo->sub_keys = newKey->next;

						// preseqn is alread set. 
						newKey->seqn			= keyInfo->last_recn + 1;
						newKey->ctime.tv_sec	= tnow.tv_sec;
						newKey->ctime.tv_nsec	= tnow.tv_nsec;
						newKey->next			= NULL;

					} // else : no gen : so free newkey. 
				}

			} 
			break; 

		case RE_HASH_GEN1: 
		case UP_HASH_GEN1: 
		default: 
			ep_app_warn("NOT SUPPORTED FUNC: %d", kparam->keygenfunID);
			ep_thr_mutex_unlock( &keyInfo->mutex ); 
			return NULL;
	}


	// if new key is generated,
	if( rKey == NULL ) {
		// Fail to generate new key.. 
		ep_app_error( "Fail to generate new key" );
		return_RKEY1( newKey );

	}  else {
		keyInfo->last_recn++;
		insert_key_log( newKey, keyInfo, FORWARD_IN );
		keyInfo->revocation = 0; 
	}

	ep_thr_mutex_unlock( &keyInfo->mutex ); 
	return newKey;
}




RKEY_1* generate_prehashkey( RKEY_1 *pRefKey ) 
{
	int				key_len; 
	RKEY_1			*newKey = NULL; 
	unsigned char	*rHash  = NULL;


	newKey = get_new_RKEY1(); 
	if( newKey == NULL ) {
		ep_app_warn("Fail to generate key becase of memory" );
		return NULL;
	}

	// The other field (seqn, ctime) is updated when this RKEY_1 is used 
	newKey->pre_seqn = pRefKey->pre_seqn; 

	key_len = strlen( (const char*)(pRefKey->sym_key) );

	if( key_len == 16 ) { // 128 bit  - sha-256 
		rHash = SHA256( pRefKey->sym_key, key_len, newKey->sym_key );
		if( rHash == NULL ) goto fail1;

		rHash = SHA256( pRefKey->sym_iv, key_len, newKey->sym_iv );
		if( rHash == NULL ) goto fail1;

	} else if( key_len == 24 ) { // 192 bit  : sha-384 
		rHash = SHA384( pRefKey->sym_key, key_len, newKey->sym_key );
		if( rHash == NULL ) goto fail1;

		rHash = SHA384( pRefKey->sym_iv, key_len, newKey->sym_iv );
		if( rHash == NULL ) goto fail1;

	} else if( key_len == 32 ) { // 256 bit  : sha-512 
		rHash = SHA512( pRefKey->sym_key, key_len, newKey->sym_key );
		if( rHash == NULL ) goto fail1;

		rHash = SHA512( pRefKey->sym_iv, key_len, newKey->sym_iv );
		if( rHash == NULL ) goto fail1;

	} else {
		goto fail1;
	}

	newKey->sym_key[key_len] = '\0';
	newKey->sym_iv[key_len]  = '\0';

	return newKey;

fail1: 
	ep_app_error("Fail to generate hash key in hash %d", key_len);
	return_RKEY1( newKey );

	return NULL;
}


// current support AES key (check tmp_evp_cipher_type ) 
RKEY_1* generate_randomsymkey( KGEN_param *inParam, RKEY_1 *outKey ) 
{
	int					t_ret;
	unsigned char		salt[8];
	unsigned char		passwd[16];
	const EVP_CIPHER	*de_cipher = NULL; 


	//1. generate pseudo-random number & use the number as salt / passwd 
	// LATER : upgrade... 	
	t_ret = RAND_pseudo_bytes( salt, 8);	
	if( t_ret < 0 ) {
		ep_app_error("Fail to create the Pseudo-random number ");
		return	NULL;
	}

	t_ret = RAND_pseudo_bytes( passwd, 16);	
	if( t_ret < 0 ) {
		ep_app_error("Fail to create the Pseudo-random number ");
		return	NULL;
	}

/*
	{
		printf("****** Key generation INFO *********\n" );
		printf("INFO] enc algo: %s\n", 
					ep_crypto_keyenc_name( inParam->sym_key_alg ) );
		printf("INFO] enc mode: %s\n", 
					ep_crypto_mode_name( inParam->sym_key_mode ) );
		printf("INFO] enc pswd: %s\n", passwd );
		printf("INFO] enc salt: %s\n", salt );
	}
*/

	// 2. generate key & IV 
	de_cipher = tmp_evp_cipher_type( inParam->sym_key_alg | 
										inParam->sym_key_mode );
	if( de_cipher == NULL ) {
		ep_app_error("Not supporeted: %s , %s", 
					ep_crypto_keyenc_name( inParam->sym_key_alg),  
					ep_crypto_mode_name( inParam->sym_key_mode) );
		return NULL;
	}
	EVP_BytesToKey( de_cipher, EVP_md5(), salt, passwd, 
					16, 3, outKey->sym_key, outKey->sym_iv );
					
	{
		printf("\n****** Generated Key INFO *********\n" );
		print_symkey( stdout, outKey, inParam );
		printf("key len: %d, IV len: %d \n", 
					de_cipher->key_len, de_cipher->iv_len );
	} 

	return outKey;
}



void print_symkey( FILE *a_fp, RKEY_1 *a_insk, KGEN_param *inParam  )
{
	int			tmp_len; 

	
	fprintf( a_fp, "******* Print symkey info *******\n" );
	fprintf( a_fp, "algo = %s\n", 
				ep_crypto_keyenc_name( inParam->sym_key_alg ) );
	fprintf( a_fp, "mode = %s\n", 
				ep_crypto_mode_name( inParam->sym_key_mode ) );

	// another debugging info print 

	tmp_len = strlen( (const char *)a_insk->sym_key );
	if( tmp_len > 0 ) { 
		ep_print_hexstr( a_fp, "key = ", tmp_len, a_insk->sym_key );
	}

	tmp_len = strlen( (const char *)a_insk->sym_iv );
	if( tmp_len > 0 ) { 
		ep_print_hexstr( a_fp, "iv = ", tmp_len, a_insk->sym_iv );
	}

}


/*
void prepare_key_generation( LKEY_info *keyInfo )
{
	int				tt; // warn flag 
	KGEN_param		*kparam = keyInfo->param;

	switch( kparam->keygenfunID ) {
		case RAND_GEN1: 
		case RO_RSA_GEN1: 
		case RE_RSA_GEN1: 
		case UP_RSA_GEN1: 
		case RO_HASH_GEN1: 
		case RE_HASH_GEN1: 
		case UP_HASH_GEN1: 
		default: 
			break;
	}
}
*/
