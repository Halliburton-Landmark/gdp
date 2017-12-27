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
** ACRULE_API : wrapper functions to support several ac rule types.  
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.15 
*/ 


#include <sysexits.h>
#include <ep/ep_app.h>
#include "acrule.h"
#include "DAC_UID_1.h"

/*
// Index	for each AC RULE TYPE 
#define		DAC_UID_BASE			0x00000001	
#define		ACR_TYPE_DAC_UID_1		DAC_UID_BASE 
#define		CAP_BASE				0x00000101	
*/

bool isSupportedACR( uint32_t a_inRtype )
{
	switch( a_inRtype ) 
	{
		case ACR_TYPE_DAC_UID_1:		return true;
		default:						return false;

	}

}


// first argu: rule type 
// second argu
// third argu: rule info managed in AC_info 
// forth argu: the length of fifth argu. 
// fifth argu:  
bool checkRequestedRight_wbuf( int a_rtype, char a_rRight, void *rules, 
												int a_dlen, void *a_data)
{

	switch( a_rtype ) 
	{
		case ACR_TYPE_DAC_UID_1:
				return check_right_wbuf_on_DAC_UID_1( a_rRight, 
												rules, a_dlen, a_data);
				
		default:
				ep_app_warn("[CHECK] Not supported Rule %08x", a_rtype );
				return false;
	}


}


bool checkRequestedRight_wtoken( int a_rtype, char a_rRight, void *rules, 
														gdp_gclmd_t *token )
{

	switch( a_rtype ) 
	{
		case ACR_TYPE_DAC_UID_1:
			{
				DAC_UID_R1			inRule, *tmpRule;

				tmpRule = convert_token_to_acrule( token, a_rRight, &inRule );
				if( tmpRule == NULL ) return false;

				return check_right_wrule_on_DAC_UID_1( a_rRight, 
														inRule, rules );
			}
				
		default:
				ep_app_warn("[CHECK] Not supported Rule %08x", a_rtype );
				return false;
	}


}

/*
char* convert_buf_from_token( int a_rtype, struct ac_token *token, int *oLen )
{

	switch( a_rtype ) 
	{
		case ACR_TYPE_DAC_UID_1:
				return convert_token_tobuf_on_DAC_UID_1( token, oLen );  
				
		default:
				ep_app_warn("[CHECK] Not supported Rule %08x", a_rtype );
				return NULL;
	}

}
*/


int reflect_ac_rule( int a_rtype, void **outRules, int a_dlen, void *a_data, 
						char *mode)
{
	int			exit_stat = EX_OK;


	switch( a_rtype ) 
	{
		case ACR_TYPE_DAC_UID_1:
				exit_stat = update_DAC_UID_1( a_dlen, a_data, outRules, mode);
				break;
		default:
				ep_app_warn("[CHECK] Not supported Rule %08x", a_rtype );
				exit_stat = EX_UNAVAILABLE; 
				break;
	}


	return exit_stat;
}


void free_ac_rule( int a_rtype, void *rules ) 
{
	switch( a_rtype ) 
	{
		case ACR_TYPE_DAC_UID_1:
				return free_rules_on_DAC_UID_1(  rules );
				
		default:
				ep_app_warn("[CHECK] Not supported Rule %08x", a_rtype );
				return ;
	}
}

