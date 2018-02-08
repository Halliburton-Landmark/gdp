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


#include <stdio.h>
#include <string.h>
#include <gdp/gdp.h>
#include <ep/ep_app.h>
#include <ep/ep_hexdump.h>
#include <hs/hs_errno.h>
#include "DAC_UID_1.h"


// hsmoon_start
/*
void print_DAC_UID_R1( DAC_UID_R1 a_inRule) 
{
	if( a_inRule.add_del == 'a')       printf("[ADD] ");
	else if( a_inRule.add_del == 'd')  printf("[DEL] ");
	else {
		printf("Rule action error (mode: %c) \n", a_inRule.add_del );
		return;
	}

	printf( "EX_mode: %d(%c) \n", a_inRule.ex_auth, a_inRule.ex_auth );
	printf( "GID(%2zu): %s \n", strlen(a_inRule.gidbuf), a_inRule.gidbuf );
	printf( "UID(%2zu): %s \n", strlen(a_inRule.uidbuf), a_inRule.uidbuf );
	printf( "DID(%2zu): %s \n", strlen(a_inRule.didbuf), a_inRule.didbuf );
}
*/


/*
** Return the description for the input right 
*/
char* get_str_exmode( int a_right ) {
	switch( a_right ) {
		case 0 : return "No Permission";
		case 1 : return "READ && READ delegation";
		case 2 : return "READ && WRITE";
		case 3 : return "READ && WRITE && READ delegation";
		case 4 : return "READ";
		case 5 : return "READ && READ delegation";
		case 6 : return "READ && WRITE";
		case 7 : return "READ && WRITE && READ delegation";
		default  : return "No Permission";
	}
}


/*
** Print the DAC_UID_R1 TYPE AC RULE info  
*/
void print_DAC_UID_R1( DAC_UID_R1 a_inRule, FILE *a_fp) 
{
	if( a_inRule.add_del == 'a')       fprintf( a_fp, "[ADD] ");
	else if( a_inRule.add_del == 'd')  fprintf( a_fp, "[DEL] ");
	else {
		fprintf( a_fp, "Rule action error (mode: %c) \n", a_inRule.add_del );
		return;
	}

	fprintf( a_fp, "EX_mode: %d(%s) ", a_inRule.ex_auth, get_str_exmode(a_inRule.ex_auth) );
	fprintf( a_fp, "GID(%2zu): %s ", strlen(a_inRule.gidbuf), a_inRule.gidbuf );
	fprintf( a_fp, "UID(%2zu): %s ", strlen(a_inRule.uidbuf), a_inRule.uidbuf );
	fprintf( a_fp, "DID(%2zu): %s \n", strlen(a_inRule.didbuf), a_inRule.didbuf );
}


// LATER: fix one format (ac_token(gdm format) based or raw buf based) 
char* convert_acrule_to_buf( char *a_dest, DAC_UID_R1 a_inRule, int a_Mlen )
{
	int			t_cPos;
	size_t		t_len;
	char		t_slen;


	// rule add/del
	a_dest[0]= a_inRule.add_del;
	a_dest[1]= a_inRule.ex_auth;


	// 1byte length info 
	t_len = strlen ( a_inRule.gidbuf );
	if( t_len > CBUF_LEN ) {
		printf("Unexpected %s len : %zu \n", "GID", t_len );
		return NULL;
	}
	t_slen = (char) ( t_len & 0xff );
	a_dest[2] = t_slen;

	strncpy( &(a_dest[3]), a_inRule.gidbuf, t_slen );
	t_cPos = 3+t_slen;

	
	t_len = strlen ( a_inRule.uidbuf );
	if( t_len > CBUF_LEN ) {
		printf("Unexpected %s len : %zu \n", "UID", t_len );
		return NULL;
	}
	t_slen = (char) ( t_len & 0xff );
	a_dest[t_cPos] = t_slen;

	strncpy( &(a_dest[t_cPos+1]), a_inRule.uidbuf, t_slen );
	t_cPos = t_cPos+1+t_slen;


	t_len = strlen ( a_inRule.didbuf );
	if( t_len > CBUF_LEN ) {
		printf("Unexpected %s len : %zu \n", "DID", t_len );
		return NULL;
	}
	t_slen = (char) ( t_len & 0xff );
	a_dest[t_cPos] = t_slen;

	strncpy( &(a_dest[t_cPos+1]), a_inRule.didbuf, t_slen );
	t_cPos = t_cPos+1+t_slen;

	a_dest[t_cPos]='\0';

	if( a_Mlen < t_cPos ) return NULL;  // case by case check?

	return a_dest;

}


char	uid1Prefix[3][3] = {"GID", "UID", "DID" };

/*
** Read the data received from AC log server 
** Convert them into the DAC_UID_R1 object 
*/ 
DAC_UID_R1* convert_buf_to_acrule( char *a_in, DAC_UID_R1 *a_outRule, 
															int a_inLen )
{
	int			t_slen;
	int			t_cPos;
	int			t_inx = 0;
	char		*t_sPos;
	

	if( a_inLen < 3 ) return NULL;

	
	// rule add/del
	a_outRule->add_del = a_in[0];
	a_outRule->ex_auth = a_in[1];
	t_cPos = 2;
	t_slen = a_in[2]  & 0xff;

	while( t_cPos+t_slen < a_inLen ) {

		if( t_slen > CBUF_LEN ) {
			printf("Unexpected %s len : %d \n", uid1Prefix[t_inx], t_slen );
			return NULL;
		}

		if( t_inx == 0 )       t_sPos = a_outRule->gidbuf ;
		else if( t_inx == 1 )  t_sPos = a_outRule->uidbuf ;
		else if( t_inx == 2 )  t_sPos = a_outRule->didbuf ;
		else {
			printf( "Wrong Output Format \n");
			return NULL;
		}

		strncpy( t_sPos, &(a_in[t_cPos+1]), t_slen );
		t_sPos[t_slen] = '\0';


		t_inx++;
		t_cPos = t_cPos + 1 + t_slen;
		t_slen =  a_in[t_cPos] & 0xff ;

	}


	if ( t_inx != 3 ) {
		printf( "Wrong Output Format [%d id entries] \n", t_inx);
		return NULL;
	}

	return a_outRule;
}
// hsmoon_end


DAC_UID_R1* convert_token_to_acrule( gdp_gclmd_t *token, char a_right, 
											DAC_UID_R1 *a_outRule  )
{
	size_t			t_slen;
	EP_STAT			estat;


	if( token==NULL || a_outRule==NULL ) return NULL;
	
	// rule add/del
	a_outRule->add_del = 'c';  // LATER ERROR CHECK: C: compare 
	a_outRule->ex_auth = a_right;


	estat = gdp_gclmd_find( token, GDP_ACT_GID, &t_slen, 
								(const void **)&(a_outRule->gidbuf) );
	if( !EP_STAT_ISOK( estat ) ) return NULL;
	if( t_slen > CBUF_LEN )		 return NULL;
	a_outRule->gidbuf[t_slen] = '\0'; 

	estat = gdp_gclmd_find( token, GDP_ACT_UID, &t_slen, 
								(const void **)&(a_outRule->uidbuf) );
	if( !EP_STAT_ISOK( estat ) ) return NULL;
	if( t_slen > CBUF_LEN )		 return NULL;
	a_outRule->uidbuf[t_slen] = '\0'; 

	estat = gdp_gclmd_find( token, GDP_ACT_DID, &t_slen, 
								(const void **)&(a_outRule->didbuf) );
	if( !EP_STAT_ISOK( estat ) ) return NULL;
	if( t_slen > CBUF_LEN )		 return NULL;
	a_outRule->didbuf[t_slen] = '\0'; 

	return a_outRule;
}


// hsmoon_start
/*
** Compare two rule ID value 
*/
int	cmp_ruleid( char *in1, char *in2) 
{
	if( in1[0] ==  '*'  || in2[0] == '*' ) {
		if( in1[0] == '*' ) {
			if( in2[0] == '*' ) return 0;
			else return -1;
		} else return 1;
	}

	return strcmp( in1, in2 );

}



/*
** Alloc & initialize the memory for new Rule node
*/
DAC_R1_node* get_new_DAC_R1_node( DAC_UID_R1 inRule, char *inID )
{
	DAC_R1_node		*newNode = NULL;


	newNode = (DAC_R1_node *)ep_mem_malloc( sizeof(DAC_R1_node) );
	if( newNode == NULL ) return NULL;

	newNode->mode    = inRule.add_del;
	newNode->ex_auth = inRule.ex_auth;
	strncpy( newNode->id, inID, CBUF_LEN+1);

	if( inRule.add_del == 'a' ) {
		newNode->allow_count = 1;
		newNode->deny_count  = 0;
	} else {  // d 
		newNode->allow_count = 0;
		newNode->deny_count  = 1;
	}

	newNode->both_count = 0;
	newNode->child = NULL;
	newNode->next  = NULL;

	return newNode;
}


/*
** Remove all child nodes in the rule tree 
*/
void remove_DAC_R1_allchilds( DAC_R1_node *parent ) 
{
	DAC_R1_node		*t_buf;
	DAC_R1_node		*t_cur =  parent->child;


	while( t_cur !=  NULL ) {
		if( t_cur->child != NULL ) 	remove_DAC_R1_allchilds( t_cur ); 

		// case 1 test 
		t_buf = t_cur->next; 
		ep_mem_free( t_cur );
		t_cur = t_buf;
	}

	parent->child = NULL;
}


/*
** Remove all child nodes in the rule tree except first child node 
*/
void remove_DAC_R1_allchilds_wo_header( DAC_R1_node *parent ) 
{
	DAC_R1_node		*t_buf;
	DAC_R1_node		*t_cur =  parent->child;

	
	t_cur = t_cur->next;
	while( t_cur !=  NULL ) {
		if( t_cur->child != NULL ) 	remove_DAC_R1_allchilds( t_cur ); 

		t_buf = t_cur->next; 
		ep_mem_free( t_cur );
		t_cur = t_buf;
	}

	parent->child->next = NULL;
}


void free_rules_on_DAC_UID_1( void **rules )
{
	DAC_R1_node		*t_buf;
	DAC_R1_node		*t_cur = (DAC_R1_node *)(*rules);


	while( t_cur != NULL ) {
		remove_DAC_R1_allchilds( t_cur ); 

		t_buf = t_cur->next; 
	
		ep_mem_free( t_cur );
		t_cur = t_buf;
	}

	(*rules) = NULL;
}


/*
** Reinit the rule node with the input rule (indicated by second argu.)
*/
void reinit_DAC_R1_nodeauth( DAC_R1_node *curNode, DAC_UID_R1 inRule )
{
		curNode->mode    = inRule.add_del;
		curNode->ex_auth = inRule.ex_auth;
		if( inRule.add_del == 'a' ) {
			curNode->allow_count = 1;
			curNode->deny_count  = 0;
		} else {
			curNode->allow_count = 0;
			curNode->deny_count  = 1;
		}
		curNode->both_count  = 0;
}


/*
** Check whether the indicated node (by first argu) includes the interested rule. 
*/
bool isSubset( DAC_R1_node *ruleNode, char cmp_mode, char cmp_exauth ) 
{
	if( ruleNode->id[0] != '*' ) return false;

	if( ruleNode->mode != cmp_mode ) return false;

	if( ruleNode->ex_auth != cmp_exauth ) return false;

	return true;
}


/*
** When the child node changes, check & update mode of parenet node. 
*/
void update_node_modestatus( DAC_R1_node *parent, char premode, char newmode )
{
	if( premode == newmode ) return;


	if( premode == 'a' )      parent->allow_count--;
	else if( premode == 'b' ) parent->both_count--;
	else if( premode == 'd' ) parent->deny_count--;


	if( newmode == 'a' )		parent->allow_count++;
	else if( newmode == 'b' )	parent->both_count++;
	else if( newmode == 'd' )	parent->deny_count++;

	if( parent->both_count == 0 ) {
		if( parent->allow_count!= 0 && parent->deny_count!= 0 ) parent->mode = 'b';
		else if( parent->allow_count != 0 ) parent->mode = 'a';
		else parent->mode = 'd'; 

	} else parent->mode = 'b'; 

}


/*
** Print the current rule tree info (depth first) for debugging 
*/
void print_rule_tree(DAC_R1_node *curNode, int depth, FILE *afp)
{
	DAC_R1_node		*tcurNode = curNode;


	while( tcurNode != NULL ) {
		fprintf( afp, "< [d%d] (%c) %s [%d] (%d:%d:%d) > \n", 
					depth, tcurNode->mode, 
					tcurNode->id, tcurNode->ex_auth, 
					tcurNode->allow_count, tcurNode->deny_count, 
					tcurNode->both_count );

		// print child.. 
		if( tcurNode->child != NULL ) {
			print_rule_tree( tcurNode->child, depth+1, afp );
		}

		// move sibling 
		tcurNode = tcurNode->next; 
	}

}


/*
** Insert the input rule info into the rule tree (indicated by third argu outInfo). 
** NAIVE version 
** Input rule info: first / second argument (received from the log server) 
** Output results: third argu (rule tree), forth argu (rule add/del mode info) 
** return value: EX_OK on success, error_num on error
**
** [NODE] acInfo must be already locked before calling. 
*/
int update_DAC_UID_1( int inDatalen, void *inData, void **outInfo, char *mode )
{
	int					cmpval = -1;
//	int					exit_stat	= EX_OK;
	DAC_R1_node			*t_pre, *t_cur, *pre_unode;
	DAC_R1_node			*gnode = NULL;
	DAC_R1_node			*unode = NULL;
	DAC_R1_node			*dnode = NULL;
	DAC_UID_R1			inRule, *tmpRule; 



	// convert acrule info from buf data. 
	tmpRule = convert_buf_to_acrule( inData, &inRule, inDatalen );
	if( tmpRule == NULL ) {
		ep_app_warn( "[CHECK] fail to convert from buf(len:%d) to structure\n", 
						inDatalen );
		ep_hexdump( inData, inDatalen, stdout, EP_HEXDUMP_HEX, 0);

		return EX_INVALIDDATA;
	}
	// print_DAC_UID_R1( inRule, stdout); 

	*mode = inRule.add_del; 

	// FIRST,  handle GID... 
	t_pre	    = NULL;
	t_cur		= (DAC_R1_node *)(*outInfo); 
	while( t_cur != NULL ) {
		cmpval = cmp_ruleid( t_cur->id, inRule.gidbuf );	

		if( cmpval < 0 )  {
			t_pre = t_cur;
			t_cur = t_cur->next;

		} else if( cmpval == 0 ) {
			gnode = t_cur;
			break;

		} else break;
			
	}


	//
	// first rule with this gid
	//
	if( gnode == NULL ) {
		// t_cur == NULL case or cmpval > 0  

		gnode = get_new_DAC_R1_node( inRule, inRule.gidbuf);
		if( gnode == NULL ) return EX_MEMERR;

		unode = get_new_DAC_R1_node( inRule, inRule.uidbuf);
		if( unode == NULL ) {
			ep_mem_free( gnode );
			return EX_MEMERR;
		}

		dnode = get_new_DAC_R1_node( inRule, inRule.didbuf);
		if( dnode == NULL ) {
			ep_mem_free( gnode );
			ep_mem_free( unode );
			return EX_MEMERR;
		}
		
		gnode->child = unode;
		unode->child = dnode;

		if( t_pre == NULL ) {
			(*outInfo) = gnode; 
			gnode->next = t_cur;

		} else {
			t_pre->next = gnode;
			gnode->next = t_cur;
		}

		return EX_OK; 
	} 

	//
	// existing rule with this gid. 
	//
	if( inRule.uidbuf[0] == '*' ) {
		// rule update with this ac. 
		// remove the existing rules below gid.  
		if( gnode->child->id[0] == '*' ) {
			remove_DAC_R1_allchilds_wo_header( gnode ); 
			reinit_DAC_R1_nodeauth( gnode->child, inRule );
			reinit_DAC_R1_nodeauth( gnode->child->child, inRule );
		} else {
			unode = get_new_DAC_R1_node( inRule, inRule.uidbuf);
			if( unode == NULL ) return EX_MEMERR;

			dnode = get_new_DAC_R1_node( inRule, inRule.didbuf);
			if( dnode == NULL ) {
				ep_mem_free( unode );
				return EX_MEMERR;
			}
			remove_DAC_R1_allchilds( gnode ); 

			gnode->child = unode;
			unode->child = dnode; 
		}

		reinit_DAC_R1_nodeauth( gnode, inRule );

		return EX_OK; 
	}


	// 
	// reflect rule with specific gid, uid 
	// 
	t_pre = NULL;
	t_cur = gnode->child;
	while( t_cur != NULL ) {
		cmpval = cmp_ruleid( t_cur->id, inRule.uidbuf );	

		if( cmpval < 0 )  {
			t_pre = t_cur;
			t_cur = t_cur->next;

		} else if( cmpval == 0 ) {
			unode = t_cur;
			break;
		} else break;
			
	}
	pre_unode = t_pre;

	// first rule with this pair of gid and uid 
	if( unode == NULL ) {
		if( isSubset( gnode->child, inRule.add_del, inRule.ex_auth ) ) {
			// head node includes this rule. so skip. 
			printf("This rule is subset of gnode %s [%d]\n", gnode->id, __LINE__ ); 
			return EX_OK;
		}

		unode = get_new_DAC_R1_node( inRule, inRule.uidbuf);
		if( unode == NULL ) return EX_MEMERR;

		dnode = get_new_DAC_R1_node( inRule, inRule.didbuf);
		if( dnode == NULL ) {
			ep_mem_free( unode );
			return EX_MEMERR;
		}

		unode->child = dnode;

		if( t_pre == NULL ) {
			gnode->child = unode;
			unode->next  = t_cur;

		} else { 
			t_pre->next = unode;
			unode->next = t_cur;
		}

		// ex_auth??? :: MUST CHECK 
		if( inRule.add_del != gnode->mode ) gnode->mode = 'b';
		if( inRule.add_del == 'a' ) gnode->allow_count++; 
		else gnode->deny_count++;

		return EX_OK;
	}


	//
	// existing rules with this pair of gid and uid
	// 
	// update the uid node... 
	if( inRule.didbuf[0] == '*' ) {
		// update existing rule with this in rule. 
		// this uidnode only has one did node.  
		char				pre_mode = unode->mode; 
	
		// check subset... 

		if( unode->child->id[0] == '*' ) {
			remove_DAC_R1_allchilds_wo_header( unode ); 
			reinit_DAC_R1_nodeauth( unode->child, inRule );
			reinit_DAC_R1_nodeauth( unode, inRule );

			if( isSubset(gnode->child, unode->mode, unode->ex_auth) ) {
				// delete sub tree pointed by unode 
				pre_unode->next = unode->next;
				ep_mem_free( unode->child );
				ep_mem_free( unode );

				update_node_modestatus( gnode, pre_mode, 0 );

			} else {
				update_node_modestatus( gnode, pre_mode, inRule.add_del );
			}

		} else {
			dnode = get_new_DAC_R1_node( inRule, inRule.didbuf);
			if( dnode == NULL ) return EX_MEMERR;

			remove_DAC_R1_allchilds( unode ); 
			unode->child = dnode; 
			reinit_DAC_R1_nodeauth( unode, inRule );
			update_node_modestatus( gnode, pre_mode, inRule.add_del );
		}

		return EX_OK; 
	}


	// reflect rule with specific gid, uid , did 
	t_pre = NULL;
	t_cur = unode->child;
	while( t_cur != NULL ) {
		cmpval = cmp_ruleid( t_cur->id, inRule.didbuf );	

		if( cmpval < 0 )  {
			t_pre = t_cur;
			t_cur = t_cur->next;
		} else if( cmpval == 0 ) {
			dnode = t_cur;
			break;
		} else break;
			
	}


	if( dnode == NULL ) {
		char		pre_mode = unode->mode;

		if( isSubset( unode->child, inRule.add_del, inRule.ex_auth ) ) {
			// subset ... so skip
			printf("This rule is subset of gnode %s unode %s\n", 
						gnode->id, unode->id); 
			return EX_OK;
		}  

		if( isSubset( gnode->child, inRule.add_del, inRule.ex_auth ) ) {
			// head node includes this rule. so skip. 
			printf("This rule is subset of gnode %s [%d]\n", gnode->id, __LINE__); 
			return EX_OK;
		}

		dnode = get_new_DAC_R1_node( inRule, inRule.didbuf);
		if( dnode == NULL ) return EX_MEMERR;

		if( t_pre == NULL ) {
			unode->child = dnode;
			dnode->next = t_cur; 
		} else {
			t_pre->next = dnode;
			dnode->next = t_cur;
		}

		if( inRule.add_del == 'a') {  // add 
			unode->allow_count++; 
			if( unode->mode == 'd' ) unode->mode = 'b'; 
		} else { // delete
			unode->deny_count++; 
			if( unode->mode == 'a' ) unode->mode = 'b'; 
		}

		update_node_modestatus( gnode, pre_mode, unode->mode );

	}  else {
		// existing specific gid, uid, did 
		char		pre_mode = unode->mode;

		if( isSubset( unode->child, inRule.add_del, inRule.ex_auth ) ) {
			// Subset so this node is not necessary .. delete this node... 
			if( dnode->mode == 'a' ) {
				unode->allow_count--;
				if( unode->allow_count == 0 ) unode->mode = 'd';
			} else {
				unode->deny_count--; 
				if( unode->deny_count == 0 ) unode->mode = 'a';
			}

			// t_pre is not NULL (* child exists)
			t_pre->next = dnode->next;
			ep_mem_free( dnode );

			// check where unode can be deleted or not. 
			if( unode->child->next == NULL ) {
				unode->ex_auth = unode->child->ex_auth;  
			}

			// remaining case... 
			update_node_modestatus( gnode, pre_mode, unode->mode );

		} else {
			if( isSubset( gnode->child, inRule.add_del, inRule.ex_auth ) ) {
				// head node includes this rule. so skip. 
				printf("This rule is subset of gnode %s [%d]\n", gnode->id, __LINE__); 

				// delete dnode 
				update_node_modestatus( unode, dnode->mode, 0 );
				update_node_modestatus( gnode, pre_mode, unode->mode );

				if( t_pre == NULL ) {
					unode->child = dnode->next;	
				} else {
					t_pre->next = dnode->next;
				}
				ep_mem_free( dnode );

				return EX_OK;
			}

			// change the existing rule 
			// dnode value is changed with inRule. 
			update_node_modestatus( unode, dnode->mode, inRule.add_del );
			update_node_modestatus( gnode, pre_mode, unode->mode );

			// change dnode value
			reinit_DAC_R1_nodeauth( dnode, inRule );
		}
	}


	return EX_OK; 
}


/*
** Check whether the request is authorized or not 
** Request info is passed through 4'th argument (ID info) & first argu(right). 
**		(3'rd argu is the length of 4'th argument.) 
** Authorization is based on the rules passed through 2'nd argu.  
*/
bool check_right_wbuf_on_DAC_UID_1( char a_Right, void *rules, 
											int a_idLen, void *reqID)
{
	DAC_UID_R1			inRule, *tmpRule; 


	// convert acrule info from buf data. 
	tmpRule = convert_buf_to_acrule( reqID, &inRule, a_idLen );
	if( tmpRule == NULL ) {
		ep_app_warn( "[CHECK] fail to convert from buf(len:%d) to rule \n", 
						a_idLen );
		ep_hexdump( reqID, a_idLen, stdout, EP_HEXDUMP_HEX, 0);

		return false;
	}

	return check_right_wrule_on_DAC_UID_1( a_Right, inRule, rules );
	
}


/*
** check whether request ID has the requested right on the current 
**		access policy (rules). 
** first: requested right info 
** second: requestd ID info
** third: access policy info 
**
** Return value: true on allowed request / false on nonallowed request
** NAVIE version 
*/
bool check_right_wrule_on_DAC_UID_1( char a_Right, DAC_UID_R1 inRule, 
											void *rules )
{
	int					cmpval = -1;
	DAC_R1_node			*t_cur;
	DAC_R1_node			*gnode  = NULL;
	DAC_R1_node			*unode = NULL;
	DAC_R1_node			*dnode = NULL;



	// FIRST, Find  GID... 
	// if there is no rule with the GID, deny.  
	t_cur		= (DAC_R1_node *)rules; 
	while( t_cur != NULL ) {
		cmpval = cmp_ruleid( t_cur->id, inRule.gidbuf );	

		if( cmpval < 0 )  {
			t_cur = t_cur->next;
		} else if( cmpval == 0 ) {
			gnode = t_cur;
			break;
		} else break;
			
	}

	if( gnode == NULL )		 return false;
	if( gnode->mode == 'd' ) return false;


	// Among the lower nodes of gnode, 
	//		find the node which is related with the requested UID & DID. 
	// Authorization is based on the following nodes. 
	//		(priority is based on the described order).   
	// If there is no rule node related following 3 cases, deny.  
	// 1. The specific node with the same UID & DID
	// 2. The node with the same UID & * DID  
	// 3. The node with * UID  

	// CHECH UID (can include *) 
	t_cur = gnode->child;
	while( t_cur != NULL ) {
		cmpval = cmp_ruleid( t_cur->id, inRule.uidbuf );	
		if( cmpval < 0 )  {
			t_cur = t_cur->next;
		} else if( cmpval == 0 ) {
			unode = t_cur;
			break;

		} else break;
	}			

	if( unode != NULL ) {
		// check  DID 
		t_cur = unode->child;
		while( t_cur != NULL ) {
			cmpval = cmp_ruleid( t_cur->id, inRule.didbuf );	
			if( cmpval < 0 )  {
				t_cur = t_cur->next;
			} else if( cmpval == 0 ) {
				dnode = t_cur;
				break;
			} else break;
		}	


		if( dnode != NULL ) {
			// case 1 : same GID, UID, and DID  
			if( dnode->mode != 'a' ) return false;

			cmpval = dnode->ex_auth & inRule.ex_auth; 

			if( cmpval == inRule.ex_auth ) return true;
			else return false;

		} 
	

		// check case 2. 
		dnode = unode->child;
		cmpval = cmp_ruleid( dnode->id, "*");
		if( cmpval == 0 ) {
			if( dnode->mode != 'a' ) return false;
			cmpval = dnode->ex_auth & inRule.ex_auth; 

			if( cmpval == inRule.ex_auth ) return true;
			else return false;
		} 

	} 


	// check case 3:  * UID 
	unode = gnode->child;
	cmpval = cmp_ruleid( unode->id, "*");

	if( cmpval == 0 ) {
		if( unode->mode != 'a' ) return false;

		cmpval = unode->ex_auth & inRule.ex_auth; 

		if( cmpval == inRule.ex_auth ) return true;
		else return false;

	} else return false;
	

}
// hsmoon_end

