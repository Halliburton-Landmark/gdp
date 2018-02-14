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
**	TOKEN_GENERATOR :: emulation for registration process... 
**		We assume the previous subscription for the related service. 
**		   However, all requests are passed without password. 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2018.01.10 
*/ 


#include <getopt.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <gdp/gdp.h>
#include <ep/ep_app.h>
#include <hs/hs_errno.h>   
#include <hs/hs_symkey_gen.h>   

#include <ac/ac_token.h>


#define RS_CERT_FILE "/home/hsmoon/etc/certs/registration/rs_ecc.pem"
#define RS_SKEY_FILE "/home/hsmoon/etc/certs/registration/rs_ecc.key"


// hsmoon start 
bool check_subscription( char *uid, char *passwd ) 
{
	// LATER 
	// registration service manages the subscription info. 
	// list of uid , the related password about each uid 
	
	// However, current always pass. 

	if( uid == NULL ) return false;

	return true;
}


void usage(void)
{ 
	fprintf(stderr, "Usage: %s [-c device internal cert] [-d DID] [-g GID] \n"
			"\t[h] [-i ID]  [-p password] [-m] \n"
	//		"\t[h] [-i ID]  [-k device secret key file] [-m] [-p password]\n"
			"\t[-r regi service's cert] [-s regi service's secret key file]\n"
			"\t[-u UID] outfilename \n"
			"    -c  the cert name (with path) of device to be accessed \n"
			"    -d  device ID (device GUID). This value can be extracted"
			"in the device certificate. But, for convenience, we use the"
			" printable name for DID now. \n"

			"    -g  group ID (device GUID). This value is used according"
			" to the access rule type used in the service \n"

			"    -h  show the help message \n"
			"    -i  registered device ID to be used later. This value is used"
			" according to the access rule type used in the service \n"

//			"    -k  the secret key file of device\n"
			"    -m  creation capability\n"
			"    -p  user password to be set through service subscription\n"
//			"    -r  the cert name (with path) of registration service \n"
			"    -s  the secret key file of the registration service \n"
			"    -p  user ID to be set through service subscription\n"

			"    outfilename is the name of generated access token \n", 
			ep_app_getprogname());
	exit(64);
}


int
main(int argc, char **argv)
{
	int					opt;
	int					m_ret=0;
	bool				show_usage		= false;
	bool				cr_cap			= false;
	FILE				*t_fp			= NULL;

	//  User Subscription & device Info for access control 
	char				*uid			= NULL;
	char				*gid			= NULL;
	char				*did			= NULL;
	char				*guid			= NULL;
	char				*passwd			= NULL;
	char				*line			= NULL;

	// For device information  
	X509				*device_cert   = NULL;
	EVP_PKEY			*device_pubkey = NULL;
//	EVP_PKEY			*device_seckey = NULL;

	// For registration service information  
	X509				*rs_cert = NULL;   // enc_dcert -> rs_cert 
	EVP_PKEY			*rs_pubkey = NULL;  // enc_dpkey -> rs_pubkey
	EVP_PKEY			*rs_seckey = NULL; // enc_kskey -> rs_seckey

    // about output file :: output token.   
	char				*ofilename		= NULL;
	FILE				*ofp			= NULL;
	struct ac_token		*token			= NULL;



	// initialize ecryption routine. 
	// gdp_lib_init(NULL);
	ep_lib_init(EP_LIB_USEPTHREADS);
	ep_crypto_init(0);


	// collect command-line arguments
//	while ((opt = getopt(argc, argv, "c:d:g:hi:k:p:r:s:u:")) > 0)
	while ((opt = getopt(argc, argv, "c:d:g:hi:mp:r:s:u:")) > 0)
	{

		switch (opt)
		{
		 case 'c':
			// open the cert file of device  	
			device_cert = ep_x509_cert_read_file( optarg );
			if( device_cert == NULL ) {
				ep_app_error("Cannot read certificate in file %s", optarg ); 
				show_usage = true;
				break;
			}

			// extract the public key in the cert file 
			device_pubkey = X509_get_pubkey(device_cert);
			if( device_pubkey == NULL ) {
				ep_app_error("Cannot read public key in cert %s", optarg );  
				show_usage = true;
			}
			break;


		 case 'd':
			did = optarg;
			break;

		 case 'g': 
			gid = optarg;
			break;
		
		 case 'h': 
			show_usage = true;
			break;

		 case 'i':
			guid = optarg; 	
			break;
/*
		 case 'k': // device's secret key : in no checking version, not necessary
			t_fp = fopen( optarg, "r");

			if( t_fp == NULL ) {
				ep_app_error("Cannot open the secret key in %s", optarg );  
				show_usage = true;
				break;
			}

			if( PEM_read_PrivateKey(t_fp, &device_seckey, NULL, NULL) == NULL ) 
			{
				ep_app_error("Cannot read secret key in %s", optarg );  
				show_usage = true;
			}

			fclose( t_fp );
			t_fp = NULL;
			break;
*/
		 case 'm':
			cr_cap = true;
			break;

		 case 'p':
			passwd = optarg; 	
			break;

		 case 'r':
			// open the cert file of registration service  	
			rs_cert = ep_x509_cert_read_file( optarg );
			if( rs_cert == NULL ) {
				ep_app_error("Cannot read certificate in file %s", optarg ); 
				show_usage = true;
				break;
			}

			// extract the public key in the cert file 
			rs_pubkey = X509_get_pubkey( rs_cert);
			if( rs_pubkey == NULL ) {
				ep_app_error("Cannot read public key in cert %s", optarg );  
				show_usage = true;
			}
			break;

		 case 's':
			t_fp = fopen( optarg, "r");

			if( t_fp == NULL ) {
				ep_app_error("Cannot open the secret key in %s", optarg );  
				show_usage = true;
				break;
			}

			if( PEM_read_PrivateKey( t_fp, &rs_seckey, NULL, NULL ) == NULL ) {
				ep_app_error("Cannot read secret key in %s", optarg );  
				show_usage = true;
			}

			fclose( t_fp );
			t_fp = NULL;
			break;

		 case 'u':
			uid = optarg; 	
			break;

		default:
			show_usage = true;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if( show_usage || argc != 1) {
		if( device_cert   != NULL ) X509_free(     device_cert   );
		if( device_pubkey != NULL ) EVP_PKEY_free( device_pubkey );	
//		if( device_seckey != NULL ) EVP_PKEY_free( device_seckey );	

		if( rs_cert       != NULL ) X509_free(     rs_cert   );
		if( rs_pubkey     != NULL ) EVP_PKEY_free( rs_pubkey );	
		if( rs_seckey     != NULL ) EVP_PKEY_free( rs_seckey );	

		usage();
	}

	if( rs_pubkey == NULL ) {
		// open the cert file of default registration service  	
		rs_cert = ep_x509_cert_read_file( RS_CERT_FILE );
		if( rs_cert == NULL ) {
			ep_app_error("Cannot read certificate in file %s", RS_CERT_FILE ); 
			show_usage = true;

		} else {
			// extract the public key in the cert file 
			rs_pubkey = X509_get_pubkey( rs_cert);
			if( rs_pubkey == NULL ) {
				ep_app_error("Cannot read public key in cert %s", RS_CERT_FILE );  
				show_usage = true;
			}
		}
	}

	if( rs_seckey == NULL ) {
		t_fp = fopen( RS_SKEY_FILE, "r");

		if( t_fp == NULL ) {
			ep_app_error("Cannot open the secret key in %s", optarg );  
			show_usage = true;

		} else if( PEM_read_PrivateKey( t_fp, &rs_seckey, NULL, NULL ) == NULL ) {
			ep_app_error("Cannot read secret key in %s", optarg );  
			show_usage = true;
		}

		fclose( t_fp );
		t_fp = NULL;
	}


	// basic error check 
	if( device_pubkey==NULL || rs_pubkey==NULL || rs_seckey == NULL )
			show_usage = true;
	else if( uid==NULL ) show_usage = true;
	else if( gid==NULL && guid==NULL ) show_usage = true;
	else if( gid!=NULL && did==NULL  ) show_usage = true;


	if( show_usage ) {
		ep_app_error("Insufficient input info"); 

		if( device_pubkey==NULL || rs_pubkey==NULL || rs_seckey == NULL )
			ep_app_error( "	>>> No Key info" );


		if( device_cert   != NULL ) X509_free(     device_cert   );
		if( device_pubkey != NULL ) EVP_PKEY_free( device_pubkey );	
//		if( device_seckey != NULL ) EVP_PKEY_free( device_seckey );	

		if( rs_cert       != NULL ) X509_free(     rs_cert   );
		if( rs_pubkey     != NULL ) EVP_PKEY_free( rs_pubkey );	
		if( rs_seckey     != NULL ) EVP_PKEY_free( rs_seckey );	

		usage();
	}

	// (Later: removed)Current version: only EC key  
	if( EVP_PKEY_type( device_pubkey->type ) != EVP_PKEY_EC || 
				EVP_PKEY_type( rs_pubkey->type ) != EVP_PKEY_EC	)  
	{
		ep_app_error("In Current version, only EC key is supported");  
		m_ret = -1;
		goto fail0;
	}

	m_ret = strlen( argv[0] ) + 4;	
	ofilename = ep_mem_malloc( m_ret );
	snprintf( ofilename, m_ret, "%s.tk", argv[0] );
	ofilename[m_ret] = '\0';
	printf("OutFile name: %s \n", ofilename );

	if( check_subscription( uid, passwd ) == false ) {
		ep_app_error("In Current version, only EC key is supported");  
		goto fail0;
	}

	
	token = make_new_token(); 
	if( token == NULL )  {
		ep_app_error("Fail to create token structure");  
		goto fail0;
	}


	// 
	// 1. Token Issuer field: Registration Service (subject)   
	// 
	line = X509_NAME_oneline( X509_get_subject_name(rs_cert), 0, 0 ); 
	if( line == NULL ) {
		ep_app_error("Fail to get info in cert");
		goto fail0;
	}
	add_token_field( token, GDP_ACT_ISSUER, strlen(line), line ); 


	// 
	// 2. Token Subject field: device (subject)   
	//
	line = X509_NAME_oneline( X509_get_subject_name(device_cert), 0, 0 ); 
	if( line == NULL ) {
		ep_app_error("Fail to get info in cert");
		goto fail0;
	}
	add_token_field( token, GDP_ACT_SUBJECT, strlen(line), line ); 


	// 
	// 3. Token UID/GID/DID/GUID field:    
	//
	add_token_field( token, GDP_ACT_UID, strlen(uid), uid ); 
	if( guid != NULL ) 
		add_token_field( token, GDP_ACT_GUID, strlen(guid), guid ); 
	if( gid != NULL ) 
		add_token_field( token, GDP_ACT_GID, strlen(gid), gid ); 
	if( did != NULL ) 
		add_token_field( token, GDP_ACT_DID, strlen(did), did ); 

	if( cr_cap ) add_token_field( token, GDP_ACT_CCAP, 1, "1");

	// 
	// 4. Token CTIME / PUBKEY field:    
	//
	add_token_ctime_field( token );
	add_token_pubkey_field( token, device_pubkey );

	m_ret = calculate_signature( token, rs_seckey );
	if( m_ret  != EX_OK ) {
		ep_app_error( "Fail to update token" );
		goto fail0;
	}

	ofp = fopen( ofilename, "wb"); 
	if( ofp == NULL ){
		ep_app_error("Fail to open outFile: %s", ofilename );  
		m_ret = -1;
		goto fail0; 
	}

	m_ret = write_actoken_to_file( token, ofp );


	// verify actoken 
	{
		EP_STAT			estat;
		unsigned char	*tdata = NULL;
		EP_CRYPTO_MD	*md = NULL;

		md = ep_crypto_vrfy_new( rs_pubkey, token->md_alg_id );
		if( md == NULL ) {
			printf("Fail to verify new \n"); 
			goto tfail2;
		}

	
		tdata = gdp_buf_getptr( token->dbuf, token->dlen ); 
		estat = ep_crypto_vrfy_update( md, (void *)tdata, token->dlen );
		printf("Crypto Update pass... \n"); 

		estat = ep_crypto_vrfy_final( md, (void *)(token->sigbuf), 
										token->siglen );
		if( EP_STAT_ISOK( estat ) ) {
			printf(" Pass to verify signature ... \n" ); 
		} else printf("Fail to verify final ... \n" ); 

tfail2: 
		if( md != NULL ) ep_crypto_vrfy_free( md );
	}



fail0:

	if( ofilename     != NULL ) ep_mem_free( ofilename ) ;
	if( ofp			  != NULL ) fclose( ofp ); 
	if( token		  != NULL ) free_token( &token ); 

	if( device_cert   != NULL ) X509_free(     device_cert   );
	if( device_pubkey != NULL ) EVP_PKEY_free( device_pubkey );	
//	if( device_seckey != NULL ) EVP_PKEY_free( device_seckey );	

	if( rs_cert       != NULL ) X509_free(     rs_cert   );
	if( rs_pubkey     != NULL ) EVP_PKEY_free( rs_pubkey );	
	if( rs_seckey     != NULL ) EVP_PKEY_free( rs_seckey );	

	return m_ret; 
}
// hsmoon end 

