/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  LOG-CREATE --- create a new log
**
**		This is a temporary interface.  Ultimately we need a log
**		creation service that does reasonable log placement rather
**		than having to name a specific log server.
**
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
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

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_crypto.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <gdp/gdp.h>

#include <openssl/rsa.h>
#include <openssl/engine.h>
#include <openssl/pem.h>

#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <pwd.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/stat.h>


// minimum secure key length
#ifndef GDP_MIN_KEY_LEN
# define GDP_MIN_KEY_LEN		1024
#endif // GDP_MIN_KEY_LEN

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl-create", "Create new log app");


const char *
select_logd_name(void)
{
	const char *p;

	p = ep_adm_getstrparam("swarm.gdp.gcl-create.server", NULL);
	if (p == NULL)
	{
		// seed the random number generator
		struct timeval tv;
		gettimeofday(&tv, NULL);
		srandom(tv.tv_usec);

		// default to one of the berkeley servers
		int r = random() % 4 + 1;
		int l = sizeof "edu.berkeley.eecs.gdp-00.gdplogd" + 1;
		char *nbuf;

		nbuf = ep_mem_malloc(l);
		snprintf(nbuf, l, "edu.berkeley.eecs.gdp-0%d.gdplogd", r);
		p = nbuf;
	}

	ep_dbg_cprintf(Dbg, 3, "Using log server %s\n", p);

	return p;
}

// command line options
#define OPTIONS		"b:c:C:D:e:G:h:k:K:qs:"

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-C creator] [-D dbgspec] [-e key_enc_alg]\n"
			"\t[-G gdpd_addr] [-q] [-s logd_name]\n"
			"\t[-h] [-k keytype] [-K keyfile] [-b keybits] [-c curve]\n"
			"\t[<mdid>=<metadata>...] [gcl_name]\n"
			"    -C  set name of log creator (owner; for metadata)\n"
			"    -D  set debugging flags\n"
			"    -e  set secret key encryption algorithm\n"
			"    -G  IP host to contact for GDP router\n"
			"    -q  don't give error if log already exists\n"
			"    -s  set name of log daemon on which to create the log\n"
			"    -h  set the hash (message digest) algorithm (defaults to sha256)\n"
			"    -k  type of key; valid key types are \"rsa\", \"dsa\", and \"ec\"\n"
			"\t(defaults to ec); \"none\" turns off key generation\n"
			"    -K  use indicated public key/place to write secret key\n"
			"\tIf -K specifies a directory, a .pem file is written there\n"
			"\twith the name of the GCL (defaults to \"KEYS\" or \".\")\n"
			"    -b  set size of key in bits (RSA and DSA only)\n"
			"    -c  set curve name (EC only)\n"
			"    creator is the id of the creator, formatted as an email address\n"
			"    logd_name is the name of the log server to host this log\n"
			"    metadata ids are (by convention) four letters or digits\n"
			"    gcl_name is the name of the log to create\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	gdp_name_t gcliname;			// internal name of GCL
	const char *gclxname = NULL;	// external name of GCL
	gdp_name_t logdiname;			// internal name of log daemon
	const char *logdxname = NULL;	// external name of log daemon
	const char *cname = NULL;		// creator/owner name (for metadata)
	gdp_gcl_t *gcl = NULL;
	gdp_gclmd_t *gmd = NULL;
	int opt;
	EP_STAT estat;
	char *gdpd_addr = NULL;
	char buf[200];
	bool show_usage = false;
	bool make_new_key = true;
	bool quiet = false;
	int md_alg_id = -1;
	int keytype = EP_CRYPTO_KEYTYPE_UNKNOWN;
	int keylen = 0;
	int exponent = 0;
	const char *curve = NULL;
	const char *keyfile = NULL;
	int keyform = EP_CRYPTO_KEYFORM_PEM;
	int key_enc_alg = -1;
	char *passwd = NULL;
	EP_CRYPTO_KEY *key = NULL;
	char *p;

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

	// collect command-line arguments
	while ((opt = getopt(argc, argv, OPTIONS)) > 0)
	{
		switch (opt)
		{
		 case 'b':
			keylen = atoi(optarg);
			break;

		 case 'c':
			curve = optarg;
			break;

		 case 'C':
			cname = optarg;
			break;

		 case 'D':
			// already done
			break;

		 case 'e':
			key_enc_alg = ep_crypto_keyenc_byname(optarg);
			if (key_enc_alg < 0)
			{
				ep_app_error("unknown key encryption algorithm %s", optarg);
				show_usage = true;
			}
			break;

		 case 'G':
			gdpd_addr = optarg;
			break;

		 case 'h':
			md_alg_id = ep_crypto_md_alg_byname(optarg);
			if (md_alg_id < 0)
			{
				ep_app_error("unknown digest hash algorithm %s", optarg);
				show_usage = true;
			}
			break;

		 case 'k':
			if (strcasecmp(optarg, "none") == 0)
			{
				make_new_key = false;
				break;
			}
			keytype = ep_crypto_keytype_byname(optarg);
			if (keytype == EP_CRYPTO_KEYTYPE_UNKNOWN)
			{
				ep_app_error("unknown key type %s", optarg);
				show_usage = true;
			}
			break;

		 case 'K':
			keyfile = optarg;
			break;

		 case 'q':
			quiet = true;
			break;

		 case 's':
			logdxname = optarg;
			break;

		 default:
			show_usage = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage || argc < 0)
		usage();

	// collect any metadata
	gmd = gdp_gclmd_new(0);
	while (argc > 0 && (p = strchr(argv[0], '=')) != NULL)
	{
		gdp_gclmd_id_t mdid = 0;
		int i;

		p++;
		for (i = 0; i < 4; i++)
		{
			if (argv[0][i] == '=')
				break;
			mdid = (mdid << 8) | (unsigned) argv[0][i];
		}

		gdp_gclmd_add(gmd, mdid, strlen(p), p);

		argc--;
		argv++;
	}

	// back compatibility for old calling convention
	if (argc == 2 && logdxname == NULL)
	{
		logdxname = *argv++;
		argc--;
	}

	// if we don't have a log daemon, pick one
	if (logdxname == NULL)
		logdxname = select_logd_name();

	gdp_parse_name(logdxname, logdiname);

	// name is optional ; if omitted one will be created
	if (argc-- > 0)
		gclxname = *argv++;

	if (show_usage || argc > 0)
		usage();

	// initialize the GDP library
	estat = gdp_init(gdpd_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	// allow thread to settle to avoid interspersed debug output
	ep_time_nanosleep(INT64_C(100000000));

	if (gclxname != NULL)
	{
		gdp_parse_name(gclxname, gcliname);

		// make sure it doesn't already exist
		estat = gdp_gcl_open(gcliname, GDP_MODE_RO, NULL, &gcl);
		if (EP_STAT_ISOK(estat))
		{
			// oops, we shouldn't be able to open it
			(void) gdp_gcl_close(gcl);
			if (!quiet)
				ep_app_error("Cannot create %s: already exists", gclxname);
			exit(EX_CANTCREAT);
		}
	}

	/**************************************************************
	**  Set up other automatic metadata
	*/

	// creation time
	{
		EP_TIME_SPEC tv;
		char timestring[40];

		ep_time_now(&tv);
		ep_time_format(&tv, timestring, sizeof timestring, EP_TIME_FMT_DEFAULT);
		gdp_gclmd_add(gmd, GDP_GCLMD_CTIME, strlen(timestring), timestring);
	}

	// creator id
	{
		char cnamebuf[200];

		if (cname == NULL)
		{
			/*
			**  Find the name of the creator.
			*/

			// user name (from password file)
			char *uname;
			char unamebuf[40];
			struct passwd *pw = getpwuid(getuid());

			if (pw != NULL)
			{
				uname = pw->pw_name;
			}
			else
			{
				snprintf(unamebuf, sizeof unamebuf, "%d", getuid());
				uname = unamebuf;
			}

			// fully qualified domain name
			//  (Linux sets HOST_NAME_MAX too low, so we use a magic constant)
			char fqdn[1025];

			// gethostname doesn't guarantee null termination
			fqdn[sizeof fqdn - 1] = '\0';
			if (gethostname(fqdn, sizeof fqdn - 1) != 0)
			{
				ep_app_error("Cannot find current host name");
				strlcpy(fqdn, "localhost", sizeof fqdn);
			}

			if (strchr(fqdn, '.') == NULL)
			{
				// need to tack on a domain name
				struct addrinfo hints, *ai;
				int i;

				memset(&hints, 0, sizeof hints);
				hints.ai_family = AF_UNSPEC;
				hints.ai_socktype = SOCK_STREAM;
				hints.ai_flags = AI_CANONNAME;

				if ((i = getaddrinfo(fqdn, NULL, &hints, &ai)) != 0)
				{
					ep_app_error("Cannot find DNS domain name: %s",
							gai_strerror(i));
				}
				else
				{
					strlcpy(fqdn, ai->ai_canonname, sizeof fqdn);
				}
			}

			// now put it together
			snprintf(cnamebuf, sizeof cnamebuf, "%s@%s", uname, fqdn);
			cname = cnamebuf;
		}
		ep_dbg_cprintf(Dbg, 1, "Creating log as %s\n", cname);
		gdp_gclmd_add(gmd, GDP_GCLMD_CID, strlen(cname), cname);
	}


	/**************************************************************
	**  Do cryptographic setup (e.g., generating keys)
	*/

	if (key_enc_alg < 0)
	{
		const char *p = ep_adm_getstrparam("swarm.gdp.crypto.keyenc.alg",
								"aes192");
		key_enc_alg = ep_crypto_keyenc_byname(p);
		if (key_enc_alg < 0)
		{
			ep_app_error("unknown secret key encryption algorithm %s", p);
			exit(EX_USAGE);
		}
	}

	if (md_alg_id < 0)
	{
		const char *p = ep_adm_getstrparam("swarm.gdp.crypto.hash.alg",
								"sha256");
		md_alg_id = ep_crypto_md_alg_byname(p);
		if (md_alg_id < 0)
		{
			ep_app_error("unknown digest hash algorithm %s", p);
			exit(EX_USAGE);
		}
	}
	if (keytype == EP_CRYPTO_KEYTYPE_UNKNOWN)
	{
		const char *p = ep_adm_getstrparam("swarm.gdp.crypto.sign.alg", "ec");
		keytype = ep_crypto_keytype_byname(p);
		if (keytype == EP_CRYPTO_KEYTYPE_UNKNOWN)
		{
			ep_app_error("unknown keytype %s", p);
			exit(EX_USAGE);
		}
	}
	switch (keytype)
	{
	case EP_CRYPTO_KEYTYPE_RSA:
		if (keylen <= 0)
			keylen = ep_adm_getintparam("swarm.gdp.crypto.rsa.keylen", 2048);
		exponent = ep_adm_getintparam("swarm.gdp.crypto.rsa.keyexp", 3);
		break;

	case EP_CRYPTO_KEYTYPE_DSA:
		if (keylen <= 0)
			keylen = ep_adm_getintparam("swarm.gdp.crypto.dsa.keylen", 2048);
		break;

	case EP_CRYPTO_KEYTYPE_EC:
		if (curve == NULL)
			curve = ep_adm_getstrparam("swarm.gdp.crypto.ec.curve", NULL);
		break;
	}

	// see if we have an existing key
	bool keyfile_is_directory = false;
	if (keyfile == NULL)
	{
		// nope -- create a key in current directory if requested
		if (make_new_key)
		{
			struct stat st;

			keyfile_is_directory = true;
			keyfile = ep_adm_getstrparam("swarm.gdp.crypto.key.dir", "KEYS");
			if (stat(keyfile, &st) != 0 || (st.st_mode & S_IFMT) != S_IFDIR)
				keyfile = ".";
		}
	}
	else
	{
		// might be a directory
		struct stat st;
		FILE *key_fp;

		if (stat(keyfile, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR)
		{
			keyfile_is_directory = true;
		}
		else if ((key_fp = fopen(keyfile, "r")) == NULL)
		{
			// no existing key file; OK if we are creating it
			if (!make_new_key)
			{
				ep_app_error("Could not read secret key file %s", keyfile);
				exit(EX_NOINPUT);
			}
		}
		else
		{
			key = ep_crypto_key_read_fp(key_fp, keyfile,
						keyform, EP_CRYPTO_F_SECRET);
			if (key == NULL)
			{
				ep_app_error("Could not read secret key %s", keyfile);
				exit(EX_DATAERR);
			}
			else
			{
				make_new_key = false;
			}
			fclose(key_fp);
		}
	}

	// if creating new key, go ahead and do it (in memory)
	if (make_new_key)
	{
		if (keylen < GDP_MIN_KEY_LEN && keytype != EP_CRYPTO_KEYTYPE_EC)
		{
			ep_app_error("Insecure key length %d; %d min",
					keylen, GDP_MIN_KEY_LEN);
			exit(EX_UNAVAILABLE);
		}
		key = ep_crypto_key_create(keytype, keylen, exponent, curve);
		if (key == NULL)
		{
			ep_app_error("Could not create new key");
			exit(EX_UNAVAILABLE);
		}
	}

	/*
	**	Now would be a good time to write the secret key to disk
	**		We do this before creating GCL so we don't risk having
	**		GCLs that exist with no access possible.  But we need
	**		to use a temporary name because we don't know the final
	**		name of the GCL yet.
	*/

	char *tempkeyfile = NULL;
	if (make_new_key)
	{
		const char *localkeyfile = keyfile;

		// it might be a directory, in which case we use a temporary name
		if (keyfile_is_directory)
		{
			size_t len;
			gdp_pname_t pbuf;
			gdp_name_t tempname;

			// use random name for the temp file
			evutil_secure_rng_get_bytes(tempname, sizeof tempname);
			gdp_printable_name(tempname, pbuf);
			len = strlen(keyfile) + sizeof pbuf + 6;
			tempkeyfile = ep_mem_malloc(len);
			snprintf(tempkeyfile, len, "%s/%s.pem", keyfile, pbuf);
			localkeyfile = tempkeyfile;
		}

		int fd;
		FILE *fp;

		if ((fd = open(localkeyfile, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0 ||
				(fp = fdopen(fd, "w")) == NULL)
		{
			ep_app_error("Cannot create %s", localkeyfile);
			exit(EX_IOERR);
		}

		estat = ep_crypto_key_write_fp(key, fp, EP_CRYPTO_KEYFORM_PEM,
						key_enc_alg, passwd, EP_CRYPTO_F_SECRET);

		// TODO: should really clear the fp buffer memory here to
		//		 avoid exposing any secret key information after free
		fclose(fp);

		if (!EP_STAT_ISOK(estat))
		{
			ep_app_abort("Couldn't write secret key to %s",
					localkeyfile);
		}
	}

	// at this point, key is initialized and secret is on disk
	if (key != NULL)
	{
		// add the public key to the metadata
		uint8_t der_buf[EP_CRYPTO_MAX_DER + 4];
		uint8_t *derp = der_buf + 4;

		der_buf[0] = md_alg_id;
		der_buf[1] = keytype;
		der_buf[2] = (keylen >> 8) & 0xff;
		der_buf[3] = keylen & 0xff;
		estat = ep_crypto_key_write_mem(key, derp, EP_CRYPTO_MAX_DER,
						EP_CRYPTO_KEYFORM_DER, key_enc_alg, NULL,
						EP_CRYPTO_F_PUBLIC);
		if (!EP_STAT_ISOK(estat))
		{
			ep_app_error("Could not create DER format public key");
			exit(EX_SOFTWARE);
		}

		gdp_gclmd_add(gmd, GDP_GCLMD_PUBKEY,
				EP_STAT_TO_INT(estat) + 4, der_buf);
	}

	/*
	**  Hello sailor, this is where the actual creation happens
	*/

	gdp_parse_name(logdxname, logdiname);
	if (gclxname == NULL)
	{
		// create a new GCL handle with a new name based on metadata
		estat = gdp_gcl_create(NULL, logdiname, gmd, &gcl);
	}
	else
	{
		// save the external name as metadata
		if (gmd == NULL)
			gmd = gdp_gclmd_new(0);
		gdp_gclmd_add(gmd, GDP_GCLMD_XID, strlen(gclxname), gclxname);

		// create a GCL with the provided name
		estat = gdp_gcl_create(gcliname, logdiname, gmd, &gcl);
	}
	EP_STAT_CHECK(estat, goto fail1);

	/*
	**  Rename the secret key file to the new name.
	*/

	if (tempkeyfile != NULL)
	{
		char *finalkeyfile = NULL;
		size_t len;
		gdp_pname_t pbuf;

		gdp_printable_name(gcliname, pbuf);
		len = strlen(keyfile) + sizeof pbuf + 6;
		finalkeyfile = ep_mem_malloc(len);
		snprintf(finalkeyfile, len, "%s/%s.pem", keyfile, pbuf);
		if (rename(tempkeyfile, finalkeyfile) != 0)
		{
			ep_app_abort("Cannot rename %s to %s", tempkeyfile, finalkeyfile);
		}

		printf("New secret key file is in %s\n", finalkeyfile);
		printf("Save this file!  You'll need it to write the new log\n");

		ep_mem_free(finalkeyfile);
		ep_mem_free(tempkeyfile);
		tempkeyfile = NULL;
	}

	// just for a lark, let the user know the (internal) name
	if (!quiet)
	{
		gdp_pname_t pname;

		printf("Created new GCL %s\n\ton log server %s\n",
				gdp_printable_name(*gdp_gcl_getname(gcl), pname), logdxname);
	}

	gdp_gcl_close(gcl);

fail1:
	// free metadata, if set
	if (gmd != NULL)
		gdp_gclmd_free(gmd);

fail0:
	// OK status can have values; hide that from the user
	if (EP_STAT_ISOK(estat))
		estat = EP_STAT_OK;
	if (!quiet)
		fprintf(stderr, "exiting with status %s\n",
				ep_stat_tostr(estat, buf, sizeof buf));
	return !EP_STAT_ISOK(estat);
}
