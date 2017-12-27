
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
**  HS_ERR ---  utility to handle error     
** Written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr)
** last modified : 2016.12.30
*/



#include "hs_errno.h"


char* str_errinfo( int  errno ) 
{

	switch( errno ) 
	{
		case EX_OK:				return "OK";	
		case EX_USAGE:			return "command line usage error";	
		case EX_DATAERR:		return "data format error";		
		case EX_NOINPUT:		return "cannot open/use input";		
		case EX_NOUSER:			return "addressee unknown";
		case EX_NOHOST:			return "host name unknown";	
		case EX_UNAVAILABLE:	return "unvailable service/data";		
		case EX_SOFTWARE:		return "internal software error";	
		case EX_OSERR:			return "system error";	
		case EX_OSFILE:			return "Critical OS file missing";	
		case EX_CANTCREAT:		return "Can't create object";	
		case EX_IOERR:			return "Input/Output error";	
		case EX_TEMPFAIL:		return "Temp failure";	
		case EX_PROTOCOL:		return "Protocol error";	
		case EX_NOPERM:			return "Permission denied";	
		case EX_CONFIG:			return "Configuration error";	
		case EX_MEMERR:			return "Memory handling error";	
		case EX_INVALIDDATA:	return "Invalid data";		
		case EX_FILERENAME:		return "Fail to rename file";		
		case EX_ENDOFDATA:		return "End of Data";		
		case EX_FAILGETDATA:	return "Fail to get the requested data ";
		case EX_FILEHANDLE:		return "Fail to handle file (ex. open)";
		case EX_NOTFOUND:		return "Fail to find the data/service";
		case EX_CONFLICT:		return "Conflict";
		case EX_FAILURE:		return "Fail the request";
		case EX_NOTINIT:		return "Do not all initialization work";
		case EX_NOT_IMPL:		return "Not implemented yet";


		case EX_NULL_GCL:		return "No related GCL";
		case EX_NULL_SESSION:	return "No related Session";
		case EX_FAIL_SMSG:		return "Fail to process sent message on Session";
		case EX_FAIL_RMSG:		return "Fail to process rcv message on Session";
		case EX_TOKEN_ERR:		return "Fail to handle access token";
		case EX_KS_REQ_ERR:		return "Fail to request KS service";
		case EX_RCV_KEY_ERR:	return "Fail to process the received key";


		default:				return "Undefined error";
	}


}


