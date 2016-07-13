/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  ----- BEGIN LICENSE BLOCK -----
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
**  ----- END LICENSE BLOCK -----
*/

#ifndef _GDP_PDU_H_
#define _GDP_PDU_H_

#include <stdio.h>
#include <netinet/in.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include "gdp_priv.h"

#define GDP_PORT_DEFAULT		8007	// default IP port

#define GDP_PROTO_CUR_VERSION	3		// current protocol version
#define GDP_PROTO_MIN_VERSION	2		// min version we can accept

#define GDP_TTL_DEFAULT			15		// hops left



/*
**	Header for a GDP Protocol Data Unit (PDU).
**
**		This struct is not the "on the wire" format, which has to be
**		put into network byte order and packed.	However, this does
**		show the order in which fields are written.
**
**		Commands are eight bits with the top three bits encoding
**		additional semantics.  Those bits are:
**
**		00x		"Blind" (unacknowledged) command
**		01x		Acknowledged command
**		10x		Positive acknowledgement
**		110		Negative acknowledgement, client side problem
**		111		Negative acknowledgement, server side problem
**
**		These roughly correspond to the "Type" and "Code" class
**		field in the CoAP header.
**
**		XXX We may still want to play with these allocations,
**			depending on how dense the various spaces become.  I
**			suspect that "acknowledged command" will have the
**			most values and the ack/naks won't have very many.
**			Remember in particular that the commands have to include
**			the commands between gdpds for things like migration,
**			resource negotiation, etc.
**
**		XXX CoAP has two "sequence numbers": a message-id which
**			relates ack/naks to commands and a "token" which is
**			a higher level construct relating (for example)
**			subscribe requests to results.	The "rid" represents
**			a shorter version of the "token".  We don't include
**			"seq" since this is a lower-level concept that is
**			subsumed by TCP.
**
**		The structure of an on-the-wire PDU is (showing length & offset):
**			1	0	protocol version and format
**			1	1	time to live (in hops)
**			1	2	reserved
**			1	3	command or ack/nak
**			32	4	destination address
**			32	36	source address
**			4	68	request id
**			2	72	signature length & digest algorithm
**			1	74	optionals length (in 32 bit words)
**			1	75	flags (indicate presence/lack of optional fields)
**			4	76	length of data portion
**			[8	__	record number (optional)]
**			[8	__	sequence number (optional)]
**			[16	__	commit timestamp (optional)]
**			V	__	additional optional data
**			V	__	data (length indicated above)
**			V	__	signature (length indicated above)
**		The structure shown below is the in-memory version and does
**		not correspond 1::1 to the on-wire format.
*/

typedef uint64_t		gdp_seqno_t;	// protocol sequence number

#define	PRIgdp_seqno	PRId64

typedef struct gdp_pdu
{
	// metadata
	TAILQ_ENTRY(gdp_pdu)	list;		// work list
	gdp_chan_t				*chan;		// I/O channel for this PDU entry
	bool					inuse:1;	// indicates that this is allocated

	// PDU data
	uint8_t				ver;		//  0 protocol version and format
	uint8_t				ttl;		//  1 time to live
	uint8_t				rsvd1;		//  2 reserved
	uint8_t				cmd;		//  3 command or ack/nak
	gdp_name_t			dst;		//  4 destination address
	gdp_name_t			src;		// 36 source address
	gdp_rid_t			rid;		// 68 request id (GDP_PDU_NO_RID => none)
	uint8_t				olen;		// 74 optionals length (in 32-bit words)
	uint8_t				flags;		// 75 see below
	gdp_seqno_t			seqno;		// ?? sequence number (XXX used?)

	// data length, recno, timestamp, signature, and data are in the datum
	gdp_datum_t			*datum;		// pointer to data record
} gdp_pdu_t;


/***** values for gdp_pdu_t flags field *****/
#define GDP_PDU_HAS_RECNO	0x02		// has a recno field
#define GDP_PDU_HAS_SEQNO	0x04		// has a seqno field
#define GDP_PDU_HAS_TS		0x08		// has a timestamp field

/***** dummy values for other fields *****/
#define GDP_PDU_NO_RID		UINT32_C(0)		// no request id
#define GDP_PDU_NO_RECNO	UINT64_C(-1)	// no record number

/***** manifest constants *****/

// size of fixed size part of header
// (ver, ttl, rsvd, cmd, dst, src, rid, sigalg, siglen, olen, flags, dlen)
#define _GDP_PDU_FIXEDHDRSZ		(1+1+1+1+32+32+4+1+1+1+1+4)

//* maximum size of options portion
#define _GDP_PDU_MAXOPTSZ		(255 * 4)

// maximum size of an on-wire header (excluding data and signature)
#define _GDP_PDU_MAXHDRSZ		(_GDP_PDU_FIXEDHDRSZ + _GDP_PDU_MAXOPTSZ)


/***** commands *****/

// functions to determine characteristics of command/ack/nak
#define GDP_CMD_NEEDS_ACK(c)	(((c) & 0xc0) == 0x40)	// expect ACK/NAK
#define GDP_CMD_IS_COMMAND(c)	(((c) & 0x80) != 0x80)	// is a command


/*
**  Protocol command values
**
**		The ACK and NAK values are tightly coupled with EP_STAT codes
**		and with COAP status codes, hence the somewhat baroque approach
**		here.
*/

#define _GDP_ACK_FROM_CODE(c)	(_GDP_CCODE_##c - 200 + GDP_ACK_MIN)
#define _GDP_NAK_C_FROM_CODE(c)	(_GDP_CCODE_##c - 400 + GDP_NAK_C_MIN)
#define _GDP_NAK_S_FROM_CODE(c)	(_GDP_CCODE_##c - 500 + GDP_NAK_S_MIN)
#define _GDP_NAK_R_FROM_CODE(c)	(_GDP_CCODE_##c - 600 + GDP_NAK_R_MIN)

//		0-63			Blind commands
#define GDP_CMD_KEEPALIVE		0			// used for keepalives
#define GDP_CMD_ADVERTISE		1			// advertise known GCLs
#define GDP_CMD_WITHDRAW		2			// withdraw advertisment
//		64-127			Acknowledged commands
#define GDP_CMD_PING			64			// test connection/subscription
#define GDP_CMD_HELLO			65			// initial startup/handshake
#define GDP_CMD_CREATE			66			// create a GCL
#define GDP_CMD_OPEN_AO			67			// open a GCL for append-only
#define GDP_CMD_OPEN_RO			68			// open a GCL for read-only
#define GDP_CMD_CLOSE			69			// close a GCL
#define GDP_CMD_READ			70			// read a given record by index
#define GDP_CMD_APPEND			71			// append a record
#define GDP_CMD_SUBSCRIBE		72			// subscribe to a GCL
#define GDP_CMD_MULTIREAD		73			// read more than one records
#define GDP_CMD_GETMETADATA		74			// fetch metadata
#define GDP_CMD_OPEN_RA			75			// open a GCL for read or append
#define GDP_CMD_NEWSEGMENT		76			// create a new segment for a log
#define GDP_CMD_FWD_APPEND		77			// forward (replicate) APPEND
//		128-191			Positive acks (HTTP 200-263)
#define GDP_ACK_MIN			128			// minimum ack code
#define GDP_ACK_SUCCESS			_GDP_ACK_FROM_CODE(SUCCESS)				// 128
#define GDP_ACK_CREATED			_GDP_ACK_FROM_CODE(CREATED)				// 129
#define GDP_ACK_DELETED			_GDP_ACK_FROM_CODE(DELETED)				// 130
#define GDP_ACK_VALID			_GDP_ACK_FROM_CODE(VALID)				// 131
#define GDP_ACK_CHANGED			_GDP_ACK_FROM_CODE(CHANGED)				// 132
#define GDP_ACK_CONTENT			_GDP_ACK_FROM_CODE(CONTENT)				// 133
#define GDP_ACK_MAX			191			// maximum ack code
//		192-223			Negative acks, client side (CoAP, HTTP 400-431)
#define GDP_NAK_C_MIN		192			// minimum client-side nak code
#define GDP_NAK_C_BADREQ		_GDP_NAK_C_FROM_CODE(BADREQ)			// 192
#define GDP_NAK_C_UNAUTH		_GDP_NAK_C_FROM_CODE(UNAUTH)			// 193
#define GDP_NAK_C_BADOPT		_GDP_NAK_C_FROM_CODE(BADOPT)			// 194
#define GDP_NAK_C_FORBIDDEN		_GDP_NAK_C_FROM_CODE(FORBIDDEN)			// 195
#define GDP_NAK_C_NOTFOUND		_GDP_NAK_C_FROM_CODE(NOTFOUND)			// 196
#define GDP_NAK_C_METHNOTALLOWED _GDP_NAK_C_FROM_CODE(METHNOTALLOWED)	// 197
#define GDP_NAK_C_NOTACCEPTABLE _GDP_NAK_C_FROM_CODE(NOTACCEPTABLE)		// 198
#define GDP_NAK_C_CONFLICT		_GDP_NAK_C_FROM_CODE(CONFLICT)			// 201
#define GDP_NAK_C_GONE			_GDP_NAK_C_FROM_CODE(GONE)				// 202
#define GDP_NAK_C_PRECONFAILED	_GDP_NAK_C_FROM_CODE(PRECONFAILED)		// 204
#define GDP_NAK_C_TOOLARGE		_GDP_NAK_C_FROM_CODE(TOOLARGE)			// 205
#define GDP_NAK_C_UNSUPMEDIA	_GDP_NAK_C_FROM_CODE(UNSUPMEDIA)		// 207
#define GDP_NAK_C_REC_DUP		_GDP_NAK_C_FROM_CODE(DUP_RECORD)		// 223
#define GDP_NAK_C_MAX		223			// maximum client-side nak code
//		224-239			Negative acks, server side (CoAP, HTTP 500-515)
#define GDP_NAK_S_MIN		224			// minimum server-side nak code
#define GDP_NAK_S_INTERNAL		_GDP_NAK_S_FROM_CODE(INTERNAL)			// 224
#define GDP_NAK_S_NOTIMPL		_GDP_NAK_S_FROM_CODE(NOTIMPL)			// 225
#define GDP_NAK_S_BADGATEWAY	_GDP_NAK_S_FROM_CODE(BADGATEWAY)		// 226
#define GDP_NAK_S_SVCUNAVAIL	_GDP_NAK_S_FROM_CODE(SVCUNAVAIL)		// 227
#define GDP_NAK_S_GWTIMEOUT		_GDP_NAK_S_FROM_CODE(GWTIMEOUT)			// 228
#define GDP_NAK_S_PROXYNOTSUP	_GDP_NAK_S_FROM_CODE(PROXYNOTSUP)		// 229
#define GDP_NAK_S_LOSTSUB		_GDP_NAK_S_FROM_CODE(LOST_SUBSCR)		// 239
#define GDP_NAK_S_MAX		239			// maximum server-side nak code
//		240-254			Negative acks, routing layer
#define GDP_NAK_R_MIN		240			// minimum routing layer nak code
#define GDP_NAK_R_NOROUTE		_GDP_NAK_R_FROM_CODE(NOROUTE)			// 240
#define GDP_NAK_R_MAX		254			// maximum routing layer nak code
//		255				Reserved


gdp_pdu_t	*_gdp_pdu_new(void);		// allocate a new PDU

void		_gdp_pdu_free(gdp_pdu_t *);	// free a PDU

EP_STAT		_gdp_pdu_out(				// send a PDU to a network buffer
				gdp_pdu_t *,			// the PDU information
				gdp_chan_t *,			// the network channel
				EP_CRYPTO_MD *);		// the crypto context for signing

void		_gdp_pdu_out_hard(			// send a PDU to a network buffer
				gdp_pdu_t *,			// the PDU information
				gdp_chan_t *,			// the network channel
				EP_CRYPTO_MD *);		// the crypto context for signing

EP_STAT		_gdp_pdu_hdr_in(			// read a PDU from a network buffer
				gdp_pdu_t *,			// the buffer to store the result
				gdp_chan_t *,			// the network channel
				size_t *pdu_sz_p,		// store the size of the pdu
				uint64_t *dlen_p);		// store the size of the data


EP_STAT		_gdp_pdu_in(				// read a PDU from a network buffer
				gdp_pdu_t *,			// the buffer to store the result
				gdp_chan_t *);			// the network channel

void		_gdp_pdu_dump(
				gdp_pdu_t *pdu,
				FILE *fp);

void		_gdp_pdu_process(
				gdp_pdu_t *pdu,
				gdp_chan_t *chan);

// generic sockaddr union	XXX does this belong in this header file?
union sockaddr_xx
{
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6 sin6;
};

#endif // _GDP_PDU_H_
