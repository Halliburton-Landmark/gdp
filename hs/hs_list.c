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


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ep/ep_mem.h>

#include "hs_list.h"


int free_list( hs_lnode *a_hdr, void (*free_nodeval)(void *)  )
{
	hs_lnode		*n_cur = a_hdr;
	hs_lnode		*n_tmp;

	int				tinx=0;

	while( n_cur != NULL ) {
//		printf( "%d] %s ", tinx, n_cur->idname );

		ep_mem_free( n_cur->idname );
		free_nodeval( n_cur->nval );
		n_tmp = n_cur->next;
		ep_mem_free( n_cur );
		n_cur = n_tmp;

//		printf( " \n"); 
		tinx++;
	}

	return tinx;
}


int print_list( hs_lnode *a_hdr , FILE *afp, bool aDetail )
{
	int				count = 0;
	hs_lnode		*n_cur = a_hdr;

	if( n_cur == NULL ) fprintf( afp, "Empty list \n");
	while( n_cur != NULL ) {
		fprintf( afp, "%s ", n_cur->idname ); 
		n_cur = n_cur->next;
		count++;
	}
	if( aDetail ) fprintf( afp, " : %d \n", count ); 
	else fprintf( afp, "\n" ); 

	return count;
}


hs_lnode* lookup_inlist( hs_lnode *a_hdr, size_t a_len, char *a_val )
{
	hs_lnode		*t_cur = a_hdr;
	int				t_val; 


	while( t_cur != NULL ) {
		t_val = strncmp( t_cur->idname, a_val, a_len );

		if( t_val > 0 ) {
			t_cur = t_cur->next;
		} else if( t_val == 0 ) {
			if( t_cur->idlen > a_len ) {
				t_cur = t_cur->next;
			} else if ( t_cur->idlen == a_len ) return t_cur;
			else return NULL;

		} else return NULL;

	}
	return NULL;
}


hs_lnode* insert_inlist( hs_lnode **a_hdr, size_t a_len, char *a_val )
{
	hs_lnode		*t_cur = *a_hdr;
	hs_lnode		*t_new = NULL;
	hs_lnode		*t_pre = NULL;
	int				t_val; 


	while( t_cur != NULL ) {
		t_val = strncmp( t_cur->idname, a_val, a_len );
//			printf(" t_val[%zu:%s]-[%zu:%s] : %d\n", t_cur->idlen, t_cur->idname, 
//						a_len, a_val, t_val );

		if( t_val > 0 ) {
			t_pre = t_cur;
			t_cur = t_cur->next;
		} else if( t_val == 0 ) {
			if( t_cur->idlen > a_len ) {
				t_pre = t_cur ;
				t_cur = t_cur->next;
			} else if ( t_cur->idlen == a_len ) return t_cur;
			else break;

		} else break;

	}


	t_new = ep_mem_malloc( sizeof(hs_lnode) );
	if( t_new == NULL ) {
		fprintf( stderr, "Fail to memory alloc in insert_inlist\n");
		return NULL;
	}

	t_new->idlen = a_len;
	t_new->idname = ep_mem_malloc( a_len );
	if( t_new->idname == NULL ) {
		fprintf( stderr, "Fail to memory alloc in insert_inlist\n");
		ep_mem_free( t_new );
		return NULL;
	}
	memcpy( t_new->idname, a_val, a_len);
	t_new->nval = NULL;

	if( *a_hdr == NULL ) {
		*a_hdr = t_new;
		t_new->next = NULL;
		//printf("first entry: %s \n", t_new->idname );

	} else if( t_pre == NULL ) {
		t_new->next = *a_hdr;
		*a_hdr = t_new;
	} else {
		t_new->next = t_pre->next;
		t_pre->next = t_new;
	}

	return t_new;
 }


hs_lnode* delete_node_inlist( hs_lnode **a_hdr, hs_lnode *a_delnode ) 
{
	hs_lnode		*t_cur = *a_hdr; 
	hs_lnode		*t_pre = NULL;

	while( t_cur != NULL ) {
		if( t_cur == a_delnode )  break; 

		t_pre = t_cur;
		t_cur = t_cur->next;
	}

	if( t_cur != a_delnode ) return NULL;

	if( t_pre == NULL ) {
		*a_hdr = (*a_hdr)->next;	
	} else {
		t_pre->next = t_cur->next;
	}

	t_cur->next = NULL;

	return t_cur;

}


hs_lnode* delete_inlist( hs_lnode **a_hdr, size_t a_len, char *a_val )
{
	hs_lnode		*t_cur = *a_hdr;
	hs_lnode		*t_pre = NULL;
	int				t_val; 


	while( t_cur != NULL ) {
		t_val = strncmp( t_cur->idname, a_val, a_len );
//			printf(" t_val[%zu:%s]-[%zu:%s] : %d\n", t_cur->idlen, t_cur->idname, 
//						a_len, a_val, t_val );

		if( t_val > 0 ) {
			t_pre = t_cur;
			t_cur = t_cur->next;
		} else if( t_val == 0 ) {
			if( t_cur->idlen > a_len ) {
				t_pre = t_cur ;
				t_cur = t_cur->next;
			} else if ( t_cur->idlen == a_len ) {
				// delete entry in the list. 
				if( t_pre == NULL ) {
					*a_hdr = (*a_hdr)->next;	
				} else {
					t_pre->next = t_cur->next;
				}
				t_cur->next = NULL;

				return t_cur;
			} else break;

		} else break;

	}

	return NULL;

}


#if 0
hs_lnode* lookup_inlist( hs_lnode *a_hdr, int (*cmp_nodeval)(void *, void *), void *a_val  )
{
	hs_lnode		*t_cur = a_hdr;
	int				t_val; 


	while( t_cur != NULL ) {
		t_val = cmp_nodeval( t_cur->nval, a_val );

		if( t_val > 0 ) {
			t_cur = t_cur->next;
		} else if( t_val == 0 ) {
			return t_cur;
		} else return NULL;

	}

	return NULL;
}
#endif

