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
**  KDS-MANAGER --- utility to manage KDS service status 
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.15 
*/ 

#include <stdio.h>
#include <getopt.h>
#include <gdp/gdp.h>

#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_mem.h>
#include <ep/ep_stat.h>

#include <hs/hs_stdin_helper.h>
#include "kdc_api.h"
#include "ksd_key_data.h"

/*
#include <ep/ep_crypto.h>
#include <ep/ep_string.h>
#include <ep/ep_hexdump.h>


#include <stdlib.h>
#include <string.h>

#include <hs/hs_errno.h>
#include <hs/gcl_helper.h>
#include "app_helper.h"
*/



void usage(void)
{
	fprintf(stderr, "Usage: %s [-D dbgspec] [-G gdpd_addr]\n"
			"    -D  set debugging flags\n"
			"    -G  IP host to contact for GDP router\n", 
			ep_app_getprogname());
	exit(EX_USAGE);
}

/*
bool			quiet		= false;
bool			Hexdump	    = true; 
*/

int main(int argc, char **argv)
{
	int				opt;
	int				exitstat	= EX_OK;
	EP_STAT			estat;
	bool			show_usage	= false;
	bool			onGoing		= true;

	char			*gdpd_addr	= NULL;

	char			ex_mode		= 's';
	char			tmpIn;
	char			rw_mode;
	gdp_pname_t		dlname;
	gdp_pname_t		alname;
	gdp_pname_t		klname;
	gdp_pname_t		ksname;
	gdp_pname_t		wdevID;

	gdp_gclmd_t		*sinfo = NULL;
	KGEN_param		*kinfo = NULL;



	// quick pass so debugging is on for initialization
	while ((opt = getopt(argc, argv, "D:G:")) > 0)
	{
		if (opt == 'D')
			ep_dbg_set(optarg);
	}
	optind = 1;
#if EP_OSCF_NEED_OPTRESET
	optreset = 1;
#endif

	// preinit library (must be early due to crypto code in arg processing)
	gdp_lib_init(NULL);


	// collect command-line arguments
	while ((opt = getopt(argc, argv, "D:G:")) > 0)
	{
		switch (opt)
		{
		 case 'D':
			// already done
			break;

		 case 'G':
			gdpd_addr = optarg;
			break;

		 default:
			show_usage = true;
			break;
		}
	}



	argc -= optind;
	argv += optind;


	if (show_usage || argc <= 0  )
		usage();

	// initialize the GDP library
	estat = gdp_init(gdpd_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		exitstat = EX_FAILURE;
		goto fail0;
	}
	kdc_init( );


	while( onGoing ) {

		// Action type for Key Service 
		printf("\n Select the action Type \n"
					"[c:change key param] [s:start key service] [q:quit]:");

		tmpIn = getchar_ignore();

		switch( tmpIn )  {
			case 'c': 
					ex_mode = tmpIn;
					break;

			case 'q':
					onGoing = false;
					break;

			case 's':
					ex_mode = tmpIn;
					break;

			default:
					printf(">>> Not supported or Undefined (%c) \n", tmpIn);
					break;
		}

		if( onGoing == false ) break;

		if( ex_mode != 'c' && ex_mode != 's' ) continue;

		scan_nonzerochars( "Input the log name for key service: ", 
											dlname, GDP_GCL_PNAME_LEN-4 );

		sprintf( ksname, "KS_%s", dlname );
		sprintf( klname, "KEY_%s", dlname );


		if( ex_mode == 's' ) {
			exitstat = scan_chars_default("Input the key log name:", 
											klname, GDP_GCL_PNAME_LEN, klname );


			scan_nonzerochars( "Input the acces log name: ", 
											alname, GDP_GCL_PNAME_LEN );

			scan_nonzerochars( "Input the writer device ID: ", 
											wdevID, GDP_GCL_PNAME_LEN );

			rw_mode = scanchar_ignore( 
								"\nSelect the mode [r(dist), w(gen/dist)]");

			printf("	>>> log			name: %s \n", dlname );
			printf("	>>> ac log		name: %s \n", alname );
			printf("	>>> key log		name: %s \n", klname );
			printf("	>>> key service	name: %s \n", ksname );
			printf("	>>> writer device ID: %s \n", wdevID );
			printf("	>>> key service mode: %c \n", rw_mode );


			sinfo  = set_ksinfo( dlname, alname, klname, rw_mode, wdevID );
			if( sinfo == NULL ) {
				ep_dbg_printf("[ERROR] Fail to set ks info\n");
				exitstat = EX_FAILURE;
				goto fail0;
			}

		}
		
		tmpIn = scanchar_ignore("\nControl detailed key param[y or n]");
		if( tmpIn == 'y' ) {
			// LATER : input the detailed key parameter values. 
		}

		kinfo = get_new_kgen_param( NULL, NULL, 0 );
		if( kinfo == NULL ) {
			ep_dbg_printf("[ERROR] Fail to set key param\n");
			exitstat = EX_FAILURE;
			goto fail0;
		}
	

		// action 
		if( ex_mode == 's' )  {
			estat = kds_service_request( ksname, KS_SERVICE_START, 
												sinfo, kinfo, 'S'  );
		} else {
			estat = kds_service_request( ksname, KS_SERVICE_CHANGE, 
												sinfo, kinfo, 'S'  );
		}
		if( !EP_STAT_ISOK( estat ) )  goto fail0; 


		if( sinfo != NULL ) {
			gdp_gclmd_free( sinfo );
			sinfo = NULL;
		}
		if( kinfo != NULL ) {
			ep_mem_free( kinfo );
			kinfo = NULL;
		}
	}


fail0: 

	if (!EP_STAT_ISOK(estat))
		ep_app_message(estat, "exiting with status");

	if( sinfo != NULL ) gdp_gclmd_free( sinfo );
	if( kinfo != NULL ) ep_mem_free( kinfo );


	return exitstat;
}
