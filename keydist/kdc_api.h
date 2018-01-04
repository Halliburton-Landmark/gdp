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
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.12.
*/ 


#ifndef __KDC_API_H__
#define __KDC_API_H__

#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>
#include <gdp/gdp_pdu.h>


#include "kds_priv.h"
#include "session_manager.h"


#define		KS_SERVICE_START		1
#define		KS_SERVICE_CHANGE		2	
#define		KS_SERVICE_CANCLE		3	

#define		KS_MODE_GEN_DIST		4	
#define		KS_MODE_ONLY_DIST		5	


void			kdc_init( );
void			kdc_exit( );

EP_STAT			kdc_get_symkey(		gdp_gcl_t *, struct sym_rkey *, 
										int *, int *, EP_TIME_SPEC * );
EP_STAT			kdc_get_latestKey(	gdp_gcl_t *, struct sym_rkey *, 
										int *, int *, EP_TIME_SPEC * );
EP_STAT			kdc_get_symkey_ontime(	gdp_gcl_t *, struct sym_rkey *, 
												const char *, int *, 
												int *, EP_TIME_SPEC * );


gdp_gclmd_t*	set_ksinfo( gdp_pname_t, gdp_pname_t, gdp_pname_t , 
												char, gdp_pname_t );

EP_STAT			kds_service_request( gdp_pname_t, char, gdp_gclmd_t *, 
												KGEN_param *, char  );

EP_STAT			kdc_gcl_open( gdp_name_t, gdp_iomode_t, 
											gdp_gcl_t **, char );
EP_STAT			kdc_gcl_close( gdp_gcl_t *, char );

int				kdc_cb_preprocessor( gdp_event_t * );

EP_STAT			kdc_gcl_append( gdp_gcl_t *, gdp_datum_t *, char );
EP_STAT			kdc_gcl_append_async( gdp_gcl_t *, gdp_datum_t *,
										gdp_event_cbfunc_t, void *, char );


EP_STAT			kdc_gcl_read(		gdp_gcl_t *, gdp_recno_t, 
											gdp_datum_t *, char );
EP_STAT			kdc_gcl_read_ts(	gdp_gcl_t *, EP_TIME_SPEC *, 
											gdp_datum_t *, char );
EP_STAT			kdc_gcl_read_async(	gdp_gcl_t *, gdp_recno_t ,
										gdp_event_cbfunc_t, void *, char );

EP_STAT			kdc_gcl_subscribe(		gdp_gcl_t *, gdp_recno_t ,
											int32_t, EP_TIME_SPEC *, 
											gdp_event_cbfunc_t, void *, char );
EP_STAT			kdc_gcl_subscribe_ts(		gdp_gcl_t *, EP_TIME_SPEC * ,
											int32_t, EP_TIME_SPEC *, 
											gdp_event_cbfunc_t, void *, char );

// LATER : get_metadata  

#define		MODE_LOGKEY_TEST		0	
#define		MODE_LOGKEY_SERVICE		1		



/*
// In test mode, this struct is used as argument for append filter. 
// Use the wrapper data structure to prepare the extended info. 
typedef struct apnd_arg_st {
	struct sym_rkey		*key_info; 
} apnd_data; 
*/

void	free_logkeymodule( );
void	notify_keychange( );

EP_STAT append_filter_for_kdc(gdp_datum_t *a_logEntry, void *a_info);
EP_STAT read_filter_for_kdc(gdp_datum_t *a_logEntry, void *a_info);

#endif

