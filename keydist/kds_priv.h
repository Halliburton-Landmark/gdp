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
**  KDS - Major Header file for key distribution service
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.12.13 
*/ 

#ifndef	_KDS_H_
#define	_KDS_H_

#include <gdp/gdp_stat.h>
#include <hs/hs_errno.h>


typedef struct key_gen_param_t  KGEN_param;

#define GDP_PDU_ASYNC_FLAG	0x20


struct sym_rkey {
	uint16_t		sym_algorithm;
	uint16_t		sym_mode;

	// LATER: MEMORY EFFICIENCY (malloc/ key length based mem pool?)
	uint8_t		sym_key[EVP_MAX_KEY_LENGTH];
	uint8_t		sym_iv[EVP_MAX_KEY_LENGTH];
};


typedef struct rKey_info_t {
	struct sym_rkey		rKey;
	int					cur_seqn;
	int					pre_seqn;
	EP_TIME_SPEC		key_time;
	gdp_gcl_t			*ks_gcl; 
} rKey_info;


#define	KDS_MODULE			7	
#define	KDS_STAT_NEW(sev, det)	EP_STAT_NEW( sev, \
									EP_REGISTRY_UCB, KDS_MODULE, det );

#define	KDS_STAT_WRONG_INPUT	KDS_STAT_NEW(ERROR, EX_INVALIDDATA)
#define	KDS_STAT_NOT_SUPPORT	KDS_STAT_NEW(ERROR, EX_NOT_IMPL)
#define	KDS_STAT_TOKEN_FAIL		KDS_STAT_NEW(ERROR, EX_TOKEN_ERR )
#define	KDS_STAT_FAIL_REQKS		KDS_STAT_NEW(ERROR, EX_KS_REQ_ERR )
#define	KDS_STAT_FAIL_RKEY		KDS_STAT_NEW(ERROR, EX_RCV_KEY_ERR )

#define	KDS_STAT_NULL_SESSION	KDS_STAT_NEW(ERROR, EX_NULL_SESSION)
#define	KDS_STAT_FAIL_SMSG		KDS_STAT_NEW(ERROR, EX_FAIL_SMSG)
#define	KDS_STAT_FAIL_RMSG		KDS_STAT_NEW(ERROR, EX_FAIL_RMSG)

#define	KDS_MUTEX_LORDER_SKEY	21



void		_kdc_gcl_freehandle(	gdp_gcl_t *, bool );

EP_STAT		_kdc_gcl_create(		gdp_name_t, gdp_name_t, gdp_gclmd_t *, 
											gdp_chan_t *, uint32_t, gdp_gcl_t ** );

EP_STAT		_kdc_gcl_open(			gdp_gcl_t *, int, char, 
											gdp_chan_t *, uint32_t );
EP_STAT		_kdc_gcl_close(			gdp_gcl_t *, char, 
											gdp_chan_t *, uint32_t );

EP_STAT		_kdc_gcl_append(		gdp_gcl_t *, gdp_datum_t *, 
											gdp_chan_t *, uint32_t, char  );
EP_STAT		_kdc_gcl_append_async(	gdp_gcl_t *, gdp_datum_t *, 
											gdp_event_cbfunc_t, void *, 
											gdp_chan_t *, uint32_t, char  );

EP_STAT		_kdc_gcl_read(			gdp_gcl_t *, gdp_datum_t *, 
											gdp_chan_t *, uint32_t, char  );
EP_STAT		_kdc_gcl_read_async(	gdp_gcl_t *, gdp_recno_t, 
											gdp_event_cbfunc_t, void *, 
											gdp_chan_t *, char  );

EP_STAT		_kdc_gcl_subscribe(		gdp_req_t *, // int32_t, EP_TIME_SPEC *,  
											gdp_event_cbfunc_t, void *,  char );


int			convert_klog_to_symkey( gdp_datum_t *, struct sym_rkey *, int * );

#endif  //_KDS_H_

