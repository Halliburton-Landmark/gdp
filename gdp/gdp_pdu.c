/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP_PDU.C --- low-level PDU internal <=> external translations
**
**		Everything is read and written in network byte order.
**
**		XXX Should we be working on raw files instead of stdio files?
**			Will make a difference if we use UDP.
**		XXX Should we pad out the PDU header to an even four bytes?
**			Might help some implementations when reading or writing
**			the data.
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
#include <ep/ep_dbg.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_log.h>
#include <ep/ep_prflags.h>
#include <ep/ep_stat.h>

#include "gdp.h"
#include "gdp_priv.h"
#include "gdp_pdu.h"

#include <event2/event.h>

#include <string.h>
#include <sys/errno.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.pdu", "GDP PDU traffic");


static EP_PRFLAGS_DESC	PduFlags[] =
{
	{	GDP_PDU_HAS_RECNO,	GDP_PDU_HAS_RECNO,	"HAS_RECNO"		},
	{	GDP_PDU_HAS_SEQNO,	GDP_PDU_HAS_SEQNO,	"HAS_SEQNO"		},
	{	GDP_PDU_HAS_TS,		GDP_PDU_HAS_TS,		"HAS_TS"		},
	{	0,					0,					NULL			}
};

void
_gdp_pdu_dump(const gdp_pdu_t *pdu, FILE *fp)
{
	flockfile(fp);
	fprintf(fp, "PDU@%p: ", pdu);
	if (pdu == NULL)
	{
		fprintf(fp, "NULL\n");
		goto done;
	}

	int len = _GDP_PDU_FIXEDHDRSZ + pdu->olen;
	fprintf(fp, "\n\tv=%d, ttl=%d, rsvd1=%d, cmd=%d=%s",
				pdu->ver, pdu->ttl, pdu->rsvd1,
				pdu->cmd, _gdp_proto_cmd_name(pdu->cmd));
	fprintf(fp, "\n\tdst=");
	gdp_print_name(pdu->dst, fp);
	fprintf(fp, "\n\tsrc=");
	gdp_print_name(pdu->src, fp);
	fprintf(fp, "\n\trid=%u, olen=%d, chan=%p, seqno=%" PRIgdp_seqno
				"\n\tflags=",
				pdu->rid, pdu->olen, pdu->chan, pdu->seqno);
	ep_prflags(pdu->flags, PduFlags, fp);
	fprintf(fp, "\n\tdatum=%p", pdu->datum);
	if (pdu->datum != NULL)
	{
		fprintf(fp, ", recno=");
		if (pdu->datum->recno == GDP_PDU_NO_RECNO)
			fprintf(fp, "(none)");
		else
		{
			fprintf(fp, "%" PRIgdp_recno, pdu->datum->recno);
			len += sizeof pdu->datum->recno;
		}
		fprintf(fp, ", dbuf=%p, dlen=%zu", pdu->datum->dbuf,
				pdu->datum->dbuf == NULL ? 0 :
						gdp_buf_getlength(pdu->datum->dbuf));
		fprintf(fp, "\n\t\tts=");
		ep_time_print(&pdu->datum->ts, fp, EP_TIME_FMT_HUMAN);
		if (EP_TIME_IS_VALID(&pdu->datum->ts))
			len += sizeof pdu->datum->ts;
		fprintf(fp, "\n\tsigmdalg=0x%x, siglen=%d, sig=%p",
				pdu->datum->sigmdalg, pdu->datum->siglen, pdu->datum->sig);
	}
	fprintf(fp, "\n\ttotal header=%d\n", len);
done:
	funlockfile(fp);
}


/*
**  Helper routine to send data with diagnostics and debugging.
*/

static EP_STAT
send_data(gdp_buf_t *obuf,
		void *data, size_t len,
		const char *where, int offset, int dbgmode)
{
	EP_STAT estat = GDP_STAT_PDU_WRITE_FAIL;

	if (ep_dbg_test(Dbg, 33))
	{
		ep_hexdump(data, len, ep_dbg_getfile(), dbgmode, offset);
	}

	if (data == NULL)
	{
		ep_dbg_cprintf(Dbg, 1, "_gdp_pdu_out: %s: no data\n", where);
	}
	else if (len <= 0)
	{
		ep_dbg_cprintf(Dbg, 1, "_gdp_pdu_out: %s: empty data (%zd)\n",
				where, len);
	}
	else if (gdp_buf_write(obuf, data, len) < 0)
	{
		char nbuf[40];

		// couldn't write output
		strerror_r(errno, nbuf, sizeof nbuf);
		ep_dbg_cprintf(Dbg, 1,
				"_gdp_pdu_out: %s write failure (len = %zd): %s\n",
				where, len, nbuf);
	}
	else
	{
		estat = EP_STAT_OK;
	}

	return estat;
}



/*
**	GDP_PDU_OUT --- send a PDU to a network buffer
**
**		Outputs PDU, including all the data in the dbuf.
*/

#define PUT16(v) \
		{ \
			*pbp++ = ((v) >> 8) & 0xff; \
			*pbp++ = ((v) & 0xff); \
		}
#define PUT32(v) \
		{ \
			*pbp++ = ((v) >> 24) & 0xff; \
			*pbp++ = ((v) >> 16) & 0xff; \
			*pbp++ = ((v) >> 8) & 0xff; \
			*pbp++ = ((v) & 0xff); \
		}
#define PUT48(v) \
		{ \
			*pbp++ = ((v) >> 40) & 0xff; \
			*pbp++ = ((v) >> 32) & 0xff; \
			*pbp++ = ((v) >> 24) & 0xff; \
			*pbp++ = ((v) >> 16) & 0xff; \
			*pbp++ = ((v) >> 8) & 0xff; \
			*pbp++ = ((v) & 0xff); \
		}
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

#define OOFF		74		// offset of olen from beginning of pdu
#define FOFF		75		// offset of flags from beginning of pdu

EP_STAT
_gdp_pdu_out(gdp_pdu_t *pdu, gdp_chan_t *chan, EP_CRYPTO_MD *basemd)
{
	EP_STAT estat = EP_STAT_OK;
	uint8_t pbuf[_GDP_PDU_MAXHDRSZ];
	uint8_t *pbp = pbuf;
	size_t dlen;
	size_t hdrlen;
	size_t offset;
	gdp_buf_t *obuf;
	uint8_t sigbuf[EP_CRYPTO_MAX_SIG];
	bool use_sigbuf = false;

	EP_ASSERT_POINTER_VALID(pdu);

        // avoid segfault if the daemon is not running
        if (chan == NULL)
        {
            return GDP_STAT_DEAD_DAEMON;
        }

        obuf = bufferevent_get_output(chan->bev);

	if (!gdp_name_is_valid(pdu->src))
	{
		// use our own name as the source if nothing specified
		memcpy(pdu->src, _GdpMyRoutingName, sizeof pdu->src);
	}

	if (ep_dbg_test(Dbg, 1))
		flockfile(ep_dbg_getfile());
	if (ep_dbg_test(Dbg, 18))
	{
		ep_dbg_printf("_gdp_pdu_out, fd = %d, basemd = %p:",
				bufferevent_getfd(chan->bev), basemd);
		if (ep_dbg_test(Dbg, 22))
		{
			ep_dbg_printf("\n");
			// remainder will be printed below
		}
		else
		{
			ep_dbg_printf(" %s\n", _gdp_proto_cmd_name(pdu->cmd));
		}
	}

	if (basemd != NULL)
	{
		// compute the signature
		uint8_t recnobuf[8];		// 64 bits
		uint8_t *pbp = recnobuf;
		size_t reclen;
		EP_CRYPTO_MD *md = ep_crypto_md_clone(basemd);
		gdp_datum_t *datum = pdu->datum;
		size_t siglen = sizeof sigbuf;

		if (md != NULL)
		{
			PUT64(pdu->datum->recno);
			ep_crypto_sign_update(md, &recnobuf, sizeof recnobuf);
			reclen = gdp_buf_getlength(datum->dbuf);
			ep_crypto_sign_update(md, gdp_buf_getptr(datum->dbuf, reclen),
					reclen);
			ep_crypto_sign_final(md, &sigbuf, &siglen);
			datum->siglen = siglen;
			datum->sigmdalg = ep_crypto_md_type(md);
			if (datum->sig != NULL)
				gdp_buf_drain(datum->sig, gdp_buf_getlength(datum->sig));
			ep_crypto_sign_free(md);
			use_sigbuf = true;
		}
		else
		{
			ep_dbg_cprintf(Dbg, 1, "_gdp_pdu_out: cannot clone message digest");
		}
	}

	if (ep_dbg_test(Dbg, 22))
	{
		ep_dbg_printf("    ");
		_gdp_pdu_dump(pdu, ep_dbg_getfile());
	}

	// version number
	if (pdu->ver < GDP_PROTO_MIN_VERSION || pdu->ver > GDP_PROTO_CUR_VERSION)
		*pbp++ = GDP_PROTO_CUR_VERSION;
	else
		*pbp++ = pdu->ver;

	// time to live (in hops)
	*pbp++ = pdu->ttl;

	// reserved field
	*pbp++ = pdu->rsvd1;

	// command
	*pbp++ = pdu->cmd;

	// destination address
	memcpy(pbp, pdu->dst, sizeof pdu->dst);
	pbp += sizeof pdu->dst;

	// source address
	memcpy(pbp, pdu->src, sizeof pdu->src);
	pbp += sizeof pdu->src;

	// request id
	PUT32(pdu->rid);

	// signature digest algorithm and size
	if (pdu->datum != NULL)
	{
		uint16_t sigtmp;
		sigtmp = (pdu->datum->siglen & 0x0fff) |
				((pdu->datum->sigmdalg & 0x0f) << 12);
		PUT16(sigtmp);
	}
	else
	{
		PUT16(0);
	}

	// length of options (filled in later)
	*pbp++ = 0;

	// flags (filled in later)
	*pbp++ = 0;

	// data length
	if (pdu->datum != NULL)
		dlen = gdp_buf_getlength(pdu->datum->dbuf);
	else
		dlen = 0;
	PUT32(dlen);

	// end of fixed part of header
	EP_ASSERT((pbp - pbuf) == _GDP_PDU_FIXEDHDRSZ);

	// record number
	if (pdu->datum != NULL && pdu->datum->recno != GDP_PDU_NO_RECNO)
	{
		pbuf[FOFF] |= GDP_PDU_HAS_RECNO;
		PUT64(pdu->datum->recno);
	}

	// sequence number
	if (pdu->seqno > 0)
	{
		pbuf[FOFF] |= GDP_PDU_HAS_SEQNO;
		PUT64(pdu->seqno);
	}

	// timestamp
	if (pdu->datum != NULL && EP_TIME_IS_VALID(&pdu->datum->ts))
	{
		pbuf[FOFF] |= GDP_PDU_HAS_TS;
		PUT64(pdu->datum->ts.tv_sec);
		PUT32(pdu->datum->ts.tv_nsec);
		PUT32(*((uint32_t *) &pdu->datum->ts.tv_accuracy));
	}

	hdrlen = pbp - pbuf;
	offset = 0;
	// convert length of optional fields to words; should never require padding
	EP_ASSERT(((hdrlen - _GDP_PDU_FIXEDHDRSZ) & 0x03) == 0);
	pbuf[OOFF] = (hdrlen - _GDP_PDU_FIXEDHDRSZ) / 4;

	ep_dbg_cprintf(Dbg, 32, "_gdp_pdu_out: sending PDU:\n");

	// send header
	gdp_buf_lock(obuf);
	estat = send_data(obuf, pbuf, hdrlen,
					"header", offset, EP_HEXDUMP_HEX);
	offset += pbp - pbuf;
	EP_STAT_CHECK(estat, goto fail0);

	// send data
	if (dlen > 0)
	{
		uint8_t *bp;

		bp = gdp_buf_getptr(pdu->datum->dbuf, dlen);
		estat = send_data(obuf, bp, dlen,
						"data", offset, EP_HEXDUMP_ASCII);
		offset += dlen;
		EP_STAT_CHECK(estat, goto fail0);
	}

	// send signature
	if (pdu->datum != NULL && pdu->datum->siglen > 0)
	{
		uint8_t *sigp = NULL;

		if (use_sigbuf)
			sigp = sigbuf;
		else if (pdu->datum->sig != NULL)
		{
			sigp = gdp_buf_getptr(pdu->datum->sig, pdu->datum->siglen);
			if (sigp == NULL && ep_dbg_test(Dbg, 1))
			{
				ep_dbg_printf("_gdp_pdu_out(%s): siglen = %d"
						" but only %zd available\n",
						_gdp_proto_cmd_name(pdu->cmd), pdu->datum->siglen,
						gdp_buf_getlength(pdu->datum->sig));
				ep_dbg_printf("    basemd = %p\n",
						basemd);
			}
		}
		else
		{
			ep_dbg_cprintf(Dbg, 1, "_gdp_pdu_out: siglen = %d but no data",
					pdu->datum->siglen);
		}
		if (sigp != NULL)
		{
			estat = send_data(obuf, sigp, pdu->datum->siglen,
							"signature", offset, EP_HEXDUMP_HEX);
			offset += pdu->datum->siglen;
		}
		EP_STAT_CHECK(estat, goto fail0);
	}

fail0:
	if (ep_dbg_test(Dbg, 1))
		funlockfile(ep_dbg_getfile());
	if (!EP_STAT_ISOK(estat))
	{
		// flush buffer so we send nothing once the buffer is unlocked
		gdp_buf_drain(obuf, gdp_buf_getlength(obuf));
	}
	gdp_buf_unlock(obuf);

	return estat;
}


/*
**	GDP_PDU_OUT_HARD --- same as above, but complain on error
*/

void
_gdp_pdu_out_hard(gdp_pdu_t *pdu, gdp_chan_t *chan, EP_CRYPTO_MD *md)
{
	EP_STAT estat = _gdp_pdu_out(pdu, chan, md);

	if (!EP_STAT_ISOK(estat))
	{
		ep_log(estat, "_gdp_pdu_out_hard: cannot put PDU");
	}
}


/*
**	GDP_PDU_IN --- read a PDU from the network
**
**		The caller has to loop on this while it returns
**		GDP_STAT_KEEP_READING, which means that some required data
**		isn't present yet.
**
**	XXX This can probably done more efficiently using gdp_buf_peek.
*/

#define GET16(v) \
		{ \
				v  = *pbp++ << 8; \
				v |= *pbp++; \
		}
#define GET32(v) \
		{ \
				v  = *pbp++ << 24; \
				v |= *pbp++ << 16; \
				v |= *pbp++ << 8; \
				v |= *pbp++; \
		}
#define GET48(v) \
		{ \
				v  = ((uint64_t) *pbp++) << 40; \
				v |= ((uint64_t) *pbp++) << 32; \
				v |= ((uint64_t) *pbp++) << 24; \
				v |= ((uint64_t) *pbp++) << 16; \
				v |= ((uint64_t) *pbp++) << 8; \
				v |= ((uint64_t) *pbp++); \
		}
#define GET64(v) \
		{ \
				v  = ((uint64_t) *pbp++) << 56; \
				v |= ((uint64_t) *pbp++) << 48; \
				v |= ((uint64_t) *pbp++) << 40; \
				v |= ((uint64_t) *pbp++) << 32; \
				v |= ((uint64_t) *pbp++) << 24; \
				v |= ((uint64_t) *pbp++) << 16; \
				v |= ((uint64_t) *pbp++) << 8; \
				v |= ((uint64_t) *pbp++); \
		}


// read the fixed header portion in; shared with routing code
EP_STAT
_gdp_pdu_hdr_in(gdp_pdu_t *pdu,
		gdp_chan_t *chan,
		size_t *pduszp,
		uint64_t *dlenp)
{
	uint64_t dlen;
	uint8_t pbuf[_GDP_PDU_MAXHDRSZ];
	uint8_t *pbp;
	gdp_buf_t *ibuf;
	size_t needed;

	ibuf = bufferevent_get_input(chan->bev);

	// see if the fixed part of the header is all in
	needed = gdp_buf_peek(ibuf, pbuf, _GDP_PDU_FIXEDHDRSZ);

	if (ep_dbg_test(Dbg, 62))
	{
		ep_dbg_printf("_gdp_pdu_in: fixed pdu header:\n");
		ep_hexdump(pbuf, needed, ep_dbg_getfile(), EP_HEXDUMP_HEX, 0);
	}

	if (needed < _GDP_PDU_FIXEDHDRSZ)
	{
		// try again after we read more in
		ep_dbg_cprintf(Dbg, 42,
						"_gdp_pdu_in: keep reading (have %zd, need %d)\n",
						gdp_buf_getlength(ibuf), _GDP_PDU_FIXEDHDRSZ);
		return GDP_STAT_KEEP_READING;
	}

	// hack: store metadata
	pdu->chan = chan;

	// read the fixed part of the PDU header in
	pbp = pbuf;
	pdu->ver = *pbp++;

	// no point in continuing if we don't recognize the PDU
	if (pdu->ver < GDP_PROTO_MIN_VERSION || pdu->ver > GDP_PROTO_CUR_VERSION)
	{
		if (ep_dbg_test(Dbg, 1))
		{
			ep_dbg_printf("_gdp_pdu_in: version %d out of range (%d-%d)\n",
					pdu->ver, GDP_PROTO_MIN_VERSION, GDP_PROTO_CUR_VERSION);
			ep_hexdump(pbuf, _GDP_PDU_FIXEDHDRSZ, ep_dbg_getfile(),
					EP_HEXDUMP_ASCII, 0);
		}

		// throw away everything we can in the hopes we can re-sync
		gdp_buf_reset(ibuf);
		return GDP_STAT_PDU_VERSION_MISMATCH;
	}

	// ok, we recognize it
	pdu->ttl = *pbp++;
	pdu->rsvd1 = *pbp++;
	pdu->cmd = *pbp++;
	memcpy(pdu->dst, pbp, sizeof pdu->dst);
	pbp += sizeof pdu->dst;
	memcpy(pdu->src, pbp, sizeof pdu->src);
	pbp += sizeof pdu->src;
	GET32(pdu->rid);
	uint16_t sigtmp;
	GET16(sigtmp);
	pdu->datum->sigmdalg = (sigtmp >> 12) & 0x0f;
	pdu->datum->siglen = sigtmp & 0x0fff;
	pdu->olen = *pbp++ * 4;
	pdu->flags = *pbp++;
	GET32(dlen);

	// do some error checking
	int olen = 0;
	if (EP_UT_BITSET(GDP_PDU_HAS_SEQNO, pdu->flags))
		olen += sizeof (gdp_seqno_t);
	if (EP_UT_BITSET(GDP_PDU_HAS_RECNO, pdu->flags))
		olen += sizeof (gdp_recno_t);
	if (EP_UT_BITSET(GDP_PDU_HAS_TS, pdu->flags))
		olen += sizeof (EP_TIME_SPEC);
	if (pdu->olen < olen)
	{
		// oops!  ten pounds in a five pound sack
		EP_STAT estat = GDP_STAT_PDU_CORRUPT;
		ep_log(estat,
				"_gdp_pdu_in: option overflow, needs %d, has %d",
				olen, pdu->olen);
		return estat;
	}

	// figure out how much additional data we will need
	needed += pdu->olen + dlen + pdu->datum->siglen;
	*dlenp = dlen;
	*pduszp = needed;

	// see if the entire PDU (header + data) is available
	if (gdp_buf_getlength(ibuf) < needed)
	{
		// still not enough data
		if (ep_dbg_test(Dbg, 42))
		{
			char xbuf[256];
			size_t s;

			s = gdp_buf_getlength(ibuf);
			ep_dbg_printf("_gdp_pdu_in: keep reading (have %zd, need %zd)\n",
						s, needed);
			if (s > sizeof xbuf)
				s = sizeof xbuf;
			gdp_buf_peek(ibuf, xbuf, s);
			ep_hexdump(xbuf, s, ep_dbg_getfile(), EP_HEXDUMP_HEX, 0);
		}
		return GDP_STAT_KEEP_READING;
	}

	return EP_STAT_OK;
}

EP_STAT
_gdp_pdu_in(gdp_pdu_t *pdu, gdp_chan_t *chan)
{
	EP_STAT estat = EP_STAT_OK;
	size_t sz;
	uint8_t pbuf[_GDP_PDU_MAXHDRSZ];
	uint8_t *pbp;
	gdp_buf_t *ibuf;
	size_t needed;
	uint64_t dlen;

	EP_ASSERT_POINTER_VALID(pdu);

	ep_dbg_cprintf(Dbg, 30, "\n\t>>>>>  _gdp_pdu_in  >>>>>\n");
	EP_ASSERT(pdu->datum != NULL);
	EP_ASSERT(pdu->datum->dbuf != NULL);
	ibuf = bufferevent_get_input(chan->bev);

	estat = _gdp_pdu_hdr_in(pdu, chan, &needed, &dlen);
	EP_STAT_CHECK(estat, return estat);

	// the entire PDU is now in ibuf

	// now drain the data we have processed
	EP_ASSERT_INSIST(_GDP_PDU_FIXEDHDRSZ + pdu->olen <= _GDP_PDU_MAXHDRSZ);
	sz = gdp_buf_read(ibuf, pbuf, _GDP_PDU_FIXEDHDRSZ + pdu->olen);
	if (sz < _GDP_PDU_FIXEDHDRSZ + pdu->olen)
	{
		// shouldn't happen, since it's already in memory
		estat = GDP_STAT_BUFFER_FAILURE;
		ep_log(estat,
				"_gdp_pdu_in: gdp_buf_drain failed, sz = %zu, needed = %u\n",
				sz, _GDP_PDU_FIXEDHDRSZ + pdu->olen);
		// buffer is now out of sync; not clear if we can continue
	}
	pbp = &pbuf[_GDP_PDU_FIXEDHDRSZ];

	if (ep_dbg_test(Dbg, 32))
	{
		flockfile(ep_dbg_getfile());
		ep_dbg_printf("_gdp_pdu_in: read PDU header:\n");
		ep_hexdump(pbuf, sz, ep_dbg_getfile(), EP_HEXDUMP_HEX, 0);
		funlockfile(ep_dbg_getfile());
	}

	// record number
	if (!EP_UT_BITSET(GDP_PDU_HAS_RECNO, pdu->flags))
		pdu->datum->recno = GDP_PDU_NO_RECNO;
	else
		GET64(pdu->datum->recno)

	// sequence number
	if (!EP_UT_BITSET(GDP_PDU_HAS_SEQNO, pdu->flags))
		pdu->seqno = 0;
	else
		GET64(pdu->seqno)

	// timestamp
	if (!EP_UT_BITSET(GDP_PDU_HAS_TS, pdu->flags))
	{
		memset(&pdu->datum->ts, 0, sizeof pdu->datum->ts);
		EP_TIME_INVALIDATE(&pdu->datum->ts);
	}
	else
	{
		GET64(pdu->datum->ts.tv_sec);
		GET32(pdu->datum->ts.tv_nsec);
		GET32(*((uint32_t *) &pdu->datum->ts.tv_accuracy));
	}

	//XXX soak up any padding bytes?

	// ibuf now points at the data block, sz is the PDU offset
	{
		size_t l;

		ep_dbg_cprintf(Dbg, 38,
				"_gdp_pdu_in: reading %zd data bytes (%zd available)\n",
				dlen, gdp_buf_getlength(ibuf));
		l = gdp_buf_move(pdu->datum->dbuf, ibuf, dlen);
		if (ep_dbg_test(Dbg, 39))
		{
			ep_hexdump(gdp_buf_getptr(pdu->datum->dbuf, l), l,
					ep_dbg_getfile(), EP_HEXDUMP_ASCII, sz);
		}
		if (l < dlen)
		{
			// should never happen since we already have all the data in memory
			ep_dbg_cprintf(Dbg, 2,
					"_gdp_pdu_in: cannot read all data; wanted %zd, got %zd\n",
					dlen, l);
		}
		sz += l;
	}

	// ibuf now points at the signature (if any)
	if (pdu->datum->sig != NULL)
		gdp_buf_reset(pdu->datum->sig);			// just in case
	if (pdu->datum->siglen > 0)
	{
		size_t l;

		ep_dbg_cprintf(Dbg, 38,
				"_gdp_pdu_in: reading %zd signature bytes (%zd available)\n",
				pdu->datum->siglen, gdp_buf_getlength(ibuf));
		if (pdu->datum->sig == NULL)
			pdu->datum->sig = gdp_buf_new();

		l = gdp_buf_move(pdu->datum->sig, ibuf, pdu->datum->siglen);
		if (ep_dbg_test(Dbg, 39))
		{
			ep_hexdump(gdp_buf_getptr(pdu->datum->sig, l), l,
					ep_dbg_getfile(), EP_HEXDUMP_HEX, sz);
		}
		if (l < pdu->datum->siglen)
		{
			// should never happen since we already have all the data in memory
			ep_dbg_cprintf(Dbg, 2,
					"_gdp_pdu_in: cannot read all signature; wanted %zd, got %zd\n",
					pdu->datum->siglen, l);
		}
	}

	if (ep_dbg_test(Dbg, 18))
	{
		char ebuf[200];

		flockfile(ep_dbg_getfile());
		ep_dbg_printf("_gdp_pdu_in(%s) => %s\n",
				_gdp_proto_cmd_name(pdu->cmd),
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		if (ep_dbg_test(Dbg, 22))
			_gdp_pdu_dump(pdu, ep_dbg_getfile());
		funlockfile(ep_dbg_getfile());
	}

	return estat;
}


/*
**  _GDP_PDU_NEW --- allocate a PDU (from free list if possible)
**  _GDP_PDU_FREE --- return a PDU to the free list
*/

static EP_THR_MUTEX		PduFreeListMutex	EP_THR_MUTEX_INITIALIZER;
static TAILQ_HEAD(pkt_head, gdp_pdu)
						PduFreeList = TAILQ_HEAD_INITIALIZER (PduFreeList);

gdp_pdu_t *
_gdp_pdu_new(void)
{
	gdp_pdu_t *pdu;

	ep_thr_mutex_lock(&PduFreeListMutex);
	if ((pdu = TAILQ_FIRST(&PduFreeList)) != NULL)
		TAILQ_REMOVE(&PduFreeList, pdu, list);
	ep_thr_mutex_unlock(&PduFreeListMutex);

	if (pdu == NULL)
	{
		pdu = ep_mem_zalloc(sizeof *pdu);
	}

	// initialize the PDU
	EP_ASSERT(!pdu->inuse);
	memset(pdu, 0, sizeof *pdu);
	pdu->ver = GDP_PROTO_CUR_VERSION;
	pdu->ttl = GDP_TTL_DEFAULT;
	pdu->datum = gdp_datum_new();
	pdu->inuse = true;

	ep_dbg_cprintf(Dbg, 48, "_gdp_pdu_new => %p\n", pdu);
	return pdu;
}

void
_gdp_pdu_free(gdp_pdu_t *pdu)
{
	ep_dbg_cprintf(Dbg, 48, "_gdp_pdu_free(%p)\n", pdu);
	EP_ASSERT(pdu->inuse);
	if (pdu->datum != NULL)
		gdp_datum_free(pdu->datum);
	pdu->datum = NULL;
	pdu->inuse = false;
	ep_thr_mutex_lock(&PduFreeListMutex);
	TAILQ_INSERT_HEAD(&PduFreeList, pdu, list);
	ep_thr_mutex_unlock(&PduFreeListMutex);
}
