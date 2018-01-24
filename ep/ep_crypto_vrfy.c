/* vim: set ai sw=8 sts=8 ts=8 : */

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

/*
**  Wrappers for cryptographic signature verification
**
**	These are direct analogues to the signing routines.
*/

#include <ep/ep.h>
#include <ep_crypto.h>


/*
**  Create a new cryptographic verification context.
*/

EP_CRYPTO_MD *
ep_crypto_vrfy_new(EP_CRYPTO_KEY *pkey, int md_alg_id)
{
	EP_CRYPTO_MD *md;
	const EVP_MD *md_alg;
	int istat;

	md_alg = _ep_crypto_md_getalg_byid(md_alg_id);
	if (md_alg == NULL)
	{
		(void) _ep_crypto_error(EP_STAT_CRYPTO_DIGEST,
				"unknown digest algorithm %d",
				md_alg_id);
		return NULL;
	}
	md = EVP_MD_CTX_create();
	if (md == NULL)
	{
		(void) _ep_crypto_error(EP_STAT_CRYPTO_DIGEST,
				"cannot create message digest for verification");
		return NULL;
}
	istat = EVP_DigestVerifyInit(md, NULL, md_alg, NULL, pkey);
	if (istat != 1)
	{
		ep_crypto_md_free(md);
		(void) _ep_crypto_error(EP_STAT_CRYPTO_DIGEST,
				"cannot initialize digest for verification");
		return NULL;
	}
	return md;
}


/*
**  Add data to verification context.
*/

EP_STAT
ep_crypto_vrfy_update(EP_CRYPTO_MD *md, void *dbuf, size_t dbufsize)
{
	int istat;

	istat = EVP_DigestVerifyUpdate(md, dbuf, dbufsize);
	if (istat != 1)
	{
		return _ep_crypto_error(EP_STAT_CRYPTO_VRFY,
				"cannot update verify digest");
	}
	return EP_STAT_OK;
}


/*
**  Finalize a verification context.
**
**	The caller passes in an existing signature.  This routine
**	checks to make sure that signature matches the data that has
**	been passed into the context.
*/

EP_STAT
ep_crypto_vrfy_final(EP_CRYPTO_MD *md, void *_obuf, size_t obufsize)
{
	uint8_t *obuf = (uint8_t *) _obuf;
	int istat;

	istat = EVP_DigestVerifyFinal(md, obuf, obufsize);
	if (istat == 1)
	{
		// success
		return EP_STAT_OK;
	}
	else if (istat == 0)
	{
		// signature verification failure
		return _ep_crypto_error(EP_STAT_CRYPTO_BADSIG,
				"signature invalid");
	}
	else
	{
		// more serious error
		return _ep_crypto_error(EP_STAT_CRYPTO_VRFY,
				"cannot finalize verify digest");
	}
}


/*
**  Free the verification context.
*/

void
ep_crypto_vrfy_free(EP_CRYPTO_MD *md)
{
	EVP_MD_CTX_destroy(md);
}
