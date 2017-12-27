/* vim: set ai sw=4 sts=4 ts=4 : */

/*
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
**  GCL_HELPER --- common utility functions used in handling gcl 
** Written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr)
** last modified : 2016.12.30
*/


#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// LATER check: ep_dbg vs. ep_app
#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_time.h>
#include <ep/ep_string.h>

#include <hs/hs_errno.h>
#include "gcl_helper.h"


const char* select_logd_name(const char* a_input)
{

	if( a_input != NULL ) return a_input;

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

	return p;
}


void add_time_in_gclmd( gdp_gclmd_t  *a_gmd )
{
	// creation time
	EP_TIME_SPEC		tv;
	char				timestring[40];

	ep_time_now(&tv);
	ep_time_format(&tv, timestring, sizeof timestring, EP_TIME_FMT_DEFAULT);
	gdp_gclmd_add( a_gmd, GDP_GCLMD_CTIME, strlen(timestring), timestring);
}


void gen_creatorId( char *a_oBuf, int a_buflen ) 
{
	//Find the name of the creator.

	// user name (from password file)
	char				*uname;
	char				unamebuf[40];
	struct passwd *pw = getpwuid(getuid());

	if (pw != NULL) uname = pw->pw_name;
	else {
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
	snprintf( a_oBuf, a_buflen, "%s@%s", uname, fqdn);
}


void add_creator_in_gclmd( gdp_gclmd_t  *a_gmd )
{
	char cnamebuf[200];

	gen_creatorId( cnamebuf, 200); 

	printf("Creating log as %s\n", cnamebuf);
	gdp_gclmd_add(a_gmd, GDP_GCLMD_CID, strlen(cnamebuf), cnamebuf);
}


EP_CRYPTO_KEY* make_new_asym_key( int a_keytype, int *o_keylen )
{
	int					keytype	= a_keytype;
	int					keylen	= 0;
	int					exponent= 0;
	const char			*curve	= NULL;
	EP_CRYPTO_KEY		*newKey = NULL;


	if( keytype == EP_CRYPTO_KEYTYPE_UNKNOWN )
	{
		const char *p	= ep_adm_getstrparam("swarm.gdp.crypto.sign.alg", "ec");
		keytype		= ep_crypto_keytype_byname(p);
		if (keytype == EP_CRYPTO_KEYTYPE_UNKNOWN)
		{
			ep_app_error("unknown keytype %s", p);
			return NULL;
		}
	}

	// set major parm for each key type. 
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


	// Make new key  
	if (keylen < GDP_MIN_KEY_LEN && keytype != EP_CRYPTO_KEYTYPE_EC)
	{
		ep_app_error("Insecure key length %d; %d min",
					keylen, GDP_MIN_KEY_LEN);
		return NULL;
	}

	newKey = ep_crypto_key_create(keytype, keylen, exponent, curve);
	*o_keylen = keylen;
	
	return newKey;
}


const char* get_keydir(const char *a_keydir)
{
	const char		*t_keydir = NULL; 
	struct stat		t_st;


	if( a_keydir == NULL )
		t_keydir = ep_adm_getstrparam("swarm.gdp.crypto.key.dir", "KEYS");
	else t_keydir = a_keydir;

	if (stat(t_keydir, &t_st) != 0 || (t_st.st_mode & S_IFMT) != S_IFDIR)
		t_keydir = ".";

	return t_keydir;
}


// write secret key in temporary file 
char*  write_secret_key(EP_CRYPTO_KEY *a_Key, const char *a_keydir, int a_key_enc_alg)
{
	int				fd;
	FILE			*fp;
	char			*tempkeyfile = NULL;
	const char		*localkeyfile;
	char			*passwd = NULL;

	size_t			len;
	gdp_pname_t		pbuf;
	gdp_name_t		tempname;

	EP_STAT			estat;


	// use random name for the temp file
	evutil_secure_rng_get_bytes(tempname, sizeof tempname);
	gdp_printable_name(tempname, pbuf);
	len = strlen( a_keydir ) + sizeof pbuf + 6;
	tempkeyfile = ep_mem_malloc(len);
	snprintf(tempkeyfile, len, "%s/%s.pem", a_keydir, pbuf);
	localkeyfile = tempkeyfile;

	
	if ((fd = open(localkeyfile, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0 ||
				(fp = fdopen(fd, "w")) == NULL)
	{
		ep_app_error("Cannot create %s", localkeyfile);
		ep_mem_free( tempkeyfile );
		return NULL;	
	}

	estat = ep_crypto_key_write_fp( a_Key, fp, EP_CRYPTO_KEYFORM_PEM,
						a_key_enc_alg, passwd, EP_CRYPTO_F_SECRET);

	// TODO: should really clear the fp buffer memory here to
	//		 avoid exposing any secret key information after free
	fclose(fp);

	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("Couldn't write secret key to %s",
					localkeyfile);

		ep_mem_free( tempkeyfile );
		return NULL;	
	}

	return tempkeyfile;
}


int add_pubkey_in_gclmd( gdp_gclmd_t  *a_gmd, EP_CRYPTO_KEY *a_wkey, 
						int a_keytype, int a_keylen, int a_key_enc_alg )
{
	int			md_alg_id = -1;
	EP_STAT		estat;
	const char  *p = ep_adm_getstrparam("swarm.gdp.crypto.hash.alg",
								"sha256");
								
	md_alg_id = ep_crypto_md_alg_byname(p);
	if (md_alg_id < 0)
	{
		ep_app_error("unknown digest hash algorithm %s", p);
		return EX_USAGE;
	}

	// add the public key to the metadata
	uint8_t der_buf[EP_CRYPTO_MAX_DER + 4];
	uint8_t *derp = der_buf + 4;

	der_buf[0] = md_alg_id;
	der_buf[1] = a_keytype;
	der_buf[2] = (a_keylen >> 8) & 0xff;
	der_buf[3] = a_keylen & 0xff;
	estat = ep_crypto_key_write_mem(a_wkey, derp, EP_CRYPTO_MAX_DER,
					EP_CRYPTO_KEYFORM_DER, a_key_enc_alg, NULL,
					EP_CRYPTO_F_PUBLIC);

	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("Could not create DER format public key");
		return EX_SOFTWARE;
	}

	gdp_gclmd_add( a_gmd, GDP_GCLMD_PUBKEY,
				EP_STAT_TO_INT(estat) + 4, der_buf);

	return 0;
}


char*  merge_fileName( const char *a_keydir, size_t a_klen, gdp_pname_t a_keyname ) 
{
	size_t			len;
	char			*t_finalkeyfile = NULL;


	len = strlen(a_keydir) + a_klen + 6;
	t_finalkeyfile = ep_mem_malloc(len);
	snprintf(t_finalkeyfile, len, "%s/%s.pem", a_keydir, a_keyname);

	return t_finalkeyfile;
}


// write secret key in temporary file 
char*  rename_secret_key( gdp_gcl_t *a_gcl, const char *a_keydir, 
							const char *a_oldpath)
{
	char			*t_finalkeyfile = NULL;
	gdp_pname_t		pbuf;


	gdp_printable_name(*gdp_gcl_getname(a_gcl), pbuf);
	t_finalkeyfile = merge_fileName( a_keydir, sizeof pbuf, pbuf );

	if (rename(a_oldpath, t_finalkeyfile) != 0)
	{
		ep_app_error("Cannot rename %s to %s", a_oldpath, t_finalkeyfile);
		ep_mem_free(t_finalkeyfile);
		return NULL;
	}

	return t_finalkeyfile; 
}


EP_STAT
signkey_cb(
		gdp_name_t gname,
		void *udata,
		EP_CRYPTO_KEY **skeyp)
{
	FILE *fp;
	EP_CRYPTO_KEY *skey;
	const char *signing_key_file = udata;

	fp = fopen(signing_key_file, "r");
	if (fp == NULL)
	{
		ep_app_error("cannot open signing key file %s", signing_key_file);
		return ep_stat_from_errno(errno);
	}

	skey = ep_crypto_key_read_fp(fp, signing_key_file,
			EP_CRYPTO_KEYFORM_PEM, EP_CRYPTO_F_SECRET);
	if (skey == NULL)
	{
		ep_app_error("cannot read signing key file %s", signing_key_file);
		return ep_stat_from_errno(errno);
	}

	*skeyp = skey;
	return EP_STAT_OK;
}
// hsmoon end 



int gdp_gclmd_find_wLen( gdp_gclmd_t *a_gmd, gdp_gclmd_id_t a_id, 
							int *inoutLen,	char **a_outval )
{
	int				index;


	ep_dbg_printf("[INFO] Gclmd_find int, gmd = %p, id = %08x .. \n", 
					a_gmd, a_id );

	if( a_gmd == NULL ) goto fail0;


	for( index=0; index<(a_gmd->nused); index++ ) 
	{
		if( a_id != a_gmd->mds[index].md_id ) continue;
	
		if( a_gmd->mds[index].md_len > *inoutLen ) {
			ep_dbg_printf("[WARN] Wrong gmdID length %d over %d at %08x \n", 
					a_gmd->mds[index].md_len, *inoutLen, a_id ); 

			return EX_DATAERR;  
		} 

		*inoutLen =	a_gmd->mds[index].md_len;
		*a_outval =  a_gmd->mds[index].md_data; 
		break;
	}


	if( index >= a_gmd->nused ) {
fail0: 
		ep_dbg_printf("[WARN] Not found gclmd_int \n" ); 
		return EX_NOTFOUND;

	} else  return EX_OK;
	
}


EP_STAT
gdp_gclmd_find_uint( gdp_gclmd_t *a_gmd, gdp_gclmd_id_t a_id, 
						uint32_t *a_outval )
{
	int				index;
	uint32_t		tmpval32;
	EP_STAT			estat = EP_STAT_OK;


	ep_dbg_printf("[INFO] Gclmd_find int, gmd = %p, id = %08x .. \n", 
					a_gmd, a_id );

	if( a_gmd == NULL ) goto fail0;


	for( index=0; index<(a_gmd->nused); index++ ) 
	{
		if( a_id != a_gmd->mds[index].md_id ) continue;
	
		if( a_gmd->mds[index].md_len != 4 ) {
			ep_dbg_printf("[WARN] Wrong gmdID length %d at %08x \n", 
					a_gmd->mds[index].md_len, a_id ); 

			return GDP_STAT_NOTFOUND; //later return/define other stat 
		} 

		memcpy( &tmpval32, a_gmd->mds[index].md_data, 4 );
		*a_outval = ntohl( tmpval32 ); 
		break;
	}


	if( index >= a_gmd->nused ) {
fail0: 
		ep_dbg_printf("[WARN] Not found gclmd_int \n" ); 
		return GDP_STAT_NOTFOUND;

	} else  return estat;
	
}


int find_gdp_gclmd_uint( gdp_gcl_t *a_gcl, gdp_gclmd_id_t a_id, 
							uint32_t *a_outval )
{
	gdp_gclmd_t			*gmd;
	EP_STAT				estat = EP_STAT_OK;


	*a_outval	= 0;

	estat = gdp_gcl_getmetadata( a_gcl, &gmd);
	if( !EP_STAT_ISOK( estat ) )  {
		return EX_FAILGETDATA;
	}

	estat = gdp_gclmd_find_uint( gmd, a_id, a_outval );
	if( !EP_STAT_ISOK( estat ) )  {
		return EX_FAILGETDATA;
	}

	return EX_OK; 
	
}

int find_gdp_gclmd_char( gdp_gclmd_t *inMd, gdp_gclmd_id_t inID, 
							char *outVal )
{
	int				index;


	ep_dbg_printf("[INFO] Gclmd_find char, gmd = %p, id = %08x .. \n", 
					inMd, inID );
	if( inMd == NULL ) return EX_NOTFOUND; 


	for( index=0; index<(inMd->nused); index++ ) 
	{
		if( inID != inMd->mds[index].md_id ) continue;
	
		if( inMd->mds[index].md_len != 1 ) {
			ep_dbg_printf("[WARN] Wrong gmdID length %d at %08x \n", 
					inMd->mds[index].md_len, inID ); 

			return EX_NOTFOUND; //later return/define other stat 
		} 

		memcpy( outVal, inMd->mds[index].md_data, 1 );
		break;
	}


	if( index >= inMd->nused ) {
		ep_dbg_printf("[WARN] Not found gclmd_int \n" ); 
		return EX_NOTFOUND;
	} else  return EX_OK;
}

