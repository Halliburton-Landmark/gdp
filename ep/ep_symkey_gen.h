/* vim: set ai sw=4 sts=4 ts=4 : */

/*
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
** This provides the utility functions to handle the key. 
** Most of functions in this file can be merged into ep_crypto_key.c 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2016.12.09 
*/ 


#ifndef _EP_SYMKEY_GEN_H_
#define _EP_SYMKEY_GEN_H_


#include <stdint.h>

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <ep/ep_crypto.h>


#define		EP_CRYPT_NON		1
#define		EP_CRYPT_SYM		2	
#define		EP_CRYPT_ASYM		3 


int			ep_crypto_mode_byname(const char *instr);
const char*	ep_crypto_mode_name( int inmode );

const EVP_CIPHER* tmp_evp_cipher_type( uint32_t cipher );

X509*	ep_x509_cert_read_file( const char *cfname );

int ep_compute_sharedkey_onEC(EVP_PKEY *my_secret, EVP_PKEY *other_public, 
				int reqLen, char *outKey );

EP_CRYPTO_KEY*	get_pubkey_from_cert_file(const char *a_certFName);

#endif //  _EP_SYMKEY_GEN_H_
