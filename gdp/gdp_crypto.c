/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  ----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
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
**  ----- END LICENSE BLOCK -----
*/

#include <ep/ep.h>
#include <ep/ep_crypto.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>

#include "gdp.h"

#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.crypto", "cryptographic operations for GDP");

/*
**  Read a secret key
**
**  This should have a better way of handling password-protected keys.
**  At the moment it uses the OpenSSL built-in "read from keyboard"
**  method.
*/

EP_CRYPTO_KEY *
_gdp_crypto_skey_read(const char *basename, const char *ext)
{
	const char *path = ep_adm_getstrparam("swarm.gdp.crypto.key.path",
			"."
			":KEYS"
			":~/.swarm/gdp/keys"
			":/usr/local/etc/gdp/keys"
			":/usr/local/etc/swarm/gdp/keys"
			":/etc/gdp/keys"
			":/etc/swarm/gdp/keys");
	char pbuf[1024];
	char *p = NULL;
	char *dir;
	int keyform = EP_CRYPTO_KEYFORM_PEM;
	int i;

	if ((i = strlen(basename)) > 4)
	{
		if (strcmp(&basename[i - 4], ".pem") == 0)
		{
			ext = NULL;
			keyform = EP_CRYPTO_KEYFORM_PEM;
		}
		else if (strcmp(&basename[i - 4], ".der") == 0)
		{
			ext = NULL;
			keyform = EP_CRYPTO_KEYFORM_DER;
		}
	}

	strlcpy(pbuf, path, sizeof pbuf);
	for (dir = pbuf; dir != NULL; dir = p)
	{
		FILE *fp;
		EP_CRYPTO_KEY *key;
		char fnbuf[512];
		const char *fmt;

		p = strchr(dir, ':');
		if (p != NULL)
			*p++ = '\0';
		if (*dir == '\0')
			continue;

		// create the candidate filename
		if (strncmp(dir, "~/", 2) == 0)
		{
			char *home = getenv("HOME");

			if (home == NULL)
				continue;
			if (ext == NULL)
				fmt = "%s%s/%s";
			else
				fmt = "%s%s/%s.%s";
			snprintf(fnbuf, sizeof fnbuf, fmt, home, dir + 1, basename, ext);
		}
		else
		{
			if (ext == NULL)
				fmt = "%s/%s";
			else
				fmt = "%s/%s.%s";
			snprintf(fnbuf, sizeof fnbuf, fmt, dir, basename, ext);
		}

		ep_dbg_cprintf(Dbg, 40,
				"_gdp_crypto_skey_read: trying %s\n",
				fnbuf);
		fp = fopen(fnbuf, "r");
		if (fp == NULL)
			continue;

		key = ep_crypto_key_read_fp(fp, fnbuf, keyform, EP_CRYPTO_F_SECRET);
		ep_dbg_cprintf(Dbg, 40,
				"_gdp_crypto_skey_read: found file, key = %p\n",
				key);
		fclose(fp);
		if (key != NULL)
			return key;
	}
	return NULL;
}
