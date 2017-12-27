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
** AC_TOKEN: token published during registration 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.15 
*/ 


#ifndef __AC_TOKEN_H__
#define __AC_TOKEN_H__

#include <gdp/gdp_buf.h>
#include <ep/ep_crypto.h>
#include <hs/gdp_extension.h>


// Some of entries is useful. (some data can be NULL)
struct ac_token 
{
	gdp_gclmd_t		*info; // for quick implementation use this structure. 
	gdp_buf_t		*dbuf; // serialzed data of info. 
	uint8_t			*sigbuf;  // signature of dbuf
	int				dlen;
	int				siglen; 
	int				md_alg_id;  // current default algid.So not in dbuf  
}; 


struct ac_token*	make_new_token();
void				free_token( struct ac_token * );
int					add_token_field( struct ac_token *, gdp_gclmd_id_t, 
										size_t, const void *);
int					add_token_ctime_field( struct ac_token * );
int					add_token_pubkey_field( struct ac_token *, EP_CRYPTO_KEY * );
int					calculate_signature( struct ac_token *, EP_CRYPTO_KEY * );
int					write_actoken_to_file( struct ac_token *, FILE * );
struct ac_token*	read_token_from_file( FILE * );
struct ac_token*	read_token_from_buf( gdp_buf_t * );
int					write_token_tobuf( struct ac_token *, gdp_buf_t * );
int					write_token_fromFile_tobuf(char *, gdp_buf_t * );
void				print_actoken( struct ac_token *, FILE * );


#endif 
