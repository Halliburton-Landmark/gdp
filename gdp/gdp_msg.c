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

	// initialize command body based on command type
	switch (cmd)
	{
	case GDP_CMD_CREATE:
		msg->body_case = GDP_MESSAGE__BODY_CMD_CREATE;
		msg->cmd_create = ep_mem_zalloc(sizeof *msg->cmd_create);
		gdp_message__cmd_create__init(msg->cmd_create);
		break;

	case GDP_CMD_OPEN_AO:
	case GDP_CMD_OPEN_RO:
	case GDP_CMD_OPEN_RA:
		msg->body_case = GDP_MESSAGE__BODY_CMD_OPEN;
		msg->cmd_open = ep_mem_zalloc(sizeof *msg->cmd_open);
		gdp_message__cmd_open__init(msg->cmd_open);
		break;

	case GDP_CMD_APPEND:
		msg->body_case = GDP_MESSAGE__BODY_CMD_APPEND;
		msg->cmd_append = ep_mem_zalloc(sizeof *msg->cmd_append);
		gdp_message__cmd_append__init(msg->cmd_append);
		break;

	case GDP_CMD_READ_BY_RECNO:
		msg->body_case =
						GDP_MESSAGE__BODY_CMD_READ_BY_RECNO;
		msg->cmd_read_by_recno = ep_mem_zalloc(sizeof *msg->cmd_read_by_recno);
		gdp_message__cmd_read_by_recno__init(msg->cmd_read_by_recno);
		break;

	case GDP_CMD_READ_BY_TS:
		msg->body_case =
						GDP_MESSAGE__BODY_CMD_READ_BY_TS;
		msg->cmd_read_by_ts = ep_mem_zalloc(sizeof *msg->cmd_read_by_ts);
		gdp_message__cmd_read_by_ts__init(msg->cmd_read_by_ts);
		break;

	case GDP_CMD_READ_BY_HASH:
		msg->body_case =
						GDP_MESSAGE__BODY_CMD_READ_BY_HASH;
		msg->cmd_read_by_hash = ep_mem_zalloc(sizeof *msg->cmd_read_by_hash);
		gdp_message__cmd_read_by_hash__init(msg->cmd_read_by_hash);
		break;

	case GDP_CMD_SUBSCRIBE_BY_RECNO:
		msg->body_case =
						GDP_MESSAGE__BODY_CMD_SUBSCRIBE_BY_RECNO;
		msg->cmd_subscribe_by_recno =
						ep_mem_zalloc(sizeof *msg->cmd_subscribe_by_recno);
		gdp_message__cmd_subscribe_by_recno__init(
						msg->cmd_subscribe_by_recno);
		break;

	case GDP_CMD_SUBSCRIBE_BY_TS:
		msg->body_case =
						GDP_MESSAGE__BODY_CMD_SUBSCRIBE_BY_TS;
		msg->cmd_subscribe_by_ts =
						ep_mem_zalloc(sizeof *msg->cmd_subscribe_by_ts);
		gdp_message__cmd_subscribe_by_ts__init(
						msg->cmd_subscribe_by_ts);
		break;

	case GDP_CMD_SUBSCRIBE_BY_HASH:
		msg->body_case =
						GDP_MESSAGE__BODY_CMD_SUBSCRIBE_BY_HASH;
		msg->cmd_subscribe_by_hash =
						ep_mem_zalloc(sizeof *msg->cmd_subscribe_by_hash);
		gdp_message__cmd_subscribe_by_hash__init(
						msg->cmd_subscribe_by_hash);
		break;

	case GDP_ACK_SUCCESS:
		msg->body_case = GDP_MESSAGE__BODY_ACK_SUCCESS;
		msg->ack_success = ep_mem_zalloc(sizeof *msg->ack_success);
		gdp_message__ack_success__init(msg->ack_success);
		break;

	case GDP_ACK_CHANGED:
		msg->body_case = GDP_MESSAGE__BODY_ACK_CHANGED;
		msg->ack_changed = ep_mem_zalloc(sizeof *msg->ack_changed);
		gdp_message__ack_changed__init(msg->ack_changed);
		break;

	case GDP_ACK_CONTENT:
		msg->body_case = GDP_MESSAGE__BODY_ACK_CONTENT;
		msg->ack_content = ep_mem_zalloc(sizeof *msg->ack_content);
		gdp_message__ack_content__init(msg->ack_content);
		msg->ack_content->datum = ep_mem_zalloc(sizeof *msg->ack_content->datum);
		gdp_datum__init(msg->ack_content->datum);
		break;

	default:
		if (cmd >= GDP_NAK_C_MIN && cmd <= GDP_NAK_S_MAX)
		{
			msg->body_case = GDP_MESSAGE__BODY_NAK;
			msg->nak = ep_mem_zalloc(sizeof *msg->nak);
			gdp_message__nak_generic__init(msg->nak);
			break;
		}
		msg->body_case = GDP_MESSAGE__BODY__NOT_SET;
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
		fprintf(fp, "(none)");
	else if (ts->sec == EP_TIME_NOTIME)
		fprintf(fp, "(notime)");
	else
		fprintf(fp, "%" PRIu64, ts->sec);
}


static void
print_pb_datum(const GdpDatum *d, FILE *fp, int indent)
{
	fprintf(fp, "datum@%p\n", d);
	fprintf(fp, "%srecno %" PRIgdp_recno ", ts ",
			_gdp_pr_indent(indent), d->recno);
	print_pb_ts(d->ts, fp);
	fprintf(fp, " data[%zd]=\n", d->data.len);
	ep_hexdump(d->data.data, d->data.len, fp, EP_HEXDUMP_ASCII, 0);
}


void
_gdp_msg_dump(const gdp_msg_t *msg, FILE *fp, int indent)
{
	gdp_pname_t pname;
	char ebuf[100];

	if (fp == NULL)
		fp = ep_dbg_getfile();
	flockfile(fp);
	fprintf(fp, "msg@%p: ", msg);
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

	fprintf(fp, "\n%sbody=", _gdp_pr_indent(indent));
	switch (msg->body_case)
	{
	case GDP_MESSAGE__BODY__NOT_SET:
		fprintf(fp, "(not set)\n");
		break;

	case GDP_MESSAGE__BODY_CMD_CREATE:
		fprintf(fp, "cmd_create:\n%slogname=%s\n%smetadata=",
				_gdp_pr_indent(indent),
				gdp_printable_name(msg->cmd_create->logname.data, pname),
				_gdp_pr_indent(indent + 1));
		if (!msg->cmd_create->has_metadata)
			fprintf(fp, "(none)\n");
		else
		{
			fprintf(fp, "\n");
			ep_hexdump(msg->cmd_create->metadata.data,
						msg->cmd_create->metadata.len,
						fp, EP_HEXDUMP_ASCII, 0);
		}
		break;

	case GDP_MESSAGE__BODY_CMD_OPEN:
		fprintf(fp, "cmd_open\n");
		break;

	case GDP_MESSAGE__BODY_CMD_CLOSE:
		fprintf(fp, "cmd_close\n");
		break;

	case GDP_MESSAGE__BODY_CMD_APPEND:
		fprintf(fp, "cmd_append: ");
		print_pb_datum(msg->cmd_append->datum, fp, indent + 1);
		break;

	case GDP_MESSAGE__BODY_CMD_READ_BY_RECNO:
		fprintf(fp, "cmd_read_by_recno: recno=%" PRIgdp_recno,
				msg->cmd_read_by_recno->recno);
		if (msg->cmd_read_by_recno->has_nrecs)
			fprintf(fp, ", nrecs=%d", msg->cmd_read_by_recno->nrecs);
		fprintf(fp, "\n");
		break;

	case GDP_MESSAGE__BODY_CMD_READ_BY_TS:
		fprintf(fp, "cmd_read_by_ts: ");
		print_pb_ts(msg->cmd_read_by_ts->timestamp, fp);
		if (msg->cmd_read_by_ts->has_nrecs)
			fprintf(fp, ", nrecs=%d", msg->cmd_read_by_ts->nrecs);
		fprintf(fp, "\n");
		break;

	case GDP_MESSAGE__BODY_CMD_READ_BY_HASH:
		fprintf(fp, "cmd_read_by_hash: UNIMPLEMENTED\n");
		break;

	case GDP_MESSAGE__BODY_CMD_SUBSCRIBE_BY_RECNO:
		fprintf(fp, "cmd_subscribe_by_recno: UNIMPLEMENTED\n");
		break;

	case GDP_MESSAGE__BODY_CMD_SUBSCRIBE_BY_TS:
		fprintf(fp, "cmd_subscribe_by_hash: UNIMPLEMENTED\n");
		break;

	case GDP_MESSAGE__BODY_CMD_SUBSCRIBE_BY_HASH:
		fprintf(fp, "cmd_subscribe_by_hash: UNIMPLEMENTED\n");
		break;

	case GDP_MESSAGE__BODY_CMD_UNSUBSCRIBE:
		fprintf(fp, "cmd_unsubscribe: (no payload)\n");
		break;

	case GDP_MESSAGE__BODY_CMD_GET_METADATA:
		fprintf(fp, "cmd_get_metadata: (no payload)\n");
		break;

	case GDP_MESSAGE__BODY_CMD_NEW_SEGMENT:
		fprintf(fp, "cmd_new_segment: (no payload)\n");
		break;

	case GDP_MESSAGE__BODY_CMD_DELETE:
		fprintf(fp, "cmd_delete: (no payload)\n");
		break;

	case GDP_MESSAGE__BODY_ACK_SUCCESS:
		fprintf(fp, "ack_success");
		if (msg->ack_success->has_recno)
			fprintf(fp, ", recno=%" PRIgdp_recno "\n",
						msg->ack_success->recno);
		fprintf(fp, ", ts=");
		print_pb_ts(msg->ack_success->ts, fp);
		fprintf(fp, "\n");
		if (msg->ack_success->has_hash)
		{
			fprintf(fp, "%shash=", _gdp_pr_indent(indent + 1));
			ep_hexdump(msg->ack_success->hash.data,
						msg->ack_success->hash.len,
						fp, EP_HEXDUMP_TERSE, 0);
		}
		if (msg->ack_success->has_metadata)
		{
			fprintf(fp, "%smetadata=\n", _gdp_pr_indent(indent + 1));
			ep_hexdump(msg->ack_success->metadata.data,
						msg->ack_success->metadata.len,
						fp, EP_HEXDUMP_ASCII, 0);
		}
		break;

	case GDP_MESSAGE__BODY_ACK_CHANGED:
		fprintf(fp, "ack_changed\n");
		break;

	case GDP_MESSAGE__BODY_ACK_CONTENT:
		fprintf(fp, "ack_content: ");
		print_pb_datum(msg->ack_content->datum, fp, indent + 1);
		break;

	case GDP_MESSAGE__BODY_NAK:
		fprintf(fp, "nak:\n");
		if (msg->nak->has_ep_stat)
			fprintf(fp, "%sep_stat=%s\n",
					_gdp_pr_indent(indent),
					ep_stat_tostr(EP_STAT_FROM_INT(msg->nak->ep_stat),
								ebuf, sizeof ebuf));
		if (msg->nak->description != NULL)
			fprintf(fp, "%sdetail=%s\n",
					_gdp_pr_indent(indent),
					msg->nak->description);
		if (msg->nak->has_recno)
			fprintf(fp, "%srecno=%" PRIgdp_recno "\n",
					_gdp_pr_indent(indent),
					msg->nak->recno);
		break;

	case GDP_MESSAGE__BODY_NAK_CONFLICT:
		fprintf(fp, "nak_conflict\n");
		if (msg->nak_conflict->has_ep_stat)
			fprintf(fp, "%sep_stat=%s\n",
					_gdp_pr_indent(indent),
					ep_stat_tostr(EP_STAT_FROM_INT(msg->nak_conflict->ep_stat),
								ebuf, sizeof ebuf));
		if (msg->nak_conflict->description != NULL)
			fprintf(fp, "%sdetail=%s\n",
					_gdp_pr_indent(indent),
					msg->nak_conflict->description);
		if (msg->nak_conflict->has_recno)
			fprintf(fp, "%srecno=%" PRIgdp_recno "\n",
					_gdp_pr_indent(indent),
					msg->nak_conflict->recno);
		break;

	default:
		fprintf(fp, "unknown body case %d\n", msg->body_case);
		break;
	}

	fprintf(fp, "%ssig=%p", _gdp_pr_indent(indent), msg->sig);
	if (msg->sig == NULL)
		fprintf(fp, "(none)\n");
	else
		fprintf(fp, "\n%ssig_type=0x%x, sig.len=%zd, sig.data=%p\n",
					_gdp_pr_indent(indent + 1),
					msg->sig->sig_type,
					msg->sig->sig.len,
					msg->sig->sig.data);
	if (msg->has_hash)
	{
		fprintf(fp, "%shash[%zd]=", _gdp_pr_indent(indent), msg->hash.len);
		ep_hexdump(msg->hash.data, msg->hash.len, fp, EP_HEXDUMP_TERSE, 0);
	}
	else
	{
		fprintf(fp, "%shash=(none)\n", _gdp_pr_indent(indent));
	}
done:
	funlockfile(fp);
}