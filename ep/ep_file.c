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
** This provides the utility functions to handle the file. 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2016.12.02 
*/ 



#include <stdio.h>
#include <stdlib.h>

/*
#include <string.h>
#include <strings.h>
#include <ep/ep.h>
#include <ep/ep_crypto.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
*/

#include <ep/ep_file.h>


// return the file size 
int ep_get_fileSize(const char* inFname) 
{
	long			t_epos = 0;
	FILE			*t_fp = NULL;

	t_fp = fopen( inFname, "rb" );
	if( t_fp == NULL ) return -1;

	// Move to the end of the file
	fseek( t_fp, 0L, SEEK_END );

	// Get the size of the file 
	t_epos = ftell( t_fp );

	fclose( t_fp );

	return t_epos;
}



// put the total file contents into the buffer indicated by 2'nd argu 
// return the file size 
int ep_get_fileContents(const char* inFname, char **outBuf) 
{
	long			t_epos = 0;
	FILE			*t_fp = NULL;

	t_fp = fopen( inFname, "rb" );
	if( t_fp == NULL ) return -1;

	// Move to the end of the file
	fseek( t_fp, 0L, SEEK_END );

	// Get the size of the file 
	t_epos = ftell( t_fp );


	if( t_epos <= 0 ) {
		fclose( t_fp );
		return t_epos;
	}

	*outBuf = malloc( sizeof(char) * (t_epos + 1) );


	// Move to the start of the file. 
	if( fseek( t_fp, 0L, SEEK_SET ) == -1 ) {
		fclose( t_fp );
	
		t_fp = fopen( inFname, "rb" );
		if( t_fp == NULL ) return -1;
	}


	// put the contents into buffer 
	size_t	t_rlen = fread( (void *)(*outBuf), sizeof(char), t_epos, t_fp );	

	if( t_rlen != t_epos ) {
		printf("ERROR] read bytes doesn't match R(%lu) I(%ld) \n", t_rlen, t_epos );
	}
	outBuf[t_epos] = '\0';

	fclose( t_fp );

	return t_epos;
}

