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
** last modified : 2016.12.31
*/ 


#ifndef	_GDP_LOGDATAKEY_H_
#define	_GDP_LOGDATAKEY_H_

//#include <time.h>
#include <gdp/gdp.h>
#include <ep/ep_stat.h>


#define		MODE_LOGKEY_TEST		0	
#define		MODE_LOGKEY_SERVICE		1		



// this struct can be located in libep if necessary. 
typedef struct logdata_enckey_info {
	// LATER: key can be periodically changed. 
	// It is necessary to check whether this key is valid on the related 
	// log data. According to the mechanism for this, 
	// the type of variable can be changed. 
	// For this, some functions handling this structure can be changed. 
//	EP_TIME_SPEC		valid_time;	

	int					sym_key_alg;	// for debugging (finally deleted)
	int					sym_key_mode;	// for debugging (finally deleted)
	int					salt_len;
	uint8_t				salt[8];

	// for debugging (finally deleted) ?? 
	uint8_t				sym_key[EVP_MAX_KEY_LENGTH];	
	uint8_t				sym_iv[EVP_MAX_KEY_LENGTH];	

	// need to check whether this covers the key & iv 
	EP_CRYPTO_CIPHER_CTX	*key_ctx;
} symkey_info;


// this struct is used as argument for append filter. 
// Use the wrapper data structure to prepare the extended info. 
typedef struct apnd_arg_st {
	symkey_info			*key_info; 

} apnd_data; 

bool check_log_key(const char a_rMode, 
					bool (*key_service)(gdp_name_t, void *), 
					gdp_name_t a_logName, void *a_oargu);
bool check_device_for_log_encdec_t1(const char *a_certP, 
									const char *a_secP);

symkey_info* load_log_enc_key_t1(symkey_info *a_curkey, void *a_oargu);

void free_logkeymodule(symkey_info *inKey);

EP_STAT append_filter_for_t1(gdp_datum_t *a_logEntry, void *a_info);
EP_STAT read_filter_for_t1(gdp_datum_t *a_logEntry, void *a_info);

#endif
