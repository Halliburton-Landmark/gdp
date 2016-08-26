/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	Status codes to string mappings
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

#include "gdp.h"
#include "gdp_priv.h"
#include "gdp_pdu.h"
#include "gdp_stat.h"

#include <ep/ep_dbg.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp_stat", "GDP status codes");

/* TODO:	Verify that these strings are accurate.  */

static struct ep_stat_to_string Stats[] =
{
	// module name
	{ GDP_STAT_NEW(OK, 0),				"Swarm-GDP",					},

	// individual codes
	{ GDP_STAT_MSGFMT,					"message format error",			},
	{ GDP_STAT_SHORTMSG,				"incomplete message",			},
	{ GDP_STAT_READ_OVERFLOW,			"read overflow",				},
	{ GDP_STAT_NOT_IMPLEMENTED,			"not implemented",				},
	{ GDP_STAT_PDU_WRITE_FAIL,			"network pdu write failure",	},
	{ GDP_STAT_PDU_READ_FAIL,			"network pdu read failure",		},
	{ GDP_STAT_PDU_VERSION_MISMATCH,	"protocol version mismatch",	},
	{ GDP_STAT_PDU_NO_SEQ,				"no sequence number",			},
	{ GDP_STAT_KEEP_READING,			"more input needed",			},
	{ GDP_STAT_NOT_OPEN,				"GCL is not open",				},
	{ GDP_STAT_UNKNOWN_RID,				"request id unknown",			},
	{ GDP_STAT_INTERNAL_ERROR,			"GDP internal error",			},
	{ GDP_STAT_BAD_IOMODE,				"GDP bad I/O mode",				},
	{ GDP_STAT_GCL_NAME_INVALID,		"invalid GCL name",				},
	{ GDP_STAT_BUFFER_FAILURE,			"gdp_buf I/O failure",			},
	{ GDP_STAT_NULL_GCL,				"GCL name required",			},
	{ GDP_STAT_PROTOCOL_FAIL,			"GDP protocol failure",			},
	{ GDP_STAT_CORRUPT_INDEX,			"corrupt GCL index",			},
	{ GDP_STAT_CORRUPT_GCL,				"corrupt GCL data file",		},
	{ GDP_STAT_DEAD_DAEMON,				"lost connection to GDP",		},
	{ GDP_STAT_GCL_VERSION_MISMATCH,	"gcl version mismatch",			},
	{ GDP_STAT_READONLY,				"cannot update read-only object",	},
	{ GDP_STAT_NOTFOUND,				"cannot find requested object",		},
	{ GDP_STAT_PDU_CORRUPT,				"corrupt pdu",						},
	{ GDP_STAT_SKEY_REQUIRED,			"secret key required",				},
	{ GDP_STAT_GCL_READ_ERROR,			"GCL read error",					},
	{ GDP_STAT_RECNO_SEQ_ERROR,			"record out of sequence",			},
	{ GDP_STAT_CRYPTO_SIGFAIL,			"signature failure",				},
	{ GDP_STAT_PHYSIO_ERROR,            "physical I/O error on log",        },
	{ GDP_STAT_RECORD_EXPIRED,			"record expired",					},
	{ GDP_STAT_DEAD_REQ,				"request freed while in use",		},
	{ GDP_STAT_BAD_REFCNT,				"invalid reference count",			},
	{ GDP_STAT_RECORD_MISSING,			"record missing (look elsewhere)",	},
	{ GDP_STAT_RECORD_DUPLICATED,		"record duplicated",				},

	{ GDP_STAT_NAK_BADREQ,				"400 bad request",					},
	{ GDP_STAT_NAK_UNAUTH,				"401 unauthorized",					},
	{ GDP_STAT_NAK_BADOPT,				"402 bad option",					},
	{ GDP_STAT_NAK_FORBIDDEN,			"403 forbidden",					},
	{ GDP_STAT_NAK_NOTFOUND,			"404 not found",					},
	{ GDP_STAT_NAK_METHNOTALLOWED,		"405 method not allowed",			},
	{ GDP_STAT_NAK_NOTACCEPTABLE,		"406 not acceptable",				},
	{ GDP_STAT_NAK_CONFLICT,			"409 conflict",						},
	{ GDP_STAT_NAK_GONE,				"410 gone",							},
	{ GDP_STAT_NAK_PRECONFAILED,		"412 precondition failed",			},
	{ GDP_STAT_NAK_TOOLARGE,			"413 request entity too large",		},
	{ GDP_STAT_NAK_UNSUPMEDIA,			"415 unsupported media type",		},
	{ GDP_STAT_NAK_DUP_RECORD,			"431 duplicate record",				},

	{ GDP_STAT_NAK_INTERNAL,			"500 internal server error",		},
	{ GDP_STAT_NAK_NOTIMPL,				"501 not implemented",				},
	{ GDP_STAT_NAK_BADGATEWAY,			"502 bad gateway",					},
	{ GDP_STAT_NAK_SVCUNAVAIL,			"503 service unavailable",			},
	{ GDP_STAT_NAK_GWTIMEOUT,			"504 gateway timeout",				},
	{ GDP_STAT_NAK_PROXYNOTSUP,			"505 proxying not supported",		},
	{ GDP_STAT_NAK_REPLICATE_FAIL,		"514 couldn't write W replicas",	},
	{ GDP_STAT_NAK_LOST_SUBSCR,			"515 lost subscription",			},

	{ GDP_STAT_NAK_NOROUTE,				"600 no route available",			},

	// end of list sentinel
	{ EP_STAT_OK,						NULL							},
};

void
_gdp_stat_init(void)
{
	ep_stat_reg_strings(Stats);
}


/*
**   Convert an ACK or NAK command code into an EP_STAT
*/

EP_STAT
_gdp_stat_from_acknak(int acknak)
{
	if (acknak >= GDP_ACK_MIN && acknak <= GDP_ACK_MAX)
		return GDP_STAT_FROM_ACK(acknak);
	if (acknak >= GDP_NAK_C_MIN && acknak <= GDP_NAK_C_MAX)
		return GDP_STAT_FROM_C_NAK(acknak);
	if (acknak >= GDP_NAK_S_MIN && acknak <= GDP_NAK_S_MAX)
		return GDP_STAT_FROM_S_NAK(acknak);
	if (acknak >= GDP_NAK_R_MIN && acknak <= GDP_NAK_R_MAX)
		return GDP_STAT_FROM_R_NAK(acknak);

	ep_dbg_cprintf(Dbg, 1, "_gdp_stat_from_acknak: code %d not an acknak\n",
			acknak);
	return GDP_STAT_INTERNAL_ERROR;
}
