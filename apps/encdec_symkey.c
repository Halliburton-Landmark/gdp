/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  Read key info file (plain version or encrypted version). 
**  If the file is encrypted, decrypt the file with its own secret key.  
**  Then, re-encrypt the file with another device's public key. 
**
**
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
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
** This is a utility program to read the symmetric key from the input file,  
** encrypt the key info for device with input device cert,  
** and store the encrypted key info into output file.  
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2016.12.09 
*/ 


#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sha.h>


#include <gdp/gdp.h>

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_crypto.h>
#include <ep/ep_file.h>
#include <ep/ep_util.h>
#include <ep/ep_symkey_gen.h>   



/*
//static EP_DBG	Dbg = EP_DBG_INIT("gdp.symkey-create", "Create the file with sym key ");
void print_hexstr( FILE *outfp, char *prefix, int inlen, uint8_t *indata)
{
	int				ti;


	if( indata == NULL ) {
		ep_app_error("Unvalid input data");
		return ;
	}

	fprintf( outfp, "%s", prefix );
	for( ti=0; ti< inlen; ti++ ) {
		fprintf( outfp, "%02X", indata[ti] );
	}
	fprintf( outfp, "\n" );
}
*/

bool is_encrypted( char		*inFname ) 
{
	const char			*t_pos;


	t_pos = strrchr( inFname, '.' );
	if( t_pos == NULL ) return false;

	t_pos++;
	if( strncasecmp( t_pos, "enc", 3) == 0 ) return true;

	return false;
}


void
usage(void)
{ 
	fprintf(stderr, "Usage: %s [-c key distributor's cert] [-d]  [-i symkey_file] \n"
			"\t[-k key distributor's secret key file] [-p device cert]\n"
			"\t[-s my secret key file] outfilename\n"
			"    -c  the cert name (with path) of key distributor.\n"
			"    -d  print the debugging message\n"
			"    -i  (mandatory) file name with symmetric data encryption key.\n"
			"		 txt extension means plain text.\n"
			"		 enc extension means encrypted info (current only shared key based on ECDH)\n"
			"		 In the case of encrypted file, -c & -s options must be required. \n"
			"    -k  (mandatory) the secret key file of key distributor\n"
			"    -p  (mandatory) the cert name of device where enc key is sent \n"
			"    -s  the secret key file of the device that receive encrypted key \n"
			"    outfilename is the name of file encrypted for another device (without extension)\n"
			"        the cert of the device is indicated with -p option. \n"
			, ep_app_getprogname());
	exit(64);
}

/*
**  Read key info file (plain version or encrypted version). 
**  If the file is encrypted, decrypt the file with its own secret key.  
**  Then, re-encrypt the file with another device's public key. 
*/

int
main(int argc, char **argv)
{
	int					opt;
	int					m_ret=0;
	int					ori_dlen		= 0;
	int					enc_dlen		= 0;
	bool				show_dbg		= false;	
	bool				show_usage		= false;
	bool				encrypted		= false;
	FILE				*t_fp			= NULL;
	EP_STAT				r_estat;

	// Input key file (plain or encrypted) 
	char				*symkeyfile		= NULL;

	// Original Key info (plain)
	char				*skBuf			= NULL;
	char				*outBuf			= NULL;

	// For decryption of encrypted input key file
	X509				*dec_kcert = NULL;
	EVP_PKEY			*dec_kpkey = NULL;
	EVP_PKEY			*dec_dskey = NULL;

	// For encrption for other device 
	X509				*enc_dcert = NULL;
	EVP_PKEY			*enc_dpkey = NULL;
	EVP_PKEY			*enc_kskey = NULL;

    // about output file with data encryption key 
	char				*ofilename		= NULL;
	FILE				*ofp			= NULL;




	// initialize ecryption routine. 
	gdp_lib_init(NULL);

	// collect command-line arguments
	while ((opt = getopt(argc, argv, "c:di:k:p:s:")) > 0)
	{
		switch (opt)
		{
		 case 'c':
			// open the cert file of key distributor 	
			dec_kcert = ep_x509_cert_read_file( optarg );
			if( dec_kcert == NULL ) {
				ep_app_error("Cannot read certificate in file %s", optarg ); 
				show_usage = true;
			}

			// extract the public key in the cert file 
			dec_kpkey = X509_get_pubkey(dec_kcert);
			if( dec_kpkey == NULL ) {
				ep_app_error("Cannot read public key in cert %s", optarg );  
				show_usage = true;
			}

			if( dec_kcert != NULL)		X509_free(  dec_kcert );
			dec_kcert = NULL;
			break;


		 case 'd':
			show_dbg = true;
			break;


		 case 'i':
			symkeyfile = optarg; 	
			encrypted = is_encrypted( symkeyfile ) ;
			break;



		 case 'k':
			t_fp = fopen( optarg, "r");

			if( t_fp == NULL ) {
				ep_app_error("Cannot open the secret key in %s", optarg );  
				show_usage = true;
			}

			if( PEM_read_PrivateKey( t_fp, &enc_kskey, NULL, NULL ) == NULL ) {
				ep_app_error("Cannot read secret key in %s", optarg );  
				show_usage = true;
				fclose( t_fp );
			}

			fclose( t_fp );
			t_fp = NULL;
			break;


		 case 'p':
			// open the cert file of device where encrypted key is sent 	
			enc_dcert = ep_x509_cert_read_file( optarg );
			if( enc_dcert == NULL ) {
				ep_app_error("Cannot read certificate in file %s", optarg ); 
				show_usage = true;
			}

			// extract the public key in the cert file 
			enc_dpkey = X509_get_pubkey(enc_dcert);
			if( enc_dpkey == NULL ) {
				ep_app_error("Cannot read public key in cert %s", optarg );  
				show_usage = true;
			}

			if( enc_dcert != NULL)		X509_free(  enc_dcert );
			enc_dcert = NULL;
			break;


		 case 's':
			t_fp = fopen( optarg, "r");

			if( t_fp == NULL ) {
				ep_app_error("Cannot open the secret key in %s", optarg );  
				show_usage = true;
			}

			if( PEM_read_PrivateKey( t_fp, &dec_dskey, NULL, NULL ) == NULL ) {
				ep_app_error("Cannot read secret key in %s", optarg );  
				show_usage = true;
			}

			fclose( t_fp );
			t_fp = NULL;
			break;

		default:
			show_usage = true;
			break;
		}
	}

	argc -= optind;
	argv += optind;


	if( symkeyfile == NULL || show_usage || argc != 1) {
		if( symkeyfile	== NULL ) ep_app_error("No input file with symmetric encryption key" );  
		if( dec_kpkey	!= NULL ) EVP_PKEY_free( dec_kpkey );	
		if( enc_dpkey	!= NULL ) EVP_PKEY_free( enc_dpkey );	
		if( enc_kskey	!= NULL ) EVP_PKEY_free( enc_kskey );	
		if( dec_dskey	!= NULL ) EVP_PKEY_free( dec_dskey );	
		usage();
	}


	if( encrypted == false ) {
		if( dec_kpkey!=NULL )  	ep_app_error("Non necessary the cert info of key distributor"); 
		if( dec_dskey!=NULL )  	ep_app_error("Non necessary the secret key info of device"); 

		//usage();
	}  


	if( enc_dpkey==NULL || enc_kskey==NULL ) {
		ep_app_error("Unsufficient key info for final encryption"); 
		if( dec_kpkey	!= NULL ) EVP_PKEY_free( dec_kpkey );	
		if( enc_dpkey	!= NULL ) EVP_PKEY_free( enc_dpkey );	
		if( enc_kskey	!= NULL ) EVP_PKEY_free( enc_kskey );	
		if( dec_dskey	!= NULL ) EVP_PKEY_free( dec_dskey );	
		usage();
	}

	// (Later: removed)Current version: only EC key  
	if( encrypted && ( EVP_PKEY_type( dec_kpkey->type ) != EVP_PKEY_EC || 
						EVP_PKEY_type( dec_dskey->type ) != EVP_PKEY_EC	) ) 
	{
		ep_app_error("In Current version, only EC key is supported");  
		m_ret = -1;
		goto fail0;
	}
	if( EVP_PKEY_type( enc_dpkey->type ) != EVP_PKEY_EC || 
			EVP_PKEY_type( enc_kskey->type ) != EVP_PKEY_EC	) 
	{
		ep_app_error("In Current version, only EC key is supported");  
		m_ret = -1;
		goto fail0;
	}


	m_ret = strlen( argv[0] ) + 5;	
	ofilename = ep_mem_malloc( m_ret );
	snprintf( ofilename, m_ret, "%s.enc", argv[0] );
	ofilename[m_ret] = '\0';

	if( show_dbg ) {
		printf("\n****** Output INFO *********\n" );
		printf("OutFile name: %s \n", ofilename );
	}


	//////////////////////////////////////////////////////
	// A. Read the input file with data encryption key  //
	//////////////////////////////////////////////////////
	if( encrypted ) {
		// read file contents & decrypt the contents 
		// result is skBuf[] 
		int					t_enclen	= 0;	
		char				*encBuf		= NULL;

		t_enclen = ep_get_fileContents( symkeyfile, &encBuf );

		// Calculate the shared key from my secret key & other's public key  
		// decrypt the input file 
		// Current : only EC 
		if( EVP_PKEY_type( dec_kpkey->type ) == EVP_PKEY_EC ) {
			int						t_outlen;	
			char					t_outkey[SHA_DIGEST_LENGTH];
			uint8_t					de_iv[EVP_MAX_IV_LENGTH];
			EP_CRYPTO_CIPHER_CTX	*tsc_ctx	= NULL;

			// Default : AES128 shared key creation 
			//////////////////////////////////////////////////////
			// A1. Calculate the new shared key for encryption  //
			//////////////////////////////////////////////////////
			t_outlen = ep_compute_sharedkey_onEC( dec_dskey, dec_kpkey, 16, t_outkey );

			if( t_outlen == 0 ) {
				ep_app_error("Fail to compute EC shared key");
				free( encBuf );
				m_ret = -1;
				goto fail0;
			}
			if( t_outlen > 16 ) t_outkey[16] = '\0'; 


			// temporary debugging 
			ep_print_hexstr( stdout, "GenSharedKey=", 16, (uint8_t *)t_outkey );

			//reuse de_iv, t_outlen
			memset( de_iv, 0, EVP_MAX_IV_LENGTH );	

			// default: aes128, cbc : encrypt(true)  
			//////////////////////////////////////////////////////
			// A2. decrypt the encrypted key info               //
			//////////////////////////////////////////////////////
			tsc_ctx = ep_crypto_cipher_new( EP_CRYPTO_SYMKEY_AES128 | EP_CRYPTO_MODE_CBC, 
							(uint8_t *)t_outkey, de_iv, false );
			if( tsc_ctx == NULL ) {
				ep_app_error("Fail to decode (new)");
				free( encBuf); 
				m_ret = -1;
				goto fail0;	
			}

			
			//reuse t_outlen
			t_outlen = t_enclen + 16;
			skBuf = ep_mem_zalloc( (size_t)t_outlen );
			if( skBuf == NULL ) {
				ep_app_error("Fail to malloc for decryption");
				free( encBuf); 
				ep_crypto_cipher_free( tsc_ctx );
				m_ret = -1;
				goto fail0;	
			}
			r_estat = ep_crypto_cipher_crypt( tsc_ctx, (void *)encBuf, (size_t)t_enclen, 
												(void *)skBuf, (size_t)t_outlen);

			// LATER : CHECK ERROR 
			ori_dlen = (int) EP_STAT_TO_INT( r_estat );
			if( ori_dlen < 0 || ori_dlen>t_enclen ) {
				ep_app_error("Fail to decrypt");
				free( encBuf); 
				ep_crypto_cipher_free( tsc_ctx );
				m_ret = -1;
				goto fail0;	
			}
			printf("Indata: %d skBuf rVal: %d \n", t_enclen, ori_dlen ); 
			skBuf[ori_dlen] = '\0'; 
		
			ep_crypto_cipher_free( tsc_ctx );
		}

		free( encBuf );
	} else {
		// read file contents into skBuf[] 
		ori_dlen = ep_get_fileContents( symkeyfile, &skBuf );
	}

	if( show_dbg ) {
		printf("****** Key file INFO *********\n" );
		printf("INFO] In file length: %d\n", ori_dlen );
		if( ori_dlen > 0 ) printf("%s\n", skBuf );
	}


	//////////////////////////////////////////////////////
	// B. Encrypt the plain key info & store it in file //
	//////////////////////////////////////////////////////

	if( EVP_PKEY_type( dec_kpkey->type ) == EVP_PKEY_EC ) {

		//////////////////////////////////////////////////////
		// B1. Calculate the new shared key for encryption  //
		//////////////////////////////////////////////////////
		int						t_outlen;	
		char					t_outkey[SHA_DIGEST_LENGTH];

		uint8_t					de_iv[EVP_MAX_IV_LENGTH];
		EP_CRYPTO_CIPHER_CTX	*tsc_ctx	= NULL;

		// Default : AES128 shared key creation 
		t_outlen = ep_compute_sharedkey_onEC( enc_kskey, enc_dpkey, 16, t_outkey );
		if( t_outlen == 0 ) {
			ep_app_error("Fail to compute EC shared key");
			m_ret = -1;
			goto fail0;
		}
		if( t_outlen > 16 ) t_outkey[16] = '\0'; 


		// temporary debugging 
		ep_print_hexstr( stdout, "GenSharedKeyforEnc=", 16, (uint8_t *)t_outkey );

		//reuse t_outlen
		memset( de_iv, 0, EVP_MAX_IV_LENGTH );	


		//////////////////////////////////////////////////////
		// B2. Encrypt the key info                         //
		//////////////////////////////////////////////////////
		// default: aes128, cbc : encrypt(true)  
		tsc_ctx = ep_crypto_cipher_new( EP_CRYPTO_SYMKEY_AES128 | EP_CRYPTO_MODE_CBC, 
						(uint8_t *)t_outkey, de_iv, true );
		if( tsc_ctx == NULL ) {
			ep_app_error("Fail to decode (new)");
			m_ret = -1;
			goto fail0;	
		}

		//reuse t_outlen
		t_outlen = ori_dlen + 16;
		outBuf   = malloc( sizeof(char) * t_outlen );
		r_estat = ep_crypto_cipher_crypt( tsc_ctx, (void *)skBuf, (size_t)ori_dlen, 
											(void *)outBuf, (size_t)t_outlen);

		// LATER : CHECK ERROR 
		enc_dlen = (int) EP_STAT_TO_INT( r_estat );
		if( enc_dlen < 0 ) {
			ep_app_error("Fail to encrypt");
			ep_crypto_cipher_free( tsc_ctx );
			m_ret = -1;
			goto fail0;	
		}
		printf("Indata: %d skBuf tmpBuf: %d rVal: %d \n", ori_dlen, t_outlen, enc_dlen ); 
		outBuf[enc_dlen] = '\0'; 

		ep_crypto_cipher_free( tsc_ctx );
	}



	//////////////////////////////////////////////////////
	// B3. store the encrypted key info into file       //
	//////////////////////////////////////////////////////
	ofp = fopen( ofilename, "wb"); 
	if( ofp == NULL ){
		ep_app_error("Fail to open outFile: %s", ofilename );  
		m_ret = -1;
		goto fail0; 
	}
	fwrite( outBuf, enc_dlen, sizeof( char ), ofp);
	fclose( ofp );
	ofp = NULL;


fail0:
	if( skBuf		!= NULL ) free( skBuf );
	if( outBuf		!= NULL ) free( outBuf );
	if( ofilename != NULL ) ep_mem_free( ofilename ) ;
	if( dec_kpkey	!= NULL ) EVP_PKEY_free( dec_kpkey );	
	if( enc_dpkey	!= NULL ) EVP_PKEY_free( enc_dpkey );	
	if( enc_kskey	!= NULL ) EVP_PKEY_free( enc_kskey );	
	if( dec_dskey	!= NULL ) EVP_PKEY_free( dec_dskey );	

	return m_ret;

}

