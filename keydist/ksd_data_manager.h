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
**  KSD_DATA_MANAGER - Manage the Key Service Data 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 

// hsmoon start
#ifndef	_KSD_DATA_MANAGER_H_
#define	_KSD_DATA_MANAGER_H_

#include <sys/queue.h>

#include <gdp/gdp.h>
#include <gdp/gdp_event.h>

#include <ep/ep_stat.h>
#include <hs/hs_list.h>
#include <ac/acrule.h>
#include <ac/ac_token.h>

//#include "kds_priv.h"
#include "ksd_key_data.h"




#define		CBUF_LEN	32


#define		NEED_INIT			0
#define		DOING_INIT			1	
#define		DONE_INIT			2	
#define		NEED_AC_SUBSCRIBE   3	
#define		WAIT_NEXT_ENTRY		4
#define		CHECK_BUF_ENTRY		5 // handled in recheck_acinfo

#define		WAIT_TIME_SEC		3
#define		NUM_WAIT_ENTRY		3



LIST_HEAD( kinfo_head, log_key_info );

typedef struct key_service_data {
	EP_THR_MUTEX		mutex;
	gdp_pname_t			dlog_pname;  // data_pname later
	gdp_pname_t			ac_pname; 
	gdp_pname_t			key_pname; 
	gdp_pname_t			wdid_pname; 
//	gdp_name_t			dlog_iname; 

	hs_lnode			*ac_data;
	hs_lnode			*key_data;

//	gdp_recno_t			latest_recno;	// necessary?
	EP_TIME_SPEC		latest_time;	
	// necessary? later usage for memory management 

	// LATER : for subscription 
	gdp_gcl_t			*gcl;
	gdp_req_t			*writer_sub;

	// NEED_INIT : NEED TO prepare ac_data, key_data : cmd_create 
	// DOING_INIT : after preparing ac_data, key_data. 
	//				need to load ac/key infos
	// DONE_INIT: after starting load ac/key info. 
	//	Although this stae is DONE_INIT, 
	//		DONE_INIT state in ac_data is also required.
	char				state; 
	bool				isDuplicated;
	bool				writer_check; 

	LIST_ENTRY(key_service_data)	loglist;
	// how to distinguish the writers of each log and readers. 
	//	when key is shared among multiple logs. 

	hs_lnode				*sessions;

} KSD_info; 

	
// LATER: Non one FIRSTrecnum.  
typedef struct ac_log_data {
	EP_THR_MUTEX		mutex;
//	gdp_pname_t			aclog_pname;  // ac_node->idname is aclog_pname. 
	gdp_name_t			aclog_iname; 
	int					ref_count;

	uint32_t			acr_type;
	gdp_recno_t			first_recn;
	gdp_recno_t			next_recn; 
	gdp_recno_t			last_recn; //  
	gdp_recno_t			reInit_recn;  //  

	// gdp_gcl_getnrecs(gcl) shows the last rec number at gcl_open time
	// NEED_INIT / NEED_SUBSCRIBE / DOING_INIT / DONE_INIT 
	char				state;	  
	time_t				ref_time;
	time_t				last_time;

	// Flags. 
	bool				isAvailable;

	// GCL to the AC Log 
	gdp_gcl_t			*gcl;


	// AC rules must be applied in order: the record num of log. 
	// However, the received logs can be out of order. 
	// To handle this, we stores the datum in buffer 
	//		& process them in correct turn
	// Buffered datum is sorted by recno. 
	struct gdp_datum		*head;

	void					*acrules;  // loaded AC infos
	// subscription list to notify the change of ac rule 
	struct kinfo_head		skeys;		


//
// LATER USAGE or CONSIDERATION
//
//	bool	reSubscription;		// for later usage (resubscription)
//	STAILQ_ENTRY(ac_log_data)		dolist;	 // LATER 		
// LATER modification from the exisiting implimentation aclist... 

} ACL_info; 


#define		ALL_KEY				1 // all key info is loaded in the memory 
#define		LATEST_KEY			2 // latest # keys are loaded in the memory  
#define		SKELETON_LATEST		3 // multiple key chain case 
								  // End key of each key chain  	

// ASSUMPTION
//	1. If the same key info can be shared among the logs 
//			with the same access policy. 

LIST_HEAD( ksd_head, key_service_data );

typedef struct log_key_info {
	EP_THR_MUTEX		mutex;

//	gdp_pname_t			klog_pname;  //key_node->idname is klog_pname. 
	gdp_name_t			klog_iname; 

	// LATER: rw_mode can be checked with the skey file 
	// LATER: OR rw_mode can be checked with creator metadata 
	// 5 mode : w (writer)  x (need to re-open for writer) 
	//		r( read / not subscrbe ) s ( read / subscribe )  
	//			t(need to re-open for read) 
	char				rw_mode;   // w :  r(or s)
	int					ref_count;

	// key generation parameter.. 
	// For simplicity, KSD provides some set of key gen algo & param. 
	//   The supported entry  can be identified by the index. 
	//		(cannot change the internal parameter values except time ) 
	//	currently support the 1 entry 	
	// LATER: Think to manage the function name & parameters info directly 
	//					(not index but direct info) 
	//				& to change the key gen algorithm dynamically 
	//					(not metadata but record)  
	int							kgen_inx;  
	KGEN_param					*param;
//	EP_TIME_SPEC				ctime;  // is necessary??? 
	int							revocation;
	LIST_ENTRY(log_key_info)	klist;


	gdp_recno_t					last_recn; 
	gdp_gcl_t					*gcl;
	gdp_gclmd_t					*gmd; // extracted info 


	// LATER : loaded KEY INFO  (change type: void -> key_record ) 
	int							count; 
	void						*keys;
	void						*k_tail; 
	RKEY_1						*sub_keys; // based on key chain gen algorithm. 
//	void						*sub_data; 
	// later consideration about sub data for log recno 
	// because this key can serve multiple logs 
	struct ksd_head				shlogs;


/* LATER VERSION with limited size log (buffer based implementation)
	gdp_gcl_t			*sub_gcl;
	gdp_name_t			sub_iname;  
	// or sub_(klog_pname) => sub_pname =>translate.. 
	void				*sub_latest;
*/


	// KEY can be used at the multiple logs. 
	// Because of key distribution to multiple logs, 
	// there can be a time difference among the real key refelction time 
	//   on each logs. so we store the key sequence number in the log entry 
	//	 with the modified gdp-writer.  
} LKEY_info; 



// 
// INIT routines
// 
// Initialization routine called before channel open to the GDP platform 
int		init_ks_info_before_chopen();
int		load_ks_info();
void	prepare_keylogdata( LKEY_info *, int );


// 
// EXIT routines 
// 
void	exit_ks_info_manager();
void	reflect_ac_shutdown( ACL_info * );
void	reflect_klog_shutdown( LKEY_info * );


// 
// Major Functions  
// 
EP_STAT		process_event( gdp_event_t *, bool, char );
int			get_ksd_handle( gdp_name_t, KSD_info **, bool, char *, int );
void		cancel_ksd_handle( KSD_info *, bool );
int			update_ac_node(  KSD_info *, char *, int );
int			load_ac_rules(   ACL_info * );
int			update_key_node( KSD_info *, char *, int, char );
int			load_key_data(   KSD_info *, int , LKEY_info *);
int			create_key_log(   KSD_info * );
int			request_key_latest( KSD_info *, gdp_datum_t *, gdp_gclmd_t * );
int			request_key_rec(    KSD_info *, gdp_datum_t *, gdp_gclmd_t * );
int			request_key_ts(     KSD_info *, gdp_datum_t *, gdp_gclmd_t * );
int			request_recno_for_ts(KSD_info *, gdp_datum_t * );
int			get_last_keyrec(     KSD_info * );
int			refresh_ks_info_file( ); 
void		notify_elapse_time( EP_TIME_SPEC );
void		notify_rule_change_toKey( ACL_info *, bool );
void		notify_change_info_toKS( KSD_info *, bool, RKEY_1 * );
EP_STAT		advertise_all_ksd(gdp_buf_t *, void *, int);
void		kds_advertise_one(gdp_name_t, int );
EP_STAT		kds_advertise_all(int );
void		check_info_state( EP_TIME_SPEC );
void		reflect_lost_channel( );


// 
// AC related functions 
// 
void	read_all_ac_data(gdp_gcl_t *, ACL_info *);
void	request_acgcl_read_async( ACL_info *, gdp_recno_t );
bool	update_ac_data( ACL_info *, gdp_datum_t *, bool);
bool	isAllowedAccess( KSD_info *, gdp_gclmd_t *, char  ); 

//
// INTERNAL functions (for test) 
// 
int  init_ks_info_from_file();
ACL_info* get_new_ac_data(size_t, char *);
void		free_aclinfo( void * );


//
// KEY related Functions  
// 
LKEY_info*	get_new_klinfo( size_t, char *, char, char ); 
void		free_lkinfo( void * );
void		free_light_lkinfo( void * );
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


#endif  //_KSD_DATA_MANAGER_H_



