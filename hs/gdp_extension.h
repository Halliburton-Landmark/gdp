/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**  Copyright (c) 2015-2017, Electronics and Telecommunications
**  Research Institute (ETRI). All rights reserved.
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
**  GDP_EXTENSION --- fundtions to extend the gdp protocol  
** Written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr)
** last modified : 2016.12.30
*/

#ifndef __GDP_EXTENSION_H__
#define __GDP_EXTENSION_H__


#include <gdp/gdp.h>

#ifndef GDP_MIN_KEY_LEN
#define GDP_MIN_KEY_LEN        1024
#endif // GDP_MIN_KEY_LEN


// for later usage... 
#define GDP_GCLMD_TYP		0x00545950   // TYP : type info : AC / KL 
#define GDP_GCLMD_ACTYPE	0x00525459	 // RTY : RULE TYPE 	
#define GDP_GCLMD_SUBKLOG   0x00534B4C	 // SKL : Sub Key Log   
#define GDP_GCLMD_FINX		0x00494E58	 // INX   
#define GDP_GCLMD_ACLOG		0x00414C4E	 // ALN    
#define GDP_GCLMD_DLOG		0x00444C4E	 // DLN   
#define GDP_GCLMD_KLOG		0x004B4C4E	 // KLN    
#define GDP_GCLMD_KGEN		0x004B4754	 // KGT    
#define GDP_GCLMD_WDID		0x00574944	 // WID    


// for access token (additional) 
#define GDP_ACT_ISSUER		0x00495355	 // ISU    
#define GDP_ACT_SUBJECT		0x00534250	 // SBJ  (optional: deleted)    
#define GDP_ACT_UID			0x00554944	 // UID    
#define GDP_ACT_GID			0x00474944	 // GID    
#define GDP_ACT_DID			0x00444944	 // DID    
#define GDP_ACT_GUID		0x00475549	 // GUI    
#define GDP_ACT_CCAP		0x00435243	 // CRC    
#define GDP_ACT_CTIME		GDP_GCLMD_CTIME    
#define GDP_ACT_PUBKEY		GDP_GCLMD_PUBKEY    



/*
#define GDP_GCLMD_TSPARM1   0x00545331	 // TS1 : short period param   
#define GDP_GCLMD_TLPARM1   0x00544C31	 // TL1 : long  period param   
*/

#endif //__GDP_EXTENSION_H__
