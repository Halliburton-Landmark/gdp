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
** AC_TOKEN: token published during registration 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.15 
*/ 


#include <stdio.h> 
#include <string.h> 
#include <arpa/inet.h> 
#include <gdp/gdp.h>
#include <gdp/gdp_gclmd.h>
#include <ep/ep_mem.h>
#include <ep/ep_dbg.h>
#include <ep/ep_stat.h>
#include <ep/ep_time.h>
#include <hs/hs_util.h>
#include <hs/hs_errno.h>
#include "ac_token.h"


struct ac_token* make_new_token( ) 
{
	struct ac_token		*newToken = NULL;


	newToken = ep_mem_zalloc( sizeof(struct ac_token) );
	if( newToken == NULL ) {
		ep_dbg_printf( "Fail to get memory for token\n" );
		return newToken; 
	}


	newToken->info		= gdp_gclmd_new(0); 
	newToken->md_alg_id	= EP_CRYPTO_MD_SHA256; 

	// The other fields are NULL now. 

	return newToken; 
}


// hsmoon_start
/*
** Free memory allocated for token 
*/
void free_token( struct ac_token	**intoken )
{
	struct ac_token		*token = (*intoken);

	if( token == NULL ) return ;

	if( token->info   != NULL ) gdp_gclmd_free( token->info ); 
	if( token->dbuf   != NULL ) gdp_buf_free( token->dbuf );
	if( token->sigbuf != NULL ) ep_mem_free( token->sigbuf );

	token->info = NULL;
	token->dbuf = NULL;
	token->sigbuf = NULL;

	ep_mem_free( token );
	*intoken = NULL;
}
// hsmoon_end


int add_token_field( struct ac_token *token, gdp_gclmd_id_t id, 
						size_t len, const void *data )
{
	EP_STAT			estat;


	if( token		== NULL ) return EX_NOINPUT;
	if( token->info == NULL ) return EX_NOINPUT; 

	estat = gdp_gclmd_add( token->info, id, len, data );

	if( EP_STAT_ISOK(estat) ) return EX_OK;
	else  return EX_FAILURE;
}


int add_token_ctime_field( struct ac_token *token ) 
{
	EP_STAT			estat;
	EP_TIME_SPEC	tv; 
	char			timestring[40];


	if( token		== NULL ) return EX_NOINPUT;
	if( token->info == NULL ) return EX_NOINPUT; 


	ep_time_now(&tv); 
	ep_time_format( &tv, timestring, sizeof timestring, EP_TIME_FMT_DEFAULT);

	estat = gdp_gclmd_add( token->info, GDP_ACT_CTIME, 
							strlen(timestring), timestring );

	if( EP_STAT_ISOK(estat) ) return EX_OK;
	else  return EX_FAILURE;
}


// keylengh: ref RSA_size() 
// keytype : pkey->type (current only support EC 
int add_token_pubkey_field( struct ac_token *token, EP_CRYPTO_KEY *pkey )
{
	EP_STAT			estat;
	uint8_t			dbuf[EP_CRYPTO_MAX_DER + 4];
	uint8_t			*wPos = dbuf + 4;


	if( token == NULL || pkey == NULL ) return EX_NOINPUT;
	if( token->info == NULL ) return EX_NOINPUT; 

	dbuf[0] = token->md_alg_id;
	dbuf[1] = pkey->type;  // EC
	dbuf[2] = 0;			// EC key len : ignorable
	dbuf[3] = 0;

	estat = ep_crypto_key_write_mem( pkey, wPos, EP_CRYPTO_MAX_DER, 
						EP_CRYPTO_KEYFORM_DER, EP_CRYPTO_SYMKEY_AES192, 
						NULL, EP_CRYPTO_F_PUBLIC );

	if( !EP_STAT_ISOK( estat ) ) {
		ep_dbg_printf("Could not insert pubkey info\n");
		return EX_SOFTWARE;
	}

	gdp_gclmd_add( token->info, GDP_ACT_PUBKEY, 
					EP_STAT_TO_INT(estat) + 4,  dbuf );

	return EX_OK;
}


int calculate_signature( struct ac_token *token, EP_CRYPTO_KEY *skey ) 
{
	size_t				dlen = 0;
	EP_CRYPTO_MD		*md = NULL;
	uint8_t				sbuf[EP_CRYPTO_MAX_SIG];


	if( token == NULL || skey == NULL ) return EX_NOINPUT;
	if( token->info == NULL           ) return EX_NOINPUT; 

	if( token->dbuf == NULL ) 
		token->dbuf = gdp_buf_new();
	else gdp_buf_reset( token->dbuf );


	dlen = _gdp_gclmd_serialize( token->info, token->dbuf );
	token->dlen = dlen;

	md = ep_crypto_sign_new( skey, token->md_alg_id );
	ep_crypto_sign_update( md, gdp_buf_getptr( token->dbuf, dlen ), dlen );
	ep_crypto_sign_final( md, sbuf, &dlen );
	token->siglen = dlen;

	token->sigbuf = ep_mem_malloc( (dlen+1) * sizeof(uint8_t) );
	if( token->sigbuf == NULL ) {
		ep_dbg_printf("[ERROR] Fail to get memory for sigbuf \n" ); 
		ep_crypto_sign_free( md );
		return EX_MEMERR; 
	}
	memcpy( token->sigbuf, sbuf, dlen );
	token->sigbuf[dlen] = '\0'; 

	ep_crypto_sign_free( md );

	return EX_OK;
}


// error check.. 
// [dlen][slen][dbuf][sigbuf]
int write_actoken_to_file( struct ac_token *token, FILE *wfp )
{
	size_t		rval; 
	uint32_t	len; 
	uint8_t		*dptr;

	// write dlen 
	len = htonl( token->dlen );
	rval = fwrite( &len, sizeof len, 1, wfp );
	if( rval != 1 ) goto fail0;

	// write slen 
	len = htonl( token->siglen );
	rval = fwrite( &len, sizeof len, 1, wfp );
	if( rval != 1 ) goto fail0;

	// write token info  
	dptr = gdp_buf_getptr( token->dbuf, token->dlen );
	rval = fwrite( dptr, sizeof(uint8_t), token->dlen, wfp ); 
	if( rval != token->dlen ) goto fail0;

	// write sig info  
	rval = fwrite( token->sigbuf, sizeof(uint8_t), token->siglen, wfp );
	if( rval != token->siglen ) goto fail0;

	return EX_OK;

fail0:
	ep_dbg_printf( "Fail to write : rval %zu \n", rval);
	return EX_FAILURE;
}


// hsmoon_start
/*
** Read the token info from the file (indicated by 1'st argu (rb mode option))
** Convert the read token info into the ac_token data structure 
** Return the converted ac_token data 
*/ 
struct ac_token* read_token_from_file( FILE *rfp )
{
	size_t				rval;
	uint32_t			rlen;
	uint8_t				dbuf[1024];  // 2048? & need error check 
	struct ac_token		*newToken = NULL;
	

	newToken = ep_mem_zalloc( sizeof(struct ac_token) );
	if( newToken == NULL ) {
		ep_dbg_printf( "Fail to get memory for token\n" );
		return newToken; 
	}
	newToken->md_alg_id	= EP_CRYPTO_MD_SHA256; 
	// The other fields are NULL now. 


	rval = fread( &rlen, sizeof rlen, 1, rfp ); 
	if( rval != 1 ) goto fail0;
	newToken->dlen = ntohl( rlen );

	rval = fread( &rlen, sizeof rlen, 1, rfp ); 
	if( rval != 1 ) goto fail0;
	newToken->siglen = ntohl( rlen );
	newToken->sigbuf = ep_mem_malloc( (newToken->siglen+1)*sizeof(uint8_t) );
	if( newToken->sigbuf == NULL ) {
		ep_dbg_printf( "Fail to get memory for token sig\n" );
		goto fail0;	
	}
	printf("dlen: %d, slen: %d \n", newToken->dlen, newToken->siglen );

	rval = fread( dbuf, sizeof(uint8_t), newToken->dlen, rfp ); 
	if( rval != newToken->dlen ) goto fail0;
	newToken->dbuf = gdp_buf_new();
	gdp_buf_write( newToken->dbuf, dbuf, newToken->dlen );


	// current: fixed alg (no siglen error)
	// LATER: With optional md alg, need error check 
	rval = fread( dbuf, sizeof(uint8_t), newToken->siglen, rfp ); 
	if( rval != newToken->siglen ) goto fail0;

/* 
	// debugging routine 
	{
		gdp_buf_peek( newToken->dbuf, dbuf, newToken->dlen );
		printf("Token data: \n");
		ep_print_hexstr( stdout, "", newToken->dlen, dbuf );
	}
*/ 

	//newToken->info = _gdp_gclmd_deserialize( newToken->dbuf );

	return newToken;


fail0:
	ep_dbg_printf( "Fail to write : rval %zu \n", rval);

	if( newToken != NULL ) free_token( &newToken );
	return NULL;
}


/*
** Read token contents in gdp_buf (indicated by first argu). 
** Convert the read contents into the ac_token type. 
**  Return converted ac_token. 
*/
struct ac_token* read_token_from_buf( gdp_buf_t *inBuf )
{
	int					tval; 
	size_t				rval1, rval2;
	uint32_t			dlen, slen;
	struct ac_token		*newToken = NULL;


	printf("In buf length: %zu \n",  gdp_buf_getlength(inBuf) ); 
	newToken = make_new_token( );
	if( newToken == NULL ) {
		ep_dbg_printf( "Fail to get memory for token\n" );
		return newToken; 
	}

	// Read the access token in inBuf 
	//  access  token in buf:  [dlen][siglen][[token_data][signature]] 
	rval1 = gdp_buf_read( inBuf, &dlen, sizeof dlen); 
	rval2 = gdp_buf_read( inBuf, &slen, sizeof slen); 
	if( rval1!=rval2 || rval1 != sizeof dlen )  {
		ep_dbg_printf("[ERROR] in reading token len %zd %zd \n",
							rval1, rval2 );
		goto fail0; 
	}

	newToken->dlen   = ntohl( dlen );
	newToken->siglen = ntohl( slen );
	newToken->sigbuf = ep_mem_malloc( (newToken->siglen+1)*sizeof(uint8_t) );
	if( newToken->sigbuf == NULL ) {
		ep_dbg_printf( "Fail to get memory for token sig\n" );
		goto fail0;	
	}
	printf("dlen: %d, slen: %d \n", newToken->dlen, newToken->siglen );
// hsmoon_end 

	newToken->dbuf = gdp_buf_new();
	tval = gdp_buf_move( newToken->dbuf, inBuf, newToken->dlen ); 
	if( tval == -1 ) goto fail0; 


	{
		// debugging routine
		unsigned char *tbuf = NULL;

		tbuf = gdp_buf_getptr( newToken->dbuf, newToken->dlen );
		printf("read tokenfrom buf: newToken dbuf data: \n");
		ep_print_hexstr( stdout, "", newToken->dlen, tbuf );
	}

	printf(".. Pass to read dbuf \n");
	printf("In buf R length: %zu \n",  gdp_buf_getlength(inBuf) ); 

	tval = gdp_buf_read( inBuf, newToken->sigbuf, newToken->siglen );
	printf(".. reading sig: %d vs %d \n", tval, newToken->siglen );
	if( tval != newToken->siglen ) goto fail0;
	newToken->sigbuf[newToken->siglen] = '\0'; 
	printf("In buf R length: %zu \n",  gdp_buf_getlength(inBuf) ); 

	//newToken->info = _gdp_gclmd_deserialize( newToken->dbuf );

	return newToken;


fail0:
	ep_dbg_printf("[ERROR] in Reading access token in dbuf \n" );
	free_token( &newToken );
	return NULL;

}


// hsmoon_start
/*
**  Put the token (indicated by first argu) 
**		in the gdp buffer indicated by second argu. 
*/ 
// Write token into buffer  
int write_token_tobuf( struct ac_token *curToken, gdp_buf_t *outBuf )
{
	unsigned char	*tbuf = NULL;

	if( curToken == NULL || outBuf == NULL ) return EX_NOINPUT; 
	if( curToken->dbuf == NULL ||  curToken->sigbuf == NULL ) return EX_NOINPUT;


	// write dlen :put* function uses htonl to send dlen  
	gdp_buf_put_uint32( outBuf, curToken->dlen ); 

	// write slen  
	gdp_buf_put_uint32( outBuf, curToken->siglen ); 

	printf("TOKEN: dlen: %u, siglen: %u\n", 
				curToken->dlen, curToken->siglen );

	// write token   
	tbuf = gdp_buf_getptr( curToken->dbuf, curToken->dlen );
	gdp_buf_write( outBuf, tbuf, curToken->dlen );

/*
	{
		// debugging routine

		tbuf = gdp_buf_getptr( curToken->dbuf, curToken->dlen );
		printf("write token: dbuf data: \n");
		ep_print_hexstr( stdout, "", curToken->dlen, tbuf );
	}
*/

	// write sig
	gdp_buf_write( outBuf, curToken->sigbuf, curToken->siglen );

	return EX_OK;
}
// hsmoon_end 



// Read token and Write it into dbuf & sigbuf of token  
int write_token_fromFile_tobuf( char *inName, gdp_buf_t *outBuf )
{
	size_t				rval;
	uint32_t			rlen, dlen, slen;
	uint8_t				dbuf[1024];  // 2048? & need error check 
	FILE				*rfp = NULL; 


	dlen = slen = 0;
	if( inName == NULL || outBuf == NULL ) return EX_NOINPUT; 

	rfp = fopen( inName, "rb" ); 
	if( rfp == NULL ) return EX_NOINPUT; 

	rval = fread( &rlen, sizeof rlen, 1, rfp ); 
	if( rval != 1 ) goto fail0;
	dlen = ntohl( rlen );

	// write dlen  
	gdp_buf_put_uint32( outBuf, dlen ); 


	rval = fread( &rlen, sizeof rlen, 1, rfp ); 
	if( rval != 1 ) goto fail0;
	slen = ntohl( rlen );

	// write slen  
	gdp_buf_put_uint32( outBuf, slen ); 


	if( dlen>1024 || slen>1024 ) {
		ep_dbg_printf("[ERROR] short buf size compared to dlen %u, slen %u \n", 
						dlen, slen );
		goto fail0;
	}

	// write token   
	rval = fread( dbuf, sizeof(uint8_t), dlen, rfp ); 
	if( rval != dlen ) goto fail0;
	gdp_buf_write( outBuf, dbuf, dlen );


	// write sig
	rval = fread( dbuf, sizeof(uint8_t), slen, rfp ); 
	if( rval != slen ) goto fail0;
	gdp_buf_write( outBuf, dbuf, slen );

	fclose( rfp );
	return EX_OK;

fail0: 
	fclose( rfp );

	ep_dbg_printf("[ERROR INFO] dlen %u slen %u outBuf len %zu \n", 
						dlen, slen, gdp_buf_getlength( outBuf ) );
	return EX_FAILURE;
}



// token->info != NULL case.. 
void print_actoken( struct ac_token *token, FILE *wfp )
{
	int					ti; 
	int					dlen; 
	uint8_t				*data;
	gdp_gclmd_t			*infos = token->info;
	gdp_gclmd_id_t		id;

	for( ti=0; ti<infos->nused; ti++ ) {
		id   = infos->mds[ti].md_id; 
		dlen = infos->mds[ti].md_len; 
		data = infos->mds[ti].md_data; 

		switch( id ) {
			case GDP_ACT_ISSUER: 
				fprintf( wfp, "issuer(%d): %s \n", dlen, data );
				break;

			case GDP_ACT_SUBJECT: 
				fprintf( wfp, "subject(%d): %s \n", dlen, data );
				break;

			case GDP_ACT_UID: 
				fprintf( wfp, "uid(%d): %s \n", dlen, data );
				break;

			case GDP_ACT_GID: 
				fprintf( wfp, "gid(%d): %s \n", dlen, data );
				break;

			case GDP_ACT_DID: 
				fprintf( wfp, "did(%d): %s \n", dlen, data );
				break;

			case GDP_ACT_GUID: 
				fprintf( wfp, "guid(%d): %s \n", dlen, data );
				break;

			case GDP_ACT_CTIME: 
				fprintf( wfp, "ctime(%d): %s \n", dlen, data );
				break;

			case GDP_ACT_PUBKEY: 
				fprintf( wfp, "PUBKEY(%d): \n", dlen ); 
				ep_print_longhex( wfp, NULL, dlen, data );
				break;

			default: fprintf( wfp, "Wrong token field: %x \n", id );
					break; 
		}

	}

	if( token->siglen != 0 ) {
		fprintf( wfp, "SIG (%d): \n", token->siglen ); 
		ep_print_longhex( wfp, NULL, token->siglen, token->sigbuf );
	}
}

