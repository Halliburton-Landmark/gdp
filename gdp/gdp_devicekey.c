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
** This provides the utility functions to handle the device certificate 
**		& its secret key. 
**
** Written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr)
** last modified : 2016.12.30
*/ 


#include <string.h>

#include <ep/ep.h>

#include "gdp_logdatakey.h"
#include "gdp_devicekey.h"


/*
** Check the Asymmetric key format from the file name 
*/
int check_key_format_from_filename(const char *a_fName)
{
	int			tlen;
	int			tval = EP_CRYPTO_KEYFORM_UNKNOWN;

	if( a_fName == NULL ) return tval;

	tlen =  strlen(a_fName);
	if( tlen < 5 ) return tval;

	if( strcmp( &a_fName[tlen-4], ".pem" ) == 0 ) 
			return EP_CRYPTO_KEYFORM_PEM;

	if( strcmp( &a_fName[tlen-4], ".der" ) == 0 ) 
			return EP_CRYPTO_KEYFORM_DER;

	return tval;

}


/*
** Check whether this device has the asymmetric key info in it. 
**     Asymmetric key info means device certificate & its secret key file. 
**
** 1'st argu: certificate full path 
** 2'nd argu: secret file full path 
**     If argument path is NULL, config value in gdp param file is used.
** rval     : true if both of them exist. otherwise, false. 
*/
bool 
check_device_asyminfo(const char *a_certPath, const char *a_secPath)
{
	const char					*t_certP; 	
	const char					*t_secP; 	


	t_certP = ep_adm_getstrparam("swarm.gdp.device.cert",   a_certPath); 	
	t_secP  = ep_adm_getstrparam("swarm.gdp.device.secret", a_secPath); 

	// Check the path info about asymmetric key info  
	if( t_certP == NULL ) return false;
	if( t_secP  == NULL ) return false;


	// Check whether info files really exist.
	// Check whether this program has the right to access the file. 
	if( access( t_certP, R_OK ) != 0 ) return false;
	if( access( t_secP,  R_OK ) != 0 ) return false;

	return true;
}


/*
** Read a device secret key 
*/ 
EP_CRYPTO_KEY *
get_gdp_device_skey_read(const char *a_certPath, const char *a_secPath)
{
	const char					*t_certP; 	
	const char					*t_secP; 	
	int							t_keyform = EP_CRYPTO_KEYFORM_UNKNOWN;
	EP_CRYPTO_KEY				*t_skey = NULL;



	t_certP = ep_adm_getstrparam("swarm.gdp.device.cert",   a_certPath); 	
	t_secP  = ep_adm_getstrparam("swarm.gdp.device.secret", a_secPath); 

	if( t_secP == NULL ) return NULL;


	//
	// Check the file format from the file name 
	//

	// check from the secret key file 
	t_keyform = check_key_format_from_filename( t_secP );
	if( t_keyform == EP_CRYPTO_KEYFORM_UNKNOWN ) {
		// check from the certificate  
		if( t_certP == NULL ) return NULL; 

		t_keyform = check_key_format_from_filename( t_certP );
	}
	if( t_keyform == EP_CRYPTO_KEYFORM_UNKNOWN ) return NULL;


	//
	// Read the secret key from file  
	//
	t_skey = ep_crypto_key_read_file( t_secP, t_keyform, EP_CRYPTO_F_SECRET );

	return t_skey;

}


