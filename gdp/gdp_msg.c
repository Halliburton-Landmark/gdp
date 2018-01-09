/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP_MSG.C --- manipulate protobuf messages
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
#include <ep/ep_log.h>
#include <ep/ep_prflags.h>
#include <ep/ep_stat.h>

#include "gdp.h"
#include "gdp_chan.h"
#include "gdp_priv.h"

#include <event2/event.h>

#include <string.h>
#include <sys/errno.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.msg", "GDP message manipulation");

/*
**  Create a new GDP message.
**		The gdp_msg_t type is really a synonym for GdpMessage.
**		In OO terms, this is a Factory.
*/

gdp_msg_t *
_gdp_msg_new(gdp_cmd_t cmd, gdp_rid_t rid, gdp_seqno_t seqno)
{
	gdp_msg_t *msg;
	GdpBody *body;

	ep_dbg_cprintf(Dbg, 24,
				"_gdp_msg_new: cmd %s (%d), rid %" PRIgdp_rid
							" seqno %" PRIgdp_seqno "\n",
				_gdp_proto_cmd_name(cmd), cmd, rid, seqno);

	EP_ASSERT(cmd >= 0 && cmd <= 255);
	msg = ep_mem_zalloc(sizeof *msg);
	gdp_message__init(msg);
	msg->cmd = cmd;
	if (rid != GDP_PDU_NO_RID)
	{
		msg->has_rid = true;
		msg->rid = rid;
	}
	if (seqno != GDP_PDU_NO_SEQNO)
	{
		msg->has_seqno = true;
		msg->seqno = seqno;
	}
	msg->body = body = ep_mem_zalloc(sizeof *msg->body);
	gdp_body__init(body);
	msg->trailer = ep_mem_zalloc(sizeof *msg->trailer);
	gdp_trailer__init(msg->trailer);

	// initialize command body based on command type
	switch (cmd)
	{
	case GDP_CMD_CREATE:
		body->command_body_case = GDP_BODY__COMMAND_BODY_CMD_CREATE;
		body->cmd_create = ep_mem_zalloc(sizeof *body->cmd_create);
		gdp_body__command_create__init(body->cmd_create);
		break;

	case GDP_CMD_OPEN_AO:
	case GDP_CMD_OPEN_RO:
	case GDP_CMD_OPEN_RA:
		body->command_body_case = GDP_BODY__COMMAND_BODY_CMD_OPEN;
		body->cmd_open = ep_mem_zalloc(sizeof *body->cmd_open);
		gdp_body__command_open__init(body->cmd_open);
		break;

	case GDP_CMD_APPEND:
		body->command_body_case = GDP_BODY__COMMAND_BODY_CMD_APPEND;
		body->cmd_append = ep_mem_zalloc(sizeof *body->cmd_append);
		gdp_body__command_append__init(body->cmd_append);
		break;

	case GDP_CMD_READ_BY_RECNO:
		body->command_body_case =
						GDP_BODY__COMMAND_BODY_CMD_READ_BY_RECNO;
		body->cmd_read_by_recno = ep_mem_zalloc(sizeof *body->cmd_read_by_recno);
		gdp_body__command_read_by_recno__init(body->cmd_read_by_recno);
		break;

	case GDP_CMD_READ_BY_TS:
		body->command_body_case =
						GDP_BODY__COMMAND_BODY_CMD_READ_BY_TS;
		body->cmd_read_by_ts = ep_mem_zalloc(sizeof *body->cmd_read_by_ts);
		gdp_body__command_read_by_ts__init(body->cmd_read_by_ts);
		break;

	case GDP_CMD_READ_BY_HASH:
		body->command_body_case =
						GDP_BODY__COMMAND_BODY_CMD_READ_BY_HASH;
		body->cmd_read_by_hash = ep_mem_zalloc(sizeof *body->cmd_read_by_hash);
		gdp_body__command_read_by_hash__init(body->cmd_read_by_hash);
		break;

	case GDP_CMD_SUBSCRIBE_BY_RECNO:
		body->command_body_case =
						GDP_BODY__COMMAND_BODY_CMD_SUBSCRIBE_BY_RECNO;
		body->cmd_subscribe_by_recno =
						ep_mem_zalloc(sizeof *body->cmd_subscribe_by_recno);
		gdp_body__command_subscribe_by_recno__init(
						body->cmd_subscribe_by_recno);
		break;

	case GDP_CMD_SUBSCRIBE_BY_TS:
		body->command_body_case =
						GDP_BODY__COMMAND_BODY_CMD_SUBSCRIBE_BY_TS;
		body->cmd_subscribe_by_ts =
						ep_mem_zalloc(sizeof *body->cmd_subscribe_by_ts);
		gdp_body__command_subscribe_by_ts__init(
						body->cmd_subscribe_by_ts);
		break;

	case GDP_CMD_SUBSCRIBE_BY_HASH:
		body->command_body_case =
						GDP_BODY__COMMAND_BODY_CMD_SUBSCRIBE_BY_HASH;
		body->cmd_subscribe_by_hash =
						ep_mem_zalloc(sizeof *body->cmd_subscribe_by_hash);
		gdp_body__command_subscribe_by_hash__init(
						body->cmd_subscribe_by_hash);
		break;

	case GDP_ACK_SUCCESS:
		body->command_body_case = GDP_BODY__COMMAND_BODY_ACK_SUCCESS;
		body->ack_success = ep_mem_zalloc(sizeof *body->ack_success);
		gdp_body__ack_success__init(body->ack_success);
		break;

	case GDP_ACK_CHANGED:
		body->command_body_case = GDP_BODY__COMMAND_BODY_ACK_CHANGED;
		body->ack_changed = ep_mem_zalloc(sizeof *body->ack_changed);
		gdp_body__ack_changed__init(body->ack_changed);
		break;

	case GDP_ACK_CONTENT:
		body->command_body_case = GDP_BODY__COMMAND_BODY_ACK_CONTENT;
		body->ack_content = ep_mem_zalloc(sizeof *body->ack_content);
		gdp_body__ack_content__init(body->ack_content);
		body->ack_content->datum = ep_mem_zalloc(sizeof *body->ack_content->datum);
		gdp_datum__init(body->ack_content->datum);
		break;

	default:
		if (cmd >= GDP_NAK_C_MIN && cmd <= GDP_NAK_S_MAX)
		{
			body->command_body_case = GDP_BODY__COMMAND_BODY_NAK;
			body->nak = ep_mem_zalloc(sizeof *body->nak);
			gdp_body__nak_generic__init(body->nak);
			break;
		}
		body->command_body_case = GDP_BODY__COMMAND_BODY__NOT_SET;
		break;
	}

	return msg;
}


void
_gdp_msg_free(gdp_msg_t **pmsg)
{
	ep_dbg_cprintf(Dbg, 24, "_gdp_msg_free(%p)\n", *pmsg);
	gdp_message__free_unpacked(*pmsg, NULL);
	*pmsg = NULL;
}


static void
print_pb_ts(const GdpTimestamp *ts, FILE *fp)
{
	if (ts == NULL)
	{
		fprintf(fp, "(none)");
		return;
	}
	fprintf(fp, "%lld", ts->sec);
}


static void
print_pb_datum(const GdpDatum *d, FILE *fp)
{
	int indent = 12;
	fprintf(fp, "datum@%p\n", d);
	fprintf(fp, "%*srecno %" PRIgdp_recno "\n",
			indent, "", d->recno);
	if (d->ts != NULL)
	{
		fprintf(fp, "%*sts ", indent, "");
		print_pb_ts(d->ts, fp);
		fprintf(fp, "\n");
	}
	if (d->sig != NULL)
		fprintf(fp, "%*ssig (someday)\n", indent, "");		//XXX
	if (d->hash != NULL)
		fprintf(fp, "%*shash (someday)\n", indent, "");		//XXX
	ep_hexdump(d->data.data, d->data.len, fp, EP_HEXDUMP_ASCII, 0);
}


void
_gdp_msg_dump(const gdp_msg_t *msg, FILE *fp)
{
	gdp_pname_t pname;
	char ebuf[100];

	if (fp == NULL)
		fp = ep_dbg_getfile();
	flockfile(fp);
	fprintf(fp, "Msg@%p: ", msg);
	if (msg == NULL)
	{
		fprintf(fp, "NULL\n");
		goto done;
	}
	fprintf(fp, "cmd=%d=%s, rid=",
			msg->cmd, _gdp_proto_cmd_name(msg->cmd));
	if (msg->rid == GDP_PDU_NO_RID)
		fprintf(fp, "(none)");
	else if (msg->rid == GDP_PDU_ANY_RID)
		fprintf(fp, "(any)");
	else
		fprintf(fp, "%" PRIgdp_rid, msg->rid);
	fprintf(fp, ", seqno=");
	if (msg->seqno == GDP_PDU_NO_SEQNO)
		fprintf(fp, "(none)");
	else
		fprintf(fp, "%" PRIgdp_seqno, msg->seqno);

	GdpBody *body = msg->body;
	fprintf(fp, "\n\tbody=");
	if (body == NULL)
	{
		fprintf(fp, "(none)\n");
		goto done;
	}

	switch (body->command_body_case)
	{
	case GDP_BODY__COMMAND_BODY__NOT_SET:
		fprintf(fp, "(not set)\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_CREATE:
		fprintf(fp, "cmd_create:\n\tlogname=%s\n\tmetadata=",
				gdp_printable_name(body->cmd_create->logname.data, pname));
		if (!body->cmd_create->has_metadata)
			fprintf(fp, "(none)\n");
		else
		{
			fprintf(fp, "\n");
			ep_hexdump(body->cmd_create->metadata.data,
						body->cmd_create->metadata.len,
						fp, EP_HEXDUMP_ASCII, 0);
		}
		break;

	case GDP_BODY__COMMAND_BODY_CMD_OPEN:
		fprintf(fp, "cmd_open\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_CLOSE:
		fprintf(fp, "cmd_close\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_APPEND:
		fprintf(fp, "cmd_append: ");
		print_pb_datum(body->cmd_append->datum, fp);
		break;

	case GDP_BODY__COMMAND_BODY_CMD_READ_BY_RECNO:
		fprintf(fp, "cmd_read_by_recno: recno=%" PRIgdp_recno,
				body->cmd_read_by_recno->recno);
		if (body->cmd_read_by_recno->has_nrecs)
			fprintf(fp, ", nrecs=%d", body->cmd_read_by_recno->nrecs);
		fprintf(fp, "\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_READ_BY_TS:
		fprintf(fp, "cmd_read_by_ts: ");
		print_pb_ts(body->cmd_read_by_ts->timestamp, fp);
		if (body->cmd_read_by_ts->has_nrecs)
			fprintf(fp, ", nrecs=%d", body->cmd_read_by_ts->nrecs);
		fprintf(fp, "\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_READ_BY_HASH:
		fprintf(fp, "cmd_read_by_hash: UNIMPLEMENTED\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_SUBSCRIBE_BY_RECNO:
		fprintf(fp, "cmd_subscribe_by_recno: UNIMPLEMENTED\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_SUBSCRIBE_BY_TS:
		fprintf(fp, "cmd_subscribe_by_hash: UNIMPLEMENTED\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_SUBSCRIBE_BY_HASH:
		fprintf(fp, "cmd_subscribe_by_hash: UNIMPLEMENTED\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_UNSUBSCRIBE:
		fprintf(fp, "cmd_unsubscribe: (no payload)\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_GET_METADATA:
		fprintf(fp, "cmd_get_metadata: (no payload)\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_NEW_SEGMENT:
		fprintf(fp, "cmd_new_segment: (no payload)\n");
		break;

	case GDP_BODY__COMMAND_BODY_CMD_DELETE:
		fprintf(fp, "cmd_delete: (no payload)\n");
		break;

	case GDP_BODY__COMMAND_BODY_ACK_SUCCESS:
		fprintf(fp, "ack_success\n");
		if (body->ack_success->has_recno)
			fprintf(fp, "\t    recno=%" PRIgdp_recno "\n",
						body->ack_success->recno);
		if (body->ack_success->ts != NULL)
		{
			fprintf(fp, "\t    ts=");
			print_pb_ts(body->ack_success->ts, fp);
			fprintf(fp, "\n");
		}
		if (body->ack_success->has_hash)
		{
			fprintf(fp, "\t    hash=\n");
			ep_hexdump(body->ack_success->hash.data,
						body->ack_success->hash.len,
						fp, EP_HEXDUMP_HEX, 0);
		}
		if (body->ack_success->has_metadata)
		{
			fprintf(fp, "\t    metadata=\n");
			ep_hexdump(body->ack_success->metadata.data,
						body->ack_success->metadata.len,
						fp, EP_HEXDUMP_ASCII, 0);
		}
		break;

	case GDP_BODY__COMMAND_BODY_ACK_CHANGED:
		fprintf(fp, "ack_changed\n");
		break;

	case GDP_BODY__COMMAND_BODY_ACK_CONTENT:
		fprintf(fp, "ack_content: ");
		print_pb_datum(body->ack_content->datum, fp);
		break;

	case GDP_BODY__COMMAND_BODY_NAK:
		fprintf(fp, "nak:\n");
		if (body->nak->has_ep_stat)
			fprintf(fp, "\t    ep_stat=%s\n",
					ep_stat_tostr(EP_STAT_FROM_INT(body->nak->ep_stat),
								ebuf, sizeof ebuf));
		if (body->nak->description != NULL)
			fprintf(fp, "\t    detail=%s\n", body->nak->description);
		if (body->nak->has_recno)
			fprintf(fp, "\t    recno=%" PRIgdp_recno "\n",
					body->nak->recno);
		break;

	case GDP_BODY__COMMAND_BODY_NAK_CONFLICT:
		fprintf(fp, "nak_conflict\n");
		if (body->nak_conflict->has_ep_stat)
			fprintf(fp, "\t    ep_stat=%s\n",
					ep_stat_tostr(EP_STAT_FROM_INT(body->nak_conflict->ep_stat),
								ebuf, sizeof ebuf));
		if (body->nak_conflict->description != NULL)
			fprintf(fp, "\t    detail=%s\n", body->nak_conflict->description);
		if (body->nak_conflict->has_recno)
			fprintf(fp, "\t    recno=%" PRIgdp_recno "\n",
					body->nak_conflict->recno);
		break;

	default:
		fprintf(fp, "unknown body case %d\n", body->command_body_case);
		break;
	}

	fprintf(fp, "\ttrailer=");
	if (msg->trailer == NULL)
	{
		fprintf(fp, "(none)\n");
		goto done;
	}
	else
	{
		fprintf(fp, "sig=%p", msg->trailer->sig);
		if (msg->trailer->sig == NULL)
			fprintf(fp, " (no signature)\n");
		else
			fprintf(fp, "\n\t    sig_type=0x%x, sig.len=%zd, sig.data=%p\n",
						msg->trailer->sig->sig_type,
						msg->trailer->sig->sig.len,
						msg->trailer->sig->sig.data);
	}
done:
	funlockfile(fp);
}
