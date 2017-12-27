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
** HS_LIST: the higher functions to handle the list (singley linked list)
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.09.28 
*/ 

#ifndef	_HS_LIST_H_
#define	_HS_LIST_H_


#include <stdbool.h>


typedef struct hs_lnode_t {
	size_t			idlen;
	char			*idname;
	void			*nval;

	struct hs_lnode_t		*next;
} hs_lnode; 


hs_lnode* lookup_inlist( hs_lnode *,  size_t, char *);
hs_lnode* insert_inlist( hs_lnode **, size_t, char *);
hs_lnode* delete_inlist( hs_lnode **, size_t, char *);
hs_lnode* delete_node_inlist( hs_lnode **, hs_lnode *);

int free_list( hs_lnode *, void (*free_nodeval)(void *) );
int print_list( hs_lnode *, FILE *, bool );

#if 0 
// if there is already the requested node, return the node. 
// else make, insert and return the node
// We can change the char * idname to void *idname [later]
// cmp_nodeval return the value more than 0 if arg 1 > arg 2. 
// equal to 0 if arg 1 == arg 2, less than 0 if arg 1 < arg 2. 
hs_lnode* insert_inlist( hs_lnode **, int (*cmp_nodeval)(void *, void *), void * );
hs_lnode* lookup_inlist( hs_lnode *,  int (*cmp_nodeval)(void *, void *), void * );
#endif

#endif // 	_HS_LIST_H_

