

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
**  HS_ERRNO ---  define the private error number with sysexits   
** Written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr)
** last modified : 2016.12.30
*/

#ifndef	_HS_ERRNO_H_
#define	_HS_ERRNO_H_

#include <sysexits.h>


#define	EX_HS_BASE			128		
#define	EX_HS_BASE2			256		
#define	EX_MEMERR			EX_HS_BASE+1		
#define	EX_INVALIDDATA		EX_HS_BASE+2		
#define	EX_FILERENAME		EX_HS_BASE+3		
#define	EX_ENDOFDATA		EX_HS_BASE+4		
#define	EX_FAILGETDATA		EX_HS_BASE+5		
#define	EX_FILEHANDLE		EX_HS_BASE+6		
#define	EX_NOTFOUND			EX_HS_BASE+7		
#define	EX_CONFLICT			EX_HS_BASE+8		
#define	EX_FAILURE			EX_HS_BASE+9		
#define	EX_NOTINIT			EX_HS_BASE+10	
#define	EX_NOT_IMPL			EX_HS_BASE+11


#define	EX_NULL_GCL			EX_HS_BASE2		
#define	EX_NULL_SESSION		EX_HS_BASE2+1		
#define	EX_FAIL_SMSG		EX_HS_BASE2+2		
#define	EX_FAIL_RMSG		EX_HS_BASE2+3		

#define	EX_TOKEN_ERR		EX_HS_BASE2+4
#define	EX_KS_REQ_ERR		EX_HS_BASE2+5
#define	EX_RCV_KEY_ERR		EX_HS_BASE2+6

char* str_errinfo( int );

#endif	//	_HS_ERRNO_H_

