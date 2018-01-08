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
**  SESSION_MANAGER - functions to support secure channel   
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 
// LATER: need to devide this into two parts: for KDS server & for general case


#include <string.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <gdp/gdp.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <hs/hs_errno.h>
#include <hs/gcl_helper.h>
#include <hs/hs_symkey_gen.h>
#include <ac/ac_token.h>

#include "session_manager.h"



//#define KS_SKEY_FIL	"/home/hsmoon/GDP/certtest/ec/registration/rs_ecc.key"
#define	RS_CERT_FILE	"/home/hsmoon/GDP/certtest/ec/registration/rs_ecc.pem"
#define MY_AC_FILE		"/home/hsmoon/etc/my_ac.tk"


//static EP_DBG	Dbg	= EP_DBG_INIT("kdist.session",
//										"Session Info manager" );

EVP_PKEY			*rs_pubkey = NULL;
EVP_PKEY			*my_seckey = NULL;
struct ac_token		*my_token  = NULL;

// 
// Internal Functions 
// 
gdp_session* get_new_session(  )
{
	gdp_session			*newSession = NULL;

	newSession = (gdp_session *)ep_mem_zalloc( sizeof(gdp_session) );

	if( newSession == NULL ) return NULL;

	return newSession;
}


sapnd_dt* get_new_session_apnddata( char mode )
{
	sapnd_dt		*newData	= NULL;
	gdp_session		*newSession	= NULL;


	newData = (sapnd_dt *)ep_mem_malloc( sizeof(sapnd_dt) );
	if( newData == NULL ) {
		ep_dbg_printf("[ERROR] Fail to create the session info \n" );
		goto fail0;
	}

	if( mode != 'I' ) {
		newSession = get_new_session(  ); 
		if( newSession == NULL ) {
			ep_dbg_printf("[ERROR] Fail to create the session info \n" );
			goto fail0;
		}
	}

	newData->mode		= mode;
	newData->curSession = newSession;
	newData->a_data		= NULL;

	return newData;

fail0:
	if( newSession != NULL )	ep_mem_free( newSession );
	if( newData != NULL )		ep_mem_free( newData ); 

	return NULL;
}


// hsmoon_start
/*
** Free memory for session data structure 
*/
void free_session( void *dVal )
{
	gdp_session		*curSession = NULL;


	curSession = (gdp_session *)dVal;

	if( curSession == NULL ) return ;

	if( curSession->ac_info != NULL ) 
		gdp_gclmd_free( curSession->ac_info );

	if( curSession->sc_ctx != NULL ) 
		ep_crypto_cipher_free( curSession->sc_ctx );

	ep_mem_free( curSession );
}


/*
** Free memory for session & session related data 
*/
void free_session_apnddata( gdp_gcl_t *gcl )
{
	if( gcl == NULL )				return ;
	if( gcl->apndfpriv == NULL )	return ;

	{
		sapnd_dt	*aData = (sapnd_dt *)gcl->apndfpriv;

		if( aData->a_data != NULL ) ep_mem_free( aData->a_data ); 
		if( aData->curSession != NULL ) {
			free_session( (void *)aData->curSession );
		}

		ep_mem_free( aData ); 
	}
}
// hsmoon_end


int calculate_sessionkey( gdp_session *inSession )   
{
	int						ti, t_outlen;
	EP_STAT					estat;
	size_t					pkbuflen;
	const uint8_t			*pkbuf = NULL; 
	EP_CRYPTO_KEY			*oth_pubkey = NULL;


	//
	// 1. Extract public key of the other device.. 
	// 
	estat = gdp_gclmd_find( inSession->ac_info, GDP_GCLMD_PUBKEY, 
								&pkbuflen, (const void **)pkbuf   );
	if( !EP_STAT_ISOK(estat) ) return EX_INVALIDDATA; 

	if( EVP_PKEY_type(my_seckey->type) != EVP_PKEY_EC ) return EX_UNAVAILABLE;
	if( pkbuf[1] != EP_CRYPTO_KEYTYPE_EC              ) return EX_UNAVAILABLE;

	oth_pubkey = ep_crypto_key_read_mem( pkbuf+4, pkbuflen-4, 
					EP_CRYPTO_KEYFORM_DER, EP_CRYPTO_F_PUBLIC ); 

	if( oth_pubkey ) return EX_INVALIDDATA; 


	//
	// 2. Calculate the session key  
	// 
	// CUR: DEFAULT AES128 CBC MODE key 
	t_outlen = ep_compute_sharedkey_onEC( my_seckey, oth_pubkey, 16, 
					(char *)(inSession->se_key.sym_key) ); 

	if( t_outlen == 0 ) return EX_CANTCREAT; 

	if( t_outlen > 16 ) t_outlen = 16;
	inSession->se_key.sym_key[t_outlen] = '\0';
	inSession->se_key.sym_iv[t_outlen]  = '\0';
	if( inSession->rlen < t_outlen ) pkbuflen = inSession->rlen;
	else pkbuflen = t_outlen; 

	for( ti=0; ti<pkbuflen; ti++ ) {
		inSession->se_key.sym_key[ti] = inSession->se_key.sym_key[ti] ^ 
										inSession->random1_r[ti]; 

		inSession->se_key.sym_iv[ti] = inSession->random2_s[ti] ^ 
										inSession->random1_r[ti]; 
	}
	inSession->se_key.sym_algorithm = EP_CRYPTO_SYMKEY_AES128; 
	inSession->se_key.sym_mode      = EP_CRYPTO_MODE_CBC; 

/*
	//
	// 3. Prepare the context. (enc) 
	// 
	inSession->sc_ctx = ep_crypto_cipher_new( 
				inSession->se_key.sym_algorithm | inSession->se_key.sym_mode, 
				inSession->se_key.sym_key, inSession->se_key.sym_iv, true ); 
*/
	return EX_OK;
}


int calculate_hmac( gdp_pdu_t *pdu, gdp_session *curSession, bool isSync )
{
	int						dlen;
	int						chval;
	HMAC_CTX				hctx;
	unsigned int			mac_len;
	unsigned char			mac_result[EVP_MAX_MD_SIZE];
	const unsigned char		*dp; 


	HMAC_CTX_init( &hctx );
	HMAC_Init_ex( &hctx, curSession->se_key.sym_key, 16, 
					EVP_sha1(), NULL );

	if( isSync ) {
		HMAC_Update( &hctx, &(pdu->rsvd1), 1 );
		HMAC_Update( &hctx, pdu->dst, sizeof(gdp_name_t) );
	}
	HMAC_Update( &hctx, pdu->src, sizeof(gdp_name_t) ); // rcv gcl->name

/*
	dp = (const unsigned char *)&(pdu->rid);
	HMAC_Update( &hctx, dp, sizeof(gdp_rid_t) );

	dp = (const unsigned char *)&(pdu->seqno);
	HMAC_Update( &hctx, dp, sizeof(gdp_seqno_t) );
*/
	dp = (const unsigned char *)&(pdu->datum->recno);
	HMAC_Update( &hctx, dp, sizeof(gdp_recno_t) );

	dlen = gdp_datum_getdlen( pdu->datum );
	if( dlen > 0 ) 
		HMAC_Update( &hctx, 
			(const unsigned char *)gdp_buf_getptr(pdu->datum->dbuf, dlen), 
					dlen );

	HMAC_Final( &hctx, mac_result, &mac_len );

	chval = htonl( mac_len );
	gdp_buf_write( pdu->datum->dbuf, &chval, sizeof chval );
	gdp_buf_write( pdu->datum->dbuf, (const void *)mac_result, mac_len );

	HMAC_CTX_cleanup( &hctx );

	return EX_OK;
}


int verify_async_hmac( gdp_gcl_t *gcl, gdp_datum_t *datum, 
						gdp_session *curSession, 
						void *data, int dlen, void *inMac, int inMlen )
{
	HMAC_CTX				hctx;
	unsigned int			mac_len;
	unsigned char			mac_result[EVP_MAX_MD_SIZE];
	const unsigned char		*dp; 


	HMAC_CTX_init( &hctx );
	HMAC_Init_ex( &hctx, curSession->se_key.sym_key, 16, 
					EVP_sha1(), NULL );

	HMAC_Update( &hctx, gcl->name, sizeof(gdp_name_t) );


	dp = (const unsigned char *)&(datum->recno);
	HMAC_Update( &hctx, dp, sizeof(gdp_recno_t) );

	if( dlen > 0 ) {
		dp = (const unsigned char*)data;
		HMAC_Update( &hctx, dp, dlen );
	}
	HMAC_Final( &hctx, mac_result, &mac_len );

	HMAC_CTX_cleanup( &hctx );

	if( inMlen != mac_len ) {
		ep_dbg_printf("[UP_cr] Fail to verify hmac %d vs %u\n" , 
						inMlen, mac_len );
		return  EX_INVALIDDATA;
	}
	if( memcmp( mac_result, inMac, mac_len ) != 0 ) {
		ep_dbg_printf("[UP_cr] Fail to verify hmac \n" );  
		return  EX_INVALIDDATA;
	}

	return EX_OK;
}


int verify_hmac( gdp_pdu_t *pdu, gdp_session *curSession, 
					void *data, int dlen, void *inMac, int inMlen )
{
	HMAC_CTX				hctx;
	unsigned int			mac_len;
	unsigned char			mac_result[EVP_MAX_MD_SIZE];
	const unsigned char		*dp; 


	HMAC_CTX_init( &hctx );
	HMAC_Init_ex( &hctx, curSession->se_key.sym_key, 16, 
					EVP_sha1(), NULL );

	HMAC_Update( &hctx, &(pdu->rsvd1), 1 );
	HMAC_Update( &hctx, pdu->dst, sizeof(gdp_name_t) );
	HMAC_Update( &hctx, pdu->src, sizeof(gdp_name_t) );

/*
	dp = (const unsigned char *)&(pdu->rid);
	HMAC_Update( &hctx, dp, sizeof(gdp_rid_t) );

	dp = (const unsigned char *)&(pdu->seqno);
	HMAC_Update( &hctx, dp, sizeof(gdp_seqno_t) );
*/

	dp = (const unsigned char *)&(pdu->datum->recno);
	HMAC_Update( &hctx, dp, sizeof(gdp_recno_t) );

	if( dlen > 0 ) {
		dp = (const unsigned char*)data;
		HMAC_Update( &hctx, dp, dlen );
	}
	HMAC_Final( &hctx, mac_result, &mac_len );

	HMAC_CTX_cleanup( &hctx );

	if( inMlen != mac_len ) {
		ep_dbg_printf("[UP_cr] Fail to verify hmac %d vs %u\n" , 
						inMlen, mac_len );
		return  EX_INVALIDDATA;
	}
	if( memcmp( mac_result, inMac, mac_len ) != 0 ) {
		ep_dbg_printf("[UP_cr] Fail to verify hmac \n" );  
		return  EX_INVALIDDATA;
	}

	return EX_OK;
}


void update_open_session( gdp_session *inSession, struct ac_token *token, 
										gdp_req_t *req, KSD_info *ksData  )
{
	int					rval;
	char				len;
	char				cval;
	char				*twid;
	EP_STAT				estat;

	

	// Update token info & mode 
	inSession->ac_info = token->info;
	token->info			= NULL;

	if( find_gdp_gclmd_char( inSession->ac_info, 
								GDP_ACT_CCAP, &cval) == EX_OK )
	{
		if( cval == 1 ) inSession->mode = 'c';
		else inSession->mode = 'r';
		// w is determined in cmd_read or cmd_subscribe 
	}

	// check wdid info 
	estat =	gdp_gclmd_find( inSession->ac_info, GDP_ACT_DID, 
								(size_t *)&rval, (const void **)&twid );	
	if( EP_STAT_ISOK( estat  ) )  {
		if( strncmp( twid, ksData->wdid_pname, rval ) == 0 ) {
			if( inSession->mode == 'c' ) inSession->mode = 'b';
			else inSession->mode = 'w'; 
		}
	}


	// update random value 
	rval =	gdp_buf_read( req->cpdu->datum->dbuf, &len, 1);
	if( rval != 1 ) {
//		inSession->random1_r[0] = '\0'; 
		len = 0;
		goto step1; 
	}
	if( len > 16 ) len = 16; 

	rval =	gdp_buf_read( req->cpdu->datum->dbuf, inSession->random1_r, len );
	if( rval != len ) len = 0; 
	inSession->rlen = len;
//	inSession->random1_r[len] = '\0'; 

step1: 
	// generate random value 2 
	if( len != 0 )  {
		rval = RAND_pseudo_bytes( inSession->random2_s, len );
	}
//	inSession->random2_s[len] = '\0';


	// prepare the crypto context & session key. 
	// current fixed algorithm. LATER: dynamic choice through handshake 
	rval = calculate_sessionkey( inSession );
	if( rval != EX_OK ) {
		ep_dbg_printf("[ERROR] FAIL to create session key: %s \n", 
						str_errinfo( rval ) );

	}  else inSession->state = 1; 

}


// 
// External Functions 
// 
int insert_mytoken( gdp_buf_t *dbuf ) 
{
	return write_token_tobuf( my_token, dbuf );
}


// hsmoon_start
/*
** This init function loads and manages the following info 
**	1. the certificate of registration service 
**	2. the secret key of running service or device 
**	3. the access token of running service or device 
**		the access token is published through registration process 
*/
int init_session_manager(  )
{
	FILE			*t_fp		= NULL;
	X509			*rs_cert	= NULL;
	char			argname[100];
	const char		*name		= NULL;
	const char		*progname	= NULL;



	progname = ep_app_getprogname();
/*
	if( progname != NULL ) printf("Prog Name: %s \n", progname );
	else printf("Prog Name: NULL \n");
*/


	//
	// 1. Load the certificate of registration & key distribution service 
	// 
	name = ep_adm_getstrparam("swarm.gdp.regi.cert", RS_CERT_FILE );
	rs_cert = ep_x509_cert_read_file( name );
	if( rs_cert == NULL ) {
		ep_dbg_printf("Cannot read certificate in session manager \n"); 
		return EX_FAILURE;
	}
	// LATER: verify the cert. (root : trusted cert: cur) 

	// extract the public key in the cert file 
	rs_pubkey = X509_get_pubkey(rs_cert);
	if( rs_pubkey == NULL ) {
		ep_dbg_printf("Cannot read public key in session manager \n"); 
		X509_free( rs_cert );
		return EX_FAILURE;
	}
	X509_free( rs_cert );



	//
	// 2. Load the secret key of running system or device
	// 

	// 2.1 check whether this program is service or not 
	//		If service, use the another param info for the service 
	snprintf( argname, sizeof argname, "swarm.%s.secret", 
									progname==NULL?"null":progname );
	name = ep_adm_getstrparam( argname, NULL );

	if( name == NULL ) {
		// 2.2 In the device case, 
		//		For test, we need to run multiple devices on one machine 
		//		To do this, we use the following parameter. 	
		name = ep_adm_getstrparam("swarm.gdp.device.name", NULL );

		if( name != NULL ) {
			snprintf( argname, sizeof argname, "swarm.gdp.%s.secret", name );
			name = ep_adm_getstrparam( argname, NULL );
		}

		if( name == NULL ) {
			name = ep_adm_getstrparam("swarm.gdp.device.secret", NULL );
		}

	}

	t_fp = fopen( name, "rb");
	if( t_fp == NULL ) {
		ep_dbg_printf("Cannot read secret key in session manager \n"); 
		return EX_FAILURE;
	}

	if( PEM_read_PrivateKey(t_fp, &my_seckey, NULL, NULL) == NULL ) 
	{
		ep_dbg_printf("Cannot read secret key in session manager \n"); 
		fclose( t_fp );
		return EX_FAILURE;
	}
	fclose( t_fp );
// hsmoon_end


	//
	// 3. Load the secret key of running system or device
	// 

	name = ep_adm_getstrparam("swarm.gdp.device.mytoken", MY_AC_FILE );
	t_fp	= fopen( name, "rb" );
	if( t_fp == NULL ) {
		ep_dbg_printf("Cannot find/read ac token in session manager \n"); 
		return EX_FAILURE;
	} 

	my_token = read_token_from_file( t_fp );
	fclose( t_fp );

	if( my_token == NULL ) {
		ep_dbg_printf("Cannot find/read ac token in session manager \n"); 
		return EX_FAILURE;
	} 

	return EX_OK;
}



void exit_session_manager( )
{
	if( rs_pubkey != NULL ) EVP_PKEY_free( rs_pubkey );	
	if( my_seckey != NULL ) EVP_PKEY_free( my_seckey );	
	if( my_token  != NULL ) free_token(    my_token  ); 
}


gdp_session* lookup_session( KSD_info * ksData, gdp_req_t *inReq )
{
	char				*src;
	size_t				sn_len      = 0;
	hs_lnode			*curNode	= NULL;
	gdp_session			*curSession = NULL;
	gdp_pname_t			pname;



	if( ksData == NULL || inReq == NULL ) return NULL;

	//
	// 1. Find the existing session info. 
	// 
	src    = (char *)(inReq->cpdu->src) ; 
	sn_len = strlen( (const char *)src );
	curNode = lookup_inlist( ksData->sessions, sn_len, src );

	if( curNode == NULL ) return NULL;
	if( curNode->nval == NULL ) return NULL;

	curSession = (gdp_session *)(curNode->nval);

	ep_dbg_printf("[Session Info] %s -> %s \n", 
					gdp_printable_name( inReq->cpdu->src, pname ), 
					gdp_printable_name( inReq->cpdu->dst, pname )  );

	return curSession;

}



// LATER: Use freelist of gdp_session
void close_relatedSession( KSD_info *ksData, gdp_req_t *inReq )
{
	char				*src;
	size_t				sn_len      = 0;
	hs_lnode			*curNode	= NULL;
	gdp_session			*curSession = NULL;



	if( ksData == NULL || inReq == NULL ) return ;

	//
	// 1. Find the existing session info. 
	// 
	src		= (char *)(inReq->cpdu->src) ; 
	sn_len  = strlen( (const char *)src );
	curNode = lookup_inlist( ksData->sessions, sn_len, src );

	if( curNode			== NULL ) return ;
	if( curNode->nval	== NULL ) return ;

	curSession = (gdp_session *)(curNode->nval);

	delete_node_inlist( &(ksData->sessions), curNode ); 

	// free the memory for gdp_session 
	free_session( curSession );
}


// LATER: Use freelist of gdp_session
void close_allSession( KSD_info *ksData )
{
	if( ksData == NULL ) return ;

	free_list( ksData->sessions, free_session );
}


gdp_session* create_session_info( KSD_info * ksData, struct ac_token * token,
									gdp_req_t *inReq )
{
	hs_lnode			*curNode	= NULL;
	size_t				sn_len      = 0;
	char				*src;
	gdp_pname_t			pname;
	gdp_session			*newSession = NULL;


	if( ksData == NULL || token == NULL || inReq == NULL ) return NULL;

	//
	// 1. Find the existing session info. If no session, create it 
	// 
	src    = (char *)(inReq->cpdu->src) ; 
	sn_len = strlen( (const char *)src );
	curNode = insert_inlist( &(ksData->sessions), sn_len, src );
	if( curNode->nval == NULL ) {
		// No existing session info. Create it. 
		newSession = get_new_session(  ); 
		if( newSession == NULL ) {
			ep_dbg_printf("[ERROR] Fail to create the session info \n" );
			return NULL;
		}
		curNode->nval = newSession;

	} else {
		ep_dbg_printf("[Duplicated Session Info] %s -> %s \n", 
					gdp_printable_name( inReq->cpdu->src, pname ), 
					gdp_printable_name( inReq->cpdu->dst, pname )  );
		newSession = (gdp_session *)(curNode->nval);
	}

	
	//
	// 2. Update session info 
	// 
	update_open_session( newSession, token, inReq, ksData );

	return newSession;

}


// general case or client session request to kds
gdp_session* request_session( gdp_req_t *curReq, char mode )
{  
//	const char			*tk_name	= NULL;
	gdp_session			*newSession = NULL;
	sapnd_dt			*newData	= NULL;



	if( curReq == NULL ) return NULL;

	newData = get_new_session_apnddata( mode );
	if( newData == NULL ) {
		ep_dbg_printf("[ERROR] Fail to create the session info \n" );
		return NULL;
	}
	newSession = newData->curSession; 
	curReq->gcl->apndfpriv = newData; 

	//
	//  1. Send ac token info 
	// 
/*
	tk_name = ep_adm_getstrparam("swarm.gdp.device.token", 
									"/home/hsmoon/etc/my_ac.tk" ); 
	write_token_fromFile_tobuf( (char *)tk_name, curReq->cpdu->datum->dbuf );
*/
	write_token_tobuf( my_token, curReq->cpdu->datum->dbuf );

	//
	// 2. generate session info  
	// 
	{
		char	rlen = 16; 
		gdp_buf_write( curReq->cpdu->datum->dbuf, &rlen, 1);

		RAND_pseudo_bytes( newSession->random1_r, rlen ); 
		gdp_buf_write( curReq->cpdu->datum->dbuf, 
								newSession->random1_r, rlen ); 
	}

	newSession->flag = INIT_SE | SENT_AUTH; 
	return newSession;  
}


// LATER: for general case
// On failure (NULL return), caller has to set rpdu->cmd - NAK  
gdp_session* process_session_req( gdp_req_t *req, char mode ) 
{
	int					rval;
	char				len;

	sapnd_dt			*seData		= NULL;
	gdp_session			*curSession	= NULL;
	struct ac_token		*token		= NULL;
	gdp_pname_t			tname;


	//
	// 0. Prepara the necessary data structure 
	//
	if( req->gcl->apndfpriv != NULL ) seData = req->gcl->apndfpriv; 
	else seData = get_new_session_apnddata( mode );

	if( seData == NULL || seData->curSession == NULL ) { 
		ep_dbg_printf("[ERROR] fail to create session info \n");
		req->rpdu->cmd = GDP_NAK_S_INTERNAL;
		return NULL;
	}

	curSession = seData->curSession;
	req->gcl->apndfpriv = seData;


	if( seData->mode ==  'I' ) return curSession;

	switch( curSession->state ) { 
		case 0:
			// 
			// 1. Authorization : Check & Verify the access token 
			// 
			token = check_actoken( req->cpdu->datum->dbuf ); 
			if( token == NULL ) {
				ep_dbg_printf("[ERROR] Fail to check access token \n"
							  "\t from %s \n\t to %s \n", 
							  gdp_printable_name(req->cpdu->src, tname),  
							  gdp_printable_name(req->cpdu->dst, tname) ); 
				req->rpdu->cmd = GDP_NAK_C_UNAUTH;
				goto fail0;
			}
	
			// Update token info & mode 
			curSession->ac_info = token->info;
			token->info			= NULL;

			// Verify MAC with actoken->pubkey & datum->sig ...  LATER  

			// 
			// 2. Calculate the shared key   
			//    LATER: more information for authorization {[len][data]}*
			// Read random value 
			rval =	gdp_buf_read( req->cpdu->datum->dbuf, &len, 1);
			if( rval != 1 ) len = 0; 
			else if( len > 16 ) len = 16;

			if( len != 0 )  {
				rval =	gdp_buf_read( req->cpdu->datum->dbuf, 
										curSession->random1_r, len );
				if( rval != len ) len = 0; 
			} 

			if( len != 0 )  {
				// generate random value 2 
				rval = RAND_pseudo_bytes( curSession->random2_s, len );
			}
			curSession->rlen = len ;

			// prepare the crypto context & session key. 
			// current fixed algorithm. 
			// LATER: dynamic choice through handshake 
			rval = calculate_sessionkey( curSession );
			if( rval != EX_OK ) {
				ep_dbg_printf("[ERROR] FAIL to create session key: %s \n", 
									str_errinfo( rval ) );
				req->rpdu->cmd = GDP_NAK_S_INTERNAL;
				goto fail0; 

			} else curSession->state = 1; 


			free_token( token );
			break;

		case 1: // check repeated request or change param LATER 
		case 2: // change param  LATER
				break;
		default:
			ep_dbg_printf("[ERROR] Wrong session state %d \n", 
								curSession->state ) ;
	}

	return	curSession; 

fail0:
	if( token != NULL )			free_token( token );
	if(	curSession != NULL )	ep_mem_free( curSession );
	if( seData != NULL )		ep_mem_free( seData );

	req->gcl->apndfpriv = NULL;

	return NULL;

}


int check_reqdbuf_onsession( gdp_pdu_t *cpdu, gdp_session *curSession ) 
{
	int					exit_status = EX_OK;
	int					t_rval, tmpVal;
	int					encLen, decLen, rmac_len;
	uint8_t				*encBuf = NULL;
	uint8_t				*decBuf = NULL;
	unsigned int		cmac_len;
	unsigned char		mac_result[EVP_MAX_MD_SIZE];
	EP_STAT				estat;



	if( cpdu==NULL || curSession==NULL ) return EX_NOINPUT;

	tmpVal = gdp_datum_getdlen( cpdu->datum );
	if( curSession->state == 0 ) {
		// Non encrypted data... 
		ep_dbg_printf("[CHECK] rcv data (%d) at state 0 \n", tmpVal);
		return EX_OK; 
	}
	if( tmpVal == 0 ) return EX_OK; // no data 


	// 
	// 1. Get the data to check. 
	//    Encrypted data. [dlen] [encrypted data] [hmac len] [hmac]
	// 
	t_rval = gdp_buf_read( cpdu->datum->dbuf, &tmpVal, sizeof tmpVal ); 
	if( t_rval != sizeof tmpVal ) return EX_INVALIDDATA;
	
	encLen = ntohl( tmpVal );

	encBuf = ep_mem_zalloc( encLen + 1 );
	decLen = encLen + 16;
	if( decLen < EVP_MAX_MD_SIZE ) decLen = EVP_MAX_MD_SIZE;
	decBuf = ep_mem_zalloc( decLen );
		
	if( decBuf == NULL || encBuf == NULL ) {
		ep_dbg_printf("[UP_cr] Fail to allocate the buffer memory\n" );
		exit_status = EX_MEMERR;
		goto tfail0;
	}

	t_rval = gdp_buf_read( cpdu->datum->dbuf, encBuf, encLen ); 
	encBuf[encLen] = '\0'; 

	t_rval = gdp_buf_read( cpdu->datum->dbuf, &tmpVal, sizeof tmpVal ); 
	if( t_rval != sizeof tmpVal ) {
		exit_status = EX_INVALIDDATA;
		goto tfail0;
	}
	rmac_len = ntohl( tmpVal );

	t_rval = gdp_buf_read( cpdu->datum->dbuf, decBuf, rmac_len ); 
	decBuf[rmac_len] = '\0'; 


	//
	// 2. Verify HMAC  
	// 
	// At this point, data: encBuf (encLen) 
	HMAC( EVP_sha1(), curSession->se_key.sym_key, 16, encBuf, 
				encLen, mac_result, &cmac_len );
	if( cmac_len != rmac_len ) {
		ep_dbg_printf("[UP_cr] Fail to verify hmac %d vs %u\n" , 
						rmac_len, cmac_len );
		exit_status = EX_INVALIDDATA;
		goto tfail0;
	}
	if( memcmp( mac_result, decBuf, cmac_len ) != 0 ) {
		ep_dbg_printf("[UP_cr] Fail to verify hmac \n" );  
		exit_status = EX_INVALIDDATA;
		goto tfail0;
	}


	//
	// 3. Decrypt the message  
	// 
	if( curSession->sc_ctx != NULL ) {
		ep_crypto_cipher_reinit( curSession->sc_ctx, 
			curSession->se_key.sym_algorithm | curSession->se_key.sym_mode, 
			curSession->se_key.sym_key, curSession->se_key.sym_iv, false ); 

	} else {
		curSession->sc_ctx = ep_crypto_cipher_new( 
			curSession->se_key.sym_algorithm | curSession->se_key.sym_mode, 
			curSession->se_key.sym_key, curSession->se_key.sym_iv, false ); 
	}

	// decrypt the message 
	estat = ep_crypto_cipher_crypt( curSession->sc_ctx, 
						(void *)encBuf, encLen, (void *)decBuf, decLen );
	t_rval = EP_STAT_TO_INT( estat );

	// LATER : check error 
	if( t_rval < 0 || t_rval > encLen ) {
		ep_dbg_printf("[UP_sr] Fail to decrypt original data (%d | %d, %d)\n", 
						t_rval, encLen, decLen );
		exit_status = EX_SOFTWARE;
		goto tfail0;
	}
	decLen         = t_rval ;
	decBuf[decLen] = '\0';


	//
	// 4. Put the decrypted message in datum 
	// 
	gdp_buf_reset( cpdu->datum->dbuf );
	gdp_buf_write( cpdu->datum->dbuf, (const void *)decBuf, decLen );
	exit_status = EX_OK;

	if( curSession->state == 1 ) curSession->state = 2;

// CUR: for checking error, don't remove the ongoing dbuf msg. 
// LATER: need to reset dbuf in error 
tfail0:
	if( decBuf != NULL ) ep_mem_free( decBuf );
	if( encBuf != NULL ) ep_mem_free( encBuf );

	return exit_status;

}


// gev->gcl. gcl->apndfpriv, gev->datum 
int update_async_rmsg_onsession( gdp_gcl_t *gcl, gdp_session *curSession, 
								gdp_datum_t *datum, char mode ) 
{
	int					rval, tmpVal;
	EP_STAT				estat;
	int					exit_status = EX_OK; 

	int					totalLen    = 0;
	int					encDlen		= 0;
	int					decLen		= 0;
	int					rmaclen		= 0;
	int					dLen		= 0;
	unsigned char		*oriBuf		= NULL;
	unsigned char		*decBuf		= NULL;
	unsigned char		*encdp		= NULL;
	unsigned char		*rmacp		= NULL;



	if( gcl==NULL ) return EX_NOINPUT;
	if( mode == 'I' ) return EX_OK; 

	// work on session 
	if( curSession==NULL ) return EX_NOINPUT;

	totalLen = gdp_datum_getdlen( datum );

	//
	// Called on state: 2/2 
	//
	// error check 
	if( curSession->state != 2 ) {
		ep_dbg_printf("[ERROR-S] Wrong execution status "
						"on Session State: Me(%d) \n", 
							curSession->state ) ; 
		return EX_FAILURE;	
	}

	oriBuf = ep_mem_zalloc( totalLen );
	if( oriBuf == NULL ) {
		ep_dbg_printf("[ERROR] Fail to get memory to pre-process rmsg\n");
		return EX_MEMERR;
	}
	rval = gdp_buf_peek( datum->dbuf, oriBuf, totalLen );
	if( rval != totalLen) {
		exit_status = EX_SOFTWARE; 
		goto fail0;
	}

	//	
	// Verify HMAC 
	// Encrypted data. [dlen] [encrypted data] [hmac len] [hmac]
	// 
	encdp = oriBuf+4;

	rval = gdp_datum_getdlen( datum );
	gdp_buf_read( datum->dbuf, &tmpVal, sizeof tmpVal ); 
	encDlen = ntohl( tmpVal );

	gdp_buf_drain( datum->dbuf, encDlen );
	gdp_buf_read( datum->dbuf, &tmpVal, sizeof tmpVal ); 
	rmaclen = ntohl( tmpVal );

	rmacp = encdp + encDlen + 4;
	dLen  = totalLen - rmaclen - 4;

	rval = verify_async_hmac( gcl, datum, curSession, oriBuf, dLen, rmacp, rmaclen );
	if( rval != EX_OK ) {
		exit_status = EX_INVALIDDATA; 
		goto fail0;
	}


	gdp_buf_reset( datum->dbuf );
	
	//
	// Decrypt the message, if necessary.
	// 
	if( encDlen == 0 ) { // NO action 
		exit_status = EX_OK;
		goto fail0; 
	}
	
	decLen = encDlen + 16;
	decBuf = ep_mem_zalloc( decLen );
	if( decBuf == NULL ) {
		ep_dbg_printf("[UP_cr] Fail to allocate the buffer memory\n" );
		exit_status = EX_MEMERR;
		goto fail0;
	}

	if( curSession->sc_ctx != NULL ) {
		ep_crypto_cipher_reinit( curSession->sc_ctx, 
			curSession->se_key.sym_algorithm | curSession->se_key.sym_mode, 
			curSession->se_key.sym_key, curSession->se_key.sym_iv, false ); 

	} else {
		curSession->sc_ctx = ep_crypto_cipher_new( 
			curSession->se_key.sym_algorithm | curSession->se_key.sym_mode, 
			curSession->se_key.sym_key, curSession->se_key.sym_iv, false ); 
	}

	estat = ep_crypto_cipher_crypt( curSession->sc_ctx, 
						(void *)encdp, encDlen, (void *)decBuf, decLen );
	rval = EP_STAT_TO_INT( estat );

	if( rval < 0 || rval > encDlen ) {
		ep_dbg_printf("[UP_sr] Fail to decrypt original data (%d | %d, %d)\n", 
						rval, encDlen, decLen );
		exit_status = EX_SOFTWARE;
		goto fail0;
	}
	decLen         = rval ;
	decBuf[decLen] = '\0';


	//
	// Put the decrypted message in datum 
	// 
	gdp_buf_write( datum->dbuf, (const void *)decBuf, decLen );
	exit_status = EX_OK;


fail0:
	if( oriBuf != NULL ) ep_mem_free( oriBuf );
	if( decBuf != NULL ) ep_mem_free( decBuf );
	return exit_status;

}



// data is stored in rpdu->datum... 
int update_rmsg_onsession( gdp_pdu_t *pdu, gdp_session *curSession, 
								char mode ) 
{
	int					rval, tmpVal;
	EP_STAT				estat;
	int					exit_status = EX_OK; 

	int					totalLen    = 0;
	int					encDlen		= 0;
	int					decLen		= 0;
	int					rmaclen		= 0;
	int					dLen		= 0;
	unsigned char		*oriBuf		= NULL;
	unsigned char		*decBuf		= NULL;
	unsigned char		*encdp		= NULL;
	unsigned char		*rmacp		= NULL;



	if( pdu==NULL ) return EX_NOINPUT;
	if( mode == 'I' ) return EX_OK; 

	// work on session 
	if( curSession==NULL ) return EX_NOINPUT;

	totalLen = gdp_datum_getdlen( pdu->datum );

	//
	// TOTAL: curSession->state: 0/1/2 : pdu->rsvd1: 0/1/2 
	//

	// CHECK ERROR CASE 
	if( curSession->state == 0 && pdu->rsvd1 == 2 ) {
		if( curSession->ac_info == NULL ) {
			ep_dbg_printf("[ERROR-S] Mismatch session process "
							"MyState(Init: NULL other), Oth(%d) \n", 
							pdu->rsvd1 ) ; 	
			return EX_FAILURE;
		}
		ep_dbg_printf("[WARN] Session State: Me(0) Oth(2) \n" );
		return EX_FAILURE;  // LATER
	}

	if( curSession->state == 1 ) {
		ep_dbg_printf("[ERROR-S] Wrong execution path or Mismatch "
						"on Session State: Me(1) Oth(%u) \n", 
							pdu->rsvd1); 
		return EX_FAILURE;	
	}

	if( curSession->state == 2 && pdu->rsvd1 != 2 ) {
		ep_dbg_printf("[ERROR-I] Later check for session re-initialization "
						"on Session State: Me(2) Oth(%u) \n", 
							pdu->rsvd1); 
		return EX_FAILURE;
	}

	// Session Process on each state. 
	// Remaining state (0:0) (0:1) (2:2) 

	if( curSession->state == 0 && pdu->rsvd1 == 0 ) {
		// NO action 
		return EX_OK; 
	} 

	oriBuf = ep_mem_zalloc( totalLen );
	if( oriBuf == NULL ) {
		ep_dbg_printf("[ERROR] Fail to get memory to pre-process rmsg\n");
		return EX_MEMERR;
	}
	rval = gdp_buf_peek( pdu->datum->dbuf, oriBuf, totalLen );
	if( rval != totalLen) {
		exit_status = EX_SOFTWARE; 
		goto fail0;
	}

	// if necessary, process the other's auth info 
	if( curSession->state == 0 && pdu->rsvd1 == 1 ) {
		char					tlen;
		gdp_pname_t				tname;
		struct ac_token			*t_token = NULL;


		t_token = check_actoken( pdu->datum->dbuf ); 
		if( t_token == NULL ) {
			ep_dbg_printf("[ERROR] Fail to check access token \n"
						  "\t from %s \n\t to %s \n", 
						  gdp_printable_name(pdu->src, tname),  
						  gdp_printable_name(pdu->dst, tname) ); 
			exit_status = EX_FAILURE;
			goto fail0;
		}

		curSession->ac_info = t_token->info;
		t_token->info		= NULL;
		free_token( t_token );

		rval = gdp_buf_read( pdu->datum->dbuf, &tlen, 1 ); 
		if( rval != 1 ) exit_status = EX_SOFTWARE; 
		if( tlen > 16 ) exit_status = EX_FAILURE; 
		if( exit_status != EX_OK ) goto fail0; 

		rval = gdp_buf_read( pdu->datum->dbuf, 
								curSession->random2_s, tlen );
		if( rval != tlen ) {
			exit_status = EX_FAILURE;
			goto fail0;
		}
		curSession->rlen = tlen;

		rval = calculate_sessionkey( curSession );
		if( rval != EX_OK ) {
			ep_dbg_printf("[ERROR] Fail to create session key: %s \n", 
							str_errinfo( rval ) );
			exit_status = EX_SOFTWARE;
			goto fail0;
		}

	}

	//	
	// Verify HMAC 
	// Encrypted data. [dlen] [encrypted data] [hmac len] [hmac]
	// 
	rval = gdp_datum_getdlen( pdu->datum );
	gdp_buf_read( pdu->datum->dbuf, &tmpVal, sizeof tmpVal ); 
	encDlen = ntohl( tmpVal );

	gdp_buf_drain( pdu->datum->dbuf, encDlen );
	gdp_buf_read( pdu->datum->dbuf, &tmpVal, sizeof tmpVal ); 
	rmaclen = ntohl( tmpVal );

	if( curSession->state == 0 && pdu->rsvd1 == 1 ) {
		tmpVal	= totalLen - rval ; 
		encdp	= oriBuf + tmpVal;
		
	} else {
		encdp = oriBuf+4;
	}
	rmacp = encdp + encDlen + 4;
	dLen  = totalLen - rmaclen - 4;

	rval = verify_hmac( pdu, curSession, oriBuf, dLen, rmacp, rmaclen );
	if( rval != EX_OK ) {
		exit_status = EX_INVALIDDATA; 
		goto fail0;
	}
	
	gdp_buf_reset( pdu->datum->dbuf );
	
	//
	// Decrypt the message, if necessary.
	// 
	if( encDlen == 0 ) { // NO action 
		exit_status = EX_OK;
		goto fail0; 
	}
	
	decLen = encDlen + 16;
	decBuf = ep_mem_zalloc( decLen );
	if( decBuf == NULL ) {
		ep_dbg_printf("[UP_cr] Fail to allocate the buffer memory\n" );
		exit_status = EX_MEMERR;
		goto fail0;
	}

	if( curSession->sc_ctx != NULL ) {
		ep_crypto_cipher_reinit( curSession->sc_ctx, 
			curSession->se_key.sym_algorithm | curSession->se_key.sym_mode, 
			curSession->se_key.sym_key, curSession->se_key.sym_iv, false ); 

	} else {
		curSession->sc_ctx = ep_crypto_cipher_new( 
			curSession->se_key.sym_algorithm | curSession->se_key.sym_mode, 
			curSession->se_key.sym_key, curSession->se_key.sym_iv, false ); 
	}

	estat = ep_crypto_cipher_crypt( curSession->sc_ctx, 
						(void *)encdp, encDlen, (void *)decBuf, decLen );
	rval = EP_STAT_TO_INT( estat );

	if( rval < 0 || rval > encDlen ) {
		ep_dbg_printf("[UP_sr] Fail to decrypt original data (%d | %d, %d)\n", 
						rval, encDlen, decLen );
		exit_status = EX_SOFTWARE;
		goto fail0;
	}
	decLen         = rval ;
	decBuf[decLen] = '\0';


	//
	// Put the decrypted message in datum 
	// 
	gdp_buf_write( pdu->datum->dbuf, (const void *)decBuf, decLen );
	exit_status = EX_OK;

	curSession->state = 2;

fail0:
	if( oriBuf != NULL ) ep_mem_free( oriBuf );
	if( decBuf != NULL ) ep_mem_free( decBuf );
	return exit_status;

}


// data is stored in rpdu->datum... 
int update_smsg_onsession( gdp_pdu_t *pdu, gdp_session *curSession, 
								char mode, bool isSync ) 
{
	EP_STAT			estat;
	int				t_rval, chval;
	int				exit_status = EX_OK; 

	int				oriLen, encLen;
	uint8_t			*oriBuf = NULL;
	uint8_t			*encBuf = NULL;


	if( my_seckey == NULL ) return EX_FAILURE;

	if( pdu==NULL ) return EX_NOINPUT;
	if( mode == 'I' ) return EX_OK; 

	// work on session 
	if( curSession==NULL ) return EX_NOINPUT;

	pdu->rsvd1 = curSession->state;

	if( curSession->state == 0 ) {
		// CASE: first request / first response on session failure. 
		// Not encrypted data... 
		// MAC: signed by device private key if necessary. 

		if( curSession->flag & SENT_AUTH ) {
			EP_CRYPTO_MD			*md = NULL;
			uint8_t					sbuf[EP_CRYPTO_MAX_SIG]; 
			size_t					siglen;

			oriLen = gdp_datum_getdlen( pdu->datum );

			// current use default value LATER 
			md = ep_crypto_sign_new( my_seckey, EP_CRYPTO_MD_SHA256 );
			estat = ep_crypto_sign_update( md, 
						gdp_buf_getptr( pdu->datum->dbuf, oriLen ), oriLen );
			estat = ep_crypto_sign_final( md, sbuf, &siglen ); 


			if( EP_STAT_ISOK( estat ) ) {
				if( pdu->datum->sig == NULL ) {
					pdu->datum->sig = gdp_buf_new();
				}
				gdp_buf_write( pdu->datum->sig, sbuf, siglen );
				pdu->datum->siglen = siglen;
				pdu->datum->sigmdalg = EP_CRYPTO_MD_SHA256;
				return EX_OK; 

			} else return EX_FAILURE; 
		}

		return EX_OK; // no data . no processing 
	}

	// Session Status 1 or 2 

	oriLen = gdp_datum_getdlen( pdu->datum );
	encLen = oriLen + 16;

	if( oriLen > 0 ) {
		oriBuf = ep_mem_zalloc( oriLen + 1 );
		encBuf = ep_mem_zalloc( encLen + 1 );

		if( oriBuf == NULL || encBuf == NULL ) {
			ep_dbg_printf("[UP_sr] Fail to allocate the buffer memory\n" );
			exit_status = EX_MEMERR;
			goto tfail0;
		}

		// fill the oriBuf with the original data in dbuf. 
		t_rval = gdp_buf_read( pdu->datum->dbuf, oriBuf, oriLen ); 
		if( t_rval != oriLen ) {
			ep_dbg_printf("[UP_sr] Fail to read original data (%d | %d)\n", 
				t_rval, oriLen );

			exit_status = EX_SOFTWARE;
			goto tfail0;
		}
		oriBuf[oriLen] = '\0';

		// 
		// Encrypt the data & Store the result in encBuf 
		// 
		if( curSession->sc_ctx != NULL ) {
			ep_crypto_cipher_reinit( curSession->sc_ctx, 
				curSession->se_key.sym_algorithm | curSession->se_key.sym_mode, 
				curSession->se_key.sym_key, curSession->se_key.sym_iv, true ); 

		} else {
			curSession->sc_ctx = ep_crypto_cipher_new( 
				curSession->se_key.sym_algorithm | curSession->se_key.sym_mode, 
				curSession->se_key.sym_key, curSession->se_key.sym_iv, true ); 
		}

		// encrypt the log contents in oriBuf 
		estat = ep_crypto_cipher_crypt( curSession->sc_ctx, 
						(void *)oriBuf, oriLen, (void *)encBuf, encLen );
		t_rval = EP_STAT_TO_INT( estat );

		// LATER : check error 
		if( t_rval < 0 || t_rval > encLen ) {
			ep_dbg_printf("[UP_sr] Fail to encrypt original data (%d | %d)\n", 
						t_rval, encLen );
			exit_status = EX_SOFTWARE;
			goto tfail0;

		}
		encBuf[t_rval] = '\0';
		encLen = t_rval;

	}

	//
	// 1. If necessary, add response msg 
	// 
	if( curSession->state == 1 ) { // flag !!= SENT_AUTH + 
		// ADD auth info : token + random value 
		write_token_tobuf( my_token, pdu->datum->dbuf );
		gdp_buf_write( pdu->datum->dbuf, &(curSession->rlen), 1);
		gdp_buf_write( pdu->datum->dbuf, curSession->random2_s, 
													curSession->rlen );
		curSession->flag |= SENT_AUTH;	
	}


	//
	// 2. Put the encrypted message if we have 
	// 
	if( oriLen > 0 ) {
		//
		// Put the encrypted log data in datum 
		// 
		chval = htonl( encLen );
		gdp_buf_write( pdu->datum->dbuf, &chval, sizeof chval );
		gdp_buf_write( pdu->datum->dbuf, (const void *)encBuf, encLen );
		exit_status = EX_OK;

	} else gdp_buf_write( pdu->datum->dbuf, &oriLen, sizeof oriLen ); 


	//
	// 3. calculate Hmac   
	// 
	calculate_hmac( pdu, curSession, isSync );

	curSession->state = 2;
	exit_status = EX_OK;


tfail0:
	if( oriBuf != NULL ) ep_mem_free( oriBuf );
	if( encBuf != NULL ) ep_mem_free( encBuf );

	return exit_status;
}

/*
int update_response_onsession( gdp_pdu_t *rpdu, gdp_session *curSession ) 
{
	int				t_rval, chval;
	int				oriLen, encLen;
	int				exit_status = EX_OK; 
	EP_STAT			estat;
	uint8_t			*oriBuf = NULL;
	uint8_t			*encBuf = NULL;



	if( rpdu==NULL || curSession==NULL ) return EX_NOINPUT;


	//
	// 1. If necessary, add response msg 
	// 
	if( curSession->state == 1 ) {
		// ADD random value 
		char		len = 16;

		gdp_buf_write( rpdu->datum->dbuf, &len, 1);
		gdp_buf_write( rpdu->datum->dbuf, curSession->random2_s, len);
	}


	//
	// 2. If necessary, encrypt the message  
	// 
	oriLen = gdp_datum_getdlen( rpdu->datum );
	if( curSession->state == 2 && oriLen > 0 ) {
	
		if( curSession->sc_ctx != NULL ) {
			ep_crypto_cipher_reinit( curSession->sc_ctx, 
				curSession->se_key.sym_algorithm | curSession->se_key.sym_mode, 
				curSession->se_key.sym_key, curSession->se_key.sym_iv, true ); 

		} else {
			curSession->sc_ctx = ep_crypto_cipher_new( 
				curSession->se_key.sym_algorithm | curSession->se_key.sym_mode, 
				curSession->se_key.sym_key, curSession->se_key.sym_iv, true ); 
		}

		encLen = oriLen + 16;

		oriBuf = ep_mem_zalloc( oriLen + 1 );
		encBuf = ep_mem_zalloc( encLen + 1 );
		

		if( oriBuf == NULL || encBuf == NULL ) {
			ep_dbg_printf("[UP_sr] Fail to allocate the buffer memory\n" );
			exit_status = EX_MEMERR;
			goto tfail0;
		}

		// fill the oriBuf with the original data in dbuf. 
		t_rval = gdp_buf_read( rpdu->datum->dbuf, oriBuf, oriLen ); 
		if( t_rval != oriLen ) {
			ep_dbg_printf("[UP_sr] Fail to read original data (%d | %d)\n", 
				t_rval, oriLen );

			exit_status = EX_SOFTWARE;
			goto tfail0;
		}
		oriBuf[oriLen] = '\0';

		//
		// encrypt the log contents in oriBuf 
		estat = ep_crypto_cipher_crypt( curSession->sc_ctx, 
						(void *)oriBuf, oriLen, (void *)encBuf, encLen );
		t_rval = EP_STAT_TO_INT( estat );

		// LATER : check error 
		if( t_rval < 0 || t_rval > encLen ) {
			ep_dbg_printf("[UP_sr] Fail to encrypt original data (%d | %d)\n", 
						t_rval, encLen );
			exit_status = EX_SOFTWARE;
			goto tfail0;

		}
		encBuf[t_rval] = '\0';


		//
		// Put the encrypted log data in datum 
		// 
		chval = htonl( t_rval );
		gdp_buf_write( rpdu->datum->dbuf, &chval, sizeof chval );
		gdp_buf_write( rpdu->datum->dbuf, (const void *)encBuf, t_rval );
		exit_status = EX_OK;

	}


	//
	// 3. If necessary, calculate Hmac   
	// 
	if( oriLen > 0 ) {
		unsigned int		mac_len;
		unsigned char		mac_result[EVP_MAX_MD_SIZE];

		if( encBuf == NULL ) {
			// Pass the encryption. So put the msg in encBuf. 
			encBuf = ep_mem_zalloc( oriLen + 1 );
			t_rval = gdp_buf_peek( rpdu->datum->dbuf, encBuf, oriLen + 1 ); 
			if( encBuf == NULL || t_rval != oriLen ) {
				ep_dbg_printf("[UP_sr] Fail to allocate memory 3 \n" );
				exit_status = EX_MEMERR;
				goto tfail0;
			}
			encLen = t_rval; 
		}
	
		// At this point, data: encBuf (encLen) 
		HMAC( EVP_sha1(), curSession->se_key.sym_key, 16, encBuf, 
				encLen, mac_result, &mac_len );

		chval = htonl( mac_len );
		gdp_buf_write( rpdu->datum->dbuf, &chval, sizeof chval );
		gdp_buf_write( rpdu->datum->dbuf, (const void *)mac_result, mac_len );
	}


// CUR: for checking error, don't remove the ongoing dbuf msg. 
// LATER: need to reset dbuf in error 
tfail0:
	if( oriBuf != NULL ) ep_mem_free( oriBuf );
	if( encBuf != NULL ) ep_mem_free( encBuf );

	return exit_status;
}
*/

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


bool verify_actoken( struct ac_token *inToken )
{
	EP_STAT				estat ;
	EP_CRYPTO_MD		*md = NULL;


	md = ep_crypto_vrfy_new( rs_pubkey, inToken->md_alg_id );
	if( md == NULL ) return false;

	estat = ep_crypto_vrfy_update( md, inToken->dbuf, inToken->dlen ); 
	if( EP_STAT_ISOK( estat ) == false ) goto fail0;

	estat = ep_crypto_vrfy_final( md, inToken->sigbuf, inToken->siglen ); 
	if( EP_STAT_ISOK( estat ) )  {
		ep_crypto_vrfy_free( md ) ; 
		return true;
	}  


fail0:
	if( md != NULL ) ep_crypto_vrfy_free( md );
	return false;

}

