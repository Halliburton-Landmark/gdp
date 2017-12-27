/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  SYMKEY INFO-CREATE --- create a symmetric key info 
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
** This is a utility program to create a symmetric key and store the key info 
** into file. Temporarily, data writer & reader can share the symmetric key 
** info for data encryption through this file. According to the user option, 
** This file has plain text or it is encrypted with the public key. 
** With valid k option, it is encrypted with the public key in the cert file.  
** Without K option, the file has plain text. 
** (This version does not include to verify the input certificate file yet.)
**
** Ultimately symmetric key needs to be disrbituted through Key Management System. 
** Each distributed key has to be managed securely on each device. 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2016.12.09 
*/ 



#include <gdp/gdp.h>
#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_crypto.h>
#include <hs/hs_symkey_gen.h>   
#include <hs/hs_util.h>   

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
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

void print_salt( FILE *outfp, uint8_t *insalt)
{

	if( insalt == NULL ) {
		ep_app_error("Unvalid input salt");
		return ;
	}
	ep_print_hexstr( outfp, "salt=", sizeof( insalt ), insalt );

}


void print_symkeyinfo( FILE *outfp, const EVP_CIPHER *inCipher, 
							uint8_t *inKey, uint8_t *inIv)
{
	if( inCipher == NULL ) {
		ep_app_error("Unvalid input EVP_CIPHER");
		return ;
	}

	// Print key info
	if( inCipher->key_len > 0 ) {
		ep_print_hexstr( outfp, "key=", inCipher->key_len, inKey );
	}

	// Print IV info
	if( inCipher->iv_len > 0 ) {
		ep_print_hexstr( outfp, "iv=", inCipher->iv_len, inIv );
	}

}

int	sprint_keyinfo( char *outBuf, char *inPrefix, uint8_t *inData, int inLen ) 
{
	int			t_offset = 0;
	int			t_ret	 = 0;
	int			ti;


	t_ret = sprintf( outBuf, "%s", inPrefix);
	t_offset += t_ret;

	for( ti=0; ti< inLen; ti++ ) {
		t_ret = sprintf( outBuf+t_offset, "%02X", inData[ti] );
		t_offset += t_ret;
	}

	t_ret = sprintf( outBuf+t_offset, "\n");

	t_offset += t_ret;

	// temporary first checking 
	// printf("%s total length: %d\n", inPrefix, t_offset );

	return t_offset;
}


int calculate_outdatalen(int algo, int mode, const EVP_CIPHER *inCipher)
{
	int			t_len = 0;

	// first line length 
	t_len = 6+ strlen( ep_crypto_keyenc_name( algo ) );

	// second line length 
	t_len = t_len + 6 + strlen( ep_crypto_mode_name( mode ) );

	// third line length  (8byte salt) 
	t_len = t_len + 6 + 16 ;

	// forth line length  (key) 
	if( inCipher->key_len > 0 ) 
			t_len = t_len + 5 + 2*(inCipher->key_len) ;

	// fifth line length  (IV) 
	if( inCipher->iv_len > 0 ) 
			t_len = t_len + 4 + 2*(inCipher->iv_len) ;

	return t_len;
}

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-d] [-e sym_key_alg] [-k public_key_file]\n"
			"\t[-m enc_mode] [-p password] [-s secret_key_file] outfilename\n"
			"    -d  print the debugging message\n"
			"    -e  symmetric key algorithm[key length] used for data encryption.\n"
			"        (supported valid values: aes128, aes192, aes256, DEFAULT:aes128) \n"
			"    -k  the cert file name (Cur version: with path, ECC cert) \n"
			"    -m  block cipher mode (supported values: cbc, cfb, ofb. DEFAULT: cbc)\n"
			"    -p  password which is used to derive the keying data \n"
			"        If no input, defulat value is used for password.\n"
			"    -s  If the cert file indicated by k option is ECC type, \n"
			"        we cannot encrypt the key info with ECC public key. \n"
			"        Instead we encrypt the key info with the symmetric key generated by \n"
			"        secret key of key distributor & public key of the device. \n"
			"        In this implementation, device can decrypt the key info with \n"
			"        symmetric key generated by public key of key distributor & \n"
			"        secret key of the device. \n"
			"    outfilename is the name of file with key info (without extension)\n\n"
			"Currently, there are two output mode.\n"    
			" The first mode has the encrypted key information in the output file."    
			" With the valid -k option, the key info is encrypted with public key."
			" Currently, we only support ECC cert with -k option."
			" Later, we can implement other cert type such as RSA."
			" The simple usage for this mode is as follow: \n"
			"         create_symkey [-d] -k <device's cert> "
			"-s <key distributor's secret key file> outfilename  \n"
			" The second mode has the plain key information in the outfile."
			" This mode can be used for debugging." 
			" Without k option, this mode is set."
			" In this mode, the outputfile has the txt extension."
			" The simple usage for this mode is as follow: \n"
			"         create_symkey [-d] outfilename  \n"
			, ep_app_getprogname());
	exit(64);
}


int
main(int argc, char **argv)
{
	int					opt;
	int					m_ret=0, t_ret;
	int					t_len;
	EP_STAT				r_estat;
	bool				show_dbg		= false;	
	bool				show_usage		= false;


	// about DATA encryption key 
	// initial value is default values 
	char				*passwd			= "wldlvlelvhfxm123";
	int					key_enc_alg		= EP_CRYPTO_SYMKEY_AES128;
	int					key_enc_mode	= EP_CRYPTO_MODE_CBC;
	unsigned char		salt[8];				
	const EVP_CIPHER	*de_cipher		= NULL;
	uint8_t				de_key[EVP_MAX_KEY_LENGTH]	= {0, };
	uint8_t				de_iv[EVP_MAX_IV_LENGTH]	= {0, };


    // about output file with data encryption key 
	char				*ofilename		= NULL;
	FILE				*okfp			= NULL;
	char				out_mode		= EP_CRYPT_NON; 
	const char			*keyfile		= NULL;

	// about the encryption of output file 
	FILE				*t_fp	= NULL;
	X509				*m_cert = NULL;
	EVP_PKEY			*m_pkey = NULL;
	EVP_PKEY			*m_skey = NULL;



	// initialize ecryption routine. 
	//gdp_lib_init(NULL);
	ep_lib_init(EP_LIB_USEPTHREADS);
	ep_crypto_init(0);


	// collect command-line arguments
	while ((opt = getopt(argc, argv, "de:k:m:np:s:")) > 0)
	{
		switch (opt)
		{
		 case 'd':
			show_dbg = true;
			break;


		 case 'e':
			key_enc_alg = ep_crypto_keyenc_byname(optarg);
			if (key_enc_alg < 0)
			{
				ep_app_error("unknown key encryption algorithm: %s", optarg);
				show_usage = true;
			}
			break;


		 case 'k':
			keyfile = optarg;
			out_mode = EP_CRYPT_ASYM;


			// open the cert file with keyfile name 	
			m_cert = ep_x509_cert_read_file( keyfile );
			if( m_cert == NULL ) {
				ep_app_error("Cannot read certificate in file %s", keyfile ); 
				show_usage = true;
				break;
			}

			// extract the public key in the cert file 
			m_pkey = X509_get_pubkey(m_cert);
			if( m_pkey == NULL ) {
				ep_app_error("Cannot read public key in cert %s", keyfile );  
				show_usage = true;
				if( m_cert != NULL)		X509_free(  m_cert );
			}

			break;


		 case 'm':
		    key_enc_mode = ep_crypto_mode_byname( optarg );
			if( key_enc_mode == -1 ) {
				ep_app_error("unknown or unsupported mode: %s", optarg);
				show_usage = true;
			}
			break;


		 case 'p':
			passwd = optarg;
			break;


		 case 's':
			t_fp = fopen( optarg, "r");

			if( t_fp == NULL ) {
				ep_app_error("Cannot open the secret key in %s", optarg );  
				show_usage = true;
				break;
			}

			if( PEM_read_PrivateKey( t_fp, &m_skey, NULL, NULL ) == NULL ) {
				ep_app_error("Cannot read secret key in %s", optarg );  
				show_usage = true;
				fclose( t_fp );
			}
			break;

		 default:
			show_usage = true;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (show_usage || argc != 1 )
		usage();


	if( out_mode != EP_CRYPT_NON && out_mode != EP_CRYPT_ASYM )  {
		ep_app_error("out_mode error : %d", out_mode );  
		usage();
	}

	if( out_mode == EP_CRYPT_ASYM ) {
		if( (EVP_PKEY_type( m_pkey->type ) == EVP_PKEY_EC) && (m_skey == NULL) ) {
			ep_app_error("In case of EC pub key, the secret key (-s option) is necessary" );  
			if( m_pkey != NULL )	EVP_PKEY_free( m_pkey );	
			if( m_skey != NULL )	EVP_PKEY_free( m_skey );	
			if( m_cert != NULL)		X509_free(  m_cert );
			usage();
		}
	}

	

	//////////////////////////////////////////////////////
	// A. Create the symmetric key for data encryption  //
	//////////////////////////////////////////////////////

	//1. generate pseudo-random number & use the number as salt 
	t_ret = RAND_pseudo_bytes( salt, 8);	
	if( t_ret < 0 ) {
		ep_app_error("Fail to create the Pseudo-random number ");
		return -1;
	}


	if( show_dbg ) {
		printf("****** Key generation INFO *********\n" );
		printf("INFO] enc algo: %s\n", ep_crypto_keyenc_name( key_enc_alg ) );
		printf("INFO] enc mode: %s\n", ep_crypto_mode_name( key_enc_mode ) );
		printf("INFO] enc pswd: %s\n", passwd );
	}

	// 2. generate key & IV 
	de_cipher = tmp_evp_cipher_type( key_enc_alg | key_enc_mode );
	if( de_cipher == NULL ) {
		ep_app_error("Not supporeted: %s , %s", ep_crypto_keyenc_name( key_enc_alg),  
					ep_crypto_mode_name( key_enc_mode) );
		return -1;
	}
	EVP_BytesToKey( de_cipher, EVP_md5(), salt, (unsigned char *) passwd, 
					strlen(passwd), 1, de_key, de_iv);  

	if( show_dbg ) {
		printf("\n****** Generated Key INFO *********\n" );
		print_salt( stdout, salt ); 
		print_symkeyinfo( stdout, de_cipher, de_key, de_iv );
		printf("key len: %d, IV len: %d \n", de_cipher->key_len, de_cipher->iv_len );
	} 



	//////////////////////////////////////////////////////
	// B. Write the key info into the outFile           //
	//////////////////////////////////////////////////////

	// For debugging, plain key file is also created currently.. 
	if( out_mode == EP_CRYPT_NON ) {
		// write plain key info into the outfile 

		// filename : argv[0].txt 
		t_len = strlen( argv[0] ) + 5;	
		ofilename = ep_mem_malloc( t_len );
		snprintf( ofilename, t_len, "%s.txt", argv[0] );
		ofilename[t_len] = '\0';

		if( show_dbg && out_mode == EP_CRYPT_NON ) {
			printf("\n****** Output INFO *********\n" );
			printf("OutFile name: %s \n", ofilename );
		}


		// write the key info into the outfile
		okfp = fopen( ofilename, "w"); 
		if( okfp == NULL ){
			ep_app_error("Fail to open outFile: %s", ofilename );  
			m_ret = -1;
			goto fail0; 
		}


		fprintf( okfp, "algo=%s\n", ep_crypto_keyenc_name( key_enc_alg ) );
		fprintf( okfp, "mode=%s\n", ep_crypto_mode_name( key_enc_mode )  );
		print_salt( okfp, salt );
		print_symkeyinfo( okfp, de_cipher, de_key, de_iv );

		fclose( okfp );
		okfp = NULL;

//	} else 
	} else if( out_mode == EP_CRYPT_ASYM ) {
//	if( out_mode == EP_CRYPT_ASYM ) {
		// write key info encrypted with public key in keyfile into the outfile 
		int						t_pos		= 0;
		int						t_outlen	= 0;
		int						t_inlen		= 0;
		char					*t_indata	= NULL;
		char					*t_spos		= NULL;
		char					*t_encBuf	= NULL;
		char					t_outkey[SHA_DIGEST_LENGTH];
		EP_CRYPTO_CIPHER_CTX	*tsc_ctx	= NULL;

//		EVP_PKEY_CTX		*t_pctx = NULL;
//		FILE				*t_cfp  = NULL;
//		EP_CRYPTO_KEY		*t_pkey = NULL;


		// out filename : argv[0].enc 
		if( ofilename == NULL ) {
			t_len = strlen( argv[0] ) + 5;	
			ofilename = ep_mem_malloc( t_len );
		}
		snprintf( ofilename, t_len, "%s.enc", argv[0] );
		ofilename[t_len] = '\0';

		if( show_dbg ) {
			printf("\n****** Output INFO *********\n" );
			printf("OutFile name: %s \n", ofilename );
		}



		// Check the length of data to be encrypted & put the data into in buffer.. 
		t_inlen = calculate_outdatalen( key_enc_alg, key_enc_mode, de_cipher);
		t_indata = ep_mem_malloc( t_inlen + 1);
		if( t_indata == NULL ) {
			ep_app_error("Cannot allocate the memory for data to be encrypted");  
			m_ret = -1;
			goto fail0; 
		}
		t_spos = t_indata;
		t_pos = sprintf( t_indata, "algo=%s\nmode=%s\n",
								ep_crypto_keyenc_name( key_enc_alg ),
								ep_crypto_mode_name( key_enc_mode ) );
		t_indata += t_pos;					
		t_pos = sprint_keyinfo( t_indata, "salt=", salt, 8 );
		t_indata += t_pos;					
		t_pos = sprint_keyinfo( t_indata, "key=", de_key, de_cipher->key_len );
		t_indata += t_pos;
		if( de_cipher->iv_len > 0 ) 
			t_pos = sprint_keyinfo( t_indata, "iv=", de_iv, de_cipher->iv_len);
		t_indata[t_pos] = '\0';	
		t_indata = t_spos;

		// temporary debugging 
		//printf("****** total Indata [%d] ********* \n", t_inlen );
		//printf("%s", t_indata );


		// read the public key in keyfile (cert file) 
		// For later check, this function fails because it reads key file not cert..
		//t_pkey = ep_crypto_key_read_fp( t_cfp, keyfile, 
		//					EP_CRYPTO_KEYFORM_PEM, EP_CRYPTO_F_PUBLIC );


		if( EVP_PKEY_type( m_pkey->type ) == EVP_PKEY_EC ) {
			// Default : AES128 shared key creation 
			t_outlen = ep_compute_sharedkey_onEC( m_skey, m_pkey, 16, t_outkey );

			if( t_outlen == 0 ) {
				ep_app_error("Fail to compute EC shared key");
				goto fail1;
			}
			if( t_outlen > 16 ) t_outkey[16] = '\0'; 


			// temporary debugging 
			ep_print_hexstr( stdout, "GenSharedKey=", 16, (uint8_t *)t_outkey );

			//reuse de_iv, t_outlen
			memset( de_iv, 0, EVP_MAX_IV_LENGTH );	

			// default: aes128, cbc : encrypt(true)  
			tsc_ctx = ep_crypto_cipher_new( EP_CRYPTO_SYMKEY_AES128 | EP_CRYPTO_MODE_CBC, 
							(uint8_t *)t_outkey, de_iv, true );
			if( tsc_ctx == NULL ) goto fail1;	

			
			//reuse t_outlen
			t_outlen = t_inlen + 16;	
			t_encBuf = ep_mem_zalloc( (size_t)t_outlen );
			if( t_encBuf == NULL ) goto fail1;
			r_estat = ep_crypto_cipher_crypt( tsc_ctx, (void *)t_indata, (size_t)t_inlen, 
												(void *)t_encBuf, (size_t)t_outlen);
			 

			okfp = fopen( ofilename, "wb"); 
			if( okfp == NULL ){
				ep_app_error("Fail to open outFile: %s", ofilename );  
				m_ret = -1;
				goto fail1; 
			}
			printf("Indata: %d EncBuf: %d, rVal: %u \n", t_inlen, t_outlen, 
									EP_STAT_TO_INT(r_estat));
			fwrite( t_encBuf, EP_STAT_TO_INT(r_estat), sizeof( char ), okfp);
			fclose( okfp );
			okfp = NULL;

		}

/*		EC does not support the encrypt func... (for later reference) 
		// However, RSA can be done through below  approach. 	
		t_pctx = EVP_PKEY_CTX_new( m_pkey, NULL );
		if( t_pctx == NULL ) {
			ep_app_error("Fail to handle public key (ctx_new) in cert %s", keyfile );  
			m_ret = -1;
			goto fail1; 
		} 

		t_ret = EVP_PKEY_encrypt_init( t_pctx );
		if( t_ret != 1 ) {
			ep_app_error("Fail to handle public key (init): rval %d", t_ret );  
			m_ret = -1;
			goto fail1; 
		}
*/

fail1:	
		if( t_indata  != NULL )	ep_mem_free( t_indata );
		if( tsc_ctx   != NULL ) ep_crypto_cipher_free( tsc_ctx );
		if( t_encBuf  != NULL ) ep_mem_free( t_encBuf );
		if( okfp	  != NULL ) fclose( okfp );

	} else {
		ep_app_error("NON-supported output mode: %d", out_mode );
		return -1;
	}
	


fail0:
	if( ofilename != NULL ) ep_mem_free( ofilename ) ;
	if( m_pkey != NULL )	EVP_PKEY_free( m_pkey );	
	if( m_skey != NULL )	EVP_PKEY_free( m_skey );	
	if( m_cert != NULL)		X509_free(  m_cert );

	return m_ret;
}


