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
**  KSD_KEY_GEN - Generate the key: peridic, event-based or dual mode
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 

#ifndef	_KSD_KEY_GEN_H_
#define	_KSD_KEY_GEN_H_


#include "ksd_data_manager.h"



#define RAND_GEN1		1
#define RO_RSA_GEN1		11
#define RE_RSA_GEN1		21
#define UP_RSA_GEN1		31
#define RO_HASH_GEN1	111	
#define RE_HASH_GEN1	121
#define UP_HASH_GEN1	131


//
// External APIs
// 
RKEY_1* generate_next_key( LKEY_info *, EP_TIME_SPEC *, char );
RKEY_1* extract_prev_key( LKEY_info *, RKEY_1 *, gdp_recno_t );
RKEY_1*	alarm_rule_change( LKEY_info * );
char	isNecessaryNewKey( LKEY_info *, RKEY_1 *, EP_TIME_SPEC );

#endif  //_KSD_KEY_GEN_H_

