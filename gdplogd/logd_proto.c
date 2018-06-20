/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
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

#include "logd.h"
#include "logd_admin.h"
#include "logd_pubsub.h"

#include <gdp/gdp_chan.h>		// for PUT64
#include <gdp/gdp_md.h>
#include <gdp/gdp_priv.h>

#include <ep/ep_assert.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.proto", "GDP Log Daemon protocol");

#define GET_PAYLOAD(req, where, bodycase)									\
			{																\
				if (!EP_ASSERT(req != NULL) ||								\
						!EP_ASSERT(req->cpdu != NULL) ||					\
						!EP_ASSERT(req->cpdu->msg != NULL))					\
					return EP_STAT_ASSERT_ABORT;							\
				GdpMessage *msg = req->cpdu->msg;							\
				if (msg->body_case !=										\
						GDP_MESSAGE__BODY_ ## bodycase)						\
				{															\
					ep_dbg_cprintf(Dbg, 1,									\
							"%s: wrong payload type %d (expected %d)\n",	\
							#where,											\
							msg->body_case,									\
							GDP_MESSAGE__BODY_ ## bodycase);				\
					return GDP_STAT_PROTOCOL_FAIL;							\
				}															\
				payload = msg->where;										\
			}

/*
**	GDPD_NAK_RESP --- helper routine for returning NAK responses
*/

static EP_STAT
gdpd_nak_resp(gdp_req_t *req,
			gdp_cmd_t nak_type,
			const char *detail,
			EP_STAT estat)
{
	gdp_pname_t pname;
	char text_message[250];
	gdp_msg_t *msg;
	char ebuf[80];

	// a bit of a hack
	if (nak_type == 0)
		nak_type = _gdp_acknak_from_estat(estat, GDP_NAK_S_INTERNAL);

	gdp_printable_name(req->cpdu->dst, pname);
	snprintf(text_message, sizeof text_message, "%s: %s: %s",
			detail, pname, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	if (GDP_CMD_IS_S_NAK(nak_type))
	{
		// server error (rather than client error)
		ep_log(estat, "%s: %s", detail, pname);
	}
	else
	{
		ep_dbg_cprintf(Dbg, 1, "%s\n", text_message);
	}
	if (req->rpdu != NULL)
	{
		ep_dbg_cprintf(Dbg, 1, "gdpd_nak_resp: flushing old rpdu %p\n",
				req->rpdu);
		_gdp_pdu_free(&req->rpdu);
	}

	ep_dbg_cprintf(Dbg, 10,
				"gdpd_nak_resp: sending %s (%s)\n",
				_gdp_proto_cmd_name(nak_type),
				text_message);
	msg = _gdp_msg_new(nak_type, req->cpdu->msg->rid, req->cpdu->msg->seqno);
	req->rpdu = _gdp_pdu_new(msg, req->cpdu->dst, req->cpdu->src);
	GdpMessage__NakGeneric *nak = msg->nak;
	nak->ep_stat = EP_STAT_TO_INT(estat);
	nak->description = ep_mem_strdup(text_message);

	return estat;
}


/*
**  GDPD_ACK_RESP --- helper routine for returning ACK responses
*/

static EP_STAT
gdpd_ack_resp(
			gdp_req_t *req,
			gdp_cmd_t ack_type)
{
	gdp_msg_t *msg;

	if (req->rpdu != NULL)
	{
		ep_dbg_cprintf(Dbg, 1, "gdpd_ack_resp: flushing old rpdu %p\n",
				req->rpdu);
		_gdp_pdu_free(&req->rpdu);
	}
	msg = _gdp_msg_new(ack_type, req->cpdu->msg->rid, req->cpdu->msg->seqno);
	req->rpdu = _gdp_pdu_new(msg, req->cpdu->dst, req->cpdu->src);
	return EP_STAT_OK;
}


/*
**	Stub
*/

EP_STAT
implement_me(const char *s)
{
	ep_app_error("Not implemented: %s", s);
	return GDP_STAT_NOT_IMPLEMENTED;
}


#if 0	//TODO
/*
**  GET_STARTING_POINT_BY_xxx --- get the starting point for a read or subscribe
*/

static EP_STAT
get_starting_point_by_recno(gdp_req_t *req, gdp_recno_t recno)
{
	EP_STAT estat = EP_STAT_OK;

	// handle record numbers relative to the end
	if (recno <= 0)
	{
		recno += req->gob->nrecs + 1;
		if (recno <= 0)
		{
			// can't read before the beginning
			recno = 1;
		}
	}
	req->nextrec = recno;
	return estat;
}

static EP_STAT
get_starting_point_by_ts(gdp_req_t *req, GdpTimestamp *ts)
{
	return implement_me("get_starting_point_by_ts");
}
#endif	//TODO


/***********************************************************************
**  GDP command implementations
**
**		Each of these takes a request as the argument.
**
**		These routines should set req->rpdu->cmd to the "ACK" reply
**		code, which will be used if the command succeeds (i.e.,
**		returns EP_STAT_OK).  Otherwise the return status is decoded
**		to produce a NAK code.  A specific NAK code can be sent
**		using GDP_STAT_FROM_NAK(nak).
**
***********************************************************************/


// print trace info about a command
#define CMD_TRACE(cmd, msg, ...)											\
			if (ep_dbg_test(Dbg, 20))										\
			{																\
				flockfile(ep_dbg_getfile());								\
				ep_dbg_printf("%s [%d]: ", _gdp_proto_cmd_name(cmd), cmd);	\
				ep_dbg_printf(msg, __VA_ARGS__);							\
				ep_dbg_printf("\n");										\
				funlockfile(ep_dbg_getfile());								\
			}


/*
**  CMD_PING --- just return an OK response to indicate that we are alive.
**
**		If this is addressed to a GOB (instead of the daemon itself),
**		it is really a test to see if the subscription is still alive.
*/

EP_STAT
cmd_ping(gdp_req_t *req)
{
	gdp_gob_t *gob;
	EP_STAT estat = EP_STAT_OK;

	if (GDP_NAME_SAME(req->cpdu->dst, _GdpMyRoutingName))
		goto done;

	estat = _gdp_gob_cache_get(req->rpdu->dst, GGCF_NOCREATE, &gob);
	if (EP_STAT_ISOK(estat))
	{
		// We know about the GOB.  How about the subscription?
		gdp_req_t *sub;

		LIST_FOREACH(sub, &req->gob->reqs, goblist)
		{
			if (GDP_NAME_SAME(sub->rpdu->dst, req->rpdu->dst) &&
					EP_UT_BITSET(GDP_REQ_SRV_SUBSCR, sub->flags))
			{
				// Yes, we have a subscription!
				goto done;
			}
		}
	}

	estat =  GDP_STAT_NAK_NOTFOUND;

done:
	if (EP_STAT_ISOK(estat))
		return gdpd_ack_resp(req, GDP_ACK_SUCCESS);
	return gdpd_nak_resp(req, GDP_NAK_S_LOST_SUBSCR,
					"cmd_ping: lost subscription",
					estat);
}


/*
**  CMD_CREATE --- create new GOB.
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
	gdp_gob_t *gob = NULL;
	gdp_md_t *gmd;
	gdp_name_t gobname;

	if (!GDP_NAME_SAME(req->cpdu->dst, _GdpMyRoutingName))
	{
		// this is directed to a GOB, not to the daemon
		return gdpd_nak_resp(req, GDP_NAK_C_BADREQ,
						"cmd_create: log name required",
						GDP_STAT_NAK_CONFLICT);
	}

	GdpMessage__CmdCreate *payload;
	GET_PAYLOAD(req, cmd_create, CMD_CREATE);

	//XXX for now, insist that the client specify the internal name;
	//XXX ultimately this should be ILLEGAL: name is hash of metadata
	if (payload->logname.len != sizeof gobname ||
			!gdp_name_is_valid(payload->logname.data))
	{
		// bad log name
		if (ep_dbg_test(Dbg, 2))
		{
			ep_dbg_printf("cmd_create: improper log name, len %zd (expected %zd)\n",
						payload->logname.len, sizeof gobname);
			ep_dbg_printf("\tpname %s\n", payload->logname.data);
		}
		estat = gdpd_nak_resp(req, GDP_NAK_C_BADREQ,
						"cmd_create: improper log name",
						GDP_STAT_GDP_NAME_INVALID);
		goto fail0;
	}
	memcpy(gobname, payload->logname.data, sizeof gobname);

	// make sure we aren't creating a log with our name
	if (GDP_NAME_SAME(gobname, _GdpMyRoutingName))
	{
		estat = gdpd_nak_resp(req, GDP_NAK_C_FORBIDDEN,
				"cmd_create: cannot create a log with same name as logd",
				GDP_STAT_GDP_NAME_INVALID);
		goto fail0;
	}

	{
		gdp_pname_t pbuf;
		ep_dbg_cprintf(Dbg, 14, "cmd_create: creating GOB %s\n",
				gdp_printable_name(gobname, pbuf));
	}

	// get the memory space for the GOB itself
	estat = gob_alloc(gobname, GDP_MODE_AO, &gob);
	EP_STAT_CHECK(estat, goto fail0);

	// collect metadata
	if (payload->metadata->data.len == 0)
	{
		estat = gdpd_nak_resp(req, GDP_NAK_C_BADOPT,
						"cmd_create: metadata required",
						GDP_STAT_METADATA_REQUIRED);
		goto fail0;
	}
	gmd = _gdp_md_deserialize(payload->metadata->data.data,
							payload->metadata->data.len);

	// have to get lock ordering right here.
	// safe because no one else can have a handle on this req.
	req->gob = gob;			// for debugging
	_gdp_req_unlock(req);
	_gdp_gob_lock(gob);
	_gdp_req_lock(req);

	// do the physical create
	gob->gob_md = gmd;
	estat = gob->x->physimpl->create(gob, gob->gob_md);
	if (!EP_STAT_ISOK(estat))
	{
		estat = gdpd_nak_resp(req, GDP_NAK_S_INTERNAL,
						"cmd_create: physical create failure",
						estat);
		goto fail1;
	}

	// cache the open GOB Handle for possible future use
	EP_ASSERT(gdp_name_is_valid(gob->name));
	gob->flags |= GOBF_DEFER_FREE;
	gob->flags &= ~GOBF_PENDING;
	_gdp_gob_cache_add(gob);

	// advertise this new GOB
	logd_advertise_one(req->chan, gob->name, GDP_CMD_ADVERTISE);

	// pass any creation info back to the caller
	// (none at this point)

fail1:
fail0:
	if (EP_STAT_ISOK(estat))
	{
		gdpd_ack_resp(req, GDP_ACK_CREATED);
	}
	char ebuf[60];
	if (gob != NULL)
	{
		admin_post_stats(ADMIN_LOG_EXIST, "log-create",
				"log-name", gob->pname,
				"status", ep_stat_tostr(estat, ebuf, sizeof ebuf),
				NULL, NULL);
	}

	ep_dbg_cprintf(Dbg, 9, "<<< cmd_create(%s): %s\n",
				gob != NULL ? gob->pname : "NULL",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));

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
	gdp_gob_t *gob = NULL;

	// see if we already know about this GOB
	estat = get_open_handle(req);
	if (!EP_STAT_ISOK(estat))
	{
		estat = gdpd_nak_resp(req, GDP_NAK_S_INTERNAL,
							"cmd_open: could not open GOB", estat);
		return estat;
	}

	// set up the response
	gdpd_ack_resp(req, GDP_ACK_SUCCESS);
	GdpMessage__AckSuccess *resp = req->rpdu->msg->ack_success;

	gob = req->gob;
	gob->flags |= GOBF_DEFER_FREE;
	if (gob->gob_md != NULL)
	{
		// send metadata as response payload
		uint8_t *obuf;
		size_t olen;
		olen = _gdp_md_serialize(gob->gob_md, &obuf);
		resp->metadata.data = obuf;
		resp->metadata.len = olen;
		resp->has_metadata = true;
	}
	resp->recno = gob->nrecs;
	resp->has_recno = true;

	// post statistics for monitoring
	{
		char ebuf[60];

		admin_post_stats(ADMIN_LOG_OPEN, "log-open",
				"log-name", gob->pname,
				"status", ep_stat_tostr(estat, ebuf, sizeof ebuf),
				NULL, NULL);
	}

	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];
		ep_dbg_printf("<<< cmd_open(%s): gob %p nrecs %" PRIgdp_recno ": %s\n",
					gob->pname, gob, gob->nrecs,
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	return estat;
}


/*
**  CMD_CLOSE --- close an open GOB
**
**		This really doesn't do much except terminate subscriptions.  We
**		let the usual cache reclaim algorithm do the actual close.
*/

EP_STAT
cmd_close(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;

	// a bit wierd to open the GOB only to close it again....
	estat = get_open_handle(req);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_nak_resp(req, GDP_NAK_C_BADREQ,
							"cmd_close: GOB not open", estat);
	}

	// set up the response
	gdpd_ack_resp(req, GDP_ACK_SUCCESS);
	GdpMessage__AckSuccess *resp = req->rpdu->msg->ack_success;

	//return number of records
	resp->recno = req->gob->nrecs;

	// remove any subscriptions
	sub_end_all_subscriptions(req->gob, req->cpdu->src, GDP_PDU_NO_RID);

	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];
		ep_dbg_printf("<<< cmd_close(%s): %s\n", req->gob->pname,
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	return estat;
}


/*
**  CMD_DELETE --- delete a GOB
**
**		This also does all the close actions.
*/

static EP_STAT
verify_req_signature(gdp_req_t *req)
{
	//TODO: IMPLEMENT_ME XXX
	//return EP_STAT_OK;						//DEBUG
	//return GDP_STAT_NAK_UNAUTH;				//DEBUG
	return GDP_STAT_NOT_IMPLEMENTED;
}

EP_STAT
cmd_delete(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;

	// a bit wierd to open the GOB only to close it again....
	estat = get_open_handle(req);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_nak_resp(req, GDP_NAK_C_BADREQ,
							"cmd_delete: GOB not open", estat);
	}

	estat = verify_req_signature(req);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_nak_resp(req, GDP_NAK_C_UNAUTH,
							"cmd_delete: signature failure", estat);
	}

	// set up the response
	gdpd_ack_resp(req, GDP_ACK_DELETED);
	GdpMessage__AckSuccess *resp = req->rpdu->msg->ack_success;

	// return number of records
	resp->recno = req->gob->nrecs;

	// remove log advertisement
	logd_advertise_one(req->chan, req->gob->name, GDP_CMD_WITHDRAW);

	// remove any subscriptions
	sub_end_all_subscriptions(req->gob, req->cpdu->src, GDP_PDU_NO_RID);

	// we will force a close and delete now
	req->gob->freefunc = NULL;
	gob_delete(req->gob);
	req->gob = NULL;

	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];
		ep_dbg_printf("<<< cmd_delete: %s\n",
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	return estat;
}


/*
**  CMD_READ_BY_xxx --- read from a GOB by { record number, timestamp, hash }
**
**		This returns the data as part of the response.  To get multiple
**		values in one call, see cmd_subscribe.
*/

static EP_STAT
cmd_read_helper(gdp_req_t *req)
{
	EP_STAT estat;
	gdp_datum_t *datum = gdp_datum_new();

	datum->recno = req->nextrec;
	CMD_TRACE(req->cpdu->msg->cmd, "%s %" PRIgdp_recno ", %d",
			req->gob->pname, req->nextrec, req->numrecs);

	if (EP_STAT_ISOK(estat))
	{
		gdpd_ack_resp(req, GDP_ACK_CONTENT);
		GdpMessage__AckContent *resp = req->rpdu->msg->ack_content;
		GdpDatum *pbd = ep_mem_zalloc(sizeof *pbd);
		gdp_datum__init(pbd);
		_gdp_datum_to_pb(datum, req->cpdu->msg, pbd);
		resp->dl->d = ep_mem_malloc(sizeof pbd);
		resp->dl->d[0] = pbd;
		resp->dl->n_d = 1;
	}
	else
	{
		if (EP_STAT_IS_SAME(estat, GDP_STAT_RECORD_EXPIRED))
			gdpd_nak_resp(req, GDP_NAK_C_GONE,
						"requested record has expired", estat);
		else if (EP_STAT_IS_SAME(estat, GDP_STAT_RECORD_MISSING))
			gdpd_nak_resp(req, GDP_NAK_C_REC_MISSING,
						"requested record is not available", estat);
		else if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOTFOUND))
			gdpd_nak_resp(req, GDP_NAK_C_NOTFOUND,
						"record not found", estat);
		else
			gdpd_nak_resp(req, GDP_NAK_S_INTERNAL,
						"unknown read failure", estat);
	}
	gdp_datum_free(datum);
	return estat;
}


static void
make_read_acknak_pdu(gdp_req_t *req, EP_STAT estat)
{
	if (EP_STAT_ISOK(estat))
	{
		// OK, the next record exists: send it
		gdpd_ack_resp(req, GDP_ACK_CONTENT);
		GdpMessage__AckContent *resp = req->rpdu->msg->ack_content;
		resp->dl->n_d = 1;		//FIXME: should handle multiples
		EP_ASSERT(resp->dl->d == NULL);
		GdpDatum *pbd;
		resp->dl->d = ep_mem_malloc(resp->dl->n_d * sizeof pbd);
		resp->dl->d[0] = pbd = ep_mem_malloc(sizeof *pbd);
		gdp_datum__init(pbd);
	}
	else if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOTFOUND))
	{
		// no results found matching query
		gdpd_ack_resp(req, GDP_NAK_C_NOTFOUND);
		GdpMessage__NakGeneric *resp = req->rpdu->msg->nak;
		resp->ep_stat = EP_STAT_TO_INT(estat);
		resp->recno = req->nextrec;
	}
	else if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_LOST_SUBSCR))
	{
		// found some results, but no more to come
		gdpd_ack_resp(req, GDP_ACK_END_OF_RESULTS);
		GdpMessage__NakGeneric *resp = req->rpdu->msg->nak;
		resp->ep_stat = EP_STAT_TO_INT(estat);
		resp->recno = req->nextrec;
	}
	else
	{
		// some other failure
		gdpd_ack_resp(req, GDP_NAK_S_INTERNAL);
		GdpMessage__NakGeneric *resp = req->rpdu->msg->nak;
		resp->ep_stat = EP_STAT_TO_INT(estat);
		resp->recno = req->nextrec;
	}
}

static EP_STAT
send_read_result(EP_STAT estat, gdp_datum_t *datum, gdp_result_ctx_t *cb_ctx)
{
	gdp_req_t *req = (gdp_req_t *) cb_ctx;

	make_read_acknak_pdu(req, estat);
	if (EP_STAT_ISOK(estat))
		_gdp_datum_to_pb(datum, req->rpdu->msg,
					req->rpdu->msg->ack_content->dl->d[0]);
	req->stat = _gdp_pdu_out(req->rpdu, req->chan, NULL);
	return estat;
}

EP_STAT
cmd_read_by_recno(gdp_req_t *req)
{
	EP_STAT estat;

	estat = get_open_handle(req);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_nak_resp(req, GDP_NAK_S_INTERNAL,
							"cmd_read_by_recno: GOB open failure", estat);
	}

	GdpMessage__CmdReadByRecno *payload;
	GET_PAYLOAD(req, cmd_read_by_recno, CMD_READ_BY_RECNO);
	req->numrecs = payload->nrecs;
	if (req->numrecs < 0)
		req->numrecs = UINT32_MAX;

	req->nextrec = payload->recno;
	if (req->nextrec < 0)
	{
		req->nextrec += req->gob->nrecs + 1;
		if (req->nextrec <= 0)
			req->nextrec = 1;
	}

	estat = req->gob->x->physimpl->read_by_recno(req->gob,
								req->nextrec, req->numrecs,
								send_read_result, req);
	// if successful, data will have already been returned
	if (EP_STAT_ISOK(estat))
		return GDP_STAT_RESPONSE_SENT;
	make_read_acknak_pdu(req, estat);
	return estat;
}

EP_STAT
cmd_read_by_ts(gdp_req_t *req)
{
	return implement_me("cmd_read_by_ts");
	EP_STAT estat;

	estat = get_open_handle(req);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_nak_resp(req, GDP_NAK_S_INTERNAL,
							"cmd_read_by_ts: GOB open failure", estat);
	}

	GdpMessage__CmdReadByTs *payload;
	GET_PAYLOAD(req, cmd_read_by_ts, CMD_READ_BY_TS);
	req->numrecs = payload->nrecs;
	if (req->numrecs < 0)
		req->numrecs = UINT32_MAX;

	EP_TIME_SPEC ts;
	_gdp_timestamp_from_pb(&ts, payload->timestamp);
	estat = req->gob->x->physimpl->read_by_timestamp(req->gob,
								&ts, req->numrecs,
								send_read_result, req);
	if (EP_STAT_ISOK(estat))
		return GDP_STAT_RESPONSE_SENT;
	make_read_acknak_pdu(req, estat);
	return estat;
}


/*
**  Initialize the digest field
**
**		This needs to be done during the append rather than the open
**		so if gdplogd is restarted, existing connections will heal.
*/

static EP_STAT
init_sig_digest(gdp_gob_t *gob)
{
	EP_STAT estat;
	size_t pklen;
	uint8_t *pkbuf;
	int pktype;
	int mdtype = gob->hashalg;
	EP_CRYPTO_KEY *key;

	if (gob->digest != NULL)
		return EP_STAT_OK;

	// assuming we have a public key, set up the message digest context
	EP_ASSERT_ELSE(gob->gob_md != NULL, return GDP_STAT_NO_METADATA);
	estat = gdp_md_find(gob->gob_md, GDP_MD_PUBKEY, &pklen,
					(const void **) &pkbuf);
	if (!EP_STAT_ISOK(estat) || pklen < 5)
		goto nopubkey;

	mdtype = pkbuf[0];
	pktype = pkbuf[1];
	ep_dbg_cprintf(Dbg, 40, "init_sig_data: mdtype=%d, pktype=%d, pklen=%zd\n",
			mdtype, pktype, pklen);
	key = ep_crypto_key_read_mem(pkbuf + 4, pklen - 4,
			EP_CRYPTO_KEYFORM_DER, EP_CRYPTO_F_PUBLIC);
	if (key != NULL)
	{
		gob->digest = ep_crypto_vrfy_new(key, mdtype);
		gob->flags |= GOBF_SIGNING;
	}
	else
	{
nopubkey:
		if (EP_UT_BITSET(GDP_SIG_PUBKEYREQ, GdpSignatureStrictness))
		{
			ep_dbg_cprintf(Dbg, 1, "ERROR: no public key for %s\n",
						gob->pname);
			estat = GDP_STAT_CRYPTO_SIGFAIL;
		}
		else
		{
			ep_dbg_cprintf(Dbg, 52, "WARNING: no public key for %s\n",
						gob->pname);
			estat = EP_STAT_OK;
		}
		gob->digest = ep_crypto_md_new(mdtype);
	}

	// include the GOB name
	ep_crypto_md_update(gob->digest, gob->name, sizeof gob->name);

	// and the metadata (re-serialized)
	{
		uint8_t *mdbuf;
		size_t mdlen = _gdp_md_serialize(gob->gob_md, &mdbuf);
		ep_crypto_md_update(gob->digest, mdbuf, mdlen);
		ep_mem_free(mdbuf);
	}

	return estat;
}


/*
**  CMD_APPEND --- append a datum to a GOB
**
**		This will have side effects if there are subscriptions pending.
*/

EP_STAT
cmd_append(gdp_req_t *req)
{
	EP_STAT estat;

	estat = get_open_handle(req);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_nak_resp(req, GDP_NAK_C_BADREQ,
							"cmd_append: GOB not open", estat);
	}

	GdpMessage__CmdAppend *payload;
	GET_PAYLOAD(req, cmd_append, CMD_APPEND);
	//FIXME: should loop through datums verifying hash chain.
	//FIXME: only last entry should have signature.
	EP_ASSERT(payload->dl->n_d == 1);		//FIXME
	GdpDatum *pbd = payload->dl->d[0];		//FIXME

	CMD_TRACE(req->cpdu->msg->cmd, "%s %" PRIgdp_recno,
			req->gob->pname, pbd->recno);

	// validate record number
	if (pbd->recno <= 0)
	{
		estat = gdpd_nak_resp(req, GDP_NAK_C_BADOPT,
						"cmd_append: recno must be > 0",
						GDP_STAT_NAK_BADOPT);
		goto fail0;
	}

	if (pbd->recno != (gdp_recno_t) (req->gob->nrecs + 1))
	{
		bool random_order_ok = EP_UT_BITSET(FORGIVE_LOG_GAPS, GdplogdForgive) &&
							EP_UT_BITSET(FORGIVE_LOG_DUPS, GdplogdForgive);

		// replay or missing a record
		ep_dbg_cprintf(Dbg, random_order_ok ? 29 : 9,
						"cmd_append: record out of sequence: got %"
						PRIgdp_recno ", expected %" PRIgdp_recno "\n"
						"\ton log %s\n",
						pbd->recno, req->gob->nrecs + 1,
						req->gob->pname);

		if (pbd->recno <= (gdp_recno_t) req->gob->nrecs)
		{
			// may be a duplicate append, or just filling in a gap
			// (should probably see if duplicates are the same data)

			if (!EP_UT_BITSET(FORGIVE_LOG_DUPS, GdplogdForgive) &&
				req->gob->x->physimpl->recno_exists(
									req->gob, pbd->recno))
			{
				char mbuf[100];
				snprintf(mbuf, sizeof mbuf,
						"cmd_append: duplicate record number %" PRIgdp_recno,
						pbd->recno);
				estat = gdpd_nak_resp(req, GDP_NAK_C_CONFLICT,
						mbuf, GDP_STAT_RECORD_DUPLICATED);
				goto fail0;
			}
		}
		else if (pbd->recno > (gdp_recno_t) (req->gob->nrecs + 1) &&
				!EP_UT_BITSET(FORGIVE_LOG_GAPS, GdplogdForgive))
		{
			// gap in record numbers
			char mbuf[100];
			snprintf(mbuf, sizeof mbuf,
					"cmd_append: record number %" PRIgdp_recno " missing",
					pbd->recno);
			estat = gdpd_nak_resp(req, GDP_NAK_C_FORBIDDEN,
					mbuf, GDP_STAT_RECNO_SEQ_ERROR);
			goto fail0;
		}
	}

	// validate sequence number and signature
	// only path to datum is via this req, so we don't have to lock it
	if (req->gob->digest == NULL)
	{
		estat = init_sig_digest(req->gob);
		EP_STAT_CHECK(estat, goto fail1);
	}

	// check the signature in the PDU
	gdp_datum_t *datum = gdp_datum_new();
	_gdp_datum_from_pb(datum, pbd, pbd->sig);

	if (req->gob->digest == NULL)
	{
		// error (maybe): no public key
		if (EP_UT_BITSET(GDP_SIG_PUBKEYREQ, GdpSignatureStrictness))
		{
			ep_dbg_cprintf(Dbg, 1, "cmd_append: no public key (fail)\n");
			goto fail1;
		}
		ep_dbg_cprintf(Dbg, 51, "cmd_append: no public key (warn)\n");
	}
	else if (pbd->sig == NULL)
	{
		// error (maybe): signature required
		if (EP_UT_BITSET(GDP_SIG_REQUIRED, GdpSignatureStrictness))
		{
			ep_dbg_cprintf(Dbg, 1, "cmd_append: missing signature (fail)\n");
			goto fail1;
		}
		ep_dbg_cprintf(Dbg, 1, "cmd_append: missing signature (warn)\n");
	}
	else if (EP_UT_BITSET(GOBF_SIGNING, req->gob->flags))
	{
		// check the signature
		size_t len;
		EP_CRYPTO_MD *md = ep_crypto_md_clone(req->gob->digest);

		_gdp_datum_digest(datum, md);	//FIXME
		len = pbd->sig->sig.len;
		estat = ep_crypto_vrfy_final(md, pbd->sig->sig.data, len);
		ep_crypto_md_free(md);
		if (!EP_STAT_ISOK(estat))
		{
			// error: signature failure
			if (EP_UT_BITSET(GDP_SIG_MUSTVERIFY, GdpSignatureStrictness))
			{
				ep_dbg_cprintf(Dbg, 1, "cmd_append: signature failure (fail)\n");
				goto fail1;
			}
			ep_dbg_cprintf(Dbg, 31, "cmd_append: signature failure (warn)\n");
		}
		else
		{
			ep_dbg_cprintf(Dbg, 51, "cmd_append: good signature\n");
		}
	}

	// append to disk file and send to subscribers
	{
		// do the physical append to disk
		estat = req->gob->x->physimpl->append(req->gob, datum);
		if (EP_STAT_ISOK(estat))
		{
			// send the new datum to any and all subscribers
			gdp_msg_t *msg = _gdp_msg_new(GDP_ACK_CONTENT,
									req->cpdu->msg->rid, req->cpdu->msg->seqno);

			GdpDatum *pbd = ep_mem_malloc(sizeof *pbd);
			gdp_datum__init(pbd);
			_gdp_datum_to_pb(datum, msg, pbd);

			GdpDatumList *dl = msg->ack_content->dl;
			dl->d = ep_mem_malloc(sizeof pbd);
			dl->n_d = 1;
			dl->d[0] = pbd;

			EP_ASSERT(req->rpdu == NULL);
			req->rpdu = _gdp_pdu_new(msg, req->cpdu->dst, req->cpdu->src);
			sub_notify_all_subscribers(req);
			_gdp_pdu_free(&req->rpdu);
		}
		gdp_datum_free(datum);
	}

	if (EP_STAT_ISOK(estat))
	{
		// update the server's view of the number of records
		req->gob->nrecs++;

		gdpd_ack_resp(req, GDP_ACK_SUCCESS);
		GdpMessage__AckSuccess *resp = req->rpdu->msg->ack_success;
		resp->recno = req->gob->nrecs;
		resp->ts = pbd->ts;
		pbd->ts = NULL;		// avoid double free
	}
	else
	{
		estat = gdpd_nak_resp(req, GDP_MSG_CODE__CMD_NONE,
						"cmd_append: append failure",
						estat);
	}

	if (false)
	{
fail1:
		estat = gdpd_nak_resp(req, GDP_NAK_C_FORBIDDEN,
						"cmd_append",
						GDP_STAT_NAK_FORBIDDEN);
		req->rpdu->msg->nak->recno = req->gob->nrecs;
	}

fail0:
	EP_ASSERT(req->rpdu != NULL);
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

	EP_ASSERT_ELSE(req != NULL, return);
	EP_ASSERT_ELSE(req->state != GDP_REQ_FREE, return);
	EP_ASSERT_ELSE(req->gob != NULL, return);
	ep_dbg_cprintf(Dbg, 38,
			"post_subscribe: numrecs %d, nextrec = %"PRIgdp_recno
			" gob->nrecs %"PRIgdp_recno "\n",
			req->numrecs, req->nextrec, req->gob->nrecs);

	if (req->numrecs <= 0)
		req->numrecs = INT32_MAX;

	// if data pre-exists in the GOB, return it now
	if (req->nextrec <= req->gob->nrecs)
	{
		// get the existing records and return them via callback
		estat = req->gob->x->physimpl->read_by_recno(
									req->gob,
									req->nextrec, req->numrecs,
									send_read_result, req);
		if (EP_STAT_ISOK(estat))
		{
			gdp_recno_t nrecs = EP_STAT_TO_INT(estat);
			req->nextrec += nrecs;
			req->numrecs -= nrecs;
		}
	}

	// done with response PDU
	if (req->rpdu != NULL)
		_gdp_pdu_free(&req->rpdu);

	if (req->numrecs < 0 || !EP_UT_BITSET(GDP_REQ_SUBUPGRADE, req->flags))
	{
		// no more to read: do cleanup & send termination notice
		sub_end_subscription(req);
	}
	else
	{
		if (ep_dbg_test(Dbg, 24))
		{
			ep_dbg_printf("post_subscribe: converting to subscription\n    ");
			_gdp_req_dump(req, NULL, GDP_PR_BASIC, 0);
		}
		req->flags |= GDP_REQ_SRV_SUBSCR;

		// link this request into the GOB so the subscription can be found
		if (!EP_UT_BITSET(GDP_REQ_ON_GOB_LIST, req->flags))
		{
			IF_LIST_CHECK_OK(&req->gob->reqs, req, goblist, gdp_req_t)
			{
				// req->gob->refcnt already allows for this reference
				LIST_INSERT_HEAD(&req->gob->reqs, req, goblist);
				req->flags |= GDP_REQ_ON_GOB_LIST;
			}
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
**			if the GOB has 20 records (1-20) and you ask for 20
**			records starting at record 11, you probably want records
**			11-30.  But if during the return of records 11-20 another
**			record (21) is written, then the second half of the
**			subscription will actually return records 22-31.
**
**		XXX	Does not implement timeouts.
**
**		XXX This assumes different commands based on the initial
**			starting "query" (recno, timestamp, hash).  Another
**			possible approach would be a single command that would
**			send at most one of those values.  Error conditions
**			galore, but fewer commands to handle.
*/

EP_STAT
cmd_subscribe_by_recno(gdp_req_t *req)
{
	EP_STAT estat;
	EP_TIME_SPEC timeout;
	gdp_gob_t *gob;

	if (req->gob != NULL)
		GDP_GOB_ASSERT_ISLOCKED(req->gob);

	GdpMessage__CmdSubscribeByRecno *payload;
	GET_PAYLOAD(req, cmd_subscribe_by_recno, CMD_SUBSCRIBE_BY_RECNO);

	// find the GOB handle
	estat = get_open_handle(req);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_nak_resp(req, GDP_NAK_C_BADREQ,
							"cmd_subscribe: GOB not open", estat);
	}

	gob = req->gob;
	if (!EP_ASSERT(GDP_GOB_ISGOOD(gob)))
	{
		ep_dbg_printf("cmd_subscribe: bad gob %p in req, flags = %x\n",
				gob, gob == NULL ? 0 : gob->flags);
		return EP_STAT_ASSERT_ABORT;
	}

	// get the additional parameters: number of records and timeout
	req->numrecs = payload->nrecs;
	//XXX following should only be in cmd_subscribe_by_ts
	if (payload->timeout != NULL)
	{
		timeout.tv_sec = payload->timeout->sec;
		timeout.tv_nsec = payload->timeout->nsec;
		timeout.tv_accuracy = payload->timeout->accuracy;
	}
	else
	{
		EP_TIME_INVALIDATE(&timeout);
	}

	if (ep_dbg_test(Dbg, 14))
	{
		ep_dbg_printf("cmd_subscribe: first = %" PRIgdp_recno ", numrecs = %d\n  ",
				payload->start, req->numrecs);
		_gdp_gob_dump(gob, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	if (req->numrecs < 0)
	{
		return gdpd_nak_resp(req, GDP_NAK_C_BADOPT,
							"cmd_subscribe: numrecs cannot be negative",
							GDP_STAT_NAK_BADOPT);
	}

	// get our starting point, which may be relative to the end
	req->nextrec = payload->start;
	if (req->nextrec <= 0)
	{
		req->nextrec += req->gob->nrecs + 1;
		if (req->nextrec <= 0)
		{
			// can't read before the beginning
			req->nextrec = 1;
		}
	}

	ep_dbg_cprintf(Dbg, 24,
			"cmd_subscribe: starting from %" PRIgdp_recno ", %d records\n",
			req->nextrec, req->numrecs);

	// see if this is refreshing an existing subscription
	{
		gdp_req_t *r1;

		if (ep_dbg_test(Dbg, 50))
		{
			ep_dbg_printf("cmd_subscribe_by_recno: starting ");
			_gdp_req_dump(req, NULL, GDP_PR_BASIC, 0);
		}
		for (r1 = LIST_FIRST(&gob->reqs); r1 != NULL;
				r1 = LIST_NEXT(r1, goblist))
		{
			EP_ASSERT(GDP_GOB_ISGOOD(r1->gob));
			EP_ASSERT(r1->gob == gob);
			if (ep_dbg_test(Dbg, 50))
			{
				ep_dbg_printf("cmd_subscribe: comparing to ");
				_gdp_req_dump(r1, NULL, GDP_PR_BASIC, 0);
			}
			if (GDP_NAME_SAME(r1->cpdu->src, req->cpdu->src) &&
					r1->cpdu->msg->rid == req->cpdu->msg->rid)
			{
				ep_dbg_cprintf(Dbg, 20, "cmd_subscribe: refreshing sub\n");
				break;
			}
		}
		if (r1 != NULL)
		{
			// abandon old request, we'll overwrite it with new request
			// (but keep the GOB around)
			ep_dbg_cprintf(Dbg, 20, "cmd_subscribe: removing old request\n");
			LIST_REMOVE(r1, goblist);
			r1->flags &= ~GDP_REQ_ON_GOB_LIST;
			_gdp_req_lock(r1);
			_gdp_req_free(&r1);
		}
	}

	// the _gdp_gob_decref better not have invalidated the GOB
	EP_ASSERT(GDP_GOB_ISGOOD(gob));

	// mark this as persistent and upgradable
	req->flags |= GDP_REQ_PERSIST | GDP_REQ_SUBUPGRADE;

	// note that the subscription is active
	ep_time_now(&req->act_ts);

	// if some of the records already exist, arrange to return them
	if (req->nextrec <= gob->nrecs)
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

		// link this request into the GOB so the subscription can be found
		if (!EP_UT_BITSET(GDP_REQ_ON_GOB_LIST, req->flags))
		{
			IF_LIST_CHECK_OK(&gob->reqs, req, goblist, gdp_req_t)
			{
				LIST_INSERT_HEAD(&gob->reqs, req, goblist);
				req->flags |= GDP_REQ_ON_GOB_LIST;
			}
			else
			{
				estat = EP_STAT_ASSERT_ABORT;
			}
		}
	}

	// we don't drop the GOB reference until the subscription is satisified

fail0:
	if (EP_STAT_ISOK(estat) && req->rpdu == NULL)
	{
		gdpd_ack_resp(req, GDP_ACK_SUCCESS);
		// ... if we want to fill in some info later....
		//GdpMessage__AckSuccess *resp = req->rpdu->msg->ack_success;
	}
	return estat;
}


/*
**  CMD_MULTIREAD --- read multiple records
**
**		Arranges to return existing data (if any) after the response
**		is sent.  No long-term subscription will ever be created, but
**		much of the infrastructure is reused.
*/

#if 0		//XXX NOT IMPLEMENTED YET
EP_STAT
cmd_multiread(gdp_req_t *req)
{
	EP_STAT estat;

	req->rpdu->cmd = GDP_ACK_SUCCESS;

	// find the GOB handle
	estat = get_open_handle(req);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_nak_resp(req, GDP_NAK_C_BADREQ,
							"cmd_multiread: GOB not open", estat);
	}

	// get the additional parameters: number of records and timeout
	req->numrecs = (int) gdp_buf_get_uint32(req->cpdu->datum->dbuf);

	if (ep_dbg_test(Dbg, 14))
	{
		ep_dbg_printf("cmd_multiread: first = %" PRIgdp_recno ", numrecs = %d\n  ",
				pbd->recno, req->numrecs);
		_gdp_gob_dump(req->gob, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	if (req->numrecs < 0)
	{
		return GDP_STAT_NAK_BADOPT;
	}

	// get our starting point, which may be relative to the end
	estat = get_starting_point_by_recno(req, payload->start);
	EP_STAT_CHECK(estat, goto fail0);

	ep_dbg_cprintf(Dbg, 24, "cmd_multiread: starting from %" PRIgdp_recno
			", %d records\n",
			req->nextrec, req->numrecs);

	// if some of the records already exist, arrange to return them
	if (req->nextrec <= req->gob->nrecs)
	{
		ep_dbg_cprintf(Dbg, 24, "cmd_multiread: doing post processing\n");
		req->postproc = &post_subscribe;

		// make this a "snapshot", i.e., don't read additional records
		int32_t nrec = req->gob->nrecs - req->nextrec;
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
#endif //XXX


/*
**  CMD_UNSUBSCRIBE --- terminate a subscription
*/

EP_STAT
cmd_unsubscribe(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;

	estat = get_open_handle(req);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_nak_resp(req, GDP_NAK_C_BADREQ,
							"cmd_unsubscribe: GOB not open", estat);
	}

	// remove any subscriptions
	sub_end_all_subscriptions(req->gob, req->cpdu->src, req->cpdu->msg->rid);

	// send the ack
	gdpd_ack_resp(req, GDP_ACK_SUCCESS);

	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];
		ep_dbg_printf("<<< cmd_unsubscribe(%s): %s\n", req->gob->pname,
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	return estat;
}


/*
**  CMD_GETMETADATA --- get metadata for a GOB
*/

EP_STAT
cmd_getmetadata(gdp_req_t *req)
{
	gdp_md_t *gmd;
	EP_STAT estat;

	estat = get_open_handle(req);
	if (!EP_STAT_ISOK(estat))
	{
		estat = gdpd_nak_resp(req, GDP_NAK_S_INTERNAL,
							"cmd_getmetadata: GOB open failure", estat);
		goto fail0;
	}

	// get the metadata into memory
	estat = req->gob->x->physimpl->getmetadata(req->gob, &gmd);
	EP_STAT_CHECK(estat, goto fail1);

	// serialize it to the client
	if (EP_STAT_ISOK(estat))
	{
		uint8_t *mdbuf;
		size_t mdlen = _gdp_md_serialize(gmd, &mdbuf);
		gdpd_ack_resp(req, GDP_ACK_SUCCESS);
		GdpMessage__AckSuccess *resp = req->rpdu->msg->ack_success;
		resp->metadata.data = mdbuf;
		resp->metadata.len = mdlen;
		resp->has_metadata = true;
	}
	else
	{
		gdpd_nak_resp(req, GDP_NAK_S_INTERNAL,
					"cannot get log metadata", estat);
	}

fail1:
fail0:
	return estat;
}


/*
**  CMD_FWD_APPEND --- forwarded APPEND command
**
**		Used for replication.  This is identical to an APPEND,
**		except it is addressed to an individual gdplogd rather
**		than to a GOB.  The actual name is in the payload.
**		On return, the message will have a source address of the
**		GOB, not the gdplogd instance (i.e., we don't just do
**		the default swap of src and dst addresses).
*/

#if 0	//XXX Move to Layer 4?
EP_STAT
cmd_fwd_append(gdp_req_t *req)
{
	EP_STAT estat;
	gdp_name_t gobname;

	// must be addressed to me
	if (!GDP_NAME_SAME(req->cpdu->dst, _GdpMyRoutingName))
	{
		// this is directed to a GOB, not to the daemon
		return gdpd_nak_resp(req, GDP_NAK_C_BADREQ,
							"cmd_create: log name required",
							GDP_STAT_NAK_CONFLICT);
	}

	// get the name of the GOB into current PDU
	{
		int i;
		gdp_pname_t pbuf;

		i = gdp_buf_read(req->cpdu->datum->dbuf, gobname, sizeof gobname);
		if (i < sizeof req->cpdu->dst)
		{
			return gdpd_nak_resp(req, GDP_NAK_S_INTERNAL,
								"cmd_fwd_append: gobname required",
								GDP_STAT_GDP_NAME_INVALID);
		}
		memcpy(req->cpdu->dst, gobname, sizeof req->cpdu->dst);

		ep_dbg_cprintf(Dbg, 14, "cmd_fwd_append: %s\n",
				gdp_printable_name(req->cpdu->dst, pbuf));
	}

	// actually do the append
	estat = cmd_append(req);

	// make response seem to come from log
	memcpy(req->rpdu->src, gobname, sizeof req->rpdu->src);

	return estat;
}
#endif	//XXX


/**************** END OF COMMAND IMPLEMENTATIONS ****************/



/*
**  GDPD_PROTO_INIT --- initialize protocol module
*/

static struct cmdfuncs	CmdFuncs[] =
{
	{ GDP_CMD_PING,					cmd_ping				},
	{ GDP_CMD_CREATE,				cmd_create				},
	{ GDP_CMD_OPEN_AO,				cmd_open				},
	{ GDP_CMD_OPEN_RO,				cmd_open				},
	{ GDP_CMD_OPEN_RA,				cmd_open				},
	{ GDP_CMD_CLOSE,				cmd_close				},
	{ GDP_CMD_APPEND,				cmd_append				},
	{ GDP_CMD_READ_BY_RECNO,		cmd_read_by_recno		},
	{ GDP_CMD_READ_BY_TS,			cmd_read_by_ts			},
//	{ GDP_CMD_READ_BY_HASH	,		cmd_read_by_hash		},
	{ GDP_CMD_SUBSCRIBE_BY_RECNO,	cmd_subscribe_by_recno	},
//	{ GDP_CMD_SUBSCRIBE_BY_TS,		cmd_subscribe_by_ts		},
//	{ GDP_CMD_SUBSCRIBE_BY_HASH,	cmd_subscribe_by_hash	},
	{ GDP_CMD_GETMETADATA,			cmd_getmetadata			},
//	{ GDP_CMD_NEWSEGMENT,			cmd_newsegment			},
//	{ GDP_CMD_FWD_APPEND,			cmd_fwd_append			},
	{ GDP_CMD_UNSUBSCRIBE,			cmd_unsubscribe			},
	{ GDP_CMD_DELETE,				cmd_delete				},
	{ 0,							NULL					}
};

EP_STAT
gdpd_proto_init(void)
{
	// register the commands we implement
	_gdp_register_cmdfuncs(CmdFuncs);
	return EP_STAT_OK;
}
