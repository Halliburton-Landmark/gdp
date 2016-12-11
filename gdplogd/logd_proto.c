/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
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

#include "logd.h"
#include "logd_admin.h"
#include "logd_pubsub.h"

#include <gdp/gdp_gclmd.h>
#include <gdp/gdp_priv.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.proto", "GDP Log Daemon protocol");

/*
**	GDPD_GCL_ERROR --- helper routine for returning errors
*/

EP_STAT
gdpd_gcl_error(gdp_name_t gcl_name, char *msg, EP_STAT logstat, EP_STAT estat)
{
	gdp_pname_t pname;

	gdp_printable_name(gcl_name, pname);
	if (EP_STAT_ISSEVERE(logstat))
	{
		// server error (rather than client error)
		ep_log(logstat, "%s: %s", msg, pname);
		if (!GDP_STAT_IS_S_NAK(logstat))
			logstat = estat;
	}
	else
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 1, "%s: %s: %s\n", msg, pname,
				ep_stat_tostr(logstat, ebuf, sizeof ebuf));
		if (!GDP_STAT_IS_C_NAK(logstat))
			logstat = estat;
	}
	return logstat;
}


/*
**  Flush information from an incoming datum
**
**		All commands should do this /before/ they write any return
**		values into the datum.  If where == NULL then data was expected
**		and will not be flagged.
*/

void
flush_input_data(gdp_req_t *req, char *where)
{
	int i;
	gdp_datum_t *datum = req->pdu->datum;

	if (datum == NULL)
		return;
	if (datum->dbuf != NULL && (i = gdp_buf_getlength(datum->dbuf)) > 0)
	{
		if (where != NULL)
			ep_dbg_cprintf(Dbg, 4,
					"flush_input_data: %s: flushing %d bytes of unexpected input\n",
					where, i);
		gdp_buf_reset(datum->dbuf);
	}
	if (datum->sig != NULL)
	{
		i = gdp_buf_getlength(datum->sig);
		if (i > 0 && ep_dbg_test(Dbg, 4) && where != NULL)
			ep_dbg_printf(
					"flush_input_data: %s: flushing %d bytes of unexpected signature\n",
					where, i);
		if (datum->siglen != i)
				ep_dbg_cprintf(Dbg, 4, "    Warning: siglen = %d\n",
						datum->siglen);
		gdp_buf_reset(datum->sig);
	}
	else if (datum->siglen > 0)
		ep_dbg_cprintf(Dbg, 4, "flush_input_datum: no sig, but siglen = %d\n",
				datum->siglen);
	datum->siglen = 0;
}


/*
**  GET_STARTING_POINT --- get the starting point for a read or subscribe
*/

static EP_STAT
get_starting_point(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_datum_t *datum = req->pdu->datum;

	if (EP_TIME_IS_VALID(&datum->ts) && datum->recno <= 0)
	{
		// read by timestamp instead of record number
		estat = req->gcl->x->physimpl->ts_to_recno(req->gcl, datum);
	}
	else
	{
		// handle record numbers relative to the end
		if (datum->recno <= 0)
		{
			datum->recno += req->gcl->nrecs + 1;
			if (datum->recno <= 0)
			{
				// can't read before the beginning
				datum->recno = 1;
			}
		}
	}
	req->nextrec = req->pdu->datum->recno;
	return estat;
}

/*
**	Command implementations
*/

EP_STAT
implement_me(char *s)
{
	ep_app_error("Not implemented: %s", s);
	return GDP_STAT_NOT_IMPLEMENTED;
}


/***********************************************************************
**  GDP command implementations
**
**		Each of these takes a request as the argument.
**
**		These routines should set req->pdu->cmd to the "ACK" reply
**		code, which will be used if the command succeeds (i.e.,
**		returns EP_STAT_OK).  Otherwise the return status is decoded
**		to produce a NAK code.  A specific NAK code can be sent
**		using GDP_STAT_FROM_NAK(nak).
**
**		All routines are expected to consume all their input from
**		the channel and to write any output to the same channel.
**		They can consume any unexpected input using flush_input_data.
**
***********************************************************************/


/*
**  CMD_PING --- just return an OK response to indicate that we are alive.
**
**		If this is addressed to a GCL (instead of the daemon itself),
**		it is really a test to see if the subscription is still alive.
*/

EP_STAT
cmd_ping(gdp_req_t *req)
{
	gdp_gcl_t *gcl;
	EP_STAT estat = EP_STAT_OK;

	req->pdu->cmd = GDP_ACK_SUCCESS;
	flush_input_data(req, "cmd_ping");

	if (GDP_NAME_SAME(req->pdu->dst, _GdpMyRoutingName))
		return EP_STAT_OK;

	gcl = _gdp_gcl_cache_get(req->pdu->dst, GDP_MODE_RO);
	if (gcl != NULL)
	{
		// We know about the GCL.  How about the subscription?
		gdp_req_t *sub;

		LIST_FOREACH(sub, &req->gcl->reqs, gcllist)
		{
			if (GDP_NAME_SAME(sub->pdu->dst, req->pdu->src) &&
					EP_UT_BITSET(GDP_REQ_SRV_SUBSCR, sub->flags))
			{
				// Yes, we have a subscription!
				goto done;
			}
		}
	}

	req->pdu->cmd = GDP_NAK_S_LOSTSUB;		// lost subscription
	estat =  GDP_STAT_NAK_NOTFOUND;

done:
	_gdp_gcl_decref(&gcl);
	return estat;
}


/*
**  CMD_CREATE --- create new GCL.
**
**		A bit unusual in that the PDU is addressed to the daemon,
**		not the log; the log name is in the payload.  However, we
**		respond using the name of the new log rather than the
**		daemon.
*/

EP_STAT
cmd_create(gdp_req_t *req)
{
	EP_STAT estat;
	gdp_gcl_t *gcl = NULL;
	gdp_gclmd_t *gmd;
	gdp_name_t gclname;
	int i;

	if (!GDP_NAME_SAME(req->pdu->dst, _GdpMyRoutingName))
	{
		// this is directed to a GCL, not to the daemon
		return gdpd_gcl_error(req->pdu->dst, "cmd_create: log name required",
							GDP_STAT_NAK_CONFLICT, GDP_STAT_NAK_BADREQ);
	}

	req->pdu->cmd = GDP_ACK_CREATED;

	// get the name of the new GCL
	ep_thr_mutex_lock(&req->pdu->datum->mutex);
	i = gdp_buf_read(req->pdu->datum->dbuf, gclname, sizeof gclname);
	if (i < sizeof gclname)
	{
		ep_dbg_cprintf(Dbg, 2, "cmd_create: gclname required\n");
		estat = GDP_STAT_GCL_NAME_INVALID;
		goto fail0;
	}

	// make sure we aren't creating a log with our name
	if (GDP_NAME_SAME(gclname, _GdpMyRoutingName))
	{
		estat = gdpd_gcl_error(gclname,
				"cmd_create: cannot create a log with same name as logd",
				GDP_STAT_GCL_NAME_INVALID, GDP_STAT_NAK_FORBIDDEN);
		goto fail0;
	}

	{
		gdp_pname_t pbuf;

		ep_dbg_cprintf(Dbg, 14, "cmd_create: creating GCL %s\n",
				gdp_printable_name(gclname, pbuf));
	}

	if (GDP_PROTO_MIN_VERSION <= 2 && req->pdu->ver == 2)
	{
		// change the request to seem to come from this GCL
		memcpy(req->pdu->dst, gclname, sizeof req->pdu->dst);
	}

	// get the memory space for the GCL itself
	estat = gcl_alloc(gclname, GDP_MODE_AO, &gcl);
	EP_STAT_CHECK(estat, goto fail0);
	req->gcl = gcl;			// for debugging

	// assume both read and write modes
	gcl->iomode = GDP_MODE_RA;

	// collect metadata, if any
	gmd = _gdp_gclmd_deserialize(req->pdu->datum->dbuf);

	// no further input, so we can reset the buffer just to be safe
	flush_input_data(req, "cmd_create");

	// do the physical create
	gcl->gclmd = gmd;
	estat = gcl->x->physimpl->create(gcl, gcl->gclmd);
	if (!EP_STAT_ISOK(estat))
	{
		req->pdu->cmd = GDP_NAK_S_INTERNAL;
		goto fail0;
	}

	// advertise this new GCL
	logd_advertise_one(gcl->name, GDP_CMD_ADVERTISE);

	// cache the open GCL Handle for possible future use
	EP_ASSERT_INSIST(gdp_name_is_valid(gcl->name));
	_gdp_gcl_cache_add(gcl, gcl->iomode);

	// pass any creation info back to the caller
	// (none at this point)

	// leave this in the cache
	gcl->flags |= GCLF_DEFER_FREE;
	_gdp_gcl_decref(&req->gcl);

fail0:
	ep_thr_mutex_unlock(&req->pdu->datum->mutex);
	if (gcl != NULL)
	{
		char ebuf[60];

		admin_post_stats(ADMIN_LOG_EXIST, "log-create",
				"log-name", gcl->pname,
				"status", ep_stat_tostr(estat, ebuf, sizeof ebuf),
				NULL, NULL);
	}

	if (ep_dbg_test(Dbg, 20))
	{
		char ebuf[100];
		ep_dbg_printf("<<< cmd_create(%s): %s\n", gcl->pname,
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	return estat;
}


/*
**  CMD_OPEN --- open for read-only, append-only, or read-append
**
**		From the point of view of gdplogd these are the same command.
*/

EP_STAT
cmd_open(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_gcl_t *gcl = NULL;

	req->pdu->cmd = GDP_ACK_SUCCESS;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_open");

	// see if we already know about this GCL
	estat = get_open_handle(req, GDP_MODE_ANY);
	if (!EP_STAT_ISOK(estat))
	{
		estat = gdpd_gcl_error(req->pdu->dst, "cmd_open: could not open GCL",
							estat, GDP_STAT_NAK_INTERNAL);
		return estat;
	}

	ep_thr_mutex_lock(&req->pdu->datum->mutex);
	gcl = req->gcl;
	gcl->flags |= GCLF_DEFER_FREE;
	gcl->iomode = GDP_MODE_RA;
	if (gcl->gclmd != NULL)
	{
		// send metadata as payload
		_gdp_gclmd_serialize(gcl->gclmd, req->pdu->datum->dbuf);
	}

	{
		char ebuf[60];

		admin_post_stats(ADMIN_LOG_OPEN, "log-open",
				"log-name", gcl->pname,
				"status", ep_stat_tostr(estat, ebuf, sizeof ebuf),
				NULL, NULL);
	}
	req->pdu->datum->recno = gcl->nrecs;

	if (ep_dbg_test(Dbg, 20))
	{
		char ebuf[100];
		ep_dbg_printf("<<< cmd_open(%s): gcl %p nrecs %" PRIgdp_recno ":\n    %s\n",
					gcl->pname, gcl, gcl->nrecs,
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	ep_thr_mutex_unlock(&req->pdu->datum->mutex);
	_gdp_gcl_decref(&req->gcl);
	return estat;
}


/*
**  CMD_CLOSE --- close an open GCL
**
**		XXX	Since GCLs are shared between clients you really can't just
**			close things willy-nilly.  Thus, this is currently a no-op
**			until such time as reference counting works.
**
**		XXX	We need to have a way of expiring unused GCLs that are not
**			closed.
*/

EP_STAT
cmd_close(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;

	req->pdu->cmd = GDP_ACK_SUCCESS;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_close");

	// a bit wierd to open the GCL only to close it again....
	estat = get_open_handle(req, GDP_MODE_ANY);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_close: GCL not open",
							estat, GDP_STAT_NAK_BADREQ);
	}

	// remove any subscriptions
	_gdp_gcl_unsubscribe(req->gcl, req->pdu->src);

	//return number of records
	req->pdu->datum->recno = req->gcl->nrecs;

	if (ep_dbg_test(Dbg, 20))
	{
		char ebuf[100];
		ep_dbg_printf("<<< cmd_close(%s): %s\n", req->gcl->pname,
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	// drop reference
	_gdp_gcl_decref(&req->gcl);

	return estat;
}


/*
**  CMD_READ --- read a single record from a GCL
**
**		This returns the data as part of the response.  To get multiple
**		values in one call, see cmd_subscribe.
*/

EP_STAT
cmd_read(gdp_req_t *req)
{
	EP_STAT estat;

	req->pdu->cmd = GDP_ACK_CONTENT;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_read");

	estat = get_open_handle(req, GDP_MODE_RO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_read: GCL open failure",
							estat, GDP_STAT_NAK_INTERNAL);
	}

	estat = get_starting_point(req);
	EP_STAT_CHECK(estat, goto fail0);

	ep_thr_mutex_lock(&req->pdu->datum->mutex);
	estat = req->gcl->x->physimpl->read_by_recno(req->gcl, req->pdu->datum);
	ep_thr_mutex_unlock(&req->pdu->datum->mutex);

	// deliver "record expired" as "not found"
	if (EP_STAT_IS_SAME(estat, GDP_STAT_RECORD_EXPIRED))
		estat = GDP_STAT_NAK_GONE;

fail0:
	_gdp_gcl_decref(&req->gcl);
	return estat;
}


/*
**  Initialize the digest field
**
**		This needs to be done during the append rather than the open
**		so if gdplogd is restarted, existing connections will heal.
*/

static EP_STAT
init_sig_digest(gdp_gcl_t *gcl)
{
	EP_STAT estat;
	size_t pklen;
	uint8_t *pkbuf;
	int pktype;
	int mdtype;
	EP_CRYPTO_KEY *key;

	if (gcl->digest != NULL)
		return EP_STAT_OK;

	// assuming we have a public key, set up the message digest context
	if (gcl->gclmd == NULL)
		goto nopubkey;
	estat = gdp_gclmd_find(gcl->gclmd, GDP_GCLMD_PUBKEY, &pklen,
					(const void **) &pkbuf);
	if (!EP_STAT_ISOK(estat) || pklen < 5)
		goto nopubkey;

	mdtype = pkbuf[0];
	pktype = pkbuf[1];
	//pkbits = (pkbuf[2] << 8) | pkbuf[3];
	ep_dbg_cprintf(Dbg, 40, "init_sig_data: mdtype=%d, pktype=%d, pklen=%zd\n",
			mdtype, pktype, pklen);
	key = ep_crypto_key_read_mem(pkbuf + 4, pklen - 4,
			EP_CRYPTO_KEYFORM_DER, EP_CRYPTO_F_PUBLIC);
	if (key == NULL)
		goto nopubkey;

	gcl->digest = ep_crypto_vrfy_new(key, mdtype);

	// include the GCL name
	ep_crypto_vrfy_update(gcl->digest, gcl->name, sizeof gcl->name);

	// and the metadata (re-serialized)
	gdp_buf_t *evb = gdp_buf_new();
	_gdp_gclmd_serialize(gcl->gclmd, evb);
	size_t evblen = gdp_buf_getlength(evb);
	ep_crypto_vrfy_update(gcl->digest, gdp_buf_getptr(evb, evblen), evblen);
	gdp_buf_free(evb);

	if (false)
	{
nopubkey:
		if (EP_UT_BITSET(GDP_SIG_PUBKEYREQ, GdpSignatureStrictness))
		{
			ep_dbg_cprintf(Dbg, 1, "ERROR: no public key for %s\n",
						gcl->pname);
			estat = GDP_STAT_CRYPTO_SIGFAIL;
		}
		else
		{
			ep_dbg_cprintf(Dbg, 52, "WARNING: no public key for %s\n",
						gcl->pname);
			estat = EP_STAT_OK;
		}
	}

	return estat;
}



/*
**  CMD_APPEND --- append a datum to a GCL
**
**		This will have side effects if there are subscriptions pending.
*/

#define PUT64(v) \
		{ \
			*pbp++ = ((v) >> 56) & 0xff; \
			*pbp++ = ((v) >> 48) & 0xff; \
			*pbp++ = ((v) >> 40) & 0xff; \
			*pbp++ = ((v) >> 32) & 0xff; \
			*pbp++ = ((v) >> 24) & 0xff; \
			*pbp++ = ((v) >> 16) & 0xff; \
			*pbp++ = ((v) >> 8) & 0xff; \
			*pbp++ = ((v) & 0xff); \
		}

EP_STAT
cmd_append(gdp_req_t *req)
{
	EP_STAT estat;

	req->pdu->cmd = GDP_ACK_CREATED;

	estat = get_open_handle(req, GDP_MODE_AO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_append: GCL not open",
							estat, GDP_STAT_NAK_BADREQ);
	}

	// validate sequence number and signature
	ep_thr_mutex_lock(&req->pdu->datum->mutex);
	if (req->pdu->datum->recno != req->gcl->nrecs + 1)
	{
		bool random_order_ok = GdplogdForgive.allow_log_gaps &&
								GdplogdForgive.allow_log_dups;

		// replay or missing a record
		ep_dbg_cprintf(Dbg, random_order_ok ? 29 : 9,
						"cmd_append: record out of sequence: got %"
						PRIgdp_recno ", expected %" PRIgdp_recno "\n"
						"\ton log %s\n",
						req->pdu->datum->recno, req->gcl->nrecs + 1,
						req->gcl->pname);

		if (req->pdu->datum->recno <= req->gcl->nrecs)
		{
			// may be a duplicate append, or just filling in a gap
			// (should probably see if duplicates are the same data)

			if (!GdplogdForgive.allow_log_dups &&
				req->gcl->x->physimpl->recno_exists(
									req->gcl, req->pdu->datum->recno))
			{
				char mbuf[100];
				snprintf(mbuf, sizeof mbuf,
						"cmd_append: duplicate record number %" PRIgdp_recno,
						req->pdu->datum->recno);
				estat = gdpd_gcl_error(req->pdu->dst, mbuf,
						GDP_STAT_RECORD_DUPLICATED, GDP_STAT_NAK_CONFLICT);
				goto fail0;
			}
		}
		else if (req->pdu->datum->recno > req->gcl->nrecs + 1 &&
				!GdplogdForgive.allow_log_gaps)
		{
			// gap in record numbers
			char mbuf[100];
			snprintf(mbuf, sizeof mbuf,
					"cmd_append: record number %" PRIgdp_recno " missing",
					req->pdu->datum->recno);
			estat = gdpd_gcl_error(req->pdu->dst, mbuf,
					GDP_STAT_RECNO_SEQ_ERROR, GDP_STAT_NAK_FORBIDDEN);
			goto fail0;
		}
	}

	if (req->gcl->digest == NULL)
	{
		estat = init_sig_digest(req->gcl);
		EP_STAT_CHECK(estat, goto fail1);
	}

	// check the signature in the PDU
	if (req->gcl->digest == NULL)
	{
		// error (maybe): no public key
		if (EP_UT_BITSET(GDP_SIG_PUBKEYREQ, GdpSignatureStrictness))
		{
			ep_dbg_cprintf(Dbg, 1, "cmd_append: no public key (fail)\n");
			goto fail1;
		}
		ep_dbg_cprintf(Dbg, 51, "cmd_append: no public key (warn)\n");
	}
	else if (req->pdu->datum->sig == NULL)
	{
		// error (maybe): signature required
		if (EP_UT_BITSET(GDP_SIG_REQUIRED, GdpSignatureStrictness))
		{
			ep_dbg_cprintf(Dbg, 1, "cmd_append: missing signature (fail)\n");
			goto fail1;
		}
		ep_dbg_cprintf(Dbg, 1, "cmd_append: missing signature (warn)\n");
	}
	else
	{
		// check the signature
		uint8_t recnobuf[8];		// 64 bits
		uint8_t *pbp = recnobuf;
		size_t len;
		gdp_datum_t *datum = req->pdu->datum;
		EP_CRYPTO_MD *md = ep_crypto_md_clone(req->gcl->digest);

		PUT64(datum->recno);
		ep_crypto_vrfy_update(md, &recnobuf, sizeof recnobuf);
		len = gdp_buf_getlength(datum->dbuf);
		ep_crypto_vrfy_update(md, gdp_buf_getptr(datum->dbuf, len), len);
		len = gdp_buf_getlength(datum->sig);
		estat = ep_crypto_vrfy_final(md, gdp_buf_getptr(datum->sig, len), len);
		ep_crypto_md_free(md);
		if (!EP_STAT_ISOK(estat))
		{
			// error: signature failure
			if (EP_UT_BITSET(GDP_SIG_MUSTVERIFY, GdpSignatureStrictness))
			{
				ep_dbg_cprintf(Dbg, 1, "cmd_append: signature failure (fail)\n");
				goto fail1;
			}
			ep_dbg_cprintf(Dbg, 51, "cmd_append: signature failure (warn)\n");
		}
		else
		{
			ep_dbg_cprintf(Dbg, 51, "cmd_append: good signature\n");
		}
	}

	// make sure the timestamp is current
	estat = ep_time_now(&req->pdu->datum->ts);

	// create the message
	estat = req->gcl->x->physimpl->append(req->gcl, req->pdu->datum);

	if (EP_STAT_ISOK(estat))
	{
		// send the new data to any subscribers
		sub_notify_all_subscribers(req, GDP_ACK_CONTENT);

		// update the server's view of the number of records
		req->gcl->nrecs++;
	}

	if (false)
	{
fail1:
		estat = GDP_STAT_NAK_FORBIDDEN;
	}

fail0:
	// return the actual last record number (even on error)
	req->pdu->datum->recno = req->gcl->nrecs;

	// we can now drop the data and signature in the request
	gdp_buf_reset(req->pdu->datum->dbuf);
	if (req->pdu->datum->sig != NULL)
		gdp_buf_reset(req->pdu->datum->sig);
	req->pdu->datum->siglen = 0;

	// we're no longer using this handle
	ep_thr_mutex_unlock(&req->pdu->datum->mutex);
	_gdp_gcl_decref(&req->gcl);

	return estat;
}


/*
**  POST_SUBSCRIBE --- do subscription work after initial ACK
**
**		Assuming the subscribe worked we are now going to deliver any
**		previously existing records.  Once those are all sent we can
**		convert this to an ordinary subscription.  If the subscribe
**		request is satisified, we remove it.
**
**		This code is also the core of multiread.
*/

void
post_subscribe(gdp_req_t *req)
{
	EP_STAT estat;

	EP_ASSERT_REQUIRE(req != NULL);
	ep_dbg_cprintf(Dbg, 38,
			"post_subscribe: numrecs = %d, nextrec = %"PRIgdp_recno"\n",
			req->numrecs, req->nextrec);

	// make sure the request has the right command
	req->pdu->cmd = GDP_ACK_CONTENT;

	while (req->numrecs >= 0)
	{
		EP_ASSERT_INSIST(req->gcl != NULL);

		// see if data pre-exists in the GCL
		if (req->nextrec > req->gcl->nrecs)
		{
			// no, it doesn't; convert to long-term subscription
			break;
		}

		// get the next record and return it as an event
		req->pdu->datum->recno = req->nextrec;
		estat = req->gcl->x->physimpl->read_by_recno(req->gcl, req->pdu->datum);
		if (EP_STAT_ISOK(estat))
		{
			// OK, the next record exists: send it
			req->stat = estat = _gdp_pdu_out(req->pdu, req->chan, NULL);

			// have to clear the old data
			gdp_buf_reset(req->pdu->datum->dbuf);

			// advance to the next record
			if (req->numrecs > 0 && --req->numrecs == 0)
			{
				// numrecs was positive, now zero, but zero means infinity
				req->numrecs--;
			}
			req->nextrec++;
		}
		else if (!EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOTFOUND))
		{
			// this is some error that should be logged
			ep_log(estat, "post_subscribe: bad read");
			req->numrecs = -1;		// terminate subscription
		}
		else
		{
			// shouldn't happen
			ep_log(estat, "post_subscribe: read EOF");
		}

		// if we didn't successfully send a record, terminate
		EP_STAT_CHECK(estat, break);

		// DEBUG: force records to be interspersed
		if (ep_dbg_test(Dbg, 101))
			ep_time_nanosleep(1 MILLISECONDS);
	}

	if (req->numrecs < 0 || !EP_UT_BITSET(GDP_REQ_SUBUPGRADE, req->flags))
	{
		// no more to read: do cleanup & send termination notice
		_gdp_gcl_lock(req->gcl);
		sub_end_subscription(req);
		_gdp_gcl_unlock(req->gcl);
	}
	else
	{
		ep_dbg_cprintf(Dbg, 24, "post_subscribe: converting to subscription\n");
		req->flags |= GDP_REQ_SRV_SUBSCR;

		// link this request into the GCL so the subscription can be found
		if (!EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags))
		{
			_gdp_gcl_incref(req->gcl);
			_gdp_gcl_lock(req->gcl);
			LIST_INSERT_HEAD(&req->gcl->reqs, req, gcllist);
			req->flags |= GDP_REQ_ON_GCL_LIST;
			_gdp_gcl_unlock(req->gcl);
		}
	}
}


/*
**  CMD_SUBSCRIBE --- subscribe command
**
**		Arranges to return existing data (if any) after the response
**		is sent, and non-existing data (if any) as a side-effect of
**		append.
**
**		XXX	Race Condition: if records are written between the time
**			the subscription and the completion of the first half of
**			this process, some records may be lost.  For example,
**			if the GCL has 20 records (1-20) and you ask for 20
**			records starting at record 11, you probably want records
**			11-30.  But if during the return of records 11-20 another
**			record (21) is written, then the second half of the
**			subscription will actually return records 22-31.
**
**		XXX	Does not implement timeouts.
*/

EP_STAT
cmd_subscribe(gdp_req_t *req)
{
	EP_STAT estat;
	EP_TIME_SPEC timeout;

	req->pdu->cmd = GDP_ACK_SUCCESS;

	// find the GCL handle
	estat = get_open_handle(req, GDP_MODE_RO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_subscribe: GCL not open",
							estat, GDP_STAT_NAK_BADREQ);
	}

	// get the additional parameters: number of records and timeout
	ep_thr_mutex_lock(&req->pdu->datum->mutex);
	req->numrecs = (int) gdp_buf_get_uint32(req->pdu->datum->dbuf);
	gdp_buf_get_timespec(req->pdu->datum->dbuf, &timeout);

	if (ep_dbg_test(Dbg, 14))
	{
		ep_dbg_printf("cmd_subscribe: first = %" PRIgdp_recno ", numrecs = %d\n  ",
				req->pdu->datum->recno, req->numrecs);
		_gdp_gcl_dump(req->gcl, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	// should have no more input data; ignore anything there
	flush_input_data(req, "cmd_subscribe");
	ep_thr_mutex_unlock(&req->pdu->datum->mutex);

	if (req->numrecs < 0)
	{
		return GDP_STAT_NAK_BADOPT;
	}

	// get our starting point, which may be relative to the end
	estat = get_starting_point(req);
	EP_STAT_CHECK(estat, goto fail0);

	ep_dbg_cprintf(Dbg, 24,
			"cmd_subscribe: starting from %" PRIgdp_recno ", %d records\n",
			req->nextrec, req->numrecs);

	// see if this is refreshing an existing subscription
	{
		gdp_req_t *r1;

		for (r1 = LIST_FIRST(&req->gcl->reqs); r1 != NULL;
				r1 = LIST_NEXT(r1, gcllist))
		{
			if (ep_dbg_test(Dbg, 50))
			{
				ep_dbg_printf("cmd_subscribe: comparing to ");
				_gdp_req_dump(r1, ep_dbg_getfile(), 0, 0);
			}
			if (GDP_NAME_SAME(r1->pdu->dst, req->pdu->src) &&
					r1->pdu->rid == req->pdu->rid)
			{
				ep_dbg_cprintf(Dbg, 20, "cmd_subscribe: refreshing sub\n");
				break;
			}
		}
		if (r1 != NULL)
		{
			// make sure we don't send data already sent
			req->nextrec = r1->nextrec;

			// abandon old request, we'll overwrite it with new request
			ep_dbg_cprintf(Dbg, 20, "cmd_subscribe: removing old request\n");
			_gdp_req_lock(r1);
			_gdp_req_free(&r1);
		}
	}

	// mark this as persistent and upgradable
	req->flags |= GDP_REQ_PERSIST | GDP_REQ_SUBUPGRADE;

	// note that the subscription is active
	ep_time_now(&req->act_ts);

	// if some of the records already exist, arrange to return them
	if (req->nextrec <= req->gcl->nrecs)
	{
		ep_dbg_cprintf(Dbg, 24, "cmd_subscribe: doing post processing\n");
		req->flags &= ~GDP_REQ_SRV_SUBSCR;
		req->postproc = &post_subscribe;
	}
	else
	{
		// this is a pure "future" subscription
		ep_dbg_cprintf(Dbg, 24, "cmd_subscribe: enabling subscription\n");
		req->flags |= GDP_REQ_SRV_SUBSCR;

		// link this request into the GCL so the subscription can be found
		if (!EP_UT_BITSET(GDP_REQ_ON_GCL_LIST, req->flags))
		{
			_gdp_gcl_lock(req->gcl);
			LIST_INSERT_HEAD(&req->gcl->reqs, req, gcllist);
			req->flags |= GDP_REQ_ON_GCL_LIST;
			_gdp_gcl_unlock(req->gcl);
		}
	}

	// we don't drop the GCL reference until the subscription is satisified

fail0:
	return EP_STAT_OK;
}


/*
**  CMD_MULTIREAD --- read multiple records
**
**		Arranges to return existing data (if any) after the response
**		is sent.  No long-term subscription will ever be created, but
**		much of the infrastructure is reused.
*/

EP_STAT
cmd_multiread(gdp_req_t *req)
{
	EP_STAT estat;

	req->pdu->cmd = GDP_ACK_SUCCESS;

	// find the GCL handle
	estat  = get_open_handle(req, GDP_MODE_RO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_multiread: GCL not open",
							estat, GDP_STAT_NAK_BADREQ);
	}

	// get the additional parameters: number of records and timeout
	ep_thr_mutex_lock(&req->pdu->datum->mutex);
	req->numrecs = (int) gdp_buf_get_uint32(req->pdu->datum->dbuf);

	if (ep_dbg_test(Dbg, 14))
	{
		ep_dbg_printf("cmd_multiread: first = %" PRIgdp_recno ", numrecs = %d\n  ",
				req->pdu->datum->recno, req->numrecs);
		_gdp_gcl_dump(req->gcl, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	// should have no more input data; ignore anything there
	flush_input_data(req, "cmd_multiread");

	if (req->numrecs < 0)
	{
		return GDP_STAT_NAK_BADOPT;
	}

	// get our starting point, which may be relative to the end
	estat = get_starting_point(req);
	ep_thr_mutex_unlock(&req->pdu->datum->mutex);
	EP_STAT_CHECK(estat, goto fail0);

	ep_dbg_cprintf(Dbg, 24, "cmd_multiread: starting from %" PRIgdp_recno
			", %d records\n",
			req->nextrec, req->numrecs);

	// if some of the records already exist, arrange to return them
	if (req->nextrec <= req->gcl->nrecs)
	{
		ep_dbg_cprintf(Dbg, 24, "cmd_multiread: doing post processing\n");
		req->postproc = &post_subscribe;

		// make this a "snapshot", i.e., don't read additional records
		int32_t nrec = req->gcl->nrecs - req->nextrec;
		if (nrec < req->numrecs || req->numrecs == 0)
			req->numrecs = nrec + 1;

		// keep the request around until the post-processing is done
		req->flags |= GDP_REQ_PERSIST;
	}
	else
	{
		// no data to read
		estat = GDP_STAT_NAK_NOTFOUND;
	}

fail0:
	return estat;
}


/*
**  CMD_UNSUBSCRIBE --- terminate a subscription
**
**		XXX not implemented yet
*/


/*
**  CMD_GETMETADATA --- get metadata for a GCL
*/

EP_STAT
cmd_getmetadata(gdp_req_t *req)
{
	gdp_gclmd_t *gmd;
	EP_STAT estat;

	req->pdu->cmd = GDP_ACK_CONTENT;

	// should have no input data; ignore anything there
	ep_thr_mutex_lock(&req->pdu->datum->mutex);
	flush_input_data(req, "cmd_getmetadata");

	estat = get_open_handle(req, GDP_MODE_RO);
	if (!EP_STAT_ISOK(estat))
	{
		estat = gdpd_gcl_error(req->pdu->dst, "cmd_getmetadata: GCL open failure",
							estat, GDP_STAT_NAK_INTERNAL);
		goto fail0;
	}

	// get the metadata into memory
	estat = req->gcl->x->physimpl->getmetadata(req->gcl, &gmd);
	EP_STAT_CHECK(estat, goto fail1);

	// serialize it to the client
	_gdp_gclmd_serialize(gmd, req->pdu->datum->dbuf);
	ep_thr_mutex_unlock(&req->pdu->datum->mutex);

fail1:
	_gdp_gcl_decref(&req->gcl);
fail0:
	ep_thr_mutex_unlock(&req->pdu->datum->mutex);
	return estat;
}


EP_STAT
cmd_newsegment(gdp_req_t *req)
{
	EP_STAT estat;
	char ebuf[60];

	req->pdu->cmd = GDP_ACK_CREATED;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_newsegment");

	estat = get_open_handle(req, GDP_MODE_AO);
	EP_STAT_CHECK(estat, goto fail0);

	if (req->gcl->x->physimpl->newsegment == NULL)
		return gdpd_gcl_error(req->pdu->dst,
				"cmd_newsegment: segments not defined for this type",
				GDP_STAT_NAK_METHNOTALLOWED, GDP_STAT_NAK_METHNOTALLOWED);
	estat = req->gcl->x->physimpl->newsegment(req->gcl);
	admin_post_stats(ADMIN_LOG_EXIST, "newsegment",
			"log-name", req->gcl->pname,
			"status", ep_stat_tostr(estat, ebuf, sizeof ebuf),
			NULL, NULL);
	return estat;

fail0:
	return gdpd_gcl_error(req->pdu->dst,
			"cmd_newsegment: cannot create new segment for",
			estat, GDP_STAT_NAK_INTERNAL);
}


/*
**  CMD_FWD_APPEND --- forwarded APPEND command
**
**		Used for replication.  This is identical to an APPEND,
**		except it is addressed to an individual gdplogd rather
**		than to a GCL.  The actual name is in the payload.
**		On return, the message will have a source address of the
**		GCL, not the gdplogd instance (i.e., we don't just do
**		the default swap of src and dst addresses).
*/

EP_STAT
cmd_fwd_append(gdp_req_t *req)
{
	EP_STAT estat;
	gdp_name_t gclname;

	// must be addressed to me
	if (!GDP_NAME_SAME(req->pdu->dst, _GdpMyRoutingName))
	{
		// this is directed to a GCL, not to the daemon
		return gdpd_gcl_error(req->pdu->dst,
							"cmd_create: log name required",
							GDP_STAT_NAK_CONFLICT,
							GDP_STAT_NAK_BADREQ);
	}

	// get the name of the GCL into current PDU
	{
		int i;
		gdp_pname_t pbuf;

		ep_thr_mutex_lock(&req->pdu->datum->mutex);
		i = gdp_buf_read(req->pdu->datum->dbuf, gclname, sizeof gclname);
		ep_thr_mutex_unlock(&req->pdu->datum->mutex);
		if (i < sizeof req->pdu->dst)
		{
			return gdpd_gcl_error(req->pdu->dst,
								"cmd_fwd_append: gclname required",
								GDP_STAT_GCL_NAME_INVALID,
								GDP_STAT_NAK_INTERNAL);
		}
		memcpy(req->pdu->dst, gclname, sizeof req->pdu->dst);

		ep_dbg_cprintf(Dbg, 14, "cmd_fwd_append: %s\n",
				gdp_printable_name(req->pdu->dst, pbuf));
	}

	// actually do the append
	estat = cmd_append(req);

	// remove excess datum content to avoid returning it on ACK
	flush_input_data(req, NULL);

	// make response seem to come from log
	memcpy(req->pdu->dst, gclname, sizeof req->pdu->dst);

	return estat;
}


/**************** END OF COMMAND IMPLEMENTATIONS ****************/



/*
**  GDPD_PROTO_INIT --- initialize protocol module
*/

static struct cmdfuncs	CmdFuncs[] =
{
	{ GDP_CMD_PING,			cmd_ping				},
	{ GDP_CMD_CREATE,		cmd_create				},
	{ GDP_CMD_OPEN_AO,		cmd_open				},
	{ GDP_CMD_OPEN_RO,		cmd_open				},
	{ GDP_CMD_CLOSE,		cmd_close				},
	{ GDP_CMD_READ,			cmd_read				},
	{ GDP_CMD_APPEND,		cmd_append				},
	{ GDP_CMD_SUBSCRIBE,	cmd_subscribe			},
	{ GDP_CMD_MULTIREAD,	cmd_multiread			},
	{ GDP_CMD_GETMETADATA,	cmd_getmetadata			},
	{ GDP_CMD_OPEN_RA,		cmd_open				},
	{ GDP_CMD_NEWSEGMENT,	cmd_newsegment			},
	{ GDP_CMD_FWD_APPEND,	cmd_fwd_append			},
	{ 0,					NULL					}
};

EP_STAT
gdpd_proto_init(void)
{
	// register the commands we implement
	_gdp_register_cmdfuncs(CmdFuncs);
	return EP_STAT_OK;
}
