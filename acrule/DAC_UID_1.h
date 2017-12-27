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
** DAC_UID_1 : AC Rule type based User ID (DAC)  
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.01 
*/ 


// hsmoon start
#ifndef	_DAC_UID_1_H_
#define	_DAC_UID_1_H_

#include <stdio.h>
#include "ac_token.h"


#define		CBUF_LEN	32


// linked list (at first) 
typedef struct acrule_DAC_UID_R1 {
	char		add_del; // a=add, d=del, 
	char		ex_auth; // [read write deletagtion] bit  
						 // for optimization, add_del can be set 
						 //		the forth bit in this field
    char		gidbuf[CBUF_LEN+1];
    char		uidbuf[CBUF_LEN+1];
    char		didbuf[CBUF_LEN+1];

	// later optional 
	// allow time 

} DAC_UID_R1; 


// private data structure 
typedef struct DAC_rule1_inode {
	
	char						mode;		// a: allow, d:deny, b: allow+deny 
	char						ex_auth;	// a: allow, d:deny, b: allow+deny 
    char						id[CBUF_LEN+1];

	int							allow_count;
	int							deny_count; 
	int							both_count;


	struct DAC_rule1_inode		*child;  // lower level info pointer 
	struct DAC_rule1_inode		*next;   

} DAC_R1_node; 


//void print_DAC_UID_R1( DAC_UID_R1 ); 
void print_DAC_UID_R1( DAC_UID_R1, FILE * ); 
DAC_UID_R1* convert_buf_to_acrule( char *, DAC_UID_R1 *, int );
char*       convert_acrule_to_buf( char *, DAC_UID_R1,   int );
int			update_DAC_UID_1( int, void *, void **, char *);
bool		check_right_wbuf_on_DAC_UID_1( char, void *, int, void *);
bool		check_right_wrule_on_DAC_UID_1( char, DAC_UID_R1,  void *);
void		free_rules_on_DAC_UID_1( void * );
DAC_UID_R1*	convert_token_to_acrule(gdp_gclmd_t *, char, DAC_UID_R1 *);

#endif		//_DAC_UID_1_H_

	
