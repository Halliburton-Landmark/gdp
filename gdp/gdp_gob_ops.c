/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	This implements GDP Connection Log (GOB) utilities.
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
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>
#include <ep/ep_prflags.h>

#include "gdp.h"
#include "gdp_event.h"
#include "gdp_gclmd.h"
#include "gdp_priv.h"

#include <event2/event.h>

#include <string.h>
#include <sys/errno.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gob.ops", "GOB operations for GDP");


/*
**	CREATE_GOB_NAME -- create a name for a new GOB
*/

EP_STAT
_gdp_gob_newname(gdp_gob_t *gob)
{
	if (!GDP_GOB_ISGOOD(gob))
		return GDP_STAT_GCL_NOT_OPEN;
	_gdp_newname(gob->name, gob->gclmd);
	gdp_printable_name(gob->name, gob->pname);
	return EP_STAT_OK;
}


/*
**	_GDP_GOB_CREATE --- create a new GOB
**
**		Creation is a bit tricky, since we don't start with an existing
**		GOB, and we address the message to the desired daemon instead
**		of to the GOB itself.  Some magic needs to occur.
*/

EP_STAT
_gdp_gob_create(gdp_name_t gobname,
				gdp_name_t logdname,
				gdp_gclmd_t *gmd,
				gdp_chan_t *chan,
				uint32_t reqflags,
				gdp_gob_t **pgob)
{
	gdp_req_t *req = NULL;
	gdp_gob_t *gob = NULL;
	EP_STAT estat = EP_STAT_OK;

	errno = 0;				// avoid spurious messages

	{
		gdp_pname_t gxname, dxname;

		ep_dbg_cprintf(Dbg, 17,
				"_gdp_gob_create: gob=%s\n\tlogd=%s\n",
				gobname == NULL ? "none" : gdp_printable_name(gobname, gxname),
				gdp_printable_name(logdname, dxname));
	}

	// create a new pseudo-GOB for the daemon so we can correlate the results
	estat = _gdp_gob_new(logdname, &gob);
	EP_STAT_CHECK(estat, goto fail0);

	// create the request
	_gdp_gob_lock(gob);
	reqflags |= GDP_REQ_ROUTEFAIL;		// don't retry on route failure
	estat = _gdp_req_new(GDP_CMD_CREATE, gob, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// send the name of the log to be created in the payload
	gdp_buf_write(req->cpdu->datum->dbuf, gobname, sizeof (gdp_name_t));

	// add the metadata to the output stream
	_gdp_gclmd_serialize(gmd, req->cpdu->datum->dbuf);

	// send command and wait for results
	estat = _gdp_invoke(req);
	GDP_GOB_ASSERT_ISLOCKED(gob);
	EP_STAT_CHECK(estat, goto fail0);

	// change GOB name
	(void) memcpy(gob->name, gobname, sizeof (gdp_name_t));

	// add new GOB to cache
	EP_ASSERT(req->gob == gob);
	req->gob = NULL;			// avoid decref in _gdp_req_free
	_gdp_req_unlock(req);		// lock ordering
	_gdp_gob_cache_add(gob);
	_gdp_req_lock(req);			// must be locked for _gdp_req_free

	// free resources and return results
	*pgob = gob;

fail0:
	if (req != NULL)
		_gdp_req_free(&req);
	if (gob != NULL)
	{
		if (!EP_STAT_ISOK(estat))
			_gdp_gob_decref(&gob, false);
		else
			_gdp_gob_unlock(gob);
	}

	if (!EP_STAT_ISOK(estat))
		*pgob = NULL;

	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 8, "_gdp_gob_create <<< %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


static EP_STAT
find_secret_key(gdp_gob_t *gob,
			gdp_gcl_open_info_t *open_info)
{
	// We will write the log, and it does have a public key.  We need
	// to find the secret to match it.
	size_t pkbuflen;
	const uint8_t *pkbuf;
	int md_alg;
	EP_CRYPTO_KEY *secretkey = NULL;
	bool my_secretkey = false;
	EP_STAT estat;

	// see if we have a public key; if not we're done
	estat = gdp_gclmd_find(gob->gclmd, GDP_GCLMD_PUBKEY,
				&pkbuflen, (const void **) &pkbuf);
	if (!EP_STAT_ISOK(estat))
	{
		ep_dbg_cprintf(Dbg, 30, "_gdp_gob_open: no public key\n");
		return EP_STAT_OK;
	}

	md_alg = pkbuf[0];

	// get the secret key if needed
	if (open_info != NULL)
	{
		secretkey = open_info->signkey;
		if (secretkey == NULL && open_info->signkey_cb != NULL)
		{
			estat = (*open_info->signkey_cb)(gob->name,
							open_info->signkey_udata, &secretkey);
			EP_STAT_CHECK(estat, return estat);
			my_secretkey = true;				// we must deallocate
		}
	}

	// nothing from user; let's try a standard search
	if (secretkey == NULL)
	{
		secretkey = _gdp_crypto_skey_read(gob->pname, "pem");

		if (secretkey == NULL)
		{
			// OK, now we have a problem --- we can't sign
			ep_dbg_cprintf(Dbg, 30, "_gdp_gob_open: no secret key\n");
			return GDP_STAT_SKEY_REQUIRED;
		}

		my_secretkey = true;			// we must deallocate
	}

	// validate the compatibility of the public and secret keys
	{
		EP_CRYPTO_KEY *pubkey = ep_crypto_key_read_mem(pkbuf + 4, pkbuflen - 4,
				EP_CRYPTO_KEYFORM_DER, EP_CRYPTO_F_PUBLIC);

		if (ep_dbg_test(Dbg, 40))
		{
			ep_crypto_key_print(pubkey, ep_dbg_getfile(), EP_CRYPTO_F_PUBLIC);
		}
		estat = ep_crypto_key_compat(secretkey, pubkey);
		ep_crypto_key_free(pubkey);
		if (!EP_STAT_ISOK(estat))
		{
			// XXX: cheat: use internal interface
			(void) _ep_crypto_error(estat,
							"public & secret keys are not compatible");
			return estat;
		}
	}

	// set up the message digest context
	gob->digest = ep_crypto_sign_new(secretkey, md_alg);

	// we can release the key now
	if (my_secretkey)
		ep_crypto_key_free(secretkey);

	if (gob->digest == NULL)
		return EP_STAT_CRYPTO_DIGEST;

	// add the GOB name to the hashed message digest
	ep_crypto_sign_update(gob->digest, gob->name, sizeof gob->name);

	// re-serialize the metadata and include it
	{
		gdp_buf_t *mdbuf = gdp_buf_new();
		_gdp_gclmd_serialize(gob->gclmd, mdbuf);
		size_t mdbuflen = gdp_buf_getlength(mdbuf);
		ep_crypto_sign_update(gob->digest, gdp_buf_getptr(mdbuf, mdbuflen),
						mdbuflen);
		//gdp_buf_drain(mdbuf, mdbuflen);
		gdp_buf_free(mdbuf);
	}

	// the GOB hash structure now has the fixed part of the hash
	return estat;
}


/*
**	_GDP_GOB_OPEN --- open a GOB for reading or further appending
*/

EP_STAT
_gdp_gob_open(gdp_gob_t *gob,
			int cmd,
			gdp_gcl_open_info_t *open_info,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req = NULL;

	EP_ASSERT_ELSE(GDP_GOB_ISGOOD(gob),
					return EP_STAT_ASSERT_ABORT);
	GDP_GOB_ASSERT_ISLOCKED(gob);

	// send the request across to the log daemon
	errno = 0;				// avoid spurious messages
	reqflags |= GDP_REQ_ALLOC_RID;			// always use a new request id
	reqflags |= GDP_REQ_ROUTEFAIL;			// don't retry on router errors
	estat = _gdp_req_new(cmd, gob, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);
	estat = _gdp_invoke(req);
	EP_STAT_CHECK(estat, goto fail0);
	// success

	// save the number of records
	gob->nrecs = req->rpdu->datum->recno;

	// read in the metadata to internal format
	gob->gclmd = _gdp_gclmd_deserialize(req->rpdu->datum->dbuf);

	// if we're not going to write, we don't need a secret key
	if (cmd == GDP_CMD_OPEN_AO || cmd == GDP_CMD_OPEN_RA)
			estat = find_secret_key(gob, open_info);

fail0:
	if (req != NULL)
		_gdp_req_free(&req);

	// log failure
	if (EP_STAT_ISOK(estat))
	{
		// success!
		if (ep_dbg_test(Dbg, 30))
		{
			ep_dbg_printf("Opened ");
			_gdp_gob_dump(gob, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
		}
		else
		{
			ep_dbg_cprintf(Dbg, 10, "Opened GOB %s\n", gob->pname);
		}
	}
	else
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 9,
				"Couldn't open GOB %s:\n\t%s\n",
				gob->pname, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**	_GDP_GOB_CLOSE --- share operation for closing a GOB handle
*/

EP_STAT
_gdp_gob_close(gdp_gob_t *gob,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;

	if (ep_dbg_test(Dbg, 38))
	{
		ep_dbg_printf("_gdp_gob_close: ");
		_gdp_gob_dump(gob, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
	}

	errno = 0;				// avoid spurious messages
	estat = _gdp_req_new(GDP_CMD_CLOSE, gob, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, return estat);

	// tell the daemon to close it
	estat = _gdp_invoke(req);

	_gdp_req_free(&req);
	return estat;
}


/*
**	_GDP_GOB_DELETE --- close and delete a GOB
*/

EP_STAT
_gdp_gob_delete(gdp_gob_t *gob,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;

	if (ep_dbg_test(Dbg, 38))
	{
		ep_dbg_printf("_gdp_gob_delete: ");
		_gdp_gob_dump(gob, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
	}

	errno = 0;				// avoid spurious messages
	estat = _gdp_req_new(GDP_CMD_DELETE, gob, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, return estat);

	// tell the daemon to delete it
	estat = _gdp_invoke(req);

	if (EP_STAT_ISOK(estat))
	{
		// invalidate gob regardless of reference count (but leave memory)
		gob->flags |= GCLF_DROPPING;
	}

	_gdp_req_free(&req);
	return estat;
}


/*
**  APPEND_COMMON --- common code for sync and async appends
**
**		datum should be locked when called.
**		req will be locked upon return.
*/

static EP_STAT
append_common(gdp_gob_t *gob,
		gdp_datum_t *datum,
		gdp_chan_t *chan,
		uint32_t reqflags,
		gdp_req_t **reqp)
{
	EP_STAT estat = GDP_STAT_BAD_IOMODE;
	gdp_req_t *req;

	errno = 0;				// avoid spurious messages

	if (!GDP_GOB_ISGOOD(gob))
		return GDP_STAT_GCL_NOT_OPEN;
	if (!GDP_DATUM_ISGOOD(datum))
		return GDP_STAT_DATUM_REQUIRED;
	EP_ASSERT_POINTER_VALID(datum);

	// create a new request structure
	estat = _gdp_req_new(GDP_CMD_APPEND, gob, chan, NULL, reqflags, reqp);
	EP_STAT_CHECK(estat, goto fail0);
	req = *reqp;

	// if the assertion fails, we may be using an already freed datum
	EP_ASSERT_ELSE(datum->inuse, return EP_STAT_ASSERT_ABORT);

	// set up for signing (req->md will be updated with data part)
	req->md = gob->digest;
	datum->recno = gob->nrecs + 1;

	// Note that this is just a guess: the append may still fail,
	// but we need to do this if there are multiple threads appending
	// at the same time.
	// If the append fails, we'll be out of sync and all hell breaks loose.
	gob->nrecs++;

	// caller owns datum
	//XXX why not just take the reference?
	gdp_datum_copy(req->cpdu->datum, datum);

fail0:
	return estat;
}


/*
**  _GDP_GOB_APPEND --- shared operation for appending to a GOB
**
**		Used both in GDP client library and gdpd.
*/

EP_STAT
_gdp_gob_append(gdp_gob_t *gob,
			gdp_datum_t *datum,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat = GDP_STAT_BAD_IOMODE;
	gdp_req_t *req = NULL;

	estat = append_common(gob, datum, chan, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// send the request to the log server
	estat = _gdp_invoke(req);
	EP_STAT_CHECK(estat, goto fail1);

	// collect results
	gob->nrecs = datum->recno;
	gdp_buf_reset(datum->dbuf);
	if (datum->sig != NULL)
		gdp_buf_reset(datum->sig);
	gdp_datum_copy(datum, req->rpdu->datum);

fail1:
	_gdp_req_free(&req);
fail0:
	if (ep_dbg_test(Dbg, 42))
	{
		ep_dbg_printf("_gdp_gob_append: returning ");
		gdp_datum_print(datum, ep_dbg_getfile(), GDP_DATUM_PRDEBUG);
	}
	return estat;
}


/*
**  _GDP_GOB_APPEND_ASYNC --- asynchronous append
*/

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

EP_STAT
_gdp_gob_append_async(
			gdp_gob_t *gob,
			gdp_datum_t *datum,
			gdp_event_cbfunc_t cbfunc,
			void *cbarg,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat;
	gdp_req_t *req = NULL;
	int i;

	// deliver results asynchronously
	reqflags |= GDP_REQ_ASYNCIO;
	estat = append_common(gob, datum, chan, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// arrange for responses to appear as events or callbacks
	_gdp_event_setcb(req, cbfunc, cbarg);

	estat = _gdp_req_send(req);

	// synchronous calls clear the data in the datum, so be consistent
	i = gdp_buf_drain(req->cpdu->datum->dbuf, SIZE_MAX);
	if (i < 0 && ep_dbg_test(Dbg, 1))
		ep_dbg_printf("_gdp_gob_append_async: gdp_buf_drain failure\n");

	gdp_buf_reset(datum->dbuf);
	if (datum->sig != NULL)
		gdp_buf_reset(datum->sig);

	// cleanup and return
	if (!EP_STAT_ISOK(estat))
	{
		_gdp_req_free(&req);
	}
	else
	{
		req->state = GDP_REQ_IDLE;
		ep_thr_cond_signal(&req->cond);
		_gdp_req_unlock(req);
	}
fail0:
	if (ep_dbg_test(Dbg, 11))
	{
		char ebuf[100];
		ep_dbg_printf("_gdp_gob_append_async => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**  _GDP_GOB_READ --- read a record from a GOB
**
**		Parameters:
**			gob --- the gob from which to read
**			datum --- the data buffer (to avoid dynamic memory)
**			chan --- the data channel used to contact the remote
**			reqflags --- flags for the request
**
**		This might be read by recno or read by timestamp based on
**		the command.  In any case the cmd is the defining factor.
*/

EP_STAT
_gdp_gob_read(gdp_gob_t *gob,
			gdp_datum_t *datum,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat = GDP_STAT_BAD_IOMODE;
	gdp_req_t *req;

	errno = 0;				// avoid spurious messages

	// sanity checks
	if (!GDP_GOB_ISGOOD(gob))
		return GDP_STAT_GCL_NOT_OPEN;
	if (!GDP_DATUM_ISGOOD(datum))
		return GDP_STAT_DATUM_REQUIRED;
	EP_ASSERT_ELSE(datum->inuse, return EP_STAT_ASSERT_ABORT);

	// create and send a new request
	estat = _gdp_req_new(GDP_CMD_READ, gob, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);
	gdp_datum_copy(req->cpdu->datum, datum);
	estat = _gdp_invoke(req);

	// ok, done!  pass the datum contents to the caller and free the request
	if (EP_STAT_ISOK(estat))
		gdp_datum_copy(datum, req->rpdu->datum);
	_gdp_req_free(&req);
fail0:
	return estat;
}


/*
**  _GDP_GOB_READ_ASYNC --- asynchronously read a record from a GOB
**
**		Parameters:
**			gob --- the gob from which to read
**			recno --- the record number to read
**			cbfunc --- the callback function (NULL => deliver as events)
**			cbarg --- user argument to cbfunc
**			chan --- the data channel used to contact the remote
**
**		This might be read by recno or read by timestamp based on
**		the command.  In any case the cmd is the defining factor.
*/

EP_STAT
_gdp_gob_read_async(gdp_gob_t *gob,
			gdp_recno_t recno,
			gdp_event_cbfunc_t cbfunc,
			void *cbarg,
			gdp_chan_t *chan)
{
	EP_STAT estat;
	gdp_req_t *req;

	errno = 0;				// avoid spurious messages

	// sanity checks
	if (!GDP_GOB_ISGOOD(gob))
		return GDP_STAT_GCL_NOT_OPEN;

	// create a new READ request (don't need a special command)
	estat = _gdp_req_new(GDP_CMD_READ, gob, chan, NULL, GDP_REQ_ASYNCIO, &req);
	EP_STAT_CHECK(estat, return estat);
	_gdp_event_setcb(req, cbfunc, cbarg);

	req->cpdu->datum->recno = recno;
	estat = _gdp_req_send(req);

	if (EP_STAT_ISOK(estat))
	{
		req->state = GDP_REQ_IDLE;
		_gdp_req_unlock(req);
	}
	else
	{
		_gdp_req_free(&req);
	}

	// ok, done!
	return estat;
}


/*
**  _GDP_GOB_GETMETADATA --- return metadata for a log
*/

EP_STAT
_gdp_gob_getmetadata(gdp_gob_t *gob,
		gdp_gclmd_t **gmdp,
		gdp_chan_t *chan,
		uint32_t reqflags)
{
	EP_STAT estat;
	gdp_req_t *req;

	if (!GDP_GOB_ISGOOD(gob))
		return GDP_STAT_GCL_NOT_OPEN;

	errno = 0;				// avoid spurious messages
	estat = _gdp_req_new(GDP_CMD_GETMETADATA, gob, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	estat = _gdp_invoke(req);
	EP_STAT_CHECK(estat, goto fail1);

	*gmdp = _gdp_gclmd_deserialize(req->rpdu->datum->dbuf);

fail1:
	_gdp_req_free(&req);

fail0:
	return estat;
}


/*
**  _GDP_GOB_NEWSEGMENT --- create a new physical segment for a log
*/

EP_STAT
_gdp_gob_newsegment(gdp_gob_t *gob,
		gdp_chan_t *chan,
		uint32_t reqflags)
{
	EP_STAT estat;
	gdp_req_t *req;

	if (!GDP_GOB_ISGOOD(gob))
		return GDP_STAT_GCL_NOT_OPEN;
	estat = _gdp_req_new(GDP_CMD_NEWSEGMENT, gob, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	estat = _gdp_invoke(req);

	_gdp_req_free(&req);
fail0:
	return estat;
}


/***********************************************************************
**  Client side implementations for commands used internally only.
***********************************************************************/

/*
**  _GDP_GOB_FWD_APPEND --- forward APPEND command
**
**		Forwards an APPEND command to a different server.  This is one
**		of the few commands that is directed directly to a gdplogd instance.
**		However, the response is going to come back from the original
**		GOB, not the gdplogd instance, so we arrange for the request to
**		be linked on that GOB's chain.
**
**		Note: Unlike other calls, the datum is not cleared.  This is
**		because we expect this to be used multiple times on a single
**		datum.  When all copies are sent, the caller must call
**		gdp_buf_drain(datum, gdp_buf_getlength(datum)).
*/

EP_STAT
_gdp_gob_fwd_append(
		gdp_gob_t *gob,
		gdp_datum_t *datum,
		gdp_name_t to_server,
		gdp_event_cbfunc_t cbfunc,
		void *cbarg,
		gdp_chan_t *chan,
		uint32_t reqflags)
{
	EP_STAT estat;
	gdp_req_t *req;

	// sanity checks
	if (!GDP_GOB_ISGOOD(gob))
		return GDP_STAT_GCL_NOT_OPEN;
	if (GDP_NAME_SAME(to_server, _GdpMyRoutingName))
	{
		// forwarding to ourselves: bad idea
		EP_ASSERT_PRINT("_gdp_gob_fwd_append: forwarding to myself");
		return EP_STAT_ASSERT_ABORT;
	}

	// deliver results asynchronously
	reqflags |= GDP_REQ_ASYNCIO;

	_gdp_gob_lock(gob);
	estat = _gdp_req_new(GDP_CMD_FWD_APPEND, gob, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// arrange for responses to appear as events or callbacks
	_gdp_event_setcb(req, cbfunc, cbarg);

	// add the actual target GDP name to the data
	gdp_buf_write(req->cpdu->datum->dbuf, req->cpdu->dst, sizeof req->cpdu->dst);

	// change the destination to be the final server, not the GOB
	memcpy(req->cpdu->dst, to_server, sizeof req->cpdu->dst);

	// copy the existing datum, including metadata
	size_t l = gdp_buf_getlength(datum->dbuf);
	gdp_buf_write(req->cpdu->datum->dbuf, gdp_buf_getptr(datum->dbuf, l), l);
	req->cpdu->datum->recno = datum->recno;
	req->cpdu->datum->ts = datum->ts;
	req->cpdu->datum->sigmdalg = datum->sigmdalg;
	req->cpdu->datum->siglen = datum->siglen;
	if (req->cpdu->datum->sig != NULL)
		gdp_buf_free(req->cpdu->datum->sig);
	req->cpdu->datum->sig = NULL;
	if (datum->sig != NULL)
	{
		l = gdp_buf_getlength(datum->sig);
		req->cpdu->datum->sig = gdp_buf_new();
		gdp_buf_write(req->cpdu->datum->sig, gdp_buf_getptr(datum->sig, l), l);
	}

	// XXX should we take a callback function?

	estat = _gdp_req_send(req);

	// unlike append_async, we leave the datum intact

	// cleanup
	req->cpdu->datum = NULL;			// owned by caller
	if (!EP_STAT_ISOK(estat))
	{
		_gdp_req_free(&req);
	}
	else
	{
		req->state = GDP_REQ_IDLE;
		ep_thr_cond_signal(&req->cond);
		_gdp_req_unlock(req);
	}

fail0:
	_gdp_gob_unlock(gob);
	if (ep_dbg_test(Dbg, 11))
	{
		char ebuf[100];
		ep_dbg_printf("_gdp_gob_fwd_append => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}
