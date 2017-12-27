/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
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
// write_start

#include <signal.h>
#include <sysexits.h>

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_log.h>
#include <ep/ep_thr.h>
#include <ep/ep_stat.h>
#include <ep/ep_assert.h>
#include <gdp/gdp.h> 
#include <gdp/gdp_priv.h> 


#include <ep/ep_sd.h>
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ksd_data_manager.h"

#include "ksd_key_data.h"



int main(int argc, char **argv)
{
//	init_ks_info_from_file(); 
//	exit_ks_info_manager();


	//
	// INTERNAL test 
	// 

	// ksd_key_data test 
/*	{
		int			tval, ti, rval;
		char		outBuf[32];

		tval = get_kgen_fname( 1, outBuf, 32 ); 
		printf("In %d, rval: %d, strlen: %zu\n --- out: %s \n", 
				1, tval, strlen( outBuf ), outBuf );
		for( ti=1; ti<10; ti++ ) {
			tval = tval*ti + 10;
			rval = get_kgen_fname( tval, outBuf, 32 );
			printf("In %d, rval: %d, strlen: %zu\n --- out: %s \n", 
					tval, rval, strlen( outBuf ), outBuf );
		} 
	}
*/ /*
	{
		KGEN_param		*tParam = NULL;

		tParam = load_kgen_param( 1 );
	}
*/

	return 0;
}
