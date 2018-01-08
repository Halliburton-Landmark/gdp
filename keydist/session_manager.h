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
#include <ep/ep_stat.h>
#include <ep/ep_crypto.h>
#include <ac/ac_token.h>

#include "ksd_data_manager.h"


#define		INIT_SE			0x01
#define		SENT_AUTH		0x02


typedef struct gdp_session_info {
	EP_THR_MUTEX		mutex;

	// Flow Info (deprecated) 
	// Server side: src: node->idname, dst: gcl name 
	// client side: one gcl has only one session 
//	gdp_name_t			src_guid;   
//	gdp_name_t			dst_guid;   
//	int					flow_id;

	// Session Status. 
	char				state;	// 0 [init] 1[one side] 2[two side: final] 	 

	// Mode is used on key dsitribution server 
	//		& determined on cmd_create / cmd_open 
	char				mode;	// creator(c) / writer(w) / reader(r) (b:c|w) 

//	uint16_t			epoch;	// cipher suit index . later
//	uint32_t			seqn;	// later. 

	// Session Chipher info 
	EP_CRYPTO_CIPHER_CTX	*sc_ctx; 

	// ac_info is used in server side for later authorization. 
	gdp_gclmd_t			*ac_info;
	struct	sym_rkey	se_key;

	char				rlen;	
	char				flag; // Initiator / sent auth...   
	uint8_t				random1_r[16];
	uint8_t				random2_s[16];

} gdp_session; 



// mode S: data encryption based on session 
// mode I: No additional data protection based on session 
typedef struct gcl_apnd_info {
	char				mode;
	gdp_session			*curSession; 
	void				*a_data;	// additional data 
} sapnd_dt; 


// 
// API for key distribution server 
// LATER: modify with the common version based on client API  
void			close_allSession( KSD_info * );
void			close_relatedSession( KSD_info *, gdp_req_t * );
gdp_session*	lookup_session( KSD_info *, gdp_req_t * );
//int			 update_response_onsession( gdp_pdu_t *, gdp_session * ); 
gdp_session*	create_session_info( KSD_info *, struct ac_token *, 
									gdp_req_t * );
//int			 check_reqdbuf_onsession( gdp_pdu_t *, gdp_session * ); 

// 
// API for KDS client  
// 
// can be common LATER
gdp_session*	request_session( gdp_req_t *, char );  
int				response_session( gdp_req_t *, gdp_session * );  
gdp_session*	process_session_req( gdp_req_t *, char );  
int				update_smsg_onsession( gdp_pdu_t *, gdp_session *, 
											char, bool ); 
int				update_rmsg_onsession( gdp_pdu_t *, gdp_session *, char ); 
int				update_async_rmsg_onsession( gdp_gcl_t *, gdp_session *, 
												gdp_datum_t *, char ); 


// 
// common API  
// 
int			init_session_manager( );
void		exit_session_manager( ); 
void		free_session_apnddata( gdp_gcl_t * );


struct ac_token*	check_actoken( gdp_buf_t * );
bool				verify_actoken( struct ac_token * );
int					insert_mytoken( gdp_buf_t * );



#endif  //_SESSION_MANAGER_H_



