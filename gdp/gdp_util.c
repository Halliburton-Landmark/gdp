/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP Utility routines
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
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

#include "gdp.h"
#include "gdp_gclmd.h"

#include <event2/event.h>

#include <ep/ep_crypto.h>
#include <ep/ep_dbg.h>

//static EP_DBG	Dbg = EP_DBG_INIT("gdp.util", "GDP utility routines");


/*
**   _GDP_NEWNAME --- create a new GDP name
**
**		Really just creates a random number (for now).
*/

void
_gdp_newname(gdp_name_t gname, gdp_gclmd_t *gmd)
{
	if (gmd == NULL)
	{
		// last resort: use random bytes
		evutil_secure_rng_get_bytes(gname, sizeof (gdp_name_t));
	}
	else
	{
		uint8_t *mdbuf;
		size_t mdlen = _gdp_gclmd_serialize(gmd, &mdbuf);

		ep_crypto_md_sha256(mdbuf, mdlen, gname);
		ep_mem_free(mdbuf);
	}
}
