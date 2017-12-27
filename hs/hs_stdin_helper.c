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
** HS_STDIN_HELPER: the functions to support standard input
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.09.28 
*/ 

#include <stdio.h>
#include <string.h>

#include "hs_errno.h"
#include "hs_stdin_helper.h"



void ignore_stdinbuf() 
{
	char tmp;

	while( (tmp=getchar()) != '\n' ) {} 

}


// Only return the first input character (including newline)  from stdin 
// if there are any characters in buffer for stdin, 
//		remove the characters including newline in the buffer 
char getchar_ignore( ) 
{
	char		rVal, tmp;


	rVal =  getchar();

	if( rVal == '\n' ) return rVal;  // no input to support default option 

	while( (tmp=getchar()) != '\n' ) {} 

	return rVal;

}


// Only return the first input character (including newline)  from stdin 
// This function shows the message related with input.   
char scanchar_ignore( char *a_Prefix ) 
{
	printf( "%s: ", a_Prefix );
	return getchar_ignore(); 
}


// Only return the first input character (including newline)  from stdin 
// This function shows the message related with input.   
int scanInt_ignore( char *a_Prefix ) 
{
	int			rval = 0; 
	int			sval = -1;
	int			inx = 0;



	while( 1 ) {
		printf( "%s: ", a_Prefix );
		rval =  scanf( "%d", &sval); 

		if( rval == 0 ) { 
			printf("Wrong Input\n"); 
			ignore_stdinbuf(); 
		}  else break; 

		inx++;
	}

	// 7 
	// input case : 4fdf 
	ignore_stdinbuf(); 

	return sval;
}


// Only return the first input character (including newline)  from stdin 
// This function shows the message related with input.   
int scanInt_ignore_maxVal( char *a_Prefix, int a_Mval ) 
{
	int		scanVal = -1;

	scanVal = scanInt_ignore( a_Prefix );

	while( scanVal > a_Mval ) {
		printf("Input value must not be larger than %d\n", a_Mval );

		scanVal = scanInt_ignore( a_Prefix );
	} 

	return scanVal;
}



// Check whether input character has a valid value representing the second arguments. 
bool isValidChar( char a_Val, char* a_valids )
{
	int			ti; 
	int			max_num = strlen( a_valids );

	

	for( ti=0; ti<max_num; ti++ ) {
		if( a_Val == a_valids[ti] ) return true;
	}

	return false;

}

// Input the Nonzero characters from stdin  and Store the characters in a_outBuf 
// MUST: at least, a_outBuf[a_maxStrlen+1] 
//       With the long input, a_outBuf[a_maxStrlen] has a '\0'. 
void scan_nonzerochars( char *a_inMsg, char *a_outBuf, int a_maxStrlen )
{
	int			tsPos; 
	char		rVal;
	char		isValidNewLine = false; 

	printf( "[MUST]%s: ", a_inMsg );
	for( tsPos=0; tsPos<a_maxStrlen; ) {
		rVal = getchar();

		if(rVal != '\n' ) {
			a_outBuf[tsPos] = rVal;
			tsPos++;
		} else if( tsPos != 0 ) {
			isValidNewLine = true;
			break; 
		} else ; 
	}

	a_outBuf[tsPos] = '\0';

	if( isValidNewLine ) return;


	while( (rVal=getchar()) != '\n' ) {} 

}


// Input characters from stdin  and Store the characters in a_outBuf 
// If  input character is only newline, store the default values in a_outBuf 
// MUST: at least, a_outBuf[a_maxStrlen+1] 
//       With the long input, a_outBuf[a_maxStrlen] has a '\0'. 
int scan_chars_default( char *a_inMsg, char *a_outBuf, int a_maxStrlen, char *a_default )
{
	int			tsPos; 
	char		rVal;

	printf( "%s [defalut:%s]: ", a_inMsg, a_default );
	for( tsPos=0; tsPos<a_maxStrlen; ) {
		rVal = getchar();

		if(rVal != '\n' ) {
			a_outBuf[tsPos] = rVal;
			tsPos++;
		} else break; 
	}

	if( tsPos == 0 ) {
		if( strlen( a_default ) >  a_maxStrlen ) {
			printf( "[ERROR] Wrong Default value  (larger length: %zu, %d) \n", 
					strlen(a_default), a_maxStrlen );
			return EX_INVALIDDATA; 
		} 

		strcpy( a_outBuf, a_default );

	} else a_outBuf[tsPos] = '\0';

	if( tsPos < a_maxStrlen ) return EX_OK;

	while( (rVal=getchar()) != '\n' ) {} 

	return EX_OK;
}

