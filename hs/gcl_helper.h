/* vim: set ai sw=4 sts=4 ts=4 : */

/*
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
**  GCL_HELPER --- common utility functions used in handling gcl 
** Written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr)
** last modified : 2016.12.30
*/


#ifndef __GCL_HELPER_H__
#define __GCL_HELPER_H__

#include <gdp/gdp.h>
#include <gdp/gdp_gclmd.h>

#include <ep/ep_crypto.h>

#include "gdp_extension.h"



void add_time_in_gclmd( gdp_gclmd_t * );
void add_creator_in_gclmd( gdp_gclmd_t * );
int  add_pubkey_in_gclmd( gdp_gclmd_t *, EP_CRYPTO_KEY *, int, int, int );
EP_CRYPTO_KEY* make_new_asym_key( int, int * );
char* write_secret_key( EP_CRYPTO_KEY *, const char *, int );
char* merge_fileName( const char *, size_t, gdp_pname_t );  
char* rename_secret_key( gdp_gcl_t *, const char *, const char * );  
const char* get_keydir( const char * );
const char* select_logd_name( const char * );

EP_STAT signkey_cb( gdp_name_t, void *, EP_CRYPTO_KEY **);


EP_STAT gdp_gclmd_find_uint( gdp_gclmd_t *, gdp_gclmd_id_t, uint32_t * );
int gdp_gclmd_find_wLen( gdp_gclmd_t *, gdp_gclmd_id_t, int *, char ** );
int find_gdp_gclmd_uint( gdp_gcl_t *, gdp_gclmd_id_t, uint32_t * );
int find_gdp_gclmd_char( gdp_gclmd_t *, gdp_gclmd_id_t, char * );

#endif // __GCL_HELPER_H__
