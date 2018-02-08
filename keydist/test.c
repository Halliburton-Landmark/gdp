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

#include <ep/ep.h>
#include <ep/ep_log.h>
#include <ep/ep_thr.h>
#include <ep/ep_assert.h>
#include <gdp/gdp_priv.h> 
#include <ep/ep_sd.h>
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_stat.h>
#include <hs/gcl_helper.h>
#include <gdp/gdp.h> 

#include "ksd_data_manager.h"
#include "../acrule/DAC_UID_1.h"

/*
#include "ksd_key_data.h"
*/


void usage( void ) 
{
	fprintf( stderr, 
			 "Usage: %s [mode] [value: mode dependent]\n"
			 "Usage: %s [-a] value\n"
			 "	-a: ac test mode: value is the name of ac log.\n", 
			 ep_app_getprogname(), ep_app_getprogname() );

	exit(EX_USAGE);
			
}


void test_acread( gdp_event_t *gev ) 
{
	bool			isAuthorized = false;
	bool			isBuffered = false;
	ACL_info		*acInfo = NULL;
	gdp_datum_t		*datum  = NULL;
	DAC_UID_R1		testRule;



	testRule.ex_auth = 4;
	strcpy( testRule.gidbuf, "rusr2");
	strcpy( testRule.uidbuf, "meta");
	strcpy( testRule.didbuf, "d2");


	acInfo = (ACL_info *)gdp_event_getudata( gev );
	datum  = gdp_event_getdatum( gev );

	isAuthorized = check_right_wrule_on_DAC_UID_1( testRule.ex_auth, 
													testRule, acInfo->acrules );

	if( isAuthorized ) printf("Pass Auth \n");
	else printf("No authorized \n" );


	acInfo->last_recn = gdp_gcl_getnrecs(acInfo->gcl);
	printf(" [CB] AC last num: %" PRIgdp_recno " \n", acInfo->last_recn );

	isBuffered = update_ac_data( acInfo, datum, false );

	if( isBuffered ) printf(" [CB] Buffered AC data \n\n");
	else printf(" [CB] Not Buffered AC data \n\n" );

//	(void)process_event( gev, true, 'a' );
	if( gdp_event_gettype(gev) == GDP_EVENT_EOS ) 
		printf( "[CB] End of Subscription \n" );


	if( isBuffered ) gev->datum = NULL;
	gdp_event_free( gev );

}


int main(int argc, char **argv)
{
	int					opt;
	char				tnum = 0;
	char				mode = 0;
	char				errMsg[128];
	EP_STAT				estat;


	while( (opt = getopt( argc, argv, "aD:")) > 0 ) {
		switch( opt ) 
		{
			case 'a': 
				mode = 'a';
				tnum++;
				break;

			case 'D':
				ep_dbg_set(optarg);
				break;

			default: 
				printf( "Wrong argument: default \n" );
				usage();
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if( tnum != 1 || argc <=0 ) {
		printf("Worng argument \n\n");
		usage();
	}


	errMsg[0] = '\0';

	estat = gdp_init( NULL );
	if( !EP_STAT_ISOK(estat) ) {
		ep_app_error( "GDP Initialization failed" );
		strcpy( errMsg, "Fail to init gdp" );
		goto fail0;
	}


	if( mode == 'a' ) {
		gdp_name_t			gclname;
		gdp_gcl_t			*gcl = NULL;
		ACL_info			*testAC = NULL;
		void (*acb_func)(gdp_event_t *) = NULL;



		printf( "******* Start Debugging for ac rule [%s] \n", argv[0] );

		estat = gdp_parse_name( argv[0], gclname );
		if( !EP_STAT_ISOK(estat) ) {
			ep_app_error( "Illegal GCL name: %s \n", argv[0] );
			sprintf( errMsg, "Illegal GCL name: %s \n", argv[0] );
			goto fail0;
		}
	

		estat = gdp_gcl_open( gclname, GDP_MODE_RO, NULL, &gcl );
		if( !EP_STAT_ISOK(estat) ) {
			sprintf( errMsg, "Cannot open GCL name: %s \n", argv[0] );
			ep_app_error( "%s", errMsg );
			goto fail0;
		}

		testAC = get_new_ac_data( strlen(argv[0]), argv[0] );
		if( testAC == NULL ) {
			sprintf( errMsg, "Cannot make ACL_info: %s in ac test mode\n", 
									argv[0] );
			ep_app_error( "%s", errMsg );
			goto tfail0;
		}


		testAC->isAvailable = true;
		find_gdp_gclmd_uint( gcl, GDP_GCLMD_ACTYPE, &(testAC->acr_type) );
		if( isSupportedACR( testAC->acr_type ) == false ) {
			testAC->isAvailable = false;
			printf("[ACR TYPE] %x - NOT supported ac type \n", testAC->acr_type );
		} else printf("[ACR type] %x - supported \n", testAC->acr_type );
		

		acb_func = test_acread; 
		testAC->gcl = gcl;
		estat = gdp_gcl_subscribe( gcl, 1, 12, NULL, acb_func, (void *)testAC );
		if( !EP_STAT_ISOK(estat) ) {
			sprintf( errMsg, "Cannot subscribe:  %s \n", argv[0] );
			ep_app_error( "%s", errMsg );
			goto tfail0;
		}

		sleep(3);

		free_rules_on_DAC_UID_1( &(testAC->acrules) );

//		estat = gdp_gcl_subscribe( gcl, 1, 1, NULL, acb_func, (void *)testAC );
//		sleep(3600);

tfail0:
		if( gcl != NULL ) gdp_gcl_close( gcl );
		if( testAC != NULL ) free_aclinfo( (void *)testAC );
		goto fail0;

	}


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

fail0:

	if( strlen(errMsg) > 0 ) {
		printf("EXIT : %s \n", errMsg );
	}

	return 0;
}
