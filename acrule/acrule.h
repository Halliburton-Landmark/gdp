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
** AC RULE : wrapper definition to support several ac rule types.  
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.15 
*/ 


#ifndef	_ACRULE_H_
#define	_ACRULE_H_


#include  <stdint.h>
#include  <stdbool.h>
//#include "DAC_UID_1.h"
#include  "ac_token.h"


// Index	for each AC RULE TYPE 
#define		DAC_UID_BASE			0x00000001	
#define		ACR_TYPE_DAC_UID_1		DAC_UID_BASE 
#define		CAP_BASE				0x00000101	


bool	isSupportedACR(uint32_t);
int		reflect_ac_rule( int, void **, int, void *, char *);
bool	checkRequestedRight_wbuf(int, char, void *, int, void *);
bool	checkRequestedRight_wtoken(int, char, void *, gdp_gclmd_t *);
//char*	convert_buf_from_token(int, struct ac_token *, int *);
void	free_ac_rule( int, void *);

#endif		//_ACRULE_H_
