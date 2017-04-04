/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
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
** This provides the utility functions to handle this & that. 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2016.12.30 
*/ 


#include <stdio.h>
#include <ep/ep_util.h>


/*
** Print the concatenion of prefix and the hex value of string  
**		in the output stream. 
**
** 1'st argu: output stream to be printed 
** 2'nd argu: the prefix of string to be printed 
** 3'rd argu: the length of string to be printed as hex value 
** 4'th argu: the part of string to be printed as hex value  
*/
void 
ep_print_hexstr( FILE *outfp, char *prefix, int inlen, uint8_t *indata)
{
	int				ti;


	if( indata == NULL ) {
		// which type for error message?? 
//		ep_app_error("Unvalid input data");
		return ;
	}

	fprintf( outfp, "%s", prefix );
	for( ti=0; ti< inlen; ti++ ) {
		fprintf( outfp, "%02X", indata[ti] );
	}
	fprintf( outfp, "\n" );
}



/*
** Search the first occurrence of delimeter (3'rd argu) in input string. 
** Search range is <0, maxLen (2'nd argu)>
**
** 1'st argu: input string to be searched  
** 2'nd argu: This function searches the delimeter by the position 
**				indicated by 2'nd argu althogh input string is longer than 
**				the argument value. If input string is shorter than this 
**				argument value, this value is ignored.   
** 3'rd argu: the delimeter 
** rval		: the position of first occurrence in input string if string 
**				contains the delimeter. Otherwise, return -1. 
*/
int 
ep_strchr_pos( char *a_inString, int a_inMaxLen, char a_delim )
{
	int				ti=0;

	for( ti=0; ti<a_inMaxLen; ti++ ) {
		if( a_inString[ti] == a_delim ) return ti;
	}

	return -1;
}



/*
** Return the real number corresponding to the character number (hex) 
**
** 1'st argu: input character number 
** rval		: real number if the character belongs to hex character number. 
**				Otherwise, return -1;
*/
int
ep_chrtoint( char a_chr ) 
{
	if( a_chr >= '0' && a_chr <= '9' ) return (a_chr - '0');
	if( a_chr >= 'a' && a_chr <= 'f' ) return (a_chr - 'a' + 10);
	if( a_chr >= 'A' && a_chr <= 'F' ) return (a_chr - 'A' + 10);

	return -1;
}
