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
**  SESSION_MANAGER - functions to support secure channel   
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.12.15 
*/ 

#ifndef	_SESSION_MANAGER_H_
#define	_SESSION_MANAGER_H_

#include <gdp/gdp.h>
#include <ep/ep_crypto.h>
#include <ac/ac_token.h>
#include "ksd_data_manager.h"


// This version: independent session handling.. 
// independent session handling??
// or cross with the gcl or ksd_info 
//	:searching time... memory size...  
//  : independent operation 
// 1. for access control & 
// 2. for  encrypted data
typedef struct gdp_session_info {
	EP_THR_MUTEX		mutex;

	// Flow Info 
//	gdp_name_t			src_guid; // node->idname  
//	gdp_name_t			dst_guid; // gcl name  to be targeted  
//	int					flow_id;

	// Session Status. 
	char				state; 
	char				mode;	// creator(c) / writer(w) / reader(r) (b:c|w) 
								// determined on cmd_create / cmd_open 
								// access token  

//	uint16_t			epoch;	// cipher suit index . later
//	uint32_t			seqn;	// later. 


	// Session Chipher info 
	EP_CRYPTO_CIPHER_CTX	*sc_ctx; 
//	EP_CRYPTO_CIPHER_CTX	*rc_ctx; 


	// For debugging. Check whether this is Really Necessary?
	// Other part's access info (cert . access token) 
	gdp_gclmd_t			*ac_info;
	struct	sym_rkey	se_key;

	uint8_t				random1_r[16];
	uint8_t				random2_s[16];

//	gdp_gcl_t			*gcl;

} gdp_session; 


int			init_session_manager( );
void		exit_session_manager( ); 
void		close_allSession( KSD_info * );

bool		verify_actoken( struct ac_token * );

gdp_session* create_session_info( KSD_info *, struct ac_token *, 
									gdp_req_t * );
gdp_session* lookup_session( KSD_info *, gdp_req_t * );
void		 close_relatedSession( KSD_info *, gdp_req_t * );

int			 update_response_onsession( gdp_pdu_t *, gdp_session * ); 
int			 check_reqdbuf_onsession( gdp_pdu_t *, gdp_session * ); 


#endif  //_SESSION_MANAGER_H_



