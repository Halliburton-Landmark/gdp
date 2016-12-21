/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	This implements the GDP API for C-based applications.
**
**  With the exception of the name manipulation (parsing,
**  printing, etc.) most of these are basically just translation
**  routines, converting the API calls into requests and handing
**  them on; the hard work is done in gdp_gcl_ops.c and gdp_proto.c.
**
**	TODO In the future this may need to be extended to have knowledge
**		 of TSN/AVB, but for now we don't worry about that.
**
**	----- BEGIN LICENSE BLOCK -----
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
**	----- END LICENSE BLOCK -----
*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>

#include "gdp.h"
#include "gdp_gclmd.h"
#include "gdp_stat.h"
#include "gdp_priv.h"

#include <event2/event.h>
#include <openssl/sha.h>

#include <errno.h>
#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.api", "C API for GDP");


/*
**  Mutex around open operations
*/

static EP_THR_MUTEX		OpenMutex		EP_THR_MUTEX_INITIALIZER;


// simplify debugging
#define PRSTAT(estat, where)												\
			if (ep_dbg_test(Dbg, EP_STAT_ISOK(estat) ? 39 : 1))				\
			{																\
				char ebuf[100];												\
				ep_dbg_printf("<<< %s: %s\\n",								\
						where, ep_stat_tostr(estat, ebuf, sizeof ebuf));	\
			}



/*
**	GDP_GCL_GETNAME --- get the name of a GCL
*/

const gdp_name_t *
gdp_gcl_getname(const gdp_gcl_t *gcl)
{
	return &gcl->name;
}


/*
**  GDP_GCL_PRINTABLE_NAME --- make a printable GCL name from an internal name
**
**		Returns the external name buffer for ease-of-use.
*/

char *
gdp_printable_name(const gdp_name_t internal, gdp_pname_t external)
{
	EP_STAT estat = ep_b64_encode(internal, sizeof (gdp_name_t),
							external, sizeof (gdp_pname_t),
							EP_B64_ENC_URL);

	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 2,
				"gdp_printable_name: ep_b64_encode failure\n"
				"\tstat = %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		strcpy("(unknown)", external);
	}
	else if (EP_STAT_TO_INT(estat) != GDP_GCL_PNAME_LEN)
	{
		ep_dbg_cprintf(Dbg, 2,
				"gdp_printable_name: ep_b64_encode length failure (%d != %d)\n",
				EP_STAT_TO_INT(estat), GDP_GCL_PNAME_LEN);
	}
	return external;
}

/*
**  GDP_GCL_PRINT_NAME --- print a GDP name to a file
*/

void
gdp_print_name(const gdp_name_t name, FILE *fp)
{
	gdp_pname_t pname;

	if (!gdp_name_is_valid(name))
		fprintf(fp, "(none)");
	else
		fprintf(fp, "%s", gdp_printable_name(name, pname));
}

/*
**  GDP_INTERNAL_NAME --- parse a string GDP name to internal representation
*/

EP_STAT
gdp_internal_name(const gdp_pname_t external, gdp_name_t internal)
{
	EP_STAT estat;

	if (strlen(external) != GDP_GCL_PNAME_LEN)
	{
		estat = GDP_STAT_GCL_NAME_INVALID;
	}
	else
	{
		estat = ep_b64_decode(external, sizeof (gdp_pname_t) - 1,
							internal, sizeof (gdp_name_t),
							EP_B64_ENC_URL);
	}

	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 2,
				"gdp_internal_name: ep_b64_decode failure\n"
				"\tname = %s\n"
				"\tstat = %s\n",
				external,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	else if (EP_STAT_TO_INT(estat) != sizeof (gdp_name_t))
	{
		ep_dbg_cprintf(Dbg, 2,
				"gdp_internal_name: ep_b64_decode length failure (%d != %zd)\n",
				EP_STAT_TO_INT(estat), sizeof (gdp_name_t));
		estat = EP_STAT_ABORT;
	}

	return estat;
}

/*
**	GDP_PARSE_NAME --- parse a (possibily human-friendly) GDP object name
**
**		An externally printable version of an internal name must be
**		exactly GDP_GCL_PNAME_LEN (43) characters long and contain only
**		valid URL-Base64 characters.  These are base64 decoded.
**		All other names are considered human-friendly and are
**		sha256-encoded to get the internal name.
*/

EP_STAT
gdp_parse_name(const char *ext, gdp_name_t name)
{
	if (strlen(ext) != GDP_GCL_PNAME_LEN ||
			!EP_STAT_ISOK(gdp_internal_name(ext, name)))
	{
		// must be human-oriented name
		ep_crypto_md_sha256((const uint8_t *) ext, strlen(ext), name);
	}
	return EP_STAT_OK;
}

/*
**	GDP_NAME_IS_VALID --- test whether a GDP object name is valid
**
**		Unfortunately, since SHA-256 is believed to be surjective
**		(that is, all values are possible), there is a slight
**		risk of a collision.
*/

bool
gdp_name_is_valid(const gdp_name_t name)
{
	const uint32_t *up;
	int i;

	up = (uint32_t *) name;
	for (i = 0; i < sizeof (gdp_name_t) / 4; i++)
		if (*up++ != 0)
			return true;
	return false;
}


/*
**  GDP_GCL_GETNRECS --- get the number of records in a GCL
*/

gdp_recno_t
gdp_gcl_getnrecs(const gdp_gcl_t *gcl)
{
	EP_ASSERT_ELSE(GDP_GCL_ISGOOD(gcl),
					return GDP_PDU_NO_RECNO);
	return gcl->nrecs;
}


/*
**  GDP_GCL_PRINT --- print a GCL (for debugging)
*/

void
gdp_gcl_print(
		const gdp_gcl_t *gcl,
		FILE *fp)
{
	_gdp_gcl_dump(gcl, fp, GDP_PR_PRETTY, 0);
}



/*
**	GDP_INIT --- initialize this library
**
**		This is the normal startup for a client process.  Servers
**		may need to do additional steps early on, and may choose
**		to advertise more than their own name.
*/

EP_STAT
gdp_init(const char *router_addr)
{
	EP_STAT estat;

	if (_GdpLibInitialized)
		return EP_STAT_OK;

	// set up global state, event loop, etc.
	estat = gdp_lib_init(NULL);
	EP_STAT_CHECK(estat, goto fail0);

	// initialize connection
	_GdpChannel = NULL;
	estat = _gdp_chan_open(router_addr, &_gdp_pdu_process, &_GdpChannel);
	EP_STAT_CHECK(estat, goto fail0);
	_GdpChannel->advertise = &_gdp_advertise_me;

	// start the event loop
	estat = _gdp_evloop_init();
	EP_STAT_CHECK(estat, goto fail0);

	// advertise ourselves
	estat = _gdp_advertise_me(GDP_CMD_ADVERTISE);
	if (!EP_STAT_ISOK(estat))
	{
		if (_GdpChannel->bev != NULL)
			bufferevent_free(_GdpChannel->bev);
		ep_mem_free(_GdpChannel);
		_GdpChannel = NULL;
		goto fail0;
	}

	_GdpLibInitialized = true;

fail0:
	{
		char ebuf[200];

		ep_dbg_cprintf(Dbg, 4, "gdp_init: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**	GDP_GCL_CREATE --- create a new GCL
*/

EP_STAT
gdp_gcl_create(gdp_name_t gclname,
				gdp_name_t logdname,
				gdp_gclmd_t *gmd,
				gdp_gcl_t **pgcl)
{
	EP_STAT estat;
	char ebuf[100];
	gdp_name_t namebuf;

	ep_dbg_cprintf(Dbg, 19, "\n>>> gdp_gcl_create\n");
	estat = GDP_CHECK_INITIALIZED;		// make sure gdp_init is done
	EP_STAT_CHECK(estat, return estat);

	if (gclname == NULL)
	{
		gclname = namebuf;
		gdp_buf_t *buf = gdp_buf_new();
		size_t mdlen = _gdp_gclmd_serialize(gmd, buf);
		ep_crypto_md_sha256(gdp_buf_getptr(buf, mdlen), mdlen, gclname);
		gdp_buf_free(buf);
	}

	estat = _gdp_gcl_create(gclname, logdname, gmd, _GdpChannel,
					GDP_REQ_ALLOC_RID, pgcl);
	if (EP_STAT_ISOK(estat))
		(*pgcl)->iomode = GDP_MODE_ANY;

	ep_dbg_cprintf(Dbg, 8, "<<< gdp_gcl_create: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	return estat;
}


/*
**	GDP_GCL_OPEN --- open a GCL for reading or further appending
*/

EP_STAT
gdp_gcl_open(gdp_name_t name,
			gdp_iomode_t mode,
			gdp_gcl_open_info_t *info,
			gdp_gcl_t **pgcl)
{
	EP_STAT estat;
	gdp_gcl_t *gcl = NULL;
	int cmd;
	EP_CRYPTO_KEY *skey = NULL;

	if (ep_dbg_test(Dbg, 19))
	{
		gdp_pname_t pname;
		ep_dbg_printf("\n>>> gdp_gcl_open(%s)\n",
					gdp_printable_name(name, pname));
	}
	estat = GDP_CHECK_INITIALIZED;		// make sure gdp_init is done
	EP_STAT_CHECK(estat, return estat);

	if (mode == GDP_MODE_RO)
		cmd = GDP_CMD_OPEN_RO;
	else if (mode == GDP_MODE_AO)
		cmd = GDP_CMD_OPEN_AO;
	else if (mode == GDP_MODE_RA)
		cmd = GDP_CMD_OPEN_RA;
	else
	{
		// illegal I/O mode
		ep_app_error("gdp_gcl_open: illegal mode %d", mode);
		return GDP_STAT_BAD_IOMODE;
	}

	if (info != NULL)
	{
		skey = info->signkey;
	}

	if (!gdp_name_is_valid(name))
	{
		// illegal GCL name
		ep_dbg_cprintf(Dbg, 6, "gdp_gcl_open: null GCL name\n");
		return GDP_STAT_NULL_GCL;
	}

	// lock this operation to keep the GCL cache consistent
	ep_thr_mutex_lock(&OpenMutex);

	// see if we already have this open
	gcl = _gdp_gcl_cache_get(name, mode);
	if (gcl != NULL)
	{
		// just increase the reference count and return it
		ep_dbg_cprintf(Dbg, 10, "gdp_gcl_open(%s): using existing GCL @ %p\n",
				gcl->pname, gcl);
		gcl->iomode |= mode;
		estat = EP_STAT_OK;
	}
	else
	{
		// it's not there yet, so create a new one
		estat = _gdp_gcl_newhandle(name, &gcl);
		EP_STAT_CHECK(estat, goto fail0);
		gcl->iomode = mode;

		estat = _gdp_gcl_open(gcl, cmd, skey, _GdpChannel, GDP_REQ_ALLOC_RID);
	}
	if (EP_STAT_ISOK(estat))
	{
		*pgcl = gcl;
	}
	else
	{
		if (gcl->refcnt > 0)
		{
			gcl->refcnt = 0;			// avoid errors in _gdp_gcl_cache_drop
			_gdp_gcl_freehandle(gcl);
		}
		else
		{
			ep_thr_mutex_destroy(&gcl->mutex);
			ep_mem_free(gcl);
		}
	}

fail0:
	ep_thr_mutex_unlock(&OpenMutex);
	if (ep_dbg_test(Dbg, 8))
	{
		char ebuf[100];
		ep_dbg_printf("<<< gdp_gcl_open(%s): %s\n", gcl->pname,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**	GDP_GCL_CLOSE --- close an open GCL
*/

EP_STAT
gdp_gcl_close(gdp_gcl_t *gcl)
{
	EP_STAT estat;
	char ebuf[100];

	ep_dbg_cprintf(Dbg, 19, "\n>>> gdp_gcl_close(%s)\n", gcl->pname);
	estat = _gdp_gcl_close(gcl, _GdpChannel, 0);
	ep_dbg_cprintf(Dbg, 8, "<<< gdp_gcl_close(%s): %s\n", gcl->pname,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	return estat;
}


/*
**	GDP_GCL_APPEND --- append a message to a writable GCL
*/

EP_STAT
gdp_gcl_append(gdp_gcl_t *gcl, gdp_datum_t *datum)
{
	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_append\n");
	return _gdp_gcl_append(gcl, datum, _GdpChannel, 0);
}


/*
**  GDP_GCL_APPEND_ASYNC --- asynchronously append to a writable GCL
*/

EP_STAT
gdp_gcl_append_async(gdp_gcl_t *gcl,
			gdp_datum_t *datum,
			gdp_event_cbfunc_t cbfunc,
			void *udata)
{
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_append_async\n");
	estat = _gdp_gcl_append_async(gcl, datum, cbfunc, udata, _GdpChannel, 0);
	PRSTAT(estat, "gdp_gcl_append_async");
	return estat;
}


/*
**	GDP_GCL_READ --- read a message from a GCL based on recno
**
**	The data is returned through the passed-in datum.
**
**	Should be named gdp_gcl_read_by_recno.
**
**		Parameters:
**			gcl --- the gcl from which to read
**			recno --- the record number to read
**			datum --- the message header (to avoid dynamic memory)
*/

EP_STAT
gdp_gcl_read(gdp_gcl_t *gcl,
			gdp_recno_t recno,
			gdp_datum_t *datum)
{
	EP_STAT estat;

	EP_ASSERT_POINTER_VALID(datum);
	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_read\n");
	gdp_datum_reset(datum);
	datum->recno = recno;

	estat = _gdp_gcl_read(gcl, datum, _GdpChannel, 0);
	PRSTAT(estat, "gdp_gcl_read");
	return estat;
}


/*
**	GDP_GCL_READ_TS --- read a message from a GCL based on timestamp
**
**	The data is returned through the passed-in datum.
**
**		Parameters:
**			gcl --- the gcl from which to read
**			ts --- the lowest timestamp we are interested in.  The
**				result will be the lowest timestamp that is greater than
**				or equal to this value.
**			datum --- the message header (to avoid dynamic memory)
*/

EP_STAT
gdp_gcl_read_ts(gdp_gcl_t *gcl,
			EP_TIME_SPEC *ts,
			gdp_datum_t *datum)
{
	EP_STAT estat;

	EP_ASSERT_POINTER_VALID(datum);
	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_read_ts\n");
	memcpy(&datum->ts, ts, sizeof datum->ts);
	datum->recno = GDP_PDU_NO_RECNO;

	estat = _gdp_gcl_read(gcl, datum, _GdpChannel, 0);
	PRSTAT(estat, "gdp_gcl_read_ts");
	return estat;
}


/*
**  GDP_GCL_READ_ASYNC --- read asynchronously
**
**  Data and status are delivered as events.  Each call to this routine
**  returns exactly one event, either data or an error.
*/

EP_STAT
gdp_gcl_read_async(gdp_gcl_t *gcl,
			gdp_recno_t recno,
			gdp_event_cbfunc_t cbfunc,
			void *cbarg)
{
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_read_async\n");
	estat = _gdp_gcl_read_async(gcl, recno, cbfunc, cbarg, _GdpChannel);
	PRSTAT(estat, "gdp_gcl_read_async");
	return estat;
}



/*
**	GDP_GCL_SUBSCRIBE --- subscribe to a GCL starting from a record number
*/

EP_STAT
gdp_gcl_subscribe(gdp_gcl_t *gcl,
		gdp_recno_t start,
		int32_t numrecs,
		EP_TIME_SPEC *timeout,
		gdp_event_cbfunc_t cbfunc,
		void *cbarg)
{
	EP_STAT estat;
	gdp_req_t *req;

	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_subscribe\n");
	// create the subscribe request
	estat = _gdp_req_new(GDP_CMD_SUBSCRIBE, gcl, _GdpChannel, NULL,
			GDP_REQ_PERSIST | GDP_REQ_CLT_SUBSCR | GDP_REQ_ALLOC_RID,
			&req);
	EP_STAT_CHECK(estat, goto fail0);

	// add start and stop parameters to PDU
	req->cpdu->datum->recno = start;
	req->numrecs = numrecs;

	// now do the hard work
	estat = _gdp_gcl_subscribe(req, numrecs, timeout, cbfunc, cbarg);
fail0:
	PRSTAT(estat, "gdp_gcl_subscribe");
	return estat;
}


/*
**	GDP_GCL_SUBSCRIBE_TS --- subscribe to a GCL starting from a timestamp
*/

EP_STAT
gdp_gcl_subscribe_ts(gdp_gcl_t *gcl,
		EP_TIME_SPEC *start,
		int32_t numrecs,
		EP_TIME_SPEC *timeout,
		gdp_event_cbfunc_t cbfunc,
		void *cbarg)
{
	EP_STAT estat;
	gdp_req_t *req;

	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_subscribe_ts\n");
	// create the subscribe request
	estat = _gdp_req_new(GDP_CMD_SUBSCRIBE, gcl, _GdpChannel, NULL,
			GDP_REQ_PERSIST | GDP_REQ_CLT_SUBSCR | GDP_REQ_ALLOC_RID,
			&req);
	EP_STAT_CHECK(estat, goto fail0);

	// add start and stop parameters to PDU
	memcpy(&req->cpdu->datum->ts, start, sizeof req->cpdu->datum->ts);
	req->numrecs = numrecs;

	// now do the hard work
	estat = _gdp_gcl_subscribe(req, numrecs, timeout, cbfunc, cbarg);
fail0:
	PRSTAT(estat, "gdp_gcl_subscribe_ts");
	return estat;
}


/*
**	GDP_GCL_MULTIREAD --- read multiple records from a GCL using recno start
**
**		Like gdp_gcl_subscribe, the data is returned through the event
**		interface or callbacks.
*/

EP_STAT
gdp_gcl_multiread(gdp_gcl_t *gcl,
		gdp_recno_t start,
		int32_t numrecs,
		gdp_event_cbfunc_t cbfunc,
		void *cbarg)
{
	EP_STAT estat;
	gdp_req_t *req;

	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_multiread\n");
	// create the multiread request
	estat = _gdp_req_new(GDP_CMD_MULTIREAD, gcl, _GdpChannel, NULL,
			GDP_REQ_PERSIST | GDP_REQ_CLT_SUBSCR | GDP_REQ_ALLOC_RID,
			&req);
	EP_STAT_CHECK(estat, goto fail0);

	// add start and stop parameters to PDU
	req->cpdu->datum->recno = start;
	req->numrecs = numrecs;

	// now do the hard work
	estat = _gdp_gcl_subscribe(req, numrecs, NULL, cbfunc, cbarg);
fail0:
	PRSTAT(estat, "gdp_gcl_multiread");
	return estat;
}


/*
**	GDP_GCL_MULTIREAD_TS --- read multiple records from a GCL using timestamp
**
**		Like gdp_gcl_subscribe, the data is returned through the event
**		interface or callbacks.
*/

EP_STAT
gdp_gcl_multiread_ts(gdp_gcl_t *gcl,
		EP_TIME_SPEC *start,
		int32_t numrecs,
		gdp_event_cbfunc_t cbfunc,
		void *cbarg)
{
	EP_STAT estat;
	gdp_req_t *req;

	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_multiread_ts\n");
	// create the multiread request
	estat = _gdp_req_new(GDP_CMD_MULTIREAD, gcl, _GdpChannel, NULL,
			GDP_REQ_PERSIST | GDP_REQ_CLT_SUBSCR | GDP_REQ_ALLOC_RID,
			&req);
	EP_STAT_CHECK(estat, goto fail0);

	// add start and stop parameters to PDU
	memcpy(&req->cpdu->datum->ts, start, sizeof req->cpdu->datum->ts);
	req->numrecs = numrecs;

	// now do the hard work
	estat = _gdp_gcl_subscribe(req, numrecs, NULL, cbfunc, cbarg);
fail0:
	PRSTAT(estat, "gdp_gcl_multiread_ts");
	return estat;
}


/*
**  GDP_GCL_GETMETADATA --- return the metadata associated with a GCL
*/

EP_STAT
gdp_gcl_getmetadata(gdp_gcl_t *gcl,
		gdp_gclmd_t **gmdp)
{
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_getmetadata\n");
	estat = _gdp_gcl_getmetadata(gcl, gmdp, _GdpChannel, 0);
	PRSTAT(estat, "gdp_gcl_getmetadata");
	return estat;
}


/*
**  GDP_GCL_NEWSEGMENT --- create new segment for GCL
**
**		This should only be invoked by a service and with appropriate
**		authorization.
*/

EP_STAT
gdp_gcl_newsegment(gdp_gcl_t *gcl)
{
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_newsegment\n");
	estat = _gdp_gcl_newsegment(gcl, _GdpChannel, 0);
	PRSTAT(estat, "gdp_gcl_newsegment");
	return estat;
}


/*
**  GDP_GCL_SET_APPEND_FILTER --- set the append filter function
*/

void
gdp_gcl_set_append_filter(gdp_gcl_t *gcl,
		EP_STAT (*appendfilter)(gdp_datum_t *, void *),
		void *filterdata)
{
	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_set_append_filter\n");
	EP_ASSERT_ELSE(GDP_GCL_ISGOOD(gcl), return);
	gcl->apndfilter = appendfilter;
	gcl->apndfpriv = filterdata;
}


/*
**  GDP_GCL_SET_READ_FILTER --- set the read filter function
*/

void
gdp_gcl_set_read_filter(gdp_gcl_t *gcl,
		EP_STAT (*readfilter)(gdp_datum_t *, void *),
		void *filterdata)
{
	ep_dbg_cprintf(Dbg, 39, "\n>>> gdp_gcl_set_read_filter\n");
	EP_ASSERT_ELSE(GDP_GCL_ISGOOD(gcl), return);
	gcl->readfilter = readfilter;
	gcl->readfpriv = filterdata;
}


/*
**  GDP GCL Open Information handling
*/

gdp_gcl_open_info_t *
gdp_gcl_open_info_new(void)
{
	gdp_gcl_open_info_t *info;

	info = ep_mem_zalloc(sizeof*info);
	return info;
}

void
gdp_gcl_open_info_free(gdp_gcl_open_info_t *info)
{
	if (info->signkey != NULL)
		ep_crypto_key_free(info->signkey);
	ep_mem_free(info);
}

EP_STAT
gdp_gcl_open_info_set_signing_key(gdp_gcl_open_info_t *info,
		EP_CRYPTO_KEY *skey)
{
	info->signkey = skey;
	return EP_STAT_OK;
}
