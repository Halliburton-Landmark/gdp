/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  GDP GCL Metadata
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
#include "gdp_priv.h"
#include "gdp_gclmd.h"

#include <ep/ep_dbg.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_prflags.h>

#include <string.h>


#define MINMDS		4		// minimum number of metadata entries (must be > 3)

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl.metadata", "GCL metadata processing");


/*
**  GDP_GCLMD_NEW --- allocate space for in-client metadata
*/

gdp_gclmd_t *
gdp_gclmd_new(int nentries)
{
	gdp_gclmd_t *gmd;
	size_t len = sizeof *gmd;

	gmd = ep_mem_zalloc(len);
	if (nentries > MINMDS)
		gmd->nalloc = nentries;
	else
		gmd->nalloc = MINMDS;
	gmd->nused = 0;
	gmd->mds = ep_mem_zalloc(gmd->nalloc * sizeof *gmd->mds);
	ep_dbg_cprintf(Dbg, 21, "gdp_gclmd_new() => %p\n", gmd);
	return gmd;
}


/*
**  GDP_GCLMD_FREE --- deallocate space for in-client metadata
*/

void
gdp_gclmd_free(gdp_gclmd_t *gmd)
{
	int i;

	ep_dbg_cprintf(Dbg, 21, "gdp_gclmd_free(%p)\n", gmd);
	if (gmd == NULL)
		return;
	if (gmd->databuf != NULL)
		ep_mem_free(gmd->databuf);
	for (i = 0; i < gmd->nused; i++)
	{
		if (EP_UT_BITSET(MDF_OWNDATA, gmd->mds[i].md_flags))
			ep_mem_free(gmd->mds[i].md_data);
	}
	ep_mem_free(gmd->mds);
	ep_mem_free(gmd);
}


/*
**  GDP_GCLMD_ADD --- add a single metadatum to a metadata list
**
**		As a special case for use in gdplogd you can pass in NULL
**		data, which reserves the slot but not the data.  That
**		can be set later using gdp_gclmd_set.
*/

EP_STAT
gdp_gclmd_add(gdp_gclmd_t *gmd,
			gdp_gclmd_id_t id,
			size_t len,
			const void *data)
{
	EP_ASSERT_POINTER_VALID(gmd);

	if (ep_dbg_test(Dbg, 36))
	{
		ep_dbg_printf("gdp_gclmd_add(%08x, %zd, %p)\n", id, len, data);
		if (data != NULL)
			ep_hexdump(data, len, ep_dbg_getfile(), EP_HEXDUMP_ASCII, 0);
	}

	if (EP_UT_BITSET(GCLMDF_READONLY, gmd->flags))
		return GDP_STAT_READONLY;

	// end of list is implicit; just ignore explicit ones
	//XXX: return OK or INFO status code?
	if (id == GDP_GCLMD_EOLIST)
		return EP_STAT_OK;

	// see if we have room for another entry
	if (gmd->nused >= gmd->nalloc)
	{
		// no room; get some more (allocate 50% more than we have)
		gmd->nalloc = (gmd->nalloc / 2) * 3;
		gmd->mds = ep_mem_realloc(gmd->mds, gmd->nalloc * sizeof *gmd->mds);
	}

	gmd->mds[gmd->nused].md_id = id;
	gmd->mds[gmd->nused].md_len = len;
	if (data != NULL)
	{
		gmd->mds[gmd->nused].md_data = ep_mem_malloc(len);
		gmd->mds[gmd->nused].md_flags = MDF_OWNDATA;
		memcpy(gmd->mds[gmd->nused].md_data, data, len);
	}

	gmd->nused++;

	return EP_STAT_OK;
}


/*
**  _GDP_GCLMD_ADDDATA --- set the pointers into a data block
**
**		This is intended for internal use only.
**
**		XXX could do more error checking.
*/

void
_gdp_gclmd_adddata(gdp_gclmd_t *gmd,
			void *data)
{
	int i;

	EP_ASSERT_POINTER_VALID(gmd);

	gmd->databuf = data;
	for (i = 0; i < gmd->nused; i++)
	{
		if (ep_dbg_test(Dbg, 37))
		{
			ep_dbg_printf("metadata[%d] = %p\n", i, data);
			if (ep_dbg_test(Dbg, 40))
				ep_hexdump(data, gmd->mds[i].md_len, ep_dbg_getfile(),
						EP_HEXDUMP_ASCII, 0);
		}
		gmd->mds[i].md_data = data;
		data += gmd->mds[i].md_len;
	}
}



/*
**  GDP_GCLMD_GET --- get metadata from a metadata list by index
*/

EP_STAT
gdp_gclmd_get(gdp_gclmd_t *gmd,
			int indx,
			gdp_gclmd_id_t *id,
			size_t *len,
			const void **data)
{
	EP_STAT estat = EP_STAT_OK;

	// see if this id is allocated
	if (indx >= gmd->nused)
		return GDP_STAT_NOTFOUND;

	// copy out any requested fields
	if (id != NULL)
		*id = gmd->mds[indx].md_id;
	if (len != NULL)
		*len = gmd->mds[indx].md_len;
	if (data != NULL)
		*data = gmd->mds[indx].md_data;

	return estat;
}


/*
**  GDP_GCLMD_FIND --- get metadata from a metadata list by name
*/

EP_STAT
gdp_gclmd_find(gdp_gclmd_t *gmd,
		gdp_gclmd_id_t id,
		size_t *len,
		const void **data)
{
	int indx;

	ep_dbg_cprintf(Dbg, 40, "gdp_gclmd_find, gmd = %p, id = %08x... ", gmd, id);
	if (gmd == NULL)
		goto fail0;

	for (indx = 0; indx < gmd->nused; indx++)
	{
		if (id != gmd->mds[indx].md_id)
			continue;
		if (len != NULL)
			*len = gmd->mds[indx].md_len;
		if (data != NULL)
			*data = gmd->mds[indx].md_data;
		break;
	}
	if (indx >= gmd->nused)
	{
fail0:
		ep_dbg_cprintf(Dbg, 40, "not found\n");
		return GDP_STAT_NOTFOUND;
	}
	else
	{
		ep_dbg_cprintf(Dbg, 40, "len %" PRIu32 "\n", gmd->mds[indx].md_len);
		return EP_STAT_OK;
	}
}


/*
**  _GDP_GCLMD_SERIALIZE --- serialize metadata list into network order
*/

size_t
_gdp_gclmd_serialize(gdp_gclmd_t *gmd, gdp_buf_t *evb)
{
	int i;
	size_t slen = 0;

	if (gmd == NULL)
		return 0;

	// write the number of entries
	{
		uint16_t t16 = htons(gmd->nused);
		gdp_buf_write(evb, &t16, sizeof t16);
		slen += sizeof t16;
	}

	// for each metadata item, write the header
	for (i = 0; i < gmd->nused; i++)
	{
		uint32_t t32;

		t32 = htonl(gmd->mds[i].md_id);
		gdp_buf_write(evb, &t32, sizeof t32);
		t32 = htonl(gmd->mds[i].md_len);
		gdp_buf_write(evb, &t32, sizeof t32);
		slen = 2 * sizeof t32;
	}

	// now write out all the data
	for (i = 0; i < gmd->nused; i++)
	{
		if (gmd->mds[i].md_len > 0)
		{
			gdp_buf_write(evb, gmd->mds[i].md_data, gmd->mds[i].md_len);
			slen += gmd->mds[i].md_len;
		}
	}
	return slen;
}


/*
**  _GDP_GCLMD_DESERIALIZE --- crack open a gdp_buf_t and store the metadata
**
**		The data is serialized as a count of entries, that number
**		of metadata headers (contains the id and data length),
**		and finally the metadata itself.  This order is so that it's
**		easy to read it in without having to reallocate space.
*/

gdp_gclmd_t *
_gdp_gclmd_deserialize(gdp_buf_t *evb)
{
	int nmd = 0;		// number of metadata entries
	int tlen = 0;		// total data length
	gdp_gclmd_t *gmd;
	int i;

	// get the number of metadata entries
	{
		uint16_t t16;

		if (gdp_buf_getlength(evb) < sizeof t16)
			return NULL;
		gdp_buf_read(evb, &t16, sizeof t16);
		nmd = ntohs(t16);
		if (nmd == 0)
			return NULL;
	}

	// make sure we have at least all the headers
	if (gdp_buf_getlength(evb) < (2 * sizeof (uint32_t)))
		return NULL;

	// allocate and populate the header
	gmd = ep_mem_zalloc(sizeof *gmd);
	gmd->flags = GCLMDF_READONLY;
	gmd->nalloc = gmd->nused = nmd;

	// allocate and read in the metadata headers
	gmd->mds = ep_mem_malloc(nmd * sizeof *gmd->mds);
	for (i = 0; i < nmd; i++)
	{
		uint32_t t32;

		gdp_buf_read(evb, &t32, sizeof t32);
		gmd->mds[i].md_id = ntohl(t32);
		gdp_buf_read(evb, &t32, sizeof t32);
		tlen += gmd->mds[i].md_len = ntohl(t32);
		gmd->mds[i].md_flags = 0;
	}

	// and now for the data....
	gmd->databuf = ep_mem_malloc(tlen);
	if (gdp_buf_read(evb, gmd->databuf, tlen) != tlen)
	{
		// bad news
		ep_mem_free(gmd->databuf);
		ep_mem_free(gmd->mds);
		ep_mem_free(gmd);
		return NULL;
	}

	// we can now insert the pointers into the data
	void *dbuf = gmd->databuf;
	for (i = 0; i < nmd; i++)
	{
		gmd->mds[i].md_data = dbuf;
		dbuf += gmd->mds[i].md_len;
	}

	if (ep_dbg_test(Dbg, 24))
	{
		ep_dbg_printf("_gdp_gclmd_deserialize:\n  ");
		gdp_gclmd_print(gmd, ep_dbg_getfile(), 4);
	}

	return gmd;
}


static EP_PRFLAGS_DESC	GclmdFlags[] =
{
	{ GCLMDF_READONLY,	GCLMDF_READONLY,	"READONLY"		},
	{ 0,				0,					NULL			},
};

static EP_PRFLAGS_DESC	MdatumFlags[] =
{
	{ MDF_OWNDATA,		MDF_OWNDATA,		"OWNDATA"		},
	{ 0,				0,					NULL			},
};

void
gdp_gclmd_print(const gdp_gclmd_t *gmd, FILE *fp, int detail)
{
	if (detail > 1)
		fprintf(fp, "GCLMD@%p: ", gmd);
	if (gmd == NULL)
	{
		fprintf(fp, "NULL\n");
		return;
	}

	if (detail > 1)
	{
		fprintf(fp, "nalloc = %d, nused = %d, databuf = %p\n    flags = ",
				gmd->nalloc, gmd->nused, gmd->databuf);
		ep_prflags(gmd->flags, GclmdFlags, fp);
		fprintf(fp, "\n    mds = %p\n", gmd->mds);
		if (detail > 2)
		{
			int i;

			for (i = 0; i < gmd->nused; i++)
			{
				fprintf(fp, "\tid = %08x, len = %" PRIu32 ", flags = ",
						gmd->mds[i].md_id, gmd->mds[i].md_len);
				ep_prflags(gmd->mds[i].md_flags, MdatumFlags, fp);
				fprintf(fp, "\n");

				if (detail > 3)
					ep_hexdump(gmd->mds[i].md_data, gmd->mds[i].md_len, fp,
							EP_HEXDUMP_ASCII, 0);
			}
		}
	}
	else if (detail == 1)
	{
		int i;

		for (i = 0; i < gmd->nused; i++)
			fprintf(fp, "\tMetadata %2d, id %8x, length %" PRIu32 "\n",
					i, gmd->mds[i].md_id, gmd->mds[i].md_len);
	}
}
