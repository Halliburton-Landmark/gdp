/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
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


/*
** This provides the utility functions to support log record enc/dec.
** This routine can be merged into gdp_util.c or libep. 
**
** Written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr)
** last modified : 2017.01.04.
*/ 



#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_crypto.h>
#include <ep/ep_statcodes.h>


#include "gdp_logdatakey.h"
#include "gdp_devicekey.h"
#include <ep/ep_symkey_gen.h>
#include <ep/ep_file.h>
#include <ep/ep_util.h>



static EP_DBG	Dbg = EP_DBG_INIT( "gdp.ldkey", 
									"GDP Log Data Key processing..." );
static int		DBG_LEV = 50;


// LATER how about valid time info : <stime, ltime> ???  
#define			SK_KEY_NUM			5
#define			MAX_KEY_LEN			5
static const char	sk_keywords[SK_KEY_NUM][MAX_KEY_LEN] = 
						{"algo", "mode", "salt", "key", "iv" };

static	EP_CRYPTO_KEY	*dev_skey  = NULL;



/*
** Check wheter input keyword belongs to the sk_keywords.  
** 
** 1'st argu: input string (start position)
** 2'nd argu: the length of input string (to be checked) 
** rval		: the related index in sk_keywords if the input string belongs
**			  to the sk_keywords. Otherwise, return -1. 
*/
int 
check_keyinfo_id( char *a_instr, int a_inLen )
{
	int				ti;


	for( ti=0; ti<SK_KEY_NUM; ti++ ) {
		if( strncmp( a_instr, sk_keywords[ti], a_inLen ) == 0 ) return ti;
	} 

	return -1;
}



/*
** Print the symkey info into file stream 
**
** 1'st argu: output file stream
** 2'nd argu: the symkey info structure to be printed
*/
void
print_symkeyinfo( FILE *a_fp, symkey_info *a_insk )
{
	int			tmp_len; 
	char		tmp_prefix[20];

	
	fprintf( a_fp, "******* Print symkey info *******\n" );
	fprintf( a_fp, "algo = %s\n", 
				ep_crypto_keyenc_name( a_insk->sym_key_alg ) );
	fprintf( a_fp, "mode = %s\n", 
				ep_crypto_mode_name( a_insk->sym_key_mode ) );


	tmp_len = a_insk->salt_len;
	if( tmp_len > 0 ) { 
		sprintf( tmp_prefix, "salt (%d) = ", tmp_len );
		ep_print_hexstr( a_fp, tmp_prefix, tmp_len, a_insk->salt );
	}


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
** Change hexstring to uint8_t array.
**
** 1'st argu: input hex string 
** 2'nd argu: the length of input hex string 
** 3'rd argu: Maximum length of input hex string to be changed. 
**				if this value is 0, change whole input string. 
** 4'th argu: Output array to store the changed string  
** rval     : the number of entry to be filled in output arrary. 
**			  Therefore, 0 means error. 
*/
int
parse_hextochr( char *a_inStr, int a_inLen, int a_maxLen, uint8_t *outData )
{
	int			ti;
	int			t_wpos = 0;
	int			t_maxL; 
	int			val1, val2;

	// error check 
	if( a_inLen % 2 != 0 ) return t_wpos;

	// check max length to be handled. 
	if( a_inLen < a_maxLen ) t_maxL = a_inLen; 
	else if( a_maxLen != 0 ) t_maxL = a_maxLen; 
	else t_maxL = a_inLen;

	// change & store 
	for( ti=0; ti<t_maxL; ti+=2, t_wpos++ ) {
		val1 = ep_chrtoint( a_inStr[ti] ); 
		val2 = ep_chrtoint( a_inStr[ti+1] ); 

		// current: skip checking error 
		outData[t_wpos] = val1*16 + val2;	
	}


	return t_wpos;
}



/*
** Parse the input data (1'st argu : key info string) 
** Fill the parsed key info into output structure (2'nd argu)
**
** 1'st argu: input string (decrypted key info string )
** 2'nd argu: output data to store the parsed key info 
** rval		: return true if all input data are parsed rightly. 
**		      return false on the error while parsing the input data. 
*/
bool 
fill_symkeyinfo_fromstr( char *inData, symkey_info *outKey)
{
	int				t_llen	 = 0;
	int				t_vlen	 = 0;
	int				t_pos	 = -1;
	int				key_id	 = -1;
	int				rval	 = 0;
	bool			error    = false;
	char			*curLine = NULL;
	char			*delim	 = "\n";



	// process line by line 
	curLine = strtok( inData, delim );
	while( curLine != NULL ) {
		//
		// parse the line :: format :: key_id=val
		// 
		// sscanf(curLine, "%s=%s", buf1, buf2);
		// printf("len: %d: %s \n", strlen(curLine), curLine );

		t_llen = strlen(curLine);
		t_pos = ep_strchr_pos( curLine, t_llen, '=' ); 	

		if( t_pos != -1 ) {
			// check whether key_id has right value.  
			key_id = check_keyinfo_id( curLine, t_pos );
		}

		if( t_pos == -1 || key_id == -1 ) {
			ep_dbg_cprintf( Dbg, DBG_LEV, 
						"Invalid key info format\n%s\n", inData );
			printf( "Invalid Key Info format\n");
			return false;
		}


		//
		// parse value according to the key_id
		//
		// t_pos points to the start point of value 
		t_pos += 1;					 
		t_vlen = t_llen - t_pos;
		switch( key_id ) 
		{
			case 0:		//  algo 
				outKey->sym_key_alg = ep_crypto_keyenc_byname( 
										(const char *)&(curLine[t_pos]) );
				if( outKey->sym_key_alg == -1 ) error = true;
				break;

			case 1:		//  mode 
				outKey->sym_key_mode = ep_crypto_mode_byname( 
										(const char *)&(curLine[t_pos]) );
				if( outKey->sym_key_mode == -1 ) error = true;
				break;

			case 2:		//  salt 
				rval = parse_hextochr( &(curLine[t_pos]), t_vlen, 16,
										outKey->salt );
				if( rval == 0 ) error = true;
				else outKey->salt_len = rval;

				if( rval < 8 ) outKey->salt[rval] = '\0';
				//else if( rval > 8 ) error = true;
				break;

			case 3:		//  key  
				// 3'rd argu can reflect the accurate value 
				//		according to the symmetric algorithm. 
				rval = parse_hextochr( &(curLine[t_pos]), t_vlen, 
								EVP_MAX_KEY_LENGTH*2, outKey->sym_key );
				if( rval == 0 ) error = true;

				if( rval < EVP_MAX_KEY_LENGTH ) outKey->sym_key[rval] = '\0';
				break;

			case 4:		//  iv 
				// 3'rd argu can reflect the accurate value 
				//		according to the symmetric algorithm. 
				rval = parse_hextochr( &(curLine[t_pos]), t_vlen, 
								EVP_MAX_KEY_LENGTH*2, outKey->sym_iv );
				if( rval == 0 ) error = true;

				if( rval < EVP_MAX_KEY_LENGTH ) outKey->sym_iv[rval] = '\0';
				break;

			// LATER: TIME INFO??
			default:	//  error  
				error = true;
				break;
		}

		if( error ) {
			ep_dbg_cprintf( Dbg, DBG_LEV, 
						"Invalid key info format\n%s\n", inData );
			printf( "Invalid Key Info format\n");
			return false;
		}

		curLine = strtok( NULL, delim );
	}


	return true;
}



/*
** Check whether this device has the log encryption key.   
** This routine is related with the key distribution service 
** running on the device. In test mode, enc key is managed in the encrypted
** file indicated by config. In service mode, enc key is handled by the argu
** function provided by service. 
** 
** 1'st argu: Checking mode: MODE_LOGKEY_TEST , MODE_LOGKEY_SERVICE    
** 2'nd argu: function provided by key distribution service. 
**				This function checks whether device has the key for the log 
**				indicated by first argu (LOG GUID). 
**				Second argu is the optional arguemnts for the function. 
** 3'rd argu: log GUID passed to 2'nd argu function.  
** 4'th argu: optional argument passed to 2'nd argu function. 
** rval     : true if device has log key. otherwise, false. 
*/
bool 
check_log_key(const char a_rMode, 
			bool (*key_service)(gdp_name_t, void *), 
			gdp_name_t	a_logName,	void	*a_oargu )
{
	if( a_rMode == MODE_LOGKEY_TEST ) {
		const char			*t_path  =  NULL;
		const char			*t_cpath =  NULL;


		t_path = ep_adm_getstrparam("swarm.gdp.log.tmpkey", NULL); 	

		if( t_path == NULL ) {
			ep_dbg_cprintf( Dbg, DBG_LEV, "No test log data key info\n");
			printf( "No test log data key info\n");
			return false;
		}
		if( access( t_path, R_OK ) != 0 ) {
			ep_dbg_cprintf( Dbg, DBG_LEV, 
						"Cannot access the test log data key info\n");
			printf( "Cannot access the test log data key info\n");
			return false;
		}


		t_cpath = ep_adm_getstrparam("swarm.gdp.keydist.cert", NULL); 	

		if( t_cpath == NULL ) {
			ep_dbg_cprintf( Dbg, DBG_LEV, 
							"No key distributor certificate\n");
			printf( "No key distributor certificate\n");
			return false;
		}
		if( access( t_cpath, R_OK ) != 0 ) {
			ep_dbg_cprintf( Dbg, DBG_LEV, 
						"Cannot access the key distributor certificate\n");
			printf( "Cannot access the key distributor certificate\n");
			return false;
		}

		return true;

	} else if ( a_rMode == MODE_LOGKEY_SERVICE ) {

		return key_service( a_logName, a_oargu );


	} 
	
	return false;

}



/*
** This is wrapper function to check whether this device has enough info
** to encrypt or decrypt the log entry in test mode.  
** This function can be extended to support the MODE_LOGKEY_SERVICE with 
** additional arguments. 
** 
** 1'st argu: certificate full path 
** 2'nd argu: secret file full path 
**     If argument path is NULL, config value in gdp param file is used.
** rval     : true if enough info exists. otherwise, false. 
*/
bool
check_device_for_log_encdec_t1(const char *a_cPath, const char *a_sPath) 
{
	bool		tmpval = false;


	tmpval = check_device_asyminfo(a_cPath, a_sPath);
	if( tmpval == false ) {
		ep_dbg_cprintf( Dbg, DBG_LEV, "Fail to handle device key info\n");
		return tmpval;
	}


	tmpval = check_log_key( MODE_LOGKEY_TEST, NULL, NULL, NULL );

	return tmpval;
}



/*
** Get the log encryption/decryption key.   
** This routine is related with the key distribution service 
** running on the device. In test mode, enc key is managed in the encrypted
** file indicated by config. In service mode, enc key is handled  
** by the argu function provided by service. 
** LATER: If necessary on key service mode, 
**				time info can be located in 5'th argu.  
** 
** 1'st argu: Checking mode: MODE_LOGKEY_TEST , MODE_LOGKEY_SERVICE    
** 2'nd argu: Device Secret Key 
** 3'rd argu: function provided by key distribution service. 
**			This function return the encryption/decryption key of the log
**				indicated by second argu (LOG GUID). 
**				first argu is device secret key.  
**				Second argu is the optional arguemnts for the function. 
** 4'th argu: log GUID passed to 3'rd argu function.  
** 5'th argu: optional argument passed to 3'rd argu function. 
** 6'th argu: if this value is not null, reuse the memory to be allocated for 
**				this argument. 
** rval     : key information. if error, NULL  
*/
symkey_info* get_logdatakey(const char a_rMode, EP_CRYPTO_KEY *a_dskey, 
		symkey_info* (*get_logenckey)(EP_CRYPTO_KEY *, gdp_name_t, void *), 
		gdp_name_t	a_logName,	void	*a_oargu, symkey_info *a_prekey )
{
	symkey_info			*t_dkey = NULL;


	if( a_rMode == MODE_LOGKEY_TEST ) {
		int						t_keytype = EVP_PKEY_type( a_dskey->type );	
		const char				*t_path   = NULL;

		int						t_sharedKeyLen = 0;
		char					t_sharedKey[SHA_DIGEST_LENGTH];
		uint8_t					de_iv[EVP_MAX_IV_LENGTH];
		EP_CRYPTO_CIPHER_CTX	*tsc_ctx = NULL;
		char					*encBuf  = NULL;
		char					*skBuf   = NULL;
		int						t_encLen = 0;
		int						tmp_len	 = 0;
		EP_STAT					r_estat;

		const char				*t_kdcert;
		EP_CRYPTO_KEY			*dist_pkey = NULL;



		t_path = ep_adm_getstrparam("swarm.gdp.log.tmpkey", NULL); 	

		// No or Unavailable log key info 
		if( t_path == NULL				) return t_dkey;
		if( access( t_path, R_OK ) != 0 ) return t_dkey;


		switch( t_keytype ) 
		{
			case EVP_PKEY_RSA: // currently not supported
				ep_dbg_cprintf( Dbg, DBG_LEV, 
							"NOT SUPPORT: RSA key in key dist\n");
				return NULL;	

			case EVP_PKEY_DSA: // currently not supported
				ep_dbg_cprintf( Dbg, DBG_LEV, 
							"NOT SUPPORT: DSA key in key dist\n");
				return NULL;	

			case EVP_PKEY_DH: // currently not supported
				ep_dbg_cprintf( Dbg, DBG_LEV, 
							"NOT SUPPORT: DH key in key dist\n");
				return NULL;	

			case EVP_PKEY_EC:
				// Get the publie key of key distributor 
				t_kdcert = ep_adm_getstrparam("swarm.gdp.keydist.cert", 
												NULL); 	
				dist_pkey = get_pubkey_from_cert_file( t_kdcert ); 
				if( dist_pkey == NULL ) {
					ep_dbg_cprintf( Dbg, DBG_LEV, 
						"Fail to get the publie key of key distributor\n");
					return NULL; 
				}


				// Calculate the encryption key of log data key file  
				// Shared key : AES128 CBC
				t_sharedKeyLen = ep_compute_sharedkey_onEC( a_dskey, 
								dist_pkey, 16, t_sharedKey );
				if( t_sharedKeyLen == 0 ) {
					ep_dbg_cprintf( Dbg, DBG_LEV, 
					"Fail to calculate the encryption key for key file\n");
					ep_crypto_key_free( dist_pkey );
					return NULL; 
				}
				t_sharedKey[16] = '\0'; 

				// temporary debugging 
				ep_print_hexstr( stdout, "GenSharedKey=", 16, 
										(uint8_t *)t_sharedKey);

				// 
				// Decrypt the encrypted key info 
				//

				// prepare environment 
				memset( de_iv, 0, EVP_MAX_IV_LENGTH );
				tsc_ctx = ep_crypto_cipher_new( 
							EP_CRYPTO_SYMKEY_AES128 | EP_CRYPTO_MODE_CBC, 
							(uint8_t *)t_sharedKey, de_iv, false );
				if(	tsc_ctx == NULL ) {
					ep_dbg_cprintf( Dbg, DBG_LEV, 
							"Fail to decode the encrypted key file\n");
					ep_crypto_key_free( dist_pkey );
					return NULL;
				}

				// get the encrypted contents in file 
				t_encLen = ep_get_fileContents( t_path, &encBuf );

				// prepare the buffer for decrypted plain contents 
				tmp_len = t_encLen + 16;
				skBuf   = ep_mem_zalloc( (size_t)tmp_len );
				if( skBuf == NULL ) {
					ep_dbg_cprintf( Dbg, DBG_LEV, 
							"Fail to malloc for decryption\n");
					ep_crypto_key_free( dist_pkey );
					ep_crypto_cipher_free( tsc_ctx );
					free( encBuf );
					return NULL;
				}

				// decrypt 
				r_estat = ep_crypto_cipher_crypt( tsc_ctx, (void *)encBuf, 
						(size_t)t_encLen, (void *)skBuf, (size_t)tmp_len);

				// LATER : check error 
				// reuse tmp_len 
				tmp_len = (int) EP_STAT_TO_INT( r_estat );
				if( tmp_len < 0 || tmp_len > t_encLen ) {
					ep_dbg_cprintf( Dbg, DBG_LEV, 
							"Fail to decrypt the encrypted key file\n");
					ep_crypto_key_free( dist_pkey );
					ep_crypto_cipher_free( tsc_ctx );
					free( encBuf );
					free( skBuf );
					return NULL;
				}
				skBuf[tmp_len] = '\0';

				// temporary debugging 
				printf("**********  decrypted key info **********\n");
				printf("INFO] In file length: %d \n", tmp_len );
				printf("%s \n", skBuf);

				
				// 
				// Parse the decrypted key info 
				// Make the symkey_info from the parsed key info 
				// 
				if( a_prekey != NULL ) {
					t_dkey = a_prekey; 

					t_dkey->sym_key_alg  = 0;
					t_dkey->sym_key_mode = 0;
					t_dkey->salt_len	 = 0;
					t_dkey->sym_key[0] 	 = '\0';
					t_dkey->sym_iv[0] 	 = '\0';

				} else t_dkey = ep_mem_zalloc( sizeof(symkey_info) );
				if( t_dkey == NULL ) {
					ep_dbg_cprintf( Dbg, DBG_LEV, 
							"Fail to get the memory for symkey_info\n");
					ep_crypto_key_free( dist_pkey );
					ep_crypto_cipher_free( tsc_ctx );
					free( encBuf );
					free( skBuf );
					return NULL;
				}
				fill_symkeyinfo_fromstr( skBuf, t_dkey );
				
				// check parsed info (for temporary debugging) :: hsmoon
				//print_symkeyinfo( stdout, t_dkey );


				// finalize 
				ep_crypto_key_free( dist_pkey );
				ep_crypto_cipher_free( tsc_ctx );
				free( encBuf );
				free( skBuf );


				return t_dkey;

			default:  
				ep_dbg_cprintf( Dbg, DBG_LEV, 
								"UNDefined Key type in key dist\n");
				return NULL;	

		}
		return t_dkey;


	} else if ( a_rMode == MODE_LOGKEY_SERVICE ) {

		return get_logenckey( a_dskey, a_logName, a_oargu );

	} 
	

	return t_dkey;
}



/*
** This is a key check function for test mode using an key file to be already 
** distributed in the device.  
** In general, the key check function provided by key management service 
** can check whether the key is valid on the requested time or log entry. 
**
** 1'st argu: the key info to be checked. 
** 2'nd argu: the info about the requested environment 
** rval		: true with valid key. false with invalid key. 
*/ 
bool
is_valid_key_on_requested_env_t1( symkey_info *a_inKey, void *a_info )
{
	// If Key already exists, return true. 
	// Else, return false. 

	if( a_inKey == NULL ) return false;

	if( strlen( (const char *)(a_inKey->sym_key) ) == 0 ) return false;

	return true;
}



/*
** This is a key load function for test mode using an key file to be already 
** distributed in the device.  
**
** 1'st argu: data structure with necessary information to load key in each  
**				Key management service. In test mode, this value is NULL. 
**				This data structure can contain the log guid, the rec num of 
**				log entry, time info, and so on. 
** rval		: loaded key info
** [NOTE] if the key_ctx of loaded key info is not NULL, 
**		calling process needs to re initialize the crypto context(key_ctx)
*/ 
symkey_info* 
load_log_enc_key_t1( symkey_info *a_curKey, void *a_Info)
{
	bool				t_status;
	symkey_info			*outKey = NULL;



	if( a_curKey != NULL ) {
		if( is_valid_key_on_requested_env_t1( a_curKey, NULL ) ) {
			return a_curKey;
		}

		// program has invalid key now... 
		// reuse the memory to be already allocated for a_curKey
		// LATER: the memory for cipher context can be reused..  
		outKey = a_curKey;
		if( outKey->key_ctx != NULL ) {
			ep_crypto_cipher_free( outKey->key_ctx );
			outKey->key_ctx = NULL;
		}

	}



	// Check whether device has the enough information to get
	//		log encryption key in test mode. 
	t_status = check_device_for_log_encdec_t1( NULL, NULL );
	if( t_status == false ) {
		printf("This device does not have enough information" 
				" for log data key in test mode \n");	
		return NULL;

		// if outKey is not NULL, t_status must be always true. 
	}


	// Get the device secret key 
	if( dev_skey == NULL ) {
		dev_skey = get_gdp_device_skey_read( NULL, NULL );
		if( dev_skey == NULL ) {
			printf("Fail to get the device secret key\n");
			return outKey; 
		}
	}




	outKey = get_logdatakey( MODE_LOGKEY_TEST, dev_skey, 
								NULL, 0, NULL, outKey );



	return outKey;

}



/*
** This function free the memory to be allocated & used in this file. 
** If load_log_enc_key function is called, this function must be called.  
*/ 
void
free_logkeymodule( symkey_info *a_inKey )
{
	if( dev_skey != NULL ) 
				ep_crypto_key_free( dev_skey );

	if( a_inKey != NULL ) {
		if( a_inKey->key_ctx != NULL ) 
				ep_crypto_cipher_free( a_inKey->key_ctx ); 

		ep_mem_free( a_inKey );
	}
}



/*
** This function is append filter to encrypt the log data in test mode 
** using an key file to be already distributed in the device.  
** Of course, in test mode, key is not changed periodically. 
**
** 1'st argu: data structure with the log entry contents 
** 2'nd argu: data structure with necessary info to encrypt the log data. 
**				In test mode, this data is symkey_info
** rval		: EP_STAT : int value has encrypted length if success. 
**					Otherwise, int value has 0 or error value?.  
*/
EP_STAT 
append_filter_for_t1(gdp_datum_t *a_logEntry, void *a_info)
{
	apnd_data				*curInfo = (apnd_data *)a_info; 
	EP_CRYPTO_CIPHER_CTX	*curCtx  = NULL;
	EP_STAT					r_estat;


	if( a_info == NULL ) {
		ep_dbg_cprintf( Dbg, 0, 
				"[FAIL_APND] Need the key information in append_filter" );
		 printf("[FAIL_APND] Need the key information in append_filter\n" );
		return EP_STAT_SEVERE;
	}


	// Load the valid encryption key at this time. 
	curInfo->key_info = load_log_enc_key_t1( curInfo->key_info, NULL );

	if( curInfo->key_info == NULL ) {
		ep_dbg_cprintf( Dbg, 0, 
				"[FAIL_APND] Fail to load enc key info" );
		 printf("[FAIL_APND] Fail to load enc key info\n" );
		return EP_STAT_SEVERE;
	}


	// Init or Reinit the crypto cipher context to encrypt the requested data
	curCtx = curInfo->key_info->key_ctx; 
	if( curCtx == NULL ) {
		// Init  (encrypt mode : true) 
		curCtx = ep_crypto_cipher_new( 
			curInfo->key_info->sym_key_alg | curInfo->key_info->sym_key_mode, 
			curInfo->key_info->sym_key, curInfo->key_info->sym_iv, true) ;

	} else {
		bool		t_isOk = false;
		t_isOk = ep_crypto_cipher_reinit( curCtx, 
			curInfo->key_info->sym_key_alg | curInfo->key_info->sym_key_mode, 
			curInfo->key_info->sym_key, curInfo->key_info->sym_iv, true) ;

		if( t_isOk == false ) {
			curCtx = ep_crypto_cipher_new( 
			curInfo->key_info->sym_key_alg | curInfo->key_info->sym_key_mode, 
			curInfo->key_info->sym_key, curInfo->key_info->sym_iv, true) ;
		}
	}

	if(	curCtx == NULL ) {
		ep_dbg_cprintf( Dbg, 0, 
				"[FAIL_APND] Fail to init or reinit crypto context" );
		printf("[FAIL_APND] Fail to init or reinit crypto context\n" );
		return EP_STAT_SEVERE;
	}
	curInfo->key_info->key_ctx = curCtx;	


	//
	// Encrypt the log data 
	// 

	// prepare the data structure 
	size_t			inLen, enLen, t_rval; 
	uint8_t			*oriBuf = NULL;
	uint8_t			*encBuf	= NULL;
	gdp_buf_t		*t_cbuf = NULL;

	inLen	= gdp_datum_getdlen( (const gdp_datum_t *)a_logEntry );
	enLen	= inLen + 16;
	oriBuf = ep_mem_zalloc( inLen + 1 );
	encBuf = ep_mem_zalloc( enLen + 1 );
	if( oriBuf == NULL || encBuf == NULL ) {
		ep_dbg_cprintf( Dbg, 0, 
				"[FAIL_APND] Fail to allocate the buffer memory" );
		printf("[FAIL_APND] Fail to allocate the buffer memory\n" );

		if( oriBuf != NULL ) ep_mem_free( oriBuf );
		if( encBuf != NULL ) ep_mem_free( encBuf );

		return EP_STAT_OUT_OF_MEMORY;
	}

	// fill the oriBuf with the original data in dbuf. 
	t_cbuf = gdp_datum_getbuf( (const gdp_datum_t *)a_logEntry );
	t_rval = gdp_buf_read( t_cbuf, oriBuf, inLen + 1 ); 
	if( t_rval != inLen ) {
		ep_dbg_cprintf( Dbg, 0, 
				"[FAIL_APND] Fail to read original data (%lu | %lu)", 
				t_rval, inLen );
		printf(	"[FAIL_APND] Fail to read original data (%lu | %lu)\n", 
				t_rval, inLen );

		if( oriBuf != NULL ) ep_mem_free( oriBuf );
		if( encBuf != NULL ) ep_mem_free( encBuf );

		return EP_STAT_SEVERE;
	}
	oriBuf[inLen] = '\0';

	//
	// encrypt the log contents in oriBuf 
	r_estat = ep_crypto_cipher_crypt( curCtx, (void *)oriBuf, inLen, 
						(void *)encBuf, enLen );
	t_rval = EP_STAT_TO_INT( r_estat );

	// LATER : check error 
	if( t_rval < 0 || t_rval > enLen ) {
		ep_dbg_cprintf( Dbg, DBG_LEV, 
				"[FAIL_APND] Fail to encrypt original data (%zd | %zd)", 
				t_rval, enLen );

		if( oriBuf != NULL ) ep_mem_free( oriBuf );
		if( encBuf != NULL ) ep_mem_free( encBuf );

		return EP_STAT_SEVERE;
	}
	encBuf[t_rval] = '\0';


	// temporary debugging 
	printf("******  Original Log data (%zu) in Apnd ******\n", inLen);
	printf("%s\n", oriBuf);
	printf("***  Encrypted Log data (%zu) in Apnd (hex) ***\n", t_rval);
	ep_print_hexstr( stdout, "", t_rval, encBuf );


	//
	// Put the encrypted log data in datum 
	// 
	gdp_buf_write( t_cbuf, (const void *)encBuf, t_rval );



	return EP_STAT_OK; 

}



/*
** This function is read filter to decrypt the log data in test mode 
** using an key file to be already distributed in the device.  
** Of course, in test mode, key is not changed periodically. 
**
** 1'st argu: data structure with the log entry contents 
** 2'nd argu: data structure with necessary info to decrypt the log data. 
**				In test mode, this data is symkey_info
** rval		: EP_STAT : int value has encrypted length if success. 
**					Otherwise, int value has 0 or error value?.  
*/
EP_STAT 
read_filter_for_t1(gdp_datum_t *a_logEntry, void *a_info)
{
	apnd_data				*curInfo = (apnd_data *)a_info; 
	EP_CRYPTO_CIPHER_CTX	*curCtx  = NULL;
	EP_STAT					r_estat;
	gdp_recno_t				curRec   = gdp_datum_getrecno( a_logEntry );	



	// Only this filter has to be applied on the log record. 
	// The record number for the first record is 1. 
	if( curRec < 1 )  return EP_STAT_OK;


	printf("RF] Called Read filter... at %" PRIgdp_recno " ******* \n", 
					curRec ); 


	// Check condition 
	if( a_info == NULL ) {
		ep_dbg_cprintf( Dbg, 0, 
				"[FAIL_RF] Need the key information in read_filter" );
		 printf("[FAIL_RF] Need the key information in read_filter\n" );
		return EP_STAT_SEVERE;
	}


	// Load the valid decryption key at this time. 
	curInfo->key_info = load_log_enc_key_t1( curInfo->key_info, NULL );

	if( curInfo->key_info == NULL ) {
		ep_dbg_cprintf( Dbg, 0, 
				"[FAIL_RF] Fail to load dec key info" );
		 printf("[FAIL_RF] Fail to load dec key info\n" );
		return EP_STAT_SEVERE;
	}


	// Prepare crypto cipher context for decryption.. 
	// Init or Reinit the crypto cipher context 
	curCtx = curInfo->key_info->key_ctx; 
	if( curCtx == NULL ) {
		// Init  (decrypt mode : false) 
		curCtx = ep_crypto_cipher_new( 
		   curInfo->key_info->sym_key_alg | curInfo->key_info->sym_key_mode, 
		   curInfo->key_info->sym_key, curInfo->key_info->sym_iv, false) ;

	} else {
		bool		t_isOk = false;
		t_isOk = ep_crypto_cipher_reinit( curCtx, 
    	   curInfo->key_info->sym_key_alg | curInfo->key_info->sym_key_mode, 
		   curInfo->key_info->sym_key, curInfo->key_info->sym_iv, false) ;

		if( t_isOk == false ) {
			curCtx = ep_crypto_cipher_new( 
		    curInfo->key_info->sym_key_alg | curInfo->key_info->sym_key_mode,
			curInfo->key_info->sym_key, curInfo->key_info->sym_iv, false) ;
		}
	}

	if(	curCtx == NULL ) {
		ep_dbg_cprintf( Dbg, 0, 
				"[FAIL_RF] Fail to init or reinit crypto context" );
		printf("[FAIL_RF] Fail to init or reinit crypto context\n" );
		return EP_STAT_SEVERE;
	}
	curInfo->key_info->key_ctx = curCtx;	


	//
	// decrypt the log data 
	// 

	// prepare the data structure 
	size_t			inLen, decLen, t_rval; 
	uint8_t			*oriBuf = NULL;
	uint8_t			*decBuf	= NULL;
	uint8_t			*t_dpos = NULL;
	gdp_buf_t		*t_cbuf = NULL;



	inLen	= gdp_datum_getdlen( (const gdp_datum_t *)a_logEntry );
	// Added length (32) can be optimized according to the sym algorithm. 
	decLen	= inLen  + 32 ;  
	oriBuf  = ep_mem_zalloc( inLen  + 1 );
	decBuf  = ep_mem_zalloc( decLen + 1 );
	if( oriBuf == NULL || decBuf == NULL ) {
		ep_dbg_cprintf( Dbg, 0, 
				"[FAIL_RF] Fail to allocate the buffer memory" );
		printf("[FAIL_RF] Fail to allocate the buffer memory\n" );

		if( oriBuf != NULL ) ep_mem_free( oriBuf );
		if( decBuf != NULL ) ep_mem_free( decBuf );

		return EP_STAT_OUT_OF_MEMORY;
	}


	// fill the oriBuf with the encrypted log record in dbuf. 
	t_cbuf = gdp_datum_getbuf( (const gdp_datum_t *)a_logEntry );
	t_dpos = gdp_buf_getptr( t_cbuf, inLen ); 
	memcpy( (void *)oriBuf, (const void *)t_dpos, inLen );
	oriBuf[inLen] = '\0';


	//
	// decrypt the log contents in oriBuf 
	// 
	r_estat = ep_crypto_cipher_crypt( curCtx, (void *)oriBuf, inLen, 
						(void *)decBuf, decLen );
	t_rval = EP_STAT_TO_INT( r_estat );

	// LATER : check error 
	if( t_rval < 0 || t_rval > decLen ) {
		ep_dbg_cprintf( Dbg, DBG_LEV, 
				"[FAIL_RF] Fail to decrypt original data (%zd | %zd)", 
				t_rval, decLen );
		printf("[FAIL_RF] Fail to decrypt original data (%zd | %zd)\n", 
				t_rval, decLen );

		if( oriBuf != NULL ) ep_mem_free( oriBuf );
		if( decBuf != NULL ) ep_mem_free( decBuf );

		return EP_STAT_SEVERE;
	}
	decBuf[t_rval] = '\0';


	// temporary debugging 
	printf("***  Original Log data (%lu) in RF (hex) ***\n", inLen);
	ep_print_hexstr( stdout, "", inLen, oriBuf );
	printf("***  Decrypted Log data (%lu) in Apnd  ***\n", t_rval);
	printf( "%s\n" , decBuf );


	//
	// Change the encrypted log data into the decrypted log data in datum 
	// 
	gdp_buf_drain( t_cbuf, inLen );
	gdp_buf_write( t_cbuf, (const void *)decBuf, t_rval );


	return EP_STAT_OK; 
}

