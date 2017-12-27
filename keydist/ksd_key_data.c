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
**  KSD_KEY_DATA - Manage the information related with Key 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 


#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include <ep/ep_dbg.h>
#include <hs/hs_file.h>


#include "ksd_key_data.h"
#include "ksd_key_gen.h"


#define	KGEN_PREFIX		"/home/hsmoon/etc/kgen"
#define	BUF_LEN			64

static EP_CRYPTO_CIPHER_CTX		*devCtx = NULL;


//
// Internal Functions
// 

// Return the file name related with the key gen param index (first argu)
// The file name is stored in the seconde argument. 
// return value: the length of output file name  
//				 0 if  an error is occurred. 
int get_kgen_fname( int a_pinx, char *outFname, int bufLen )
{
	int			tlen; 

	tlen = snprintf( outFname, bufLen, "%s/%d.enc", KGEN_PREFIX, a_pinx );
	if( tlen >= bufLen ) {
		ep_dbg_printf("[ERROR] Need more longer buffer (%d vs. %d)"
						" for kgen_fname\n",  tlen, bufLen );
		return 0;
	}

	return tlen;
}


// Open the file with kgen parameter 
// Return the file handle 
FILE* open_kgen_file( int a_pinx, const char *a_mode )
{
	int			tval;
	FILE		*ofp = NULL;
	char		name[BUF_LEN];


	// current not encoding (for quick debugging) 
	// later :: name encoding is necessary? 
	tval = get_kgen_fname( a_pinx, name, BUF_LEN ); 
	if( tval == 0 ) return NULL;

	ofp = fopen( name, a_mode );

	return ofp;
}
// hsmoon_dend


// Current, use the fixed encryption key 
EP_CRYPTO_CIPHER_CTX* load_dev_crypt_ctx( bool EncMode )	
{
	static uint8_t	sym_key[] = "E13C05168A62242778DBE9296BC2435A";
	static uint8_t	sym_iv[]  = "D642F42D9B5E20C16A2F8BA6B96707A7";
	int				sym_key_alg  = EP_CRYPTO_SYMKEY_AES128;  
	int				sym_key_mode = EP_CRYPTO_MODE_CBC;


	if( devCtx == NULL ) {
		devCtx = ep_crypto_cipher_new( 
			sym_key_alg | sym_key_mode, 
			sym_key, sym_iv, EncMode );

	} else {
		bool		isOK = false; 

		isOK = ep_crypto_cipher_reinit( devCtx, 
			sym_key_alg | sym_key_mode, 
			sym_key, sym_iv, EncMode );

		if( isOK == false ) {
			devCtx = ep_crypto_cipher_new( 
					sym_key_alg | sym_key_mode, 
					sym_key, sym_iv, EncMode );
		}

	}

	if( devCtx == NULL ) {
		ep_dbg_printf("[ERROR] Fail to init / reinit crypto context\n");
	}

	return devCtx;
}


void update_kgen_param( KGEN_param *inParam )
{
	// Decide the final security_level & keygenfunID 
	//	supported in this service. 
	// current only one key gen function: 
	//	1 hash based key rotation / 1 random generation. 
	if( inParam->security_level < 30 ) {
		// currently: not supported 
		inParam->security_level += 30;
		inParam->chain_length    = 12;

//		if( inParam->security_level > 50 ) 
//			inParam->change_thval2  = 1000;  

	} else if( inParam->security_level > 60 && inParam->security_level < 90 ) {
		inParam->security_level -= 30;
	}

	if( inParam->security_level > 90 ) {
		// later: random selection among random generation functions.. 
		inParam->keygenfunID =  RAND_GEN1;
	} else {
		// later: decision based on "security_level%10" 
		// Now only one.. 
		inParam->keygenfunID = RO_HASH_GEN1;
	}

}


// if there is not enough buffer space to store, return 0. 
// else return the stored length. 
int convert_from_kgenparam_to_instr( KGEN_param *inParam, 
										char *outBuf, int bufLen )
{
	int			cLen = 0;
	int			rLen = bufLen;
	int			wLen = 0;
	char		*wPos = outBuf; 

	if( inParam == NULL ) return wLen; 

	cLen = snprintf( wPos, rLen, "%s\n", 
				ep_crypto_keyenc_name( inParam->sym_key_alg ) );
	wPos += cLen; 
	rLen -= cLen;
	if( rLen <= 0 ) return 0; 

	cLen = snprintf( wPos, rLen, "%s\n", 
				ep_crypto_mode_name( inParam->sym_key_mode ) );
	wPos += cLen; 
	rLen -= cLen;
	if( rLen <= 0 ) return 0; 
	
	cLen = snprintf( wPos, rLen, "%d %d\n", inParam->security_level,  
						inParam->chain_length);
	wPos += cLen; 
	rLen -= cLen;
	if( rLen <= 0 ) return 0; 

	cLen = snprintf( wPos, rLen, "%d %d\n", inParam->min_key_period, 
						inParam->max_key_period);
	wPos += cLen; 
	rLen -= cLen;
	if( rLen <= 0 ) return 0; 

	cLen = snprintf( wPos, rLen, "%d\n", inParam->change_thval1 );
//	cLen = snprintf( wPos, rLen, "%d %d\n", inParam->change_thval1, 
//						inParam->change_thval2);
	wPos += cLen; 
	rLen -= cLen;
	if( rLen <= 0 ) return 0; 

	wLen = wPos - outBuf;
	*wPos = '\0';


	return wLen;
}


// error check 
int convert_from_instr_to_kgenparam( KGEN_param *outParam, char *inBuf )
{
	int			line_num	= 0;
	int			exit_status = EX_OK; 
	char		*curLine	= NULL;
	int			val1, val2; 

	if( outParam == NULL ) return EX_INVALIDDATA; 

	curLine = strtok( inBuf, "\n" );
	while( curLine != NULL ) {

		if( line_num == 0 ) {
			outParam->sym_key_alg = ep_crypto_keyenc_byname( curLine ); 

		} else if( line_num == 1 ) {
			outParam->sym_key_mode = ep_crypto_mode_byname( curLine );

		} else if( line_num == 2 ) {
			sscanf( curLine, "%d %d", &val1, &val2 );
			outParam->security_level = val1;
			outParam->chain_length	 = val2;

		} else if( line_num == 3 ) {
			sscanf( curLine, "%d %d", &val1, &val2 );
			outParam->min_key_period = val1;
			outParam->max_key_period = val2;

		} else if( line_num == 4 ) {
//			sscanf( curLine, "%d %d", &val1, &val2 );
			sscanf( curLine, "%d", &val1 );
			outParam->change_thval1 = val1;
//			outParam->change_thval2 = val2;

		} else break;

		line_num++;
		curLine = strtok( NULL, "\n" );

	}


	return exit_status;

}


void print_kgenparam( KGEN_param *inParam, FILE *ofp ) 
{
	if( inParam == NULL ) {
		fprintf( ofp, "[ERROR] Null kgen_param\n");
		return ;
	}

	flockfile( ofp );

	fprintf( ofp, "----- Param at %p \n", inParam ); 
	fprintf( ofp, "Alg = %s \n",  
				ep_crypto_keyenc_name( inParam->sym_key_alg ) );

	fprintf( ofp, "Mode = %s \n",  
				ep_crypto_mode_name( inParam->sym_key_mode ) );

	fprintf( ofp, "funID = %d \n", inParam->keygenfunID );  
	fprintf( ofp, "sLevel = %d \n", inParam->security_level );  
	fprintf( ofp, "Chain len = %d \n", inParam->chain_length);
	fprintf( ofp, "Period = %d %d\n", inParam->min_key_period, 
						inParam->max_key_period);
	fprintf( ofp, "Thval = %d\n", inParam->change_thval1 );
//	fprintf( ofp, "Thval = %d %d\n", inParam->change_thval1, 
//						inParam->change_thval2 );

	funlockfile( ofp );
}


int read_int_in_nbuf( unsigned char *inData )
{
	int			nVal, hVal;
	
	memcpy( &nVal, inData, 4);
	hVal = ntohl( nVal );

	return hVal;
}


//
// External APIs
// 

//
// Functions related with KGEN_param data structure
// 

// After reading the file identified by first argu 
//  create and reaturn the kgen_param 
KGEN_param* load_kgen_param( int  a_pinx, char rw_mode )
{
	int						tval, wLen;
	char					name[BUF_LEN];
	char					*encBuf = NULL;
	char					decBuf[1024]; 
	EP_STAT					estat;
	KGEN_param				*newParam	= NULL;
	EP_CRYPTO_CIPHER_CTX	*curCtx		= NULL;



	newParam = (KGEN_param *)ep_mem_malloc( sizeof(KGEN_param) );
	if( newParam == NULL ) {
		ep_dbg_printf( "[ERROR] Fail to get memeory for kgen_param \n" );
		return NULL;
	}

	if( rw_mode == 'r' ) {
		newParam->sym_key_alg  = 0;
		newParam->sym_key_mode = 0;
		return newParam;
	}

	tval = get_kgen_fname( a_pinx, name, BUF_LEN ); 
	if( tval == 0 ) {
		ep_dbg_printf("[ERROR] Fail to get filename kgen_param %d \n", a_pinx);
		ep_mem_free( newParam );
		return NULL;
	}


	// 1. read the encrypted contents in the file 
	tval = ep_get_fileContents( (const char*)name, &encBuf );


	// 2. prepare cipher context for decryption 
	curCtx = load_dev_crypt_ctx( false );

	// 3. decrypt 
	estat = ep_crypto_cipher_crypt( curCtx, (void *)encBuf, tval, 
									(void *)decBuf, 1024 ); 
	wLen = EP_STAT_TO_INT( estat );
	if( wLen < 0 || wLen > 1024 ) {
		ep_dbg_printf("[ERROR] KGEN_param decrypt rval: %d \n", wLen );
		free( encBuf );
		ep_mem_free( newParam );
		return NULL; 
	}
	decBuf[wLen] = '\0';


	// 4. convert instr to KGEN_param 
	tval = convert_from_instr_to_kgenparam( newParam, decBuf );
	if( tval != EX_OK ) {
		ep_dbg_printf("[ERROR] Fail to parse the KGEN_param at %d \n", a_pinx );
		free( encBuf );
		ep_mem_free( newParam );
		return NULL; 
	}

	free( encBuf );

	return newParam;
}


int store_kgen_param( int a_pinx, KGEN_param *inData ) 
{
	FILE					*wfp		= NULL;
	int						exit_status = EX_OK;
	int						rLen, wLen;
	char					plainBuf[1024];
	char					encBuf[1024];
	EP_CRYPTO_CIPHER_CTX	*curCtx		= NULL;
	EP_STAT					estat; 
	

	wfp = open_kgen_file( a_pinx, "wb" );
	if( wfp == NULL ) {
		ep_dbg_printf("[ERROR] KGEN_param file open err: %s \n", 
				strerror( errno ) );
		return EX_FILEHANDLE; 
	}


	// Prepare the crypto cipher context 
	curCtx = load_dev_crypt_ctx( true );

	// Prepare the param string 
	rLen = convert_from_kgenparam_to_instr( inData, plainBuf, 1024 );
	if( rLen == 0 ) {
		ep_dbg_printf("[ERROR] KGEN_param convert Buffer size: %d \n%s", 
				1024, plainBuf );
		fclose( wfp );
		return EX_FILEHANDLE; 
	}

	// Encrypt the param string  
	estat = ep_crypto_cipher_crypt( curCtx, (void *)plainBuf, rLen, 
									(void *)encBuf, 1024 ); 
	wLen = EP_STAT_TO_INT( estat );
	if( wLen < 0 || wLen > 1024 ) {
		ep_dbg_printf("[ERROR] KGEN_param encrypt rval: %d \n%s", 
				wLen, plainBuf );
		fclose( wfp );
		return EX_FILEHANDLE; 
	}
	encBuf[wLen] = '\0';

	// Write the encrypted param string 
	fwrite( encBuf, wLen, sizeof( char ), wfp ); 

	fclose( wfp );

	return exit_status;
}




// Convert the Data received from network to the KGEN_param data structure. 
// Data (second argu: inData) is network byte order
// This data is one part of byte array pointed by gdp_buf_getptr( datum->dbuf )
// ref>  gdp_buf_getlength  	
KGEN_param* convert_to_kgen_param( unsigned char *inData )
{
	KGEN_param		*newParam = NULL;


	newParam = (KGEN_param *) ep_mem_malloc( sizeof(KGEN_param) );
	if( newParam == NULL ) {
		ep_dbg_printf( "[ERROR] Fail to get memory for KGEN_param \n" );
		return NULL;
	}

	// inData : network byte order... 
	newParam->sym_key_alg = read_int_in_nbuf( inData );
	inData += 4;

	newParam->sym_key_mode = read_int_in_nbuf( inData );
	inData += 4;

	newParam->security_level = read_int_in_nbuf( inData );
	inData += 4;

	newParam->min_key_period = read_int_in_nbuf( inData );
	inData += 4;

	newParam->max_key_period = read_int_in_nbuf( inData );
	inData += 4;

	newParam->change_thval1 = read_int_in_nbuf( inData );
	inData += 4;

//	newParam->change_thval2 = read_int_in_nbuf( inData );
//	inData += 4;

	newParam->chain_length = read_int_in_nbuf( inData );

	update_kgen_param( newParam );
	
	return newParam;
}


// Put KGEN_param data strucutre in the gdp buf
//			to send the data through network. 
// Converted data is based on network-byte order. 
void put_kgenparam_to_buf( KGEN_param *inParam, gdp_datum_t *outData )
{
	gdp_buf_t		*odBuf =  gdp_datum_getbuf( (const gdp_datum_t *)outData ); 


	// LATER: the entries in KGEN_param can have the int16_t type 
	//				instead of int type. 
	gdp_buf_put_int32( odBuf, inParam->sym_key_alg );
	gdp_buf_put_int32( odBuf, inParam->sym_key_mode );
	gdp_buf_put_int32( odBuf, inParam->security_level );
	gdp_buf_put_int32( odBuf, inParam->min_key_period );
	gdp_buf_put_int32( odBuf, inParam->max_key_period );
	gdp_buf_put_int32( odBuf, inParam->change_thval1 );
//	gdp_buf_put_int32( odBuf, inParam->change_thval2 );
	gdp_buf_put_int32( odBuf, inParam->chain_length );

}


// Return the KGEN_param data 
// If any input argument has the value of 0 or NULL string, 
//		the related field has the default value. 
KGEN_param* get_new_kgen_param( char *inAlg, char *inMode, int inSlevel )
{
	int				tmpVal;
	KGEN_param		*newParam = NULL;
	
	
	newParam = (KGEN_param *)ep_mem_malloc( sizeof(KGEN_param) );
	if( newParam == NULL ) {
		ep_dbg_printf( "[ERROR] Fail to get memory for KGEN_param data \n");
		return newParam;
	}


	if( inAlg == NULL ) newParam->sym_key_alg = EP_CRYPTO_SYMKEY_AES128;
	else {
		tmpVal = ep_crypto_keyenc_byname( inAlg );

		if( tmpVal == -1 || tmpVal == EP_CRYPTO_SYMKEY_NONE ) 
					newParam->sym_key_alg = EP_CRYPTO_SYMKEY_AES128;
		else newParam->sym_key_alg  = tmpVal;
	}


	if( inMode == NULL ) newParam->sym_key_mode = EP_CRYPTO_MODE_CBC; 
	else {
		tmpVal = ep_crypto_mode_byname( inMode );

		if( tmpVal == -1 ) newParam->sym_key_mode = EP_CRYPTO_MODE_CBC;
		else newParam->sym_key_mode  = tmpVal;
	}


	// 
	// LATER: consider the default value including security level.. 
	// inSlevel value is changned to one of the below values  
	//			according to the security priority (ex> min, mid, max) and 
	//			HW (ex> CPU, memory ... ) sepc. 
	//			Currently, changed value is used. 	

	
	tmpVal = inSlevel % 10;
	if( tmpVal > 5 ) inSlevel = inSlevel - 4; 
	// The inSlevel%10 identifies the key generation algorithm. 
	//	case 1: // key rotation
	//	case 2: // key regression
	//	case 3: // key updating 
	//	case 4: // buffer for other key generation
	//	case 5: // buffer for other key generation



	if( inSlevel == 0 ) inSlevel = 31;
	else if( inSlevel >5 && inSlevel<10 ) inSlevel = inSlevel-5;
	else newParam->security_level = inSlevel; 

	newParam->security_level = inSlevel;
	newParam->chain_length   = 0;
	newParam->change_thval1  = 0;
//	newParam->change_thval2  = 0;
	newParam->max_key_period = 0;  
	newParam->min_key_period = 0;

	tmpVal = inSlevel / 10; 
	// the inSlevel/10 identifies the detailed key gen parameter... 
	switch( tmpVal ) {
		// one hash chain. so RSA based key generation.  
		// Key is changed periodically regardless of user revocation
		case 0: 
			newParam->max_key_period = 3600; // default key change period 1hour 
			break;

		// one hash chain. so RSA based key generation.  
		// Key is changed each revocation  
		case 1: 
			break;

		// one hash chain. so RSA based key generation.  
		// Key change can be affected by both revocation and time 
		case 2: 
			newParam->change_thval1  = 100;
			newParam->max_key_period = 3600;	// 1 hour  
			newParam->min_key_period = 60;		// 1 min 
			break;


		// multiple hash chain.  hash based key generation 
		// Key is changed periodically regardless of user revocation
		case 3: 
			newParam->chain_length   = 12;
			newParam->max_key_period = 3600; // default key change period 1hour 
			break;

		// multiple hash chain.  hash based key generation 
		// Key is changed each revocation  
		case 4: 
			newParam->chain_length   = 12;
			break;

		// multiple hash chain.  hash based key generation 
		// Key change can be affected by both revocation and time 
		case 5: 
			newParam->chain_length   = 12;
			newParam->change_thval1  = 100;
//			newParam->change_thval2  = 1000;  
			newParam->max_key_period = 3600;	// 1 hour  
			newParam->min_key_period = 60;		// 1 min 
			break;


		// multiple hash chain.  RSA based key generation 
		// Key is changed periodically regardless of user revocation
		case 6: 
			newParam->chain_length   = 16;
			newParam->max_key_period = 3600; // default key change period 1hour 
			break;

		// multiple hash chain.  RSA based key generation 
		// Key is changed each revocation  
		case 7: 
			newParam->chain_length   = 16;
			break;

		// multiple hash chain.  RSA based key generation 
		// Key change can be affected by both revocation and time 
		case 8: 
			newParam->chain_length   = 32;
			newParam->change_thval1  = 100;
//			newParam->change_thval2  = 1600;	
			newParam->max_key_period = 3600;	// 1 hour  
			newParam->min_key_period = 300;		// 5 min 
			break;

		// no hash chain.  random key generation 
		// Key is changed periodically regardless of user revocation
		case 9: 
			newParam->chain_length   = 1;
			newParam->max_key_period = 3600; // default key change period 1hour 
			break;

		// no hash chain.  random key generation 
		// Key is changed each revocation  
		case 10: 
			newParam->chain_length   = 1;
			break;

		// no hash chain.  random key generation 
		// Key change can be affected by both revocation and time 
		case 11: 
		default: // above value 
			newParam->chain_length   = 1;
			newParam->change_thval1  = 100;
			newParam->max_key_period = 3600;	// 1 hour  
			newParam->min_key_period = 300;		// 5 min 
			break;

	}

	return newParam;
}


int		free_Rkey_num   = 0;
RKEY_1	*freeRkey1 = NULL;

void return_RKEY1( RKEY_1 *rKey )
{
	if( free_Rkey_num > 50 ) {
		ep_mem_free( rKey );
		return ; 
	}

	rKey->next = freeRkey1;
	freeRkey1  = rKey;

	free_Rkey_num++;
}


void return_RKEY1s( RKEY_1 *rKey )
{
	RKEY_1		*tmpKey;
	RKEY_1		*delKey = rKey;

	if( free_Rkey_num > 50 ) {
		while( delKey != NULL ) {
			tmpKey = delKey->next;
			ep_mem_free( delKey );
			delKey = tmpKey;
		}
		return ; 
	}

	while( delKey->next != NULL ) {
		delKey = delKey->next;
		free_Rkey_num++;
	}

	delKey->next = freeRkey1;
	freeRkey1  = rKey;
	free_Rkey_num++;

}


RKEY_1* get_new_RKEY1( )
{
	RKEY_1		*newKey = NULL;

	if( freeRkey1 != NULL ) {
		newKey = freeRkey1;
		freeRkey1 = freeRkey1->next;

		newKey->next = NULL; 
		free_Rkey_num--;

		return newKey;

	} else  {
		newKey = (RKEY_1 *) ep_mem_malloc( sizeof( RKEY_1 ) ); 
		if( newKey == NULL )  {
			ep_dbg_printf("[ERROR] Fail to get memory for key \n");
			return NULL;
		}

		newKey->next = NULL; 
		return newKey;
	}

}


// Put KGEN_param data strucutre in the gdp buf
//			to send the data through network. 
// Converted data is based on network-byte order. 
void fill_dbuf_withKey( gdp_datum_t *oDatum, RKEY_1 *cKey, int alg, int mode )
{
	char			charval;
	uint16_t		sval, chsval;
	gdp_buf_t		*odBuf = NULL;
	
	
	odBuf = gdp_datum_getbuf( (const gdp_datum_t *)oDatum ); 


	// 1byte sym_key_alg
	charval = (char) (alg & 0xff); 
	gdp_buf_write( odBuf, &charval, 1 );

	// 1byte sym_key_mode 
	charval = (char) (mode & 0xff); 
	gdp_buf_write( odBuf, &charval, 1 );

	// 2byte key length 
	sval	= (uint16_t) ( strlen((const char *)(cKey->sym_key)) & 0xffff );
	chsval	= htons( sval );
	gdp_buf_write( odBuf, &chsval, 2 );

	// RKEY->sym_key : key length 
	gdp_buf_write( odBuf, cKey->sym_key, sval );

	// 2byte iv length  
	gdp_buf_write( odBuf, &chsval, 2 );

	// RKEY->sym_iv : iv length 
	gdp_buf_write( odBuf, cKey->sym_iv, sval );


	// 4byte pre_seqn
	gdp_buf_put_int32( odBuf, cKey->pre_seqn );

	// seqn info
	oDatum->recno = cKey->seqn;

	// time info 
	memcpy( &(oDatum->ts), &(cKey->ctime), sizeof cKey->ctime );
}



RKEY_1* convert_klog_to_RKEY( gdp_datum_t *rDatum, char *oVal1, char *oVal2 )
{
	RKEY_1				*newKey; 
	gdp_buf_t			*lBuf;
	int					rLlen, rPos; 
	uint16_t			tval, chval;
	unsigned char		*buf;



	if( rDatum == NULL ) return NULL;

	lBuf = gdp_datum_getbuf( rDatum );
	if( lBuf == NULL ) return NULL;

	rLlen = gdp_buf_getlength( lBuf );
	if( rLlen < 4 ) return NULL;
	buf   = gdp_buf_getptr( lBuf, rLlen );

	memcpy( (void *)&tval, (void *)&(buf[2]), 2 );
	chval = ntohs( tval );

	// finish to check the basic length error 
	if( rLlen < 2*chval + 6 ) return NULL;


	newKey = get_new_RKEY1( ); 
	if( newKey == NULL )  return NULL;

	// Need to compare the value in LKEY_info WITH buf[0] / buf[1] 
	if(  oVal1 != NULL ) {
		*oVal1 = buf[0];
		*oVal2 = buf[1];
	}

	memcpy( (void *)(newKey->sym_key), (void *)&(buf[4]), chval );
	newKey->sym_key[chval] = '\0'; 

	rPos = 4+chval;
	memcpy( (void *)&tval, (void *)&(buf[rPos]), 2 );
	chval = ntohs( tval );
	memcpy( (void *)(newKey->sym_iv), (void *)&(buf[rPos+2]), chval );
	newKey->sym_iv[chval] = '\0'; 

	// pre_seqn
	rPos = rPos+2+chval;
	if( rLlen < rPos + 4 )  newKey->pre_seqn = 0;
	else {
		int			intVal;

		memcpy( (void *)&intVal, (void *)&(buf[rPos]), 4 );
		newKey->pre_seqn = ntohl( intVal );
	}


	newKey->seqn = gdp_datum_getrecno( rDatum );
	gdp_datum_getts( rDatum, &(newKey->ctime) );
	newKey->next = NULL;

	return newKey;
}


int convert_klog_to_symkey( gdp_datum_t *rDatum, struct sym_rkey *oKey, 
															int *pre_seqn )
{
	int					rLlen, rPos; 
	int					exit_status = EX_OK; 
	uint16_t			tval, chval;
	unsigned char		*buf;
	gdp_buf_t			*lBuf;
	

	if( rDatum == NULL || oKey == NULL ) return EX_NOINPUT;

	lBuf = gdp_datum_getbuf( rDatum );
	if( lBuf == NULL ) return EX_NOINPUT;

	rPos  = 0;
	rLlen = gdp_buf_getlength( lBuf );
	if( rLlen < 4 ) return EX_INVALIDDATA;
	buf   = gdp_buf_getptr( lBuf, rLlen );

	
	oKey->sym_algorithm = buf[0];
	oKey->sym_mode      = buf[1];

	memcpy( (void *)&tval, (void *)&(buf[2]), 2 );
	chval = ntohs( tval );

	if( rLlen < 2*chval + 6 ) return EX_INVALIDDATA;

	memcpy( (void *)(oKey->sym_key), (void *)&(buf[4]), chval );
	oKey->sym_key[chval] = '\0'; 

	rPos = 4+chval;
	memcpy( (void *)&tval, (void *)&(buf[rPos]), 2 );
	chval = ntohs( tval );
	memcpy( (void *)(oKey->sym_iv), (void *)&(buf[rPos+2]), chval );
	oKey->sym_iv[chval] = '\0'; 

	
	rLlen = gdp_buf_getlength( lBuf );
	if( rLlen >= 4 ) {
		// 4byte pre_seqn
		*pre_seqn =	gdp_buf_get_int32( lBuf );
	}  else *pre_seqn = rDatum->recno - 1; 


	return exit_status;
}


char* serialize_klogdata( char algo, char mode, RKEY_1 *inKey, 
							char *outBuf, int oBuflen )
{
	int					wPos;
	uint16_t			tlen, chval;
	

	// 1byte algo, mode
	outBuf[0] = algo;
	outBuf[1] = mode;

	// 2byte key len
	tlen = (uint16_t) strlen( (const char *) (inKey->sym_key) );

	if( oBuflen < 2*tlen + 11 ) {
		ep_dbg_printf( "[ERROR] Short Buffer Size for klogdata %d vs. %d \n", 
					oBuflen, 2*tlen + 11 );
		return NULL;
	}

	chval = htons( tlen );
	memcpy( (void *)&(outBuf[2]), &chval, 2 ); 

	// key 
	memcpy( (void *)&(outBuf[4]), (void *)(inKey->sym_key), tlen ); 
	wPos = 4+ tlen; 


	// 2byte iv len
	tlen = (uint16_t) strlen( (const char *) (inKey->sym_iv) );
	chval = htons( tlen );
	memcpy( (void *)&(outBuf[wPos]), &chval, 2 ); 

	// iv 
	memcpy( (void *)&(outBuf[wPos+2]), (void *)(inKey->sym_iv), tlen ); 
	wPos = wPos + 2 + tlen; 


	if( inKey->pre_seqn != 0 ) {
		int			chintVal = htonl( inKey->pre_seqn ); 
		memcpy( (void *)&(outBuf[wPos]), &chintVal, 4 ); 
	}

	outBuf[wPos+4] = '\0'; 

	return outBuf;

}


struct hdr_rkey1* get_new_rkeyHdr1( RKEY_1 *inKey )
{
	struct hdr_rkey1	*newHdr = NULL;


	newHdr = (struct hdr_rkey1 *)ep_mem_malloc( sizeof( struct hdr_rkey1 ) );
	if( newHdr == NULL ) {
		ep_dbg_printf("[ERROR] Fail to get memory key hdr" );
		return NULL;
	}

	newHdr->child_num = 1;

	newHdr->childh    = inKey;
	newHdr->childt    = inKey;
	newHdr->next      = NULL;
	newHdr->pre       = NULL;

	ep_time_now( &(newHdr->latest_time) );
//	newHdr->latest_time.tv_sec  = 0;
//	newHdr->latest_time.tv_nsec = 0;

	return newHdr;
}


struct hdr_rkey2* get_new_rkeyHdr2( RKEY_1 *inKey )
{
	struct hdr_rkey2	*newHdr = NULL;


	newHdr = (struct hdr_rkey2 *)ep_mem_malloc( sizeof( struct hdr_rkey2 ) );
	if( newHdr == NULL ) {
		ep_dbg_printf("[ERROR] Fail to get memory key hdr" );
		return NULL;
	}

	newHdr->key_entry = inKey;
	newHdr->pre       = NULL;
	newHdr->next       = NULL;

	ep_time_now( &(newHdr->latest_time) );
//	newHdr->latest_time.tv_sec  = 0;
//	newHdr->latest_time.tv_nsec = 0;

	return newHdr;
}


void free_rkeyHdr1_keys( struct hdr_rkey1 *head ) 
{
	struct hdr_rkey1	*buf, *cur;

	cur = head;
	while( cur != NULL ) {
		buf = cur->next;

		return_RKEY1s( cur->childh );
		cur->childh = cur->childt = NULL; 

		cur = buf;
	}
}


void free_rkeyHdr2_keys( struct hdr_rkey2 *head ) 
{
	struct hdr_rkey2	*buf, *cur;

	cur = head;
	while( cur != NULL ) {
		buf = cur->next;
		return_RKEY1( cur->key_entry );
		cur = buf;
	}

}


void exit_key_data( )
{
	RKEY_1			*buf = NULL;


	while( freeRkey1 != NULL ) {
		buf = freeRkey1->next;
		ep_mem_free( freeRkey1 );
		freeRkey1 = buf;
	}
	free_Rkey_num   = 0;

	if( devCtx != NULL ) ep_crypto_cipher_free( devCtx );

}

