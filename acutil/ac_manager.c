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
**  AC-MANAGER --- utility to manage access control information
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.15 
*/ 

// hsmoon start
#include <ep/ep_dbg.h>
#include <ep/ep_app.h>
#include <ep/ep_stat.h>
#include <ep/ep_crypto.h>
#include <ep/ep_string.h>
#include <ep/ep_hexdump.h>

#include <gdp/gdp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <hs/hs_errno.h>
#include <hs/hs_stdin_helper.h>
#include <hs/gcl_helper.h>
#include <ac/acrule.h>
#include <ac/DAC_UID_1.h>
#include "app_helper.h"
#include "acinfo_helper.h"
// hsmoon end 


// hsmoon start
static EP_DBG	Dbg = EP_DBG_INIT("gdp.ac-manager", "AC INFO MANAGE APP");


#define AC_LOG_KDIR		"/home/hsmoon/etc/keys/ac"		
#define AC_INFO			"/home/hsmoon/etc/ac_info.txt"  
#define AC_LOG_SVR		"kr.re.etri.gdp-01.gdplogd"
#define OPTIONS			"c:D:gG:k:l:qr:s:w"

//#define AC_LOG_SVR		"kr.re.etri.hsm.ld1"  // LATER

// Help information 
//		When a log is generated, the key pair is necessary. 
//		Currently, this key pair is genearted.
//      Public key is stored with metadata in Log server
//      Secret key is encrypted (aes192:default) and stored in the pem file (device) 
//	LATER: CHECK SIGNATURE OF WRITTEN DATA 
// uid gid [Later did] dinfo G_sub# AC_Log_name 
void usage(void)
{
	fprintf(stderr, "Usage: %s [-c dinfo] [-D dbgspec] [-g] [-G gdpd_addr]\n"
			"\t[-k keytype] [-l uid] [-q] [-r read option]\n"
			"\t[-s logd_name] [-w] \n"
			"\t[val: gid_uid | ac log name]\n"
			"    -c  create the access control log for device (dinfo/gid_uid) & update\n"
			"    -D  set debugging flags\n"
			"    -g  show the list of gid & exit\n"
			"    -G  IP host to contact for GDP router\n"
			"    -k  type of key; valid key types are \"rsa\", \"dsa\", and \"ec\"\n"
			"		 Accordign to the key type, default value is used (RSA/DSA 2048 ..)\n"
			"    -l  show the ac info related with the uid and exit\n"
			"    -q  don't give error if log already exists\n"
			"    -r  read the access control log info (ac log name) & exit\n"
			"		 Accordign to the option, show different values\n"
			"		 s: subscribe, a: all record, m: metadata, n#3: read # entries\n"
			"        r#1-#2: #1 ~ #2 record, r#: one record (record number #)\n"
			"    -s  set name of log daemon on which to create the log\n"
			"    -w  write the access control log info (ac log name) with the other info \n"
			"    val: gid_uid is used with -c option. log name is used with -r and -w option\n"
			"[Usabe Example] \n"
			"    -c dsensor_0001 smdev_sensor1 \n"
			"    -r a smdev_1 | -w smdev_1 \n"
			"    -g || -l sensor1 \n",
			ep_app_getprogname());
	exit(EX_USAGE);
}


bool			quiet		= false;
bool			Hexdump	    = true; 

EP_STAT swrite_record(gdp_datum_t *datum, gdp_gcl_t *gcl)
{
	EP_STAT estat;

	// echo the input for that warm fuzzy feeling
	if (!quiet)
	{
		gdp_buf_t *dbuf = gdp_datum_getbuf(datum);
		int l = gdp_buf_getlength(dbuf);
		unsigned char *buf = gdp_buf_getptr(dbuf, l);

		if (!Hexdump)
			fprintf(stdout, "Got input %s%.*s%s\n",
					EpChar->lquote, l, buf, EpChar->rquote);
		else
			ep_hexdump(buf, l, stdout, EP_HEXDUMP_ASCII, 0);
	}

	if (ep_dbg_test(Dbg, 60))
		gdp_datum_print(datum, ep_dbg_getfile(), GDP_DATUM_PRDEBUG);

	// then send the buffer to the GDP
//	LOG("W");
/*	if (AsyncIo)
	{
		estat = gdp_gcl_append_async(gcl, datum, showstat, NULL);
		EP_STAT_CHECK(estat, return estat);

		// return value will be printed asynchronously
	}
	else 
	{  */
		estat = gdp_gcl_append(gcl, datum);

		if (EP_STAT_ISOK(estat))
		{
			// print the return value (shows the record number assigned)
			if (!quiet)
				gdp_datum_print(datum, stdout, 0);
		}
		else if (!quiet)
		{
			char ebuf[100];
			ep_app_error("Append error: %s",
						ep_stat_tostr(estat, ebuf, sizeof ebuf));
		}
//	}
	return estat;
}


int main(int argc, char **argv)
{
	// other variables 
	int				opt;

	// flags
	bool			exit_proc	= false;
	int				exitstat	= EX_OK;
	bool			show_usage	= false;
	char			ex_mode		= 'n';
	EP_STAT			estat;

	// variables for access control information 
	uint32_t		rType		= 0;
	char			*roption	= NULL;
	char			*devinfo	= NULL;
	FILE			*acfp		= NULL;
	gdp_name_t		gcliname;			// internal name of GCL for log
	const char		*gclxname	= NULL;	// log readable name  
	struct acf_line	*log_info	= NULL;	// log readable name  


	// variables related with  writer's key (asym)
	int				keytype		 = EP_CRYPTO_KEYTYPE_UNKNOWN;
	int				keylen		 = -1;
	int				key_enc_alg  = -1;
	const char		*keydir		 = NULL;  // directory to be stored
	EP_CRYPTO_KEY	*wKey		 = NULL;
	gdp_pname_t		r_pbuf;


	// variables for GDP connection 
	gdp_gcl_t		*gcl		= NULL;
	gdp_gclmd_t		*gmd		= NULL;
	char			*gdpd_addr	= NULL;
	const char		*logdxname	= NULL;	// external name of log daemon
	gdp_name_t		logdiname;			// internal name of log daemon

	gdp_gcl_open_info_t			*gcl_info;



	// quick pass so debugging is on for initialization
	while ((opt = getopt(argc, argv, OPTIONS)) > 0)
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

	if( acfp == NULL ) {
		acfp = fopen( AC_INFO, "a+"); 
	}
	if( acfp == NULL ) {
		fprintf(stderr, "Cannot Fail to read %s\n", AC_INFO);
		printf("Cannot Fail to read %s\n", AC_INFO);
		exit(EX_NOINPUT);
	}

	exitstat = load_ac_info_from_fp( acfp ); 
	if( exitstat != EX_OK ) {
		printf("Cannot load %s\n", AC_INFO);
		goto pexit0;
	}


	// collect command-line arguments
	while ((opt = getopt(argc, argv, OPTIONS)) > 0)
	{
		switch (opt)
		{
		 case 'c':   
			devinfo = optarg;
			ex_mode = 'c';
			break;

		 case 'D':
			// already done
			break;

		 case 'g':   
			// print info 
			show_glist();
			exit_proc = true; 
			break;

		 case 'G':
			gdpd_addr = optarg;
			break;

		 case 'k':
			keytype = ep_crypto_keytype_byname(optarg);
			if (keytype == EP_CRYPTO_KEYTYPE_UNKNOWN)
			{
				ep_app_error("unknown key type %s", optarg);
				show_usage = true;
			}
			break;

		 case 'l':   
			// print info 
			if( show_uinfo(optarg) < 0 ) {
				show_ulist();
			}
			exit_proc = true; 
			break;

		 case 'q':
			quiet = true;
			break;

		 case 'r':
			roption = optarg;
			ex_mode = 'r';
			break;

		 case 's':
			logdxname = optarg;
			break;

		 case 'w':
			ex_mode = 'w';
			break;

		 default:
			show_usage = true;
			break;
		}
	}


	if( exit_proc )		goto pexit0;

	argc -= optind;
	argv += optind;


	if (show_usage || argc <= 0 || ex_mode == 'n' )
		usage();


	// Check & Validate the required information
	// according to the ex_mode (c: create, r: read, w:update) 
	// c: check the new log name  
	//		On successful log creation, the log info is written in the file : LATER
	// r|w: check the existing log name  
	if( ex_mode == 'c' ) {
		log_info = get_new_aclogname( argv[0], devinfo );

		if( log_info == NULL ) {
			ep_app_error("Cannot create the access log for %s %s \n", 
								argv[0], devinfo);
			goto pexit0;	
		}

		gclxname = log_info->logname; 

	} else { // r or w 
		// LATER: check the existing name in loaded ac info. 
		gclxname = argv[0];
	}


	if( ex_mode=='r' && roption==NULL ) {
		ep_app_error("Wrong input option at read mode \n" );
		goto pexit0;	
	}


	// initialize the GDP library
	estat = gdp_init(gdpd_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}
	gdp_parse_name(gclxname, gcliname);
	gdp_printable_name( gcliname, r_pbuf );
	//printf("~~~~~ gclxname: %s \n", gclxname );
	//printf("~~~~~ gcliname: %s \n", r_pbuf);
	//gdp_print_name(	gcliname , stdout);
// hsmoon end 

	// allow thread to settle to avoid interspersed debug output
	ep_time_nanosleep(INT64_C(100000000));
	
	// GCL open with read mode  	
	estat = gdp_gcl_open(gcliname, GDP_MODE_RO, NULL, &gcl);
	if (EP_STAT_ISOK(estat))
	{
		gdp_printable_name( *gdp_gcl_getname(gcl), r_pbuf );

		if( ex_mode == 'c' ) {  // create or write mode 
			// The log we want to creat already exists.  
			gdp_gcl_close(gcl);

			if( !quiet )
				ep_app_error("Cannot create %s: already exists", gclxname);
			
			exitstat = EX_CANTCREAT;
			goto pexit0;
		} 
	
		// Extract the rule Type info from metadata. 
		estat = gdp_gcl_getmetadata( gcl, &gmd ); 
		exitstat = EX_FAILGETDATA;
		EP_STAT_CHECK( estat, goto pexit0 );
		exitstat = EX_OK;
	
		estat = gdp_gclmd_find_uint( gmd, GDP_GCLMD_ACTYPE, &rType ); 
		if( !EP_STAT_ISOK( estat ) ) {
			ep_app_error("Cannot get AC Rule Type info  from %s ", gclxname);
			exitstat = EX_FAILGETDATA;
			goto pexit0;
		}

		
		if( ex_mode == 'w' ) {
			gdp_gcl_close(gcl);
		} 

	} else {
		if( ex_mode != 'c' ) {
			// read or write mode :: ERR 
			ep_app_message(estat, "Cannot open GCL %s", gclxname);
			goto fail0;
		}
	}


	// current status
	// Read request: GCL open success. 
	// Create / Write request; Need to open GCL with open_info 
	if( ex_mode == 'c' ) {
		// Prepare the data necessary to create the log. 
		char		*tempkeyfile = NULL;
		char		*finalkeyfile = NULL;
		char		tmpIn;
		bool		onGoing		= true;


		// Key generation 
		wKey = make_new_asym_key( keytype, &keylen );
		if( wKey == NULL ) {
			ep_app_error("Cannot create the new asym key for writing \n");  
			exitstat = EX_CANTCREAT;
			goto pexit0;	
		}
	
		// Write secret key in the file. 
		if (key_enc_alg < 0)
		{
			const char *p = ep_adm_getstrparam("swarm.gdp.crypto.keyenc.alg",
								"aes192");
			key_enc_alg = ep_crypto_keyenc_byname(p);
			if (key_enc_alg < 0)
			{
				ep_app_error("unknown secret key encryption algorithm %s", p);
				exitstat = EX_USAGE;
				goto fail1;	
			}
		}

		keydir = get_keydir( AC_LOG_KDIR );
		tempkeyfile = write_secret_key( wKey, keydir, key_enc_alg );
		if( tempkeyfile == NULL ) {
			ep_app_error("Couldn't write secret key");
			exitstat = EX_IOERR;
			goto fail1;	
		}
		printf("~~~ Generated Key len: %d \n", keylen );

		// log metadata  
		gmd = gdp_gclmd_new(0);
		add_time_in_gclmd( gmd );
		add_creator_in_gclmd( gmd );
		exitstat = add_pubkey_in_gclmd( gmd, wKey, keytype, keylen, key_enc_alg );
		if( exitstat != 0 ) {
			ep_app_error("Couldn't add pub key \n");
			ep_mem_free( tempkeyfile );
			goto fail1;	
		}
		gdp_gclmd_add( gmd, GDP_GCLMD_XID, strlen(gclxname), gclxname);
		gdp_gclmd_add( gmd, GDP_GCLMD_TYP,		2, "AC");


		// Get AC Rule Type info 
		while( onGoing ) {
			printf("\nSelect the rule type [d(user based DAC 1) c(cert based)]: ");
			tmpIn = getchar_ignore();

			switch( tmpIn )  {
				case 'c': 
					printf(">>> Not supported yet (%c) \n", tmpIn);
					break;

				case 'd':
					rType = ACR_TYPE_DAC_UID_1; 
					onGoing = false;
					break;

				default:
					printf(">>> Not supported or Undefined (%c) \n", tmpIn);
					break;
			}

		}
		gdp_gclmd_add( gmd, GDP_GCLMD_ACTYPE,	4, &rType );


		// if we don't have a log daemon, pick one
		if (logdxname == NULL)
				logdxname = select_logd_name(AC_LOG_SVR);
		gdp_parse_name(logdxname, logdiname);
		ep_dbg_cprintf( Dbg, 3, "Using log server %s \n", logdxname); 

		// create a GCL with the provided name
		estat = gdp_gcl_create(gcliname, logdiname, gmd, &gcl);
		if( !EP_STAT_ISOK(estat) ) {
			ep_app_error("Couldn't create GCL for %s at %s\n", 
							gclxname, logdxname );
			ep_mem_free( tempkeyfile );
			goto fail0;	
		}

		finalkeyfile = rename_secret_key( gcl, keydir, tempkeyfile );
		if( finalkeyfile == NULL ) 
		{
			ep_mem_free( tempkeyfile );
			gdp_gcl_close(gcl);
			exitstat = EX_FILERENAME;
			goto fail1;
		} 

		if( !quiet ) {
			ep_dbg_cprintf( Dbg, 3, "New secret key is in %s", finalkeyfile);
			ep_dbg_cprintf( Dbg, 3, "Save this file!  You'll need it to write the new log");
		}

		ep_mem_free( tempkeyfile );
		ep_mem_free( finalkeyfile );
		tempkeyfile = NULL;

		// just for a lark, let the user know the (internal) name
		if (!quiet)
		{
			gdp_pname_t pname;

			printf("Created new GCL %s\n\ton log server %s\n",
				gdp_printable_name(*gdp_gcl_getname(gcl), pname), logdxname);
		}

		// ac file update 
		// Does not use the loaded ac info. 
		// So don't need to reflect the created info  
		update_ac_info_to_fp( acfp, log_info );

//		gdp_gcl_close( gcl );
	} 
	
	
	if( ex_mode != 'r' ) {
		// create(+update) or write mode 
		// need the gcl_open 
		// For convenience, synchronous.. 
		int							trval;
		char						rMode;
		int  						right;
		char						*tRbuf;
		char						tDbuf[1024];  // LATER : consider the other rule type (C) 
		DAC_UID_R1					rInfo;
		char						*finalkeyfile = NULL;
		gdp_datum_t					*datum = gdp_datum_new();


		gdp_gcl_close( gcl );

		keydir = get_keydir( AC_LOG_KDIR );
		finalkeyfile = merge_fileName( keydir, sizeof r_pbuf, r_pbuf );
		if( finalkeyfile == NULL ) {
			ep_app_error("Cannot draw the signing key file \n");  
			exitstat = EX_USAGE;
			goto pexit0;	
		}
		if( ex_mode == 'c' ) printf( "ReAccess to the key file for writing \n" );

		gcl_info = gdp_gcl_open_info_new();
		gdp_gcl_open_info_set_signkey_cb( gcl_info, signkey_cb, 
							finalkeyfile );


		estat = gdp_gcl_open(gcliname, GDP_MODE_AO, gcl_info, &gcl);
		if( !EP_STAT_ISOK( estat )  ) {
			ep_app_error("Cannot open the log[w]: %s - %s \n", gclxname, gcliname );  
			exitstat = EX_IOERR;
			ep_mem_free( finalkeyfile );
			gdp_gcl_open_info_free( gcl_info );
			goto fail0;	
		}


		// command check : quit / add rule / remove rule 
		printf("\nSelect the mode [q(quit), a(add rule), d(delete rule)]: ");
		rMode = getchar_ignore();

		while( isValidChar( rMode, "ad" ) ) {
			// 
			// Process rMode a / d 
			// The other value including q is considerred as exit command. 
			// 

			// Input the rule information.. 
			// allow  R gid * * | gid uid * | gid uid did [right]
			// allow  C certID [rigth] (LATER) 
			// deny   R gid * * | gid uid * | gid uid did [right]
			// deny   C certID [right] (LATER)
			// revoke C  capability ID  list  (LATER)
			// LATER : check type (R/C)

			// command check : quit / add rule / remove rule 
			trval = 1;

			switch( rType ) {

				case ACR_TYPE_DAC_UID_1:
					rInfo.add_del = rMode;

					scan_nonzerochars( "Input the GID", rInfo.gidbuf, CBUF_LEN );
					//printf(">>> GID: %s \n", rInfo.gidbuf );

				
					exitstat = scan_chars_default("Input the UID", rInfo.uidbuf, CBUF_LEN, "*" );
					if( exitstat != EX_OK ) goto fail1; 
					//printf(">>> UID: %s \n", rInfo.uidbuf );

					if( strcmp("*", rInfo.uidbuf) == 0 ) {
						strcpy( rInfo.didbuf, "*" );
					} else {
						exitstat = scan_chars_default("Input the DID", rInfo.didbuf, CBUF_LEN, "*" );
						if( exitstat != EX_OK ) goto fail1; 
						//printf(">>> DID: %s \n", rInfo.didbuf );
					}


					right  = scanInt_ignore_maxVal("Input the right[0xrwd(read, write, delegation) ex:7, 5, 1]", 7);
					rInfo.ex_auth = (char) (right & 0x07);

					// for debugging 
					print_DAC_UID_R1( rInfo, stdout );


					// concatenate the data to send them through network 
					tRbuf = convert_acrule_to_buf( tDbuf, rInfo, 1024); 
					if( tRbuf == NULL ) {
						ep_app_error("Fail to convert the input rule info \n");
						print_DAC_UID_R1( rInfo, stdout );
					}

					gdp_buf_write( gdp_datum_getbuf(datum), tDbuf, strlen(tDbuf) );

					// write the data... 
					estat = swrite_record( datum, gcl );

					break;

				default: 
						printf(">>> Not supported or Undefined (%08x) \n", rType);
						trval = 0;

			}


			if( !EP_STAT_ISOK(estat) ) break;

			if( trval == 0 ) {
				printf(">>> Not supported or Undefined Rule type. So Exit...  \n" );
				break;
			}

			// reinput.. 
			rMode = scanchar_ignore( "\nSelect the mode [q(quit), a(add rule), d(delete rule)]");
		}


		gdp_datum_free( datum );
		gdp_gcl_open_info_free( gcl_info );
		ep_mem_free( finalkeyfile );

		printf( "end of write \n");

	} else {
		// read mode 
		// already open 
		char			rMode    = roption[0];
		char			*tsplit  = NULL;
		int				t_recnum = 0;
		int				t_frec   = 0;
		int				t_lrec   = 0;
		char			buf[10];
		int				tlen     = 0;
		
		switch( rMode )
		{
			case 'a':
				estat = do_simpleread( gcl, 0, NULL, t_recnum ); 

				if( EP_STAT_IS_SAME( estat, EP_STAT_END_OF_FILE ) ) estat = EP_STAT_OK; 
				break;

			case 'm':
				print_metadata( gcl );
				break;

			case 'n':
				if( strlen( roption ) < 2 ) {
					ep_app_error("Wrong input option at read mode [%c] %s\n", 
									rMode, roption );
					break;
				}
				t_recnum = atoi( &(roption[1]) );
				estat = do_simpleread( gcl, 0, NULL, t_recnum ); 
				if( EP_STAT_IS_SAME( estat, EP_STAT_END_OF_FILE ) ) { 
					printf("[WARN] Some requested record does not exist [1:%d] \n", t_recnum); 
					estat = EP_STAT_OK; 
				}

				break;

			case 'r':
				if( strlen( roption ) < 2 ) {
					ep_app_error("Wrong input option at read mode [%c] %s\n", 
									rMode, roption );
					break;
				}
				tsplit = strchr( roption, '-' );

				if( tsplit == NULL ) {
					t_frec = atoi( &(roption[1]) );
					estat = do_simpleread( gcl, t_frec, NULL, 1 ); 
					if( !EP_STAT_ISOK( estat) ) { 
						printf("[WARN] Some requested record does not exist [%d:%d] \n", t_frec, t_frec); 
					}
					break;
					
				} 
			

				if( tsplit-roption+1 < strlen( roption ) ) {
					t_lrec = atoi( &(tsplit[1]) );

					tlen   = tsplit-roption-1;
					strncpy( buf, &(roption[1]), tlen );
					buf[tlen] = '\0';
					t_frec = atoi( buf );
					

					if( t_lrec < t_frec ) {
						t_recnum = t_frec - t_lrec +1; 
						t_frec   = t_lrec;
					} else {
						t_recnum = t_lrec - t_frec +1;
					}
		
					estat = do_simpleread( gcl, t_frec, NULL, t_recnum ); 
					if( !EP_STAT_ISOK( estat) ) { 
						printf("[WARN] Some requested record does not exist [%d:%d] \n", t_frec, t_lrec); 
					}

				} else {
					ep_app_error("Wrong input option at read mode [%c] %s\n", 
									rMode, roption );
				}
				
				break;

			case 's':
				estat = do_multiread( gcl, 0, NULL, 0, true, true );
				break; 


			default:
				ep_app_error("Wrong input option at read mode [%c] \n", rMode );
				break;
		}

	}

	gdp_gcl_close(gcl);



fail0:
	if (EP_STAT_ISOK(estat))
		exitstat = EX_OK;
	else if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOROUTE))
		exitstat = EX_NOHOST;
	else if (EP_STAT_ISABORT(estat))
		exitstat = EX_SOFTWARE;
	else
		exitstat = EX_UNAVAILABLE;

	// OK status can have values; hide that from the user
	if (EP_STAT_ISOK(estat))
		estat = EP_STAT_OK;

	if (!EP_STAT_ISOK(estat))
		ep_app_message(estat, "exiting with status");
	else if (!quiet)
		fprintf(stderr, "Exiting with status OK\n");

fail1:
	if( wKey != NULL ) ep_crypto_key_free( wKey );

	// free metadata, if set
	if (gmd != NULL)
		gdp_gclmd_free(gmd);


pexit0: 
	if( acfp != NULL ) {
		fclose( acfp );
		exit_ac_info();
	}
	if( log_info != NULL ) ep_mem_free( log_info ); 

	if( exitstat != EX_OK ) ep_app_error( "[INFO] Existing with status: %s", str_errinfo( exitstat ) );

	return exitstat;
}
