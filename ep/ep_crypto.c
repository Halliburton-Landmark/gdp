/* vim: set ai sw=4 sts=4 ts=4 : */

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2017, Eric P. Allman.  All rights reserved.
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
**  ----- END LICENSE BLOCK -----
***********************************************************************/

#include <ep/ep.h>
#include <ep_crypto.h>
#include <ep_dbg.h>

#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>


//static EP_DBG	Dbg = EP_DBG_INIT("libep.crypto", "crypto support");

/*
**  LIBEP CRYPTOGRAPHIC SUPPORT
*/


/*
**  Initialize crypto support.
**		This basically loads everything.  If running in a small
**		address space you might want to adjust this.
*/

void
ep_crypto_init(uint32_t flags)
{
	static bool initialized = false;

	if (initialized)
		return;
	OPENSSL_load_builtin_modules();
	OpenSSL_add_all_algorithms();

	if (ep_adm_getboolparam("libep.crypto.dev", true))
		ENGINE_load_cryptodev();
	initialized = true;
}


/*
**  Internal helper for printing errors.
*/

void *
_ep_crypto_error(const char *msg, ...)
{
	va_list ap;
	FILE *fp = ep_dbg_getfile();
	static bool initialized = false;

	// load openssl error strings if not already done
	if (!initialized)
	{
		ERR_load_crypto_strings();
		initialized = true;
	}

	//XXX should be on some flag (don't print unconditionally)
	va_start(ap, msg);
	ep_dbg_printf("EP Crypto Error: ");
	vfprintf(fp, msg, ap);
	ep_dbg_printf("\n");
	ERR_print_errors_fp(fp);

	return NULL;
}
