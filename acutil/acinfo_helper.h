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
#ifndef	_AC_INFO_HELPER_H_
#define	_AC_INFO_HELPER_H_

#include <hs/hs_list.h>


#define		CBUF_LEN	32


// linked list (at first) 
typedef struct ac_info_t {
	char					*logname;
	char					*dinfo;
	struct ac_info_t		*next;
} ac_info; 


struct gid_info {
	int				sub_inx;
}; 

struct uid_info {
	hs_lnode			*ginfo;
	ac_info				*linfo;
}; 

struct acf_line {
	char		uid[CBUF_LEN];
	char		gid[CBUF_LEN];
	char		did[CBUF_LEN];
	int			inx;
	char		logname[CBUF_LEN];
	hs_lnode	*gnode;
	hs_lnode	*unode;
};

// if success, return 0. else return error code (not defined yet.) 
int		load_ac_info_from_fp(FILE *);
void	show_glist();
void	show_ulist();
int		show_uinfo(char *);
void	exit_ac_info();

struct acf_line* get_new_aclogname( char *, char *);
void	update_ac_info_to_fp(FILE *, struct acf_line *);

#endif	//_AC_INFO_HELPER_H_
// hsmoon end 

