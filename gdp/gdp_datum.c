/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  Message management
**
**		Messages contain the header and data information for a single
**		message.
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

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_string.h>
#include <ep/ep_thr.h>

#include "gdp.h"
#include "gdp_priv.h"

#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.datum", "GDP datum processing");

static gdp_datum_t		*DatumFreeList;
static EP_THR_MUTEX		DatumFreeListMutex EP_THR_MUTEX_INITIALIZER2(GDP_MUTEX_LORDER_LEAF);


/*
**  Create a new datum.
**		The datum is returned unlocked with a data buffer.
*/

gdp_datum_t *
gdp_datum_new(void)
{
	gdp_datum_t *datum;

	// get a message off the free list, if any
	ep_thr_mutex_lock(&DatumFreeListMutex);
	if ((datum = DatumFreeList) != NULL)
	{
		DatumFreeList = datum->next;
	}
	ep_thr_mutex_unlock(&DatumFreeListMutex);

	if (datum == NULL)
	{
		// nothing on the free list; allocate anew
		datum = ep_mem_zalloc(sizeof *datum);
		ep_thr_mutex_init(&datum->mutex, EP_THR_MUTEX_DEFAULT);
		ep_thr_mutex_setorder(&datum->mutex, GDP_MUTEX_LORDER_DATUM);
	}
	datum->next = NULL;

	EP_ASSERT(!datum->inuse);

	// initialize metadata
	gdp_datum_reset(datum);
	ep_dbg_cprintf(Dbg, 48, "gdp_datum_new => %p\n", datum);
	datum->inuse = true;
	VALGRIND_HG_CLEAN_MEMORY(datum, sizeof *datum);
	return datum;
}


void
gdp_datum_free(gdp_datum_t *datum)
{
	ep_dbg_cprintf(Dbg, 48, "gdp_datum_free(%p)\n", datum);

	// sanity
	if (datum == NULL)
		return;
	EP_ASSERT_ELSE(datum->inuse, return);

	datum->inuse = false;
	if (datum->dbuf != NULL)
	{
		size_t ndrain = gdp_buf_getlength(datum->dbuf);
		ep_dbg_cprintf(Dbg, 50, "  ... draining %zd bytes\n", ndrain);
		if (ndrain > 0)
			gdp_buf_drain(datum->dbuf, ndrain);
	}
	if (datum->sig != NULL)
	{
		//XXX retain this buffer?
		gdp_buf_free(datum->sig);
		datum->sig = NULL;
	}

	// make sure the datum is unlocked before putting on the free list
	if (ep_thr_mutex_trylock(&datum->mutex) != 0)
	{
		// shouldn't happen
		ep_dbg_cprintf(Dbg, 1, "gdp_datum_free(%p): was locked\n", datum);
	}
	ep_thr_mutex_unlock(&datum->mutex);
	ep_thr_mutex_lock(&DatumFreeListMutex);
	datum->next = DatumFreeList;
	DatumFreeList = datum;
	ep_thr_mutex_unlock(&DatumFreeListMutex);
}


/*
**  Reset a datum
**
**		This is what you would get if you freed it and reallocated.
*/

void
gdp_datum_reset(gdp_datum_t *datum)
{
	if (datum->dbuf == NULL)
		datum->dbuf = gdp_buf_new();
	else
		gdp_buf_reset(datum->dbuf);
	datum->siglen = datum->sigmdalg = 0;
	if (datum->sig != NULL)
		gdp_buf_reset(datum->sig);
	datum->recno = GDP_PDU_NO_RECNO;
	EP_TIME_INVALIDATE(&datum->ts);
}


gdp_recno_t
gdp_datum_getrecno(const gdp_datum_t *datum)
{
	return datum->recno;
}

void
gdp_datum_getts(const gdp_datum_t *datum, EP_TIME_SPEC *ts)
{
	memcpy(ts, &datum->ts, sizeof *ts);
}

size_t
gdp_datum_getdlen(const gdp_datum_t *datum)
{
	return gdp_buf_getlength(datum->dbuf);
}

gdp_buf_t *
gdp_datum_getbuf(const gdp_datum_t *datum)
{
	return datum->dbuf;
}

gdp_buf_t *
gdp_datum_getsig(const gdp_datum_t *datum)
{
	return datum->sig;
}

short
gdp_datum_getsigmdalg(const gdp_datum_t *datum)
{
	return datum->sigmdalg;
}

/*
**	GDP_DATUM_PRINT --- print a datum (for debugging)
*/

void
gdp_datum_print(const gdp_datum_t *datum, FILE *fp, uint32_t flags)
{
	unsigned char *d;
	int l;
	bool quiet = EP_UT_BITSET(GDP_DATUM_PRQUIET, flags);
	bool debug = EP_UT_BITSET(GDP_DATUM_PRDEBUG, flags);

	if (quiet && (debug || EP_UT_BITSET(GDP_DATUM_PRMETAONLY, flags)))
		quiet = false;

	if (!EP_ASSERT(fp != NULL))
		return;

	flockfile(fp);
	if (debug)
		fprintf(fp, "datum @ %p: ", datum);
	if (datum == NULL)
	{
		if (!quiet)
			fprintf(fp, "null datum\n");
		goto done;
	}

	if (!quiet)
		fprintf(fp, "recno %" PRIgdp_recno ", ", datum->recno);

	if (datum->dbuf == NULL)
	{
		if (!quiet)
			fprintf(fp, "no data");
		d = NULL;
		l = -1;
	}
	else
	{
		l = gdp_buf_getlength(datum->dbuf);
		if (!quiet)
			fprintf(fp, "len %d", l);
		if (l > 0)
			d = gdp_buf_getptr(datum->dbuf, l);
		else
			d = (unsigned char *) "";
	}

	if (!quiet)
	{
		if (EP_TIME_IS_VALID(&datum->ts))
		{
			fprintf(fp, ", ts ");
			ep_time_print(&datum->ts, fp, EP_TIME_FMT_HUMAN);
		}
		else
		{
			fprintf(fp, ", no timestamp");
		}

		fprintf(fp, "%s\n", datum->inuse ? "" : ", !inuse");
	}

	if (EP_UT_BITSET(GDP_DATUM_PRMETAONLY, flags))
	{
		fprintf(fp, "\n");
		goto done;
	}

	if (EP_UT_BITSET(GDP_DATUM_PRTEXT, flags))
		fprintf(fp, "%.*s\n", l, d);
	else
		ep_hexdump(d, l, fp, EP_HEXDUMP_ASCII, 0);

	if (EP_UT_BITSET(GDP_DATUM_PRSIG, flags))
	{
		fprintf(fp, "  Sig len = %d, digest alg = %d\n",
				datum->siglen, datum->sigmdalg);
		if (datum->sig != NULL && EP_UT_BITSET(GDP_DATUM_PRDEBUG, flags))
		{
			d = gdp_buf_getptr(datum->sig, datum->siglen);
			ep_hexdump(d, datum->siglen, fp, EP_HEXDUMP_HEX, 0);
		}
	}
done:
	funlockfile(fp);
}


/*
**  Copy contents of one datum into another
*/

void
gdp_datum_copy(gdp_datum_t *to, const gdp_datum_t *from)
{
	to->recno = from->recno;
	to->ts = from->ts;
	to->sigmdalg = from->sigmdalg;
	to->siglen = from->siglen;
	to->inuse = from->inuse;
	if (from->dbuf != NULL)
	{
		if (EP_ASSERT(to->dbuf != NULL))
			gdp_buf_copy(to->dbuf, from->dbuf);
	}
	if (from->sig != NULL)
	{
		if (EP_ASSERT(to->sig != NULL))
			gdp_buf_copy(to->sig, from->sig);
	}
}


/*
**  Duplicate a datum (internal use)
*/

gdp_datum_t *
gdp_datum_dup(const gdp_datum_t *datum)
{
	gdp_datum_t *ndatum;

	ndatum = gdp_datum_new();
	ndatum->recno = datum->recno;
	ndatum->ts = datum->ts;
	gdp_buf_copy(ndatum->dbuf, datum->dbuf);
	ndatum->sigmdalg = datum->sigmdalg;
	ndatum->siglen = datum->siglen;
	if (datum->sig != NULL)
		ndatum->sig = gdp_buf_dup(datum->sig);

	return ndatum;
}


void
_gdp_datum_dump(const gdp_datum_t *datum,
			FILE *fp)
{
	gdp_datum_print(datum, fp, GDP_DATUM_PRDEBUG);
}
