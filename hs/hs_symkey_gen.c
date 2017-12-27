/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**  Copyright (c) 2015-2017, Electronics and Telecommunications
**  Research Institute (ETRI). All rights reserved.
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
** This provides the utility functions to handle the key. 
** Most of functions in this file can be merged into ep_crypto_key.c 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2016.12.09 
*/ 


#include <string.h>
#include <strings.h>
#include <ep/ep.h>
#include <ep/ep_crypto.h>
#include <openssl/pem.h>
#include <openssl/sha.h>

#include <hs/hs_symkey_gen.h>



/*
**  Convert a symmetric algorithm mode to or from an internal EP type.
*/
struct name_to_format	
{
	const char			*str;	// string name of format / mode
	int					form;	// internal name 
};

static struct name_to_format	ModeStrings[] =
{
	{ "cbc",		EP_CRYPTO_MODE_CBC,		},
	{ "cfb",		EP_CRYPTO_MODE_CFB,		},
	{ "ofb",		EP_CRYPTO_MODE_OFB,		},
	{ NULL,			-1,				}
};


int
ep_crypto_mode_byname(const char *instr)
{
	struct	name_to_format		*cur= NULL;
	
	for( cur = ModeStrings; cur->str != NULL; cur++ ) 
	{
		if( strcasecmp(cur->str, instr) == 0 ) break;
	}

	return cur->form;

}

const char *
ep_crypto_mode_name(int inmode)
{
	struct	name_to_format		*cur= NULL;
	
	for( cur = ModeStrings; cur->str != NULL; cur++ ) 
	{
		if( cur->form == inmode ) return cur->str;
	}

	return "unknown";
}




/*
** This function is the same as evp_cipher_type in ep/ep_crypto_cipher.c 
** Currently, the original function is limited within the file. 
** So, temporarily, define the same function in this file.
**
** Convert EP cipher code to OpenSSL cipher (helper routine)
*/

const EVP_CIPHER *
tmp_evp_cipher_type(uint32_t cipher)
{
	switch (cipher)
	{
	  case EP_CRYPTO_SYMKEY_AES128 | EP_CRYPTO_MODE_CBC:
		return EVP_aes_128_cbc();
	  case EP_CRYPTO_SYMKEY_AES128 | EP_CRYPTO_MODE_CFB:
		return EVP_aes_128_cfb();
	  case EP_CRYPTO_SYMKEY_AES128 | EP_CRYPTO_MODE_OFB:
		return EVP_aes_128_ofb();

	  case EP_CRYPTO_SYMKEY_AES192 | EP_CRYPTO_MODE_CBC:
		return EVP_aes_192_cbc();
	  case EP_CRYPTO_SYMKEY_AES192 | EP_CRYPTO_MODE_CFB:
		return EVP_aes_192_cfb();
	  case EP_CRYPTO_SYMKEY_AES192 | EP_CRYPTO_MODE_OFB:
		return EVP_aes_192_ofb();

	  case EP_CRYPTO_SYMKEY_AES256 | EP_CRYPTO_MODE_CBC:
		return EVP_aes_256_cbc();
	  case EP_CRYPTO_SYMKEY_AES256 | EP_CRYPTO_MODE_CFB:
		return EVP_aes_256_cfb();
	  case EP_CRYPTO_SYMKEY_AES256 | EP_CRYPTO_MODE_OFB:
		return EVP_aes_256_ofb();
	}

	return NULL;
}



/*
** This function reads the cert file whose name is arguement 
** and return the X509 certificate obj. 
** Currently, CERT TYPE is decided with the file extension. 
** (current version: pem_password_cb is always NULL)
*/
X509* ep_x509_cert_read_file( const char *inCertname)
{
	int				t_format;
	const char		*t_pos;
	FILE			*tfp; 
	X509			*tCert = NULL;

	if( inCertname == NULL ) {
		_ep_crypto_error( "Input Cert Name : NULL" );
		return NULL;
	}

	// Check cert format : PEM or DER 
	t_pos = strrchr( inCertname, '.' );
	if( t_pos == NULL ) {
		_ep_crypto_error("Cannot decide cert format from name : %s", inCertname);
		return NULL;
	}
	t_pos++;

	if( strncasecmp( t_pos, "pem", 3) == 0 ) t_format = EP_CRYPTO_KEYFORM_PEM; 
	else if( strncasecmp( t_pos, "der", 3) == 0 ) t_format = EP_CRYPTO_KEYFORM_DER; 
	else {
		_ep_crypto_error("Cannot decide cert format from name : %s", inCertname);
		return NULL;
	}


	// read cert file 
	tfp = fopen( inCertname, "r");
	if( tfp == NULL ) {
		_ep_crypto_error("Cannot open the file: %s", inCertname);
		return NULL;
	}

	if( t_format == EP_CRYPTO_KEYFORM_PEM ) {
		tCert = PEM_read_X509( tfp, NULL, NULL, NULL );

	} else if( t_format == EP_CRYPTO_KEYFORM_DER ) {
		tCert = d2i_X509_fp( tfp, NULL );	
	}  
	fclose( tfp );

	return tCert;

}


// Maximum length: 20 byte : 160 bit... 
static void *KDF1_SHA1(const void *in, size_t inlen, void *out, size_t *outlen)
{
	if( *outlen < SHA_DIGEST_LENGTH ) return NULL;
	*outlen = SHA_DIGEST_LENGTH ;

	return SHA1( in, inlen, out );
}



// According to the ECDH algorithm, 
//	calculate the shared key based on my secret key & the other's public key. 
//	calculated shared key is stored in outkey 
//	if calculated key len is less than reqLen, fail to create the shared key.  
//  In cur version, Min size of outKey is SHA_DIGEST_LENGTH. 
// return value: output key length on success. 0 on failure. 
int ep_compute_sharedkey_onEC(EVP_PKEY *my_secret, EVP_PKEY *other_public, 
										int reqLen, char *outKey )
{
	int				t_buflen = 0;
	int				t_outlen = 0;
	EC_KEY			*t_mySecret =  EVP_PKEY_get1_EC_KEY(my_secret);
	const EC_KEY	*t_oPublic  =  EVP_PKEY_get1_EC_KEY(other_public);

	t_buflen = SHA_DIGEST_LENGTH;
	t_outlen = ECDH_compute_key( outKey, t_buflen, 
				EC_KEY_get0_public_key(t_oPublic), t_mySecret, KDF1_SHA1 );

/* extension version 
	int				t_deflen = 0;
	t_deflen = (EC_GROUP_get_degree( EC_KEY_get0_group(t_mySecret) )  + 7 )/ 8;
	// temporary checking 
	//printf( "Default Key len: %d \n", t_deflen );


	if( reqLen > t_deflen || reqLen > SHA_DIGEST_LENGTH ) return 0;

	if( reqLen <= SHA_DIGEST_LENGTH )  {
		t_buflen = SHA_DIGEST_LENGTH;
		outKey = ep_mem_malloc( SHA_DIGEST_LENGTH );
		t_outlen = ECDH_compute_key( outKey, t_buflen, 
				EC_KEY_get0_public_key(t_oPublic), t_mySecret, KDF1_SHA1 );

	} else if( reqLen <= t_deflen ) {
		t_buflen = t_deflen;
		outKey = ep_mem_malloc( t_buflen );
		t_outlen = ECDH_compute_key( outKey, t_buflen, 
				EC_KEY_get0_public_key(t_oPublic), t_mySecret, NULL );

	}
*/

	EC_KEY_free( t_mySecret );
	EC_KEY_free( (EC_KEY *)t_oPublic  );

	return t_outlen;
}



/*
** Extract the publie key in the certificate. 
** Return the extracted publie key. 
** 
** 1'st argu: full name of certificate
** rval     : extracted publie key 
*/
//EVP_PKEY* 
EP_CRYPTO_KEY *
get_pubkey_from_cert_file(const char *a_certFName )
{
	X509				*t_cert  = NULL;
	EVP_PKEY			*pub_key = NULL;


/*
	if( a_certFName == NULL ) {
		// temporary debugging 
		printf("NULL input cert file \n" ); 
		return NULL;
	}
*/


	// open the cert file  	
	t_cert = ep_x509_cert_read_file( a_certFName );
	if( t_cert == NULL ) {
		// temporary debugging 
		printf("Cannot read certificate in file %s\n", a_certFName ); 
		return NULL;
	}


	// extract the public key in the cert file 
	pub_key = X509_get_pubkey(t_cert);
	if( pub_key == NULL ) {
		// temporary debugging 
		printf("Cannot read public key in cert %s\n", a_certFName );  
	}

	if( t_cert != NULL)		X509_free(  t_cert );
	t_cert = NULL;

	return (EP_CRYPTO_KEY *)pub_key;

}


