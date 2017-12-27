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
**  HS_HASHCHAIN ---  Higher function to support hash table 
**                   using linked list based seperate chaining.  
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.09.28 
*/ 

#ifndef	_HS_HASHCHAIN_H_
#define	_HS_HASHCHAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include "hs_list.h"


typedef struct hs_hashchaind_t {
	int			hash_size;
	int			(*cal_hash)( int, int, void * );
	hs_lnode**  htable; 

} hs_hashchain; 

hs_hashchain*	get_new_hashtable( int, int (*func)(int, int, void *) ) ;
void			print_hashtable( hs_hashchain *, FILE * );
void			free_hashtable(  hs_hashchain *, void(*func)(void *)  );
hs_lnode*		lookup_entry_in_htable( hs_hashchain *, size_t, char *); 
hs_lnode*		insert_entry_in_htable( hs_hashchain *, size_t, char *); 
hs_lnode*		delete_entry_in_htable( hs_hashchain *, size_t, char *); 


#endif // 	_HS_HASHCHAIN_H_

