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
**  ACINFO_HELPER --- provide the utility for checking the managed access control info  
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.15 
*/ 


// hsmoon start
#include <stdio.h>
#include <string.h>

#include <ep/ep_dbg.h>
#include <ep/ep_mem.h>

#include <hs/hs_errno.h>
#include "acinfo_helper.h"


//#define		DBG_DETAIL   
#define		TDLV		32			// Debug level value for Detailed debugging message
//bool			dbgFlag = true;		// Debug flag for basic debugging message 
bool			dbgFlag = false;	// Debug flag for basic debugging message 


static EP_DBG	Dbg	= EP_DBG_INIT("kdist.achelper", "AC Info Manager for convenience" );

hs_lnode		*glist = NULL;
hs_lnode		*ulist = NULL;



// private functions
// Print the acf_line data structure info 
void print_acfline( struct acf_line *a_in )
{
	printf( "[INFO: AC] UID:%s(%zu) GID:%s(%zu) DID:%s(%zu) inx:%d, Log:%s(%zu) \n", 
				a_in->uid, strlen( a_in->uid ), a_in->gid, strlen( a_in->gid ), 
				a_in->did, strlen( a_in->did ), a_in->inx, 
				a_in->logname, strlen( a_in->logname ) );
}


// Return the data structure with the values for access control log 
struct acf_line* get_new_aclogname( char *a_giduid, char *a_dinfo )
{
	int					t_alen = 0;
	int					t_glen = 0;
	char				*tp;
	hs_lnode			*t_unode = NULL;	
	hs_lnode			*t_gnode = NULL;	
	struct	acf_line	*cdata = NULL;



	// 
	// Parsing the information (GID_UID) 
	//
	t_alen	= strlen( a_giduid );
	tp		= strchr( a_giduid, '_' );
	if( tp == NULL ) {
		fprintf( stderr, "Wrong Input format: %s \n", a_giduid );
		return NULL;
	}
	cdata = ep_mem_zalloc( sizeof( struct acf_line ) );

	tp++;
	strcpy( cdata->uid, tp ); 
	t_glen = t_alen - strlen( tp ) - 1; 
	strncpy( cdata->gid, a_giduid, t_glen );
	cdata->gid[t_glen] = '\0'; 
	strcpy( cdata->did, a_dinfo ); 



	// 
	// Find the user id info related with UID. 
	//
	t_unode = lookup_inlist( ulist, strlen(tp), cdata->uid ); 
	if( t_unode == NULL ) {
		t_gnode = lookup_inlist( glist, t_glen, cdata->gid );
		cdata->unode = NULL;
		cdata->gnode = t_gnode;

		if( t_gnode == NULL ) {
			if( dbgFlag ) printf("First obj for U(%s) / G(%s) \n", cdata->uid, cdata->gid ); 
			cdata->inx = 1;
		} else {
			if( dbgFlag ) printf("First U(%s) on G(%s) \n", cdata->uid, cdata->gid ); 
			cdata->inx = ((struct gid_info *)(t_gnode->nval))->sub_inx + 1;
		}

		snprintf( cdata->logname, CBUF_LEN, "%s_%d", cdata->gid, cdata->inx ); 

	} else {
		struct uid_info *tmp_val = (struct uid_info *)(t_unode->nval); 

		if( dbgFlag ) printf("Existing obj for %s \n", t_unode->idname );

		cdata->unode = t_unode;
		cdata->gnode = tmp_val->ginfo;

		// one uid has only one gid. 
		if( strcmp( cdata->gnode->idname, cdata->gid ) != 0 ) {
			fprintf( stderr, "Wrong UID/ GID info: %s/ %s \n", 
								cdata->uid, cdata->gid );
			ep_mem_free( cdata );
			return NULL;
		}

		// dinfo must be different??? [ LATER ] 
		t_gnode = tmp_val->ginfo;
		cdata->inx = ((struct gid_info *)(t_gnode->nval))->sub_inx + 1;

		snprintf( cdata->logname, CBUF_LEN, "%s_%d", cdata->gid, cdata->inx ); 
	}
	print_acfline( cdata );

	return cdata;
}


void update_ac_info_to_fp(FILE *afp, struct acf_line *aIn )
{
	fprintf( afp, "%s\t%s\t%s\t%d\t%s\n", 
			aIn->uid, aIn->gid, aIn->did, aIn->inx, aIn->logname);
}


int load_ac_info_from_fp(FILE *afp)
{
	int			tval;
	int			tinx;
	char		tuid[CBUF_LEN];
	char		tgid[CBUF_LEN];
	char		tdid[CBUF_LEN];
	char		tlog[CBUF_LEN];
	char		lbuf[1024];



	hs_lnode			*t_cur;
	hs_lnode			*cur_gnode;
	struct gid_info		*tg_cinx;
	struct uid_info		*tu_info;

#ifdef DBG_DETAIL
	int			dt_inx = 0;
#endif


	if( afp == NULL ) return 0;

	// Read the ac info from the file 
	while( feof( afp ) == 0 ) {

		if( fgets( lbuf, 1024, afp )  == NULL ) {
			ep_dbg_cprintf( Dbg, 3, "No existing AC Rule \n" ); 
			break;

		} 

		tval = sscanf( lbuf, "%s\t%s\t%s\t%d\t%s\n", tuid, tgid, tdid, &tinx, tlog);

		if( tval != 5 ) {
			ep_dbg_cprintf( Dbg, 3, "Fail to Read ac info at [%zu]%s\n", sizeof(lbuf), lbuf ); 
			return	EX_INVALIDDATA;
		}

#ifdef DBG_DETAIL
		dt_inx++;
		ep_dbg_cprintf( Dbg, TDLV, "%d> [%zu]%s [%zu]%s [%zu]%s %d [%zu]%s \n", 
						dt_inx, strlen(tuid), tuid, strlen(tgid), tgid, strlen(tdid), tdid,
						tinx, strlen(tlog), tlog );
#endif

		// TREAT Glist 
		t_cur = insert_inlist( &glist, strlen(tgid), tgid );
		if( t_cur == NULL ) {
			ep_dbg_cprintf( Dbg, 3, "Fail to allocate memory\n"); 
			return	EX_MEMERR;
		}

		tg_cinx = (struct gid_info *)(t_cur->nval) ;
		if( t_cur->nval == NULL ) {
			// new object  
#ifdef DBG_DETAIL
			ep_dbg_cprintf( Dbg, TDLV, "Insert Ginfo(%s) %d\n", tgid, tinx );
//			if( ep_dbg_test( Dbg, TDLV ) ) {
//				print_list( glist, stdout, false );
//			}
#endif

			tg_cinx = ep_mem_malloc( sizeof(struct gid_info) );
			// mem check LATER 
			tg_cinx->sub_inx = tinx;
			t_cur->nval = (void *)tg_cinx ;

		} else {
			// existing object 
#ifdef DBG_DETAIL
			ep_dbg_cprintf( Dbg, TDLV, "Update Ginfo(%s) value %d -> %d \n", 
						tgid, tg_cinx->sub_inx, tinx );
#endif
			tg_cinx->sub_inx = tinx; 	
		}
		cur_gnode = t_cur;


		// TREAT Ulist 
		t_cur = insert_inlist( &ulist, strlen(tuid), tuid );
		if( t_cur == NULL ) {
			ep_dbg_cprintf( Dbg, 3, "Fail to allocate memory\n"); 
			return	EX_MEMERR;
		}

		tu_info = (struct uid_info *)(t_cur->nval) ;
		if( t_cur->nval == NULL ) {
			// new object  
#ifdef DBG_DETAIL
			ep_dbg_cprintf( Dbg, TDLV, "Insert Uinfo(%s) %s\n", tuid, tlog );
			if( ep_dbg_test( Dbg, TDLV ) ) {
				print_list( ulist, stdout, false );
			}
#endif

			// mem check LATER 
			tu_info = ep_mem_malloc( sizeof(struct uid_info) );
			tu_info->ginfo = cur_gnode;
			tu_info->linfo = NULL;
			t_cur->nval = (void *)tu_info;

		} else {
			// existing object 
#ifdef DBG_DETAIL
			ep_dbg_cprintf( Dbg, TDLV, "Update Uinfo(%s) %s | ginfo(%s vs. %s)\n", tuid, tlog,
							tgid, tu_info->ginfo->idname );
#endif
			if( strcmp( tgid, tu_info->ginfo->idname ) != 0 ) {
				ep_dbg_cprintf( Dbg, 3, "Different Group name: %s vs %s \n",  
							tgid, tu_info->ginfo->idname );
				return	EX_INVALIDDATA;
			}
			
		}

		// update ac_info  
		int			t_accmp = 0;
		ac_info		*t_pre = NULL;
		ac_info		*t_cur = tu_info->linfo;
		while( t_cur != NULL ) {
			// find the same log name 
			t_accmp = strcmp( t_cur->logname, tlog );
//			printf("Cmp val[%s:%s]: %d\n", t_cur->logname, tlog, t_accmp );
			if( t_accmp > 0 ) {
				t_pre = t_cur;
				t_cur = t_cur->next;
			} else if( t_accmp == 0 ) {
				ep_dbg_cprintf( Dbg, 3, "Same log name: %s at %s_%s \n",  
							tlog, tuid, tgid );
				return	EX_INVALIDDATA;
			} else break;
		} 

		ac_info		*t_new	= ep_mem_malloc( sizeof(ac_info) );
		t_new->logname		= ep_mem_malloc( strlen(tlog) );
		t_new->dinfo		= ep_mem_malloc( strlen(tdid) );
		memcpy( t_new->logname, tlog, strlen(tlog) );	
		memcpy( t_new->dinfo,   tdid, strlen(tdid) );	

		if( tu_info->linfo == NULL ) {
			tu_info->linfo	= t_new;
			t_new->next		= NULL;

		} else if( t_pre == NULL ) {
			t_new->next		= tu_info->linfo;
			tu_info->linfo	= t_new;
		} else {
			t_pre->next = t_new;
			t_new->next	= t_cur;
		}

	}

	return EX_OK;
}


int show_uinfo(char *a_uname)
{
	int					t_inx   = 0;
	ac_info				*ta_cur	= NULL;
	hs_lnode			*t_user = NULL;
	struct uid_info		*t_info = NULL;

	t_user = lookup_inlist( ulist, strlen(a_uname), a_uname );
	if( t_user == NULL ) {
		printf("No user info about %s \n", a_uname);
		return -1;
	}

	t_info = (struct uid_info *)(t_user->nval);
	ta_cur = t_info->linfo;
	printf("[Uinfo] user:%s group:%s ", a_uname, 
					t_info->ginfo->idname);

	while( ta_cur != NULL ) {
		t_inx++;
		printf("\n %d] %s - %s", t_inx, 
				ta_cur->dinfo , ta_cur->logname);
		ta_cur = ta_cur->next;
	}

	if( t_inx == 0 ) printf(" 0 info \n");
	else printf("\n Total %d info \n", t_inx);

	return t_inx;
}


void show_ulist()
{
	int					tinx = 0;
	hs_lnode			*t_cur=ulist;

	while( t_cur != NULL ) {
		tinx++;
		printf("[%d] uname: %s \n", tinx, t_cur->idname);
		t_cur = t_cur->next;
	}
	printf("Total %d user \n", tinx);
}

void show_glist()
{
	int					tinx = 0;
	hs_lnode			*t_cur=glist;

	while( t_cur != NULL ) {
		tinx++;
		printf("[%d] gname: %s (%d) \n", tinx, t_cur->idname,
				((struct gid_info *)(t_cur->nval))->sub_inx );
		t_cur = t_cur->next;
	}
	printf("Total %d group \n", tinx);
}


void free_gidinfo( void *a_val ) 
{
	struct gid_info	*t_val = (struct gid_info *)a_val;
	//printf( " %d ", t_val->sub_inx );
	
	ep_mem_free( t_val );
}

void free_uidinfo( void *a_val ) 
{
	struct uid_info	*t_val = (struct uid_info *)a_val;
	ac_info			*t_cur;
	ac_info			*t_tmp;


	// Free ac_info (ginfo is free through free_gidinfo) 
	t_cur = t_val->linfo;
	while( t_cur != NULL ) {
//		printf( " [%s|%s] ", t_cur->logname, t_cur->dinfo );
		ep_mem_free( t_cur->logname );
		ep_mem_free( t_cur->dinfo );

		t_tmp = t_cur->next; 
		ep_mem_free( t_cur );
		t_cur = t_tmp;
	}

	ep_mem_free( t_val );
}

void exit_ac_info()
{
	// Free glist 
	free_list( glist, free_gidinfo );

	// Free ulist 
	free_list( ulist, free_uidinfo );
}
// hsmoon end



