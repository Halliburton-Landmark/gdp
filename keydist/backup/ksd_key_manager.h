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
**  KSD_KEY_MANAGER - functions to support the data structure for key handling  
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 

#ifndef	_KSD_KEY_MANAGER_H_
#define	_KSD_KEY_MANAGER_H_



#include <sys/queue.h>
#include "ksd_key_data.h"
#include "ksd_data_manager.h"


#define		ALL_KEY				1 // all key info is loaded in the memory 
#define		LATEST_KEY			2 // latest # keys are loaded in the memory  
#define		SKELETON_LATEST		3 // multiple key chain case 
								  // End key of each key chain  	



//
// INIT routines 
// 
void	prepare_keylogdata( LKEY_info *, int );


//
// EXIT routines 
// 
void	reflect_klog_shutdown( LKEY_info * );


//
// Major Functions  
// 
LKEY_info*	get_new_klinfo( size_t, char *, char, char ); 
void		free_lkinfo( void * );
void		insert_key_log( RKEY_1 *, LKEY_info *, char );
void		update_rcv_key_datum( LKEY_info *, gdp_datum_t *, bool);
RKEY_1*		get_latest_key( LKEY_info * ); 
int			check_keylogname( LKEY_info * );
void		fill_dbuf_withKey( gdp_datum_t *, RKEY_1 *, int, int );
int			find_proper_keyrecnum( LKEY_info *, EP_TIME_SPEC *, RKEY_1 ** );
RKEY_1*		request_key_with_ts( LKEY_info *, EP_TIME_SPEC * ); 
RKEY_1*		get_key_wrecno( LKEY_info *, gdp_recno_t ); 
void		notify_rule_change_toKS( LKEY_info *, ACL_info *, bool );
int			store_new_generated_key( LKEY_info *, RKEY_1 * );


#endif  //_KSD_KEY_MANAGER_H_



