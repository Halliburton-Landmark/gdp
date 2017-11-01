/* vim: set ai sw=8 sts=8 ts=8 :*/

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

#include "ep.h"
#include "ep_uuid.h"

#include <string.h>

#if EP_OSCF_HAS_BSD_UUID
static EP_STAT
uuid_stat_to_ep_stat(uint32_t uustat)
{
	switch (uustat)
	{
	  case uuid_s_ok:
		return EP_STAT_OK;
	  case uuid_s_bad_version:
		return EP_STAT_UUID_VERSION;
	  case uuid_s_invalid_string_uuid:
		return EP_STAT_UUID_PARSE_ERROR;
	  case uuid_s_no_memory:
		return EP_STAT_OUT_OF_MEMORY;
	  default:
		return EP_STAT_SOFTWARE_ERROR;
	}
}
#endif

EP_STAT
ep_uuid_generate(EP_UUID *uu)
{
#if EP_OSCF_HAS_BSD_UUID
	uint32_t status;
	uuid_create(&uu->uu, &status);
	return uuid_stat_to_ep_stat(status);
#else
	uuid_generate(uu->uu);
	return EP_STAT_OK;
#endif
}

EP_STAT
ep_uuid_tostr(EP_UUID *uu, EP_UUID_STR str)
{
#if EP_OSCF_HAS_BSD_UUID
	uint32_t status;
	char *s = NULL;

	uuid_to_string(&uu->uu, &s, &status);
	if (s != NULL)
	{
		memcpy(str, s, sizeof(EP_UUID_STR));
		free(s);
	}
	return uuid_stat_to_ep_stat(status);
#else
	uuid_unparse(uu->uu, str);
	return EP_STAT_OK;
#endif
}


EP_STAT
ep_uuid_parse(EP_UUID *uu, EP_UUID_STR str)
{
#if EP_OSCF_HAS_BSD_UUID
	uint32_t status;
	uuid_from_string(str, &uu->uu, &status);
	return uuid_stat_to_ep_stat(status);
#else
	int status = uuid_parse(str, uu->uu);
	if (status == 0)
		return EP_STAT_OK;
	return EP_STAT_UUID_PARSE_ERROR;
#endif
}

EP_STAT
ep_uuid_clear(EP_UUID *uu)
{
#if EP_OSCF_HAS_BSD_UUID
	uint32_t status;
	uuid_create_nil(&uu->uu, &status);
	return uuid_stat_to_ep_stat(status);
#else
	uuid_clear(uu->uu);
	return EP_STAT_OK;
#endif
}

bool
ep_uuid_is_null(EP_UUID *uu)
{
#if EP_OSCF_HAS_BSD_UUID
	uint32_t status;		// does this ever get set?
	return uuid_is_nil(&uu->uu, &status) != 0;
#else
	return uuid_is_null(uu->uu) != 0;
#endif
}

int
ep_uuid_compare(EP_UUID *uu1, EP_UUID *uu2)
{
#if EP_OSCF_HAS_BSD_UUID
	uint32_t status;		// no way to return status, if any
	return uuid_compare(&uu1->uu, &uu2->uu, &status);
#else
	return uuid_compare(uu1->uu, uu2->uu);
#endif
}
