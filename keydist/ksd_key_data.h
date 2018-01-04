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

#ifndef	_KSD_KEY_DATA_H_
#define	_KSD_KEY_DATA_H_

#include <errno.h>

#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>

#include <ep/ep_mem.h>
#include <ep/ep_time.h>
#include <ep/ep_crypto.h>

#include <hs/hs_errno.h>
#include <hs/hs_symkey_gen.h>

#include "kds_priv.h"


#define BACKWARD_IN		0
#define RANDOM_IN		1	
#define FORWARD_IN		2




// The data used to generate key in the key gen/distribution service 
// must be managed securely. 
// These data need to be stored in the secure area such as
// secure container. 
// These data need to be encrypted in the container. 
// The secure access to the data is also required. 
// However, in this first draft,  
// these data are encrypted with the fixed encryption key 
// and then store in the local file. 
// Currently, support only AES.. / key rotation... 
//typedef struct key_gen_param_t {
struct key_gen_param_t {
	int		sym_key_alg;
	int		sym_key_mode;
	int		keygenfunID;    // necessary? 
	int		security_level;	// necessary? ex> with low value, IV can be ignored 
							// all zero or same IV (first IV is only delivered)
							// it can affect to pick up the key gen function. 	
	// salt value is changed every key  generation time
	// IV is also changed every key generation

	// time parameter.. (0 means not to use the parameter)
	// If you want to use simple periodically chaned key with only one hash chain
	//		set max_key_period = period with the other parameters set to 0. 
	// If you want to use simple periodically chaned key with several key chain
	//		set max_key_period and chain_length with the other parameters set to 0. 
	// all 0 (w/o chain_length) means the key chang at every user revocation. 
	// Key is not changed at least during this time value. (ex> 1min / 10 min)  
	int		min_key_period;   
	// Key must be changed within this time value (ex> 1 hour /  1day)
	int		max_key_period;  
	// if the number of user revocation exceeds this value, 
	//		change the encryption key regardless of min_key_period 
	int		change_thval1;   
	// if the number of user revocation exceeds this value, 
	//		change the encryption key chain.. 
//	int		change_thval2;   
	int		chain_length;	// max key chain length. 

} ;
//} KGEN_param;



typedef struct key_raw_1 {
	// current version, can be replaced with record num of key log	
	// But later buffer based implementation need this value.. 
	int		seqn;
	int		pre_seqn;

	EP_TIME_SPEC	ctime; 
	// current version, can be replaced with key log entry time info


	// LATER: MEMORY EFFICIENCY (malloc/ key length based mem pool?)
	uint8_t		sym_key[EVP_MAX_KEY_LENGTH];
	uint8_t		sym_iv[EVP_MAX_KEY_LENGTH];
	

	// LATER: consider the efficient data structure to manage these data
	struct key_raw_1	*next;
} RKEY_1;


struct hdr_rkey1 {
	// no meanning in SHELETON_LATEST mode 
	int					child_num;	// necessary? 
	EP_TIME_SPEC		latest_time; // latest read time

	RKEY_1				*childh; 
	RKEY_1				*childt; 
	struct hdr_rkey1	*next;	// lasted key is inserted in header. 
	struct hdr_rkey1	*pre;	// lasted key is inserted in header. 
};

struct hdr_rkey2 {
	EP_TIME_SPEC		latest_time; // latest read time

	RKEY_1				*key_entry; 
	struct hdr_rkey2	*next;	// lasted key is inserted in header. 
	struct hdr_rkey2	*pre;	// lasted key is inserted in header. 
};


/*
// For test 
int get_kgen_fname(int, char *, int );
*/


//
// External APIs
// 
void		exit_key_data( );


//
//  kgen_param functions
// 
KGEN_param* load_kgen_param( int, char ); 
int			store_kgen_param( int, KGEN_param *);
KGEN_param*	convert_to_kgen_param( unsigned char * ); 
void		put_kgenparam_to_buf( KGEN_param *, gdp_datum_t *);
KGEN_param* get_new_kgen_param( char *, char *, int );


//
// RKEY functions
// 
RKEY_1*		convert_klog_to_RKEY( gdp_datum_t *, char *, char * ); 
RKEY_1*		get_new_RKEY1( ); 
void		return_RKEY1( RKEY_1 * );
void		return_RKEY1s( RKEY_1 * );


struct hdr_rkey1*	get_new_rkeyHdr1( RKEY_1 * );
struct hdr_rkey2*	get_new_rkeyHdr2( RKEY_1 * );
void				free_rkeyHdr1_keys( struct hdr_rkey1 * );
void				free_rkeyHdr2_keys( struct hdr_rkey2 * );

#endif  //_KSD_KEY_DATA_H_

