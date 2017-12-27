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
** HASH CHAIN: the utility functions to handle the table of lists 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.09.28 
*/ 

#include <stdint.h>
#include <stdlib.h>
#include <ep/ep_mem.h>
#include "hs_hashchain.h"


char		dbgFlag = false;


hs_hashchain* get_new_hashtable( int a_tsize, int (*hfunc)(int, int, void * ) ) 
{
	hs_hashchain		*Htable = NULL;


	if( a_tsize < 0 ) a_tsize = 1;  

	Htable = (hs_hashchain *)ep_mem_malloc( sizeof( hs_hashchain ) ); 
	if( Htable == NULL ) {
		printf("[ERROR] Fail to get memory for Hash table 1\n" );
		return NULL;
	}

	Htable->htable = (hs_lnode **) ep_mem_zalloc( a_tsize * sizeof( hs_lnode * ) ); 
	if( Htable->htable == NULL ) {
		printf("[ERROR] Fail to get memory for Hash table 2\n" );
		ep_mem_free( Htable ); 
		return NULL;
	}

	Htable->hash_size = a_tsize;
	Htable->cal_hash = hfunc;

	return Htable;
}


void free_hashtable( hs_hashchain *a_Table, void (*free_nodeval)(void *) ) 
{
	int					ti;
	int					tnum;	


	if( dbgFlag ) printf(">>> Start to free hash table \n");

	for( ti=0; ti<a_Table->hash_size; ti++ )
	{
		tnum = free_list( a_Table->htable[ti], free_nodeval ); 
		if( dbgFlag ) printf("[%d] Start to free %d node in hash chain \n", ti, tnum );
	}

}


void print_hashtable( hs_hashchain *a_Table, FILE * afp )
{
	int					ti, tnum;



	printf("[INFO] hash table - size %d \n", a_Table->hash_size );

	for( ti=0, tnum=0; ti<a_Table->hash_size; ti++ ) 
	{
		if( a_Table->htable[ti] != NULL ) tnum++;	
	}
	printf("- Non empty table entry: %d \n", tnum );


	for( ti=0, tnum=0; ti<a_Table->hash_size; ti++ ) 
	{
		if( a_Table->htable[ti] != NULL ) 
			tnum += print_list( a_Table->htable[ti], stdout, true ); 	
	}
	printf("- Total entry: %d \n", tnum );
}


hs_lnode* lookup_entry_in_htable( hs_hashchain *a_hTable, size_t a_len, char *a_val )
{
	int				t_inx;
	hs_lnode*		curEntry; 

	t_inx = a_hTable->cal_hash( a_hTable->hash_size, (int)a_len,  (void *)a_val );

	if( t_inx < 0 ) {
		printf( "[ERROR] Non zero hash value : %d in hash table \n", t_inx );
		return NULL;
	}

	t_inx = t_inx % a_hTable->hash_size; 

	curEntry =  lookup_inlist( a_hTable->htable[t_inx], a_len, a_val ); 

	return curEntry;
}



hs_lnode* insert_entry_in_htable( hs_hashchain *a_hTable, size_t a_len, char *a_val )
{
	int				t_inx;
	hs_lnode*		curEntry; 

	t_inx = a_hTable->cal_hash( a_hTable->hash_size, (int)a_len,  (void *)a_val );

	if( t_inx < 0 ) {
		printf( "[ERROR] Non zero hash value : %d in hash table \n", t_inx );
		return NULL;
	}

	t_inx = t_inx % a_hTable->hash_size; 

	curEntry =  insert_inlist( &(a_hTable->htable[t_inx]), a_len, a_val ); 

	return curEntry;
}


hs_lnode* delete_entry_in_htable( hs_hashchain *a_hTable, size_t a_len, char *a_val )
{
	int				t_inx;
	hs_lnode*		curEntry; 

	t_inx = a_hTable->cal_hash( a_hTable->hash_size, (int)a_len,  (void *)a_val );

	if( t_inx < 0 ) {
		printf( "[ERROR] Non zero hash value : %d in hash table \n", t_inx );
		return NULL;
	}

	t_inx = t_inx % a_hTable->hash_size; 

	curEntry =  delete_inlist( &(a_hTable->htable[t_inx]), a_len, a_val ); 

	return curEntry;
}

