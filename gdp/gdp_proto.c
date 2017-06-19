/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	This implements the GDP Protocol.
**
**	In the future this may need to be extended to have knowledge of
**	TSN/AVB, but for now we don't worry about that.
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
#include "gdp_chan.h"
#include "gdp_event.h"
#include "gdp_priv.h"

#include <ep/ep_dbg.h>
#include <ep/ep_log.h>

#include <string.h>
#include <sys/errno.h>


static EP_DBG	Dbg = EP_DBG_INIT("gdp.proto", "GDP protocol processing");
static EP_DBG	DbgCmdTrace = EP_DBG_INIT("gdp.proto.command.trace",
							"GDP command execution tracing");



/*
**	GDP_INVOKE --- do a remote invocation to the GDP daemon
**
**		This acts as an remote procedure call.  In particular it
**		waits to get a result, which makes it inappropriate for
**		instances where multiple commands are in flight, or
**		where a command can return multiple values (e.g., subscribe).
**
**		The req must be locked before this is called.
*/

EP_STAT
_gdp_invoke(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;
	EP_TIME_SPEC abs_to;
	long delta_to;				// how long to wait for a response
	int retries;				// how many times to retry
	long retry_delay;			// how long to delay between retries
	EP_TIME_SPEC delta_ts;
	const char *cmdname;

	EP_ASSERT_POINTER_VALID(req);
	if (req->gob != NULL)
		GDP_GOB_ASSERT_ISLOCKED(req->gob);
	cmdname = _gdp_proto_cmd_name(req->cpdu->cmd);
	if (ep_dbg_test(Dbg, 10))
	{
		ep_dbg_printf("\n>>> _gdp_invoke(req=%p rid=%" PRIgdp_rid "): %s (%d), gob@%p\n",
				req,
				req->cpdu->rid,
				cmdname,
				req->cpdu->cmd,
				req->gob);
		if (ep_dbg_test(Dbg, 11))
		{
			ep_dbg_printf("\t");
			_gdp_datum_dump(req->cpdu->datum, ep_dbg_getfile());
		}
	}
	EP_ASSERT_ELSE(req->state == GDP_REQ_ACTIVE, return EP_STAT_ASSERT_ABORT);
	//EP_ASSERT(ep_thr_mutex_islocked(&req->mutex));

	// scale timeout to milliseconds
	delta_to = ep_adm_getlongparam("swarm.gdp.invoke.timeout", 10000L);
	ep_time_from_nsec(delta_to * INT64_C(1000000), &delta_ts);
	retry_delay = ep_adm_getlongparam("swarm.gdp.invoke.retrydelay", 5000L);

	// loop to allow for retransmissions
	retries = ep_adm_getintparam("swarm.gdp.invoke.retries", 3);
	if (retries < 1)
		retries = 1;
	do
	{
		/*
		**  Top Half: sending the command
		*/

		ep_dbg_cprintf(Dbg, 36,
				"_gdp_invoke: sending %d, retries=%d\n",
				req->cpdu->cmd, retries);

		estat = _gdp_req_send(req);
		EP_STAT_CHECK(estat, continue);

		/*
		**  Bottom Half: read the response
		*/

		// wait until we receive a result
		ep_dbg_cprintf(Dbg, 37, "_gdp_invoke: waiting on %p\n", req);
		ep_time_deltanow(&delta_ts, &abs_to);
		estat = EP_STAT_OK;
		req->state = GDP_REQ_WAITING;
		req->flags &= ~GDP_REQ_ASYNCIO;
		while (!EP_UT_BITSET(GDP_REQ_DONE, req->flags))
		{
			// release the GOB while we're waiting
			if (req->gob != NULL)
				_gdp_gob_unlock(req->gob);

			// cond_wait will unlock the mutex
			int e = ep_thr_cond_wait(&req->cond, &req->mutex, &abs_to);

			// re-acquire GOB lock
			if (req->gob != NULL)
			{
				// have to unlock the req so lock ordering is right
				//XXX possible race condition?
				_gdp_req_unlock(req);
				_gdp_gob_lock(req->gob);
				_gdp_req_lock(req);
			}

			char ebuf[100];
			ep_dbg_cprintf(Dbg, 52,
					"_gdp_invoke wait: got %d, done=%d, state=%d,\n"
					"    stat=%s\n",
					e, EP_UT_BITSET(GDP_REQ_DONE, req->flags), req->state,
					ep_stat_tostr(req->stat, ebuf, sizeof ebuf));
			if (e != 0)
			{
				estat = ep_stat_from_errno(e);
				if (e != ETIMEDOUT)			// retry on timeouts
					retries = 0;
				break;
			}
		}

		if (ep_dbg_test(Dbg, 46))
		{
			char e1buf[100], e2buf[100];
			ep_dbg_printf(
					"_gdp_invoke: after cond_wait, estat %s, req->stat %s\n",
					ep_stat_tostr(estat, e1buf, sizeof e1buf),
					ep_stat_tostr(req->stat, e2buf, sizeof e2buf));
		}
		req->state = GDP_REQ_ACTIVE;
		if (EP_STAT_ISOK(estat))
		{
			estat = req->stat;

			// if we succeeded or it's our fault, don't try again
			if (EP_STAT_ISOK(req->stat) || GDP_STAT_IS_C_NAK(req->stat) ||
					GDP_STAT_IS_S_NAK(req->stat))
			{
				break;				// we're done, don't retry
			}
			else if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOROUTE) &&
					EP_UT_BITSET(GDP_REQ_ROUTEFAIL, req->flags))
			{
				// route failure on open: don't retry
				break;
			}
		}

		// do a retry, after re-locking the GOB
		estat = _gdp_req_unsend(req);
		EP_STAT_CHECK(estat, break);
		estat = GDP_STAT_INVOKE_TIMEOUT;	//XXX why?
		if (retries > 1)
		{
			// if ETIMEDOUT, maybe the router had a glitch:
			//   wait and try again
			ep_time_nanosleep(retry_delay MILLISECONDS);
		}
	} while (--retries > 0);

	// if we had any pending asynchronous events, deliver them
	_gdp_event_trigger_pending(&req->events);

	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[200];

		flockfile(ep_dbg_getfile());
		ep_dbg_printf("<<< _gdp_invoke(%p rid=%" PRIgdp_rid ") %s: %s\n",
				req, req->cpdu->rid, cmdname,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		if (ep_dbg_test(Dbg, 22))
		{
			ep_dbg_printf("  ");
			_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
			ep_dbg_printf("\n");
		}
		funlockfile(ep_dbg_getfile());
	}
	return estat;
}



/***********************************************************************
**
**	Protocol processing (CMD/ACK/NAK)
**
**		All of these take as parameters:
**			req --- the request information (including PDU header)
**
**		They can return GDP_STAT_KEEP_READING to tell the upper
**		layer that the whole PDU hasn't been read yet.
**
***********************************************************************/

typedef struct
{
	cmdfunc_t	*func;		// function to call
	const char	*name;		// name of command (for debugging)
	EP_STAT		estat;		// corresponding status
} dispatch_ent_t;

static dispatch_ent_t	DispatchTable[256];


/*
**  Common code for ACKs and NAKs
**
**		When called, the ack/nak PDU should be in req->rpdu.
*/

static EP_STAT
acknak(gdp_req_t *req, const char *where, bool reuse_pdu)
{
	EP_STAT estat = EP_STAT_WARN;
	const char *emsg = NULL;

	// we require a request
	if (req == NULL)
	{
		estat = GDP_STAT_PROTOCOL_FAIL;
		emsg = "null request";
		goto fail0;
	}

	ep_dbg_cprintf(Dbg, 20, "%s: received %s for %s\n", where,
			req->rpdu == NULL ? "???" : _gdp_proto_cmd_name(req->rpdu->cmd),
			req->cpdu == NULL ? "???" : _gdp_proto_cmd_name(req->cpdu->cmd));

	// we want to re-use caller's datum for (e.g.) read commands
	if (req->rpdu == NULL)
	{
		emsg = "acknak: req->rpdu == NULL";
	}
	else if (req->rpdu == req->cpdu)
	{
		emsg = "acknak: req->rpdu == req->cpdu";
	}
	else
	{
		estat = DispatchTable[req->rpdu->cmd].estat;
		if (req->cpdu->datum != NULL && reuse_pdu)
		{
			if (ep_dbg_test(Dbg, 43))
			{
				ep_dbg_printf("%s: reusing old datum for req %p\n   ",
						where, req);
				_gdp_datum_dump(req->cpdu->datum, ep_dbg_getfile());
			}

			// save the dbuf that the user may already hold a pointer to
			gdp_buf_t *user_dbuf = req->cpdu->datum->dbuf;

			// move the contents of the response dbuf into the user dbuf
			gdp_buf_reset(user_dbuf);
			gdp_buf_move(user_dbuf, req->rpdu->datum->dbuf, -1);

			// we can now discard the response dbuf entirely and replace it
			gdp_buf_free(req->rpdu->datum->dbuf);
			req->rpdu->datum->dbuf = user_dbuf;

			// same for signature
			gdp_buf_t *user_sig = req->cpdu->datum->sig;
			if (user_sig != NULL)
			{
				gdp_buf_reset(user_sig);
				if (req->rpdu->datum->sig != NULL)
					gdp_buf_move(user_sig, req->rpdu->datum->sig, -1);
			}
			if (req->rpdu->datum->sig != NULL)
				gdp_buf_free(req->rpdu->datum->sig);
			req->rpdu->datum->sig = user_sig;
			req->rpdu->datum->siglen = user_sig == NULL
								? 0
								: gdp_buf_getlength(user_sig);

			// copy the contents of the response datum over the user datum
			// (this has the pointer to user dbuf with response contents)
			memcpy(req->cpdu->datum, req->rpdu->datum, sizeof *req->cpdu->datum);

			// user datum now complete; can remove the response datum
			req->rpdu->datum->dbuf = NULL;		// but not the user dbuf!
			req->rpdu->datum->sig = NULL;		// or the signature!
			gdp_datum_free(req->rpdu->datum);

			// point the new PDU at the old datum
			req->rpdu->datum = req->cpdu->datum;
			req->cpdu->datum = NULL;
			EP_ASSERT(req->rpdu->datum->inuse);
		}
	}

fail0:
	if (!EP_STAT_ISOK(estat))
	{
		if (EP_STAT_ISSFAIL(estat))
		{
			ep_log(estat, "%s: %s",
					where,
					emsg != NULL ? emsg : "unknown severe failure");
		}
		else if (ep_dbg_test(Dbg, 1))
		{
			char ebuf[100];
			if (emsg != NULL)
				ep_dbg_printf("%s: %s: %s\n",
						where, emsg,
						ep_stat_tostr(estat, ebuf, sizeof ebuf));
			else
				ep_dbg_printf("%s: %s\n",
						where, ep_stat_tostr(estat, ebuf, sizeof ebuf));
		}
		if (emsg != NULL && ep_dbg_test(Dbg, 1))
			_gdp_req_dump(req, NULL, GDP_PR_BASIC, 0);
	}
	return estat;
}


/*
**  ACKs (success)
*/

static EP_STAT
ack(gdp_req_t *req, const char *where)
{
	EP_STAT estat;

	estat = acknak(req, where, true);
	EP_STAT_CHECK(estat, return estat);

	if (req->rpdu->datum == NULL)
	{
		ep_log(estat, "ack: null datum");
		estat = EP_STAT_OK;
	}
	else
	{
		estat = GDP_STAT_FROM_ACK(req->rpdu->cmd);
	}
	return estat;
}


static EP_STAT
ack_success(gdp_req_t *req)
{
	EP_STAT estat;
	gdp_gob_t *gob;

	estat = ack(req, "ack_success");
	EP_STAT_CHECK(estat, goto fail0);

	// mark this request as active (for subscriptions)
	ep_time_now(&req->act_ts);

	//	If we started with no gob id, adopt from incoming PDU.
	//	This can happen when creating a GOB.
	gob = req->gob;
	if (gob != NULL && !gdp_name_is_valid(gob->name))
	{
		memcpy(gob->name, req->rpdu->src, sizeof gob->name);
		gdp_printable_name(gob->name, gob->pname);
	}

	// if this is an open response, the GOB is now fully open
	if (gob != NULL)
		gob->flags &= ~GCLF_PENDING;

fail0:
	return estat;
}

// response to append command
static EP_STAT
ack_data_changed(gdp_req_t *req)
{
	EP_STAT estat;

	estat = ack_success(req);
	EP_STAT_CHECK(estat, return estat);

	// keep track of number of records (in case we lose sync)
	if (req->gob != NULL && req->rpdu->datum != NULL)
		req->gob->nrecs = req->rpdu->datum->recno;

	return estat;
}

// response to read command
static EP_STAT
ack_data_content(gdp_req_t *req)
{
	EP_STAT estat;

	EP_ASSERT_ELSE(req->gob != NULL, return EP_STAT_ASSERT_ABORT);
	EP_ASSERT_ELSE(req->rpdu != NULL, return EP_STAT_ASSERT_ABORT);
	EP_ASSERT_ELSE(req->rpdu->datum != NULL, return EP_STAT_ASSERT_ABORT);

	estat = ack_success(req);
	EP_STAT_CHECK(estat, return estat);

	// hack to try to "self heal" in case we get out of sync
	if (req->gob->nrecs < req->rpdu->datum->recno)
		req->gob->nrecs = req->rpdu->datum->recno;

	// keep track of how many more records we expect
	if (req->numrecs > 0)
		req->numrecs--;

	// do read filtering if requested
	if (req->gin != NULL && req->gin->readfilter != NULL)
		estat = req->gin->readfilter(req->rpdu->datum, req->gin->readfpriv);

	return estat;
}


/*
**  NAKs (failures)
*/

static EP_STAT
nak(gdp_req_t *req, const char *where)
{
	EP_STAT estat;

	estat = acknak(req, where, true);
	return estat;
}


static EP_STAT
nak_client(gdp_req_t *req)
{
	return nak(req, "nak_client");
}


static EP_STAT
nak_server(gdp_req_t *req)
{
	return nak(req, "nak_server");
}


static EP_STAT
nak_router(gdp_req_t *req)
{
	return acknak(req, "nak_router", false);
}


// called when a record number has been repeated
static EP_STAT
nak_conflict(gdp_req_t *req)
{
	EP_STAT estat = nak_client(req);

	// adjust nrecs to match the server's view
	if (req->gob != NULL && req->rpdu->datum != NULL)
		req->gob->nrecs = req->rpdu->datum->recno;

	return estat;
}



/*
**	Command/Ack/Nak Dispatch Table
*/

#define NOENT		{ NULL, NULL, GDP_STAT_NOT_IMPLEMENTED }

static dispatch_ent_t	DispatchTable[256] =
{
	{ NULL,				"CMD_KEEPALIVE",	EP_STAT_OK					},	// 0
	{ NULL,				"CMD_ADVERTISE",	EP_STAT_OK					},	// 1
	{ NULL,				"CMD_WITHDRAW",		EP_STAT_OK					},	// 2
	NOENT,				// 3
	NOENT,				// 4
	NOENT,				// 5
	NOENT,				// 6
	NOENT,				// 7
	NOENT,				// 8
	NOENT,				// 9
	NOENT,				// 10
	NOENT,				// 11
	NOENT,				// 12
	NOENT,				// 13
	NOENT,				// 14
	NOENT,				// 15
	NOENT,				// 16
	NOENT,				// 17
	NOENT,				// 18
	NOENT,				// 19
	NOENT,				// 20
	NOENT,				// 21
	NOENT,				// 22
	NOENT,				// 23
	NOENT,				// 24
	NOENT,				// 25
	NOENT,				// 26
	NOENT,				// 27
	NOENT,				// 28
	NOENT,				// 29
	NOENT,				// 30
	NOENT,				// 31
	NOENT,				// 32
	NOENT,				// 33
	NOENT,				// 34
	NOENT,				// 35
	NOENT,				// 36
	NOENT,				// 37
	NOENT,				// 38
	NOENT,				// 39
	NOENT,				// 40
	NOENT,				// 41
	NOENT,				// 42
	NOENT,				// 43
	NOENT,				// 44
	NOENT,				// 45
	NOENT,				// 46
	NOENT,				// 47
	NOENT,				// 48
	NOENT,				// 49
	NOENT,				// 50
	NOENT,				// 51
	NOENT,				// 52
	NOENT,				// 53
	NOENT,				// 54
	NOENT,				// 55
	NOENT,				// 56
	NOENT,				// 57
	NOENT,				// 58
	NOENT,				// 59
	NOENT,				// 60
	NOENT,				// 61
	NOENT,				// 62
	NOENT,				// 63
	{ NULL,				"CMD_PING",				GDP_STAT_ACK_SUCCESS		},	// 64
	{ NULL,				"CMD_HELLO",			GDP_STAT_ACK_SUCCESS		},	// 65
	{ NULL,				"CMD_CREATE",			GDP_STAT_ACK_SUCCESS		},	// 66
	{ NULL,				"CMD_OPEN_AO",			GDP_STAT_ACK_SUCCESS		},	// 67
	{ NULL,				"CMD_OPEN_RO",			GDP_STAT_ACK_SUCCESS		},	// 68
	{ NULL,				"CMD_CLOSE",			GDP_STAT_ACK_SUCCESS		},	// 69
	{ NULL,				"CMD_READ",				GDP_STAT_ACK_SUCCESS		},	// 70
	{ NULL,				"CMD_APPEND",			GDP_STAT_ACK_SUCCESS		},	// 71
	{ NULL,				"CMD_SUBSCRIBE",		GDP_STAT_ACK_SUCCESS		},	// 72
	{ NULL,				"CMD_MULTIREAD",		GDP_STAT_ACK_SUCCESS		},	// 73
	{ NULL,				"CMD_GETMETADATA",		GDP_STAT_ACK_SUCCESS		},	// 74
	{ NULL,				"CMD_OPEN_RA",			GDP_STAT_ACK_SUCCESS		},	// 75
	{ NULL,				"CMD_NEWSEGMENT",		GDP_STAT_ACK_SUCCESS		},	// 76
	{ NULL,				"CMD_FWD_APPEND",		GDP_STAT_ACK_SUCCESS		},	// 77
	{ NULL,				"CMD_UNSUBSCRIBE",		GDP_STAT_ACK_SUCCESS		},	// 78
	{ NULL,				"CMD_DELETE",			GDP_STAT_ACK_SUCCESS		},	// 79
	NOENT,				// 80
	NOENT,				// 81
	NOENT,				// 82
	NOENT,				// 83
	NOENT,				// 84
	NOENT,				// 85
	NOENT,				// 86
	NOENT,				// 87
	NOENT,				// 88
	NOENT,				// 89
	NOENT,				// 90
	NOENT,				// 91
	NOENT,				// 92
	NOENT,				// 93
	NOENT,				// 94
	NOENT,				// 95
	NOENT,				// 96
	NOENT,				// 97
	NOENT,				// 98
	NOENT,				// 99
	NOENT,				// 100
	NOENT,				// 101
	NOENT,				// 102
	NOENT,				// 103
	NOENT,				// 104
	NOENT,				// 105
	NOENT,				// 106
	NOENT,				// 107
	NOENT,				// 108
	NOENT,				// 109
	NOENT,				// 110
	NOENT,				// 111
	NOENT,				// 112
	NOENT,				// 113
	NOENT,				// 114
	NOENT,				// 115
	NOENT,				// 116
	NOENT,				// 117
	NOENT,				// 118
	NOENT,				// 119
	NOENT,				// 120
	NOENT,				// 121
	NOENT,				// 122
	NOENT,				// 123
	NOENT,				// 124
	NOENT,				// 125
	NOENT,				// 126
	NOENT,				// 127
	{ ack_success,		"ACK_SUCCESS",			GDP_STAT_ACK_SUCCESS		},	// 128
	{ ack_success,		"ACK_DATA_CREATED",		GDP_STAT_ACK_CREATED		},	// 129
	{ ack_success,		"ACK_DATA_DEL",			GDP_STAT_ACK_DELETED		},	// 130
	{ ack_success,		"ACK_DATA_VALID",		GDP_STAT_ACK_VALID			},	// 131
	{ ack_data_changed,	"ACK_DATA_CHANGED",		GDP_STAT_ACK_CHANGED		},	// 132
	{ ack_data_content,	"ACK_DATA_CONTENT",		GDP_STAT_ACK_CONTENT		},	// 133
	NOENT,				// 134
	NOENT,				// 135
	NOENT,				// 136
	NOENT,				// 137
	NOENT,				// 138
	NOENT,				// 139
	NOENT,				// 140
	NOENT,				// 141
	NOENT,				// 142
	NOENT,				// 143
	NOENT,				// 144
	NOENT,				// 145
	NOENT,				// 146
	NOENT,				// 147
	NOENT,				// 148
	NOENT,				// 149
	NOENT,				// 150
	NOENT,				// 151
	NOENT,				// 152
	NOENT,				// 153
	NOENT,				// 154
	NOENT,				// 155
	NOENT,				// 156
	NOENT,				// 157
	NOENT,				// 158
	NOENT,				// 159
	NOENT,				// 160
	NOENT,				// 161
	NOENT,				// 162
	NOENT,				// 163
	NOENT,				// 164
	NOENT,				// 165
	NOENT,				// 166
	NOENT,				// 167
	NOENT,				// 168
	NOENT,				// 169
	NOENT,				// 170
	NOENT,				// 171
	NOENT,				// 172
	NOENT,				// 173
	NOENT,				// 174
	NOENT,				// 175
	NOENT,				// 176
	NOENT,				// 177
	NOENT,				// 178
	NOENT,				// 179
	NOENT,				// 180
	NOENT,				// 181
	NOENT,				// 182
	NOENT,				// 183
	NOENT,				// 184
	NOENT,				// 185
	NOENT,				// 186
	NOENT,				// 187
	NOENT,				// 188
	NOENT,				// 189
	NOENT,				// 190
	NOENT,				// 191

	{ nak_client,		"NAK_C_BADREQ",			GDP_STAT_NAK_BADREQ			},	// 192
	{ nak_client,		"NAK_C_UNAUTH",			GDP_STAT_NAK_UNAUTH			},	// 193
	{ nak_client,		"NAK_C_BADOPT",			GDP_STAT_NAK_BADOPT			},	// 194
	{ nak_client,		"NAK_C_FORBIDDEN",		GDP_STAT_NAK_FORBIDDEN		},	// 195
	{ nak_client,		"NAK_C_NOTFOUND",		GDP_STAT_NAK_NOTFOUND		},	// 196
	{ nak_client,		"NAK_C_METHNOTALLOWED",	GDP_STAT_NAK_METHNOTALLOWED	},	// 197
	{ nak_client,		"NAK_C_NOTACCEPTABLE",	GDP_STAT_NAK_NOTACCEPTABLE	},	// 198
	NOENT,				// 199
	NOENT,				// 200
	{ nak_conflict,		"NAK_C_CONFLICT",		GDP_STAT_NAK_CONFLICT		},	// 201
	{ nak_client,		"NAK_C_GONE",			GDP_STAT_NAK_GONE			},	// 202
	NOENT,				// 203
	{ nak_client,		"NAK_C_PRECONFAILED",	GDP_STAT_NAK_PRECONFAILED	},	// 204
	{ nak_client,		"NAK_C_TOOLARGE",		GDP_STAT_NAK_TOOLARGE		},	// 205
	NOENT,				// 206
	{ nak_client,		"NAK_C_UNSUPMEDIA",		GDP_STAT_NAK_UNSUPMEDIA		},	// 207
	NOENT,				// 208
	NOENT,				// 209
	NOENT,				// 210
	NOENT,				// 211
	NOENT,				// 212
	NOENT,				// 213
	NOENT,				// 214
	NOENT,				// 215
	NOENT,				// 216
	NOENT,				// 217
	NOENT,				// 218
	NOENT,				// 219
	NOENT,				// 220
	NOENT,				// 221
	{ nak_client,		"NAK_C_MISSING_RECORD",	GDP_STAT_NAK_REC_MISSING	},	// 222
	{ nak_client,		"NAK_C_REC_DUP",		GDP_STAT_NAK_REC_DUP		},	// 223

	{ nak_server,		"NAK_S_INTERNAL",		GDP_STAT_NAK_INTERNAL		},	// 224
	{ nak_server,		"NAK_S_NOTIMPL"	,		GDP_STAT_NAK_NOTIMPL		},	// 225
	{ nak_server,		"NAK_S_BADGATEWAY",		GDP_STAT_NAK_BADGATEWAY		},	// 226
	{ nak_server,		"NAK_S_SVCUNAVAIL",		GDP_STAT_NAK_SVCUNAVAIL		},	// 227
	{ nak_server,		"NAK_S_GWTIMEOUT",		GDP_STAT_NAK_GWTIMEOUT		},	// 228
	{ nak_server,		"NAK_S_PROXYNOTSUP",	GDP_STAT_NAK_PROXYNOTSUP	},	// 229
	NOENT,				// 230
	NOENT,				// 231
	NOENT,				// 232
	NOENT,				// 233
	NOENT,				// 234
	NOENT,				// 235
	NOENT,				// 236
	NOENT,				// 237
	{ nak_server,		"NAK_S_REC_MISSING",	GDP_STAT_NAK_REC_MISSING	},	// 238
	{ nak_server,		"NAK_S_LOSTSUB",		GDP_STAT_NAK_LOST_SUBSCR	},	// 239

	{ nak_router,		"NAK_R_NOROUTE",		GDP_STAT_NAK_NOROUTE		},	// 240
	NOENT,				// 241
	NOENT,				// 242
	NOENT,				// 243
	NOENT,				// 244
	NOENT,				// 245
	NOENT,				// 246
	NOENT,				// 247
	NOENT,				// 248
	NOENT,				// 249
	NOENT,				// 250
	NOENT,				// 251
	NOENT,				// 252
	NOENT,				// 253
	NOENT,				// 254
	NOENT,				// 255
};


/*
**	_GDP_CMD_NAME --- return name of command
*/

const char *
_gdp_proto_cmd_name(uint8_t cmd)
{
	dispatch_ent_t *d = &DispatchTable[cmd];

	if (d->name != NULL)
	{
		return d->name;
	}
	else
	{
		// not thread safe, but shouldn't happen
		static char buf[10];

		snprintf(buf, sizeof buf, "%d", cmd);
		return buf;
	}
}


/*
**  Add any additional command functions.
**		Applications that add additional functionality (e.g.,
**		gdplogd) can add implementations by calling this function.
*/

void
_gdp_register_cmdfuncs(struct cmdfuncs *cf)
{
	for (; cf->func != NULL; cf++)
	{
		DispatchTable[cf->cmd].func = cf->func;
	}
}


/*
**  Called for unimplemented commands
*/

static EP_STAT
cmd_not_implemented(gdp_req_t *req)
{
	// just ignore unknown commands
	if (ep_dbg_test(Dbg, 1))
	{
		flockfile(ep_dbg_getfile());
		ep_dbg_printf("_gdp_req_dispatch: Unknown cmd, req:\n");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		funlockfile(ep_dbg_getfile());
	}

	return GDP_STAT_NOT_IMPLEMENTED;
}


/*
**  Dispatch command to implementation function.
**
**		The req should be locked when this is called.
*/

EP_STAT
_gdp_req_dispatch(gdp_req_t *req, int cmd)
{
	EP_STAT estat;
	dispatch_ent_t *d;
	gdp_pname_t pname;

	if (req->gob != NULL)
	{
		memcpy(pname, req->gob->pname, sizeof pname);
		GDP_GOB_ASSERT_ISLOCKED(req->gob);
	}
	else
	{
		pname[0] = '\0';
	}
	if (ep_dbg_test(Dbg, 28) || ep_dbg_test(DbgCmdTrace, 28))
	{
		flockfile(ep_dbg_getfile());
		ep_dbg_printf("_gdp_req_dispatch >>> %s",
				_gdp_proto_cmd_name(cmd));
		if (pname[0] != '\0')
			ep_dbg_printf("(%s)", req->gob->pname);
		if (req->gob != NULL && ep_dbg_test(Dbg, 70))
				ep_dbg_printf(" [gob->refcnt %d]", req->gob->refcnt);
		ep_dbg_printf("\n");
		if (ep_dbg_test(Dbg, 51))
		{
			ep_dbg_printf("    ");
			_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		}
		funlockfile(ep_dbg_getfile());
	}

	d = &DispatchTable[cmd];
	if (d->func == NULL)
		estat = cmd_not_implemented(req);
	else
		estat = (*d->func)(req);

	// command function should not change lock state of GOB
	if (req->gob != NULL && !GDP_GOB_ASSERT_ISLOCKED(req->gob))
		_gdp_gob_dump(req->gob, NULL, GDP_PR_BASIC, 0);

	if (ep_dbg_test(Dbg, 18) || ep_dbg_test(DbgCmdTrace, 18))
	{
		char ebuf[200];

		flockfile(ep_dbg_getfile());
		ep_dbg_printf("_gdp_req_dispatch <<< %s",
				_gdp_proto_cmd_name(cmd));
		if (pname[0] != '\0')
			ep_dbg_printf("(%s)", pname);
		else if (req->gob != NULL && req->gob->pname[0] != '\0')
			ep_dbg_printf("(%s)", req->gob->pname);
		if (req->gob != NULL && ep_dbg_test(Dbg, 70))
			ep_dbg_printf(" [gob->refcnt %d]", req->gob->refcnt);
		ep_dbg_printf(": %s\n", ep_stat_tostr(estat, ebuf, sizeof ebuf));
		if (ep_dbg_test(Dbg, 70))
		{
			ep_dbg_printf("    ");
			_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		}
		funlockfile(ep_dbg_getfile());
	}

	return estat;
}


/*
**  Advertise me only
*/

EP_STAT
_gdp_advertise_me(gdp_chan_t *chan, int cmd, void *adata)
{
	gdp_chan_advert_cr_t *cr_cb = NULL;			//XXX XXX
	gdp_adcert_t *adcert = NULL;				//XXX XXX
	return _gdp_chan_advertise(chan, _GdpMyRoutingName, adcert, cr_cb, adata);
}
