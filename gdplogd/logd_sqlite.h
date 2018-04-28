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

#ifndef _GDPLOGD_SQLITE_H_
#define _GDPLOGD_SQLITE_H_		1

#include "logd.h"
#include <sqlite3.h>

/*
**	Headers for the physical log implementation.
**		This is how bytes are actually laid out on the disk.
**		This module is private to the physical layer, but it is
**			used by apps/log-view since that needs to crack the
**			physical layout.
*/

// default directory for GDP Log storage
#define GDP_LOG_DIR			"/var/swarm/gdp/glogs"

// magic numbers and versions for on-disk database
#define GLOG_MAGIC			UINT32_C(0x47434C30)	// 'GCL0'
#define GLOG_VERSION		UINT32_C(20180428)		// current version
#define GLOG_MINVERS		UINT32_C(20180428)		// lowest readable version
#define GLOG_MAXVERS		UINT32_C(20180428)		// highest readable version
#define GLOG_SUFFIX			".glog"

#define GLOG_READ_BUFFER_SIZE	4096			// size of I/O buffers


/*
**  On-disk data format
**
**		Data in logs are stored in individual segment files, each of which
**		stores a contiguous series of records.  These definitions really
**		apply to individual segments, not the log as a whole.
**
**		TODO:	Is the metadata copied in each segment, or is it just in
**		TODO:	segment 0?  Should there be a single file representing
**		TODO:	the log as a whole, but contains no data, only metadata,
**		TODO:	kind of like a superblock?
**
**		TODO:	It probably makes sense to have a cache file that stores
**		TODO:	dynamic data about a log (e.g., how many records it has).
**		TODO:	Since this file is written a lot, it is important that it
**		TODO:	can be reconstructed.
**
**		Each segment has a fixed length segment header (called here a log
**		header), followed by the log metadata, followed by a series of
**		data records.  Each record has a header followed by data.
**
**		The GCL metadata consists of n_md_entries (N)
**		descriptors, each of which has a uint32_t "name" and a
**		uint32_t length.  In other words, the descriptors are an
**		an array of names and lengths: (n1, l1, n2, l2 ... nN, lN).
**		The descriptors are followed by the actual metadata content.
**
**		Some of the metadata names are reserved for internal use (e.g.,
**		storage of the public key associated with the log), but
**		other metadata names can be added that will be interpreted
**		by applications.
**
**		Following the data header there can be several optional fields:
**			chash --- the hash of the previous record ("chain hash")
**			dhash --- the hash of the data
**			data --- the actual data
**			sig --- the signature
**		chash and/or dhash have lengths implied by the hashalgs field.
**		The data length is explicit.
**		The signature length (and hash algorithm) is encoded in sigmeta.
**
**		The extra reserved fields in the record header aren't
**		anticipated to be needed anytime soon; they are a relic
**		of earlier implementations, and are here to keep the
**		record header from changing.  Also, I don't trust the C
**		compilers to keep padding consistent between implementations,
**		so this ensures that the disk layout won't change.
*/



/*
**  In-Memory representation of per-segment info
**
**		This only includes the information that may (or does) vary
**		on a per-segment basis.  For example, different segments might
**		have different versions or header sizes, but not different
**		metadata, which must be fixed per log (even though on disk
**		that information is actually replicated in each segment.
*/

typedef struct
{
	FILE				*fp;				// file pointer to segment
	uint32_t			ver;				// on-disk file version
	uint32_t			segno;				// segment number
	size_t				header_size;		// size of segment file hdr
	gdp_recno_t			recno_offset;		// first recno in segment - 1
	off_t				max_offset;			// size of segment file
	EP_TIME_SPEC		retain_until;		// retain at least until this date
	EP_TIME_SPEC		remove_by;			// must be gone by this date
} segment_t;


/*
**  Per-log info.
**
**		There is no single instantiation of a log, so this is really
**		a representation of an abstraction.  It includes information
**		about all segments and the index.
*/

struct physinfo
{
	// reading and writing to the log requires holding this lock
	EP_THR_RWLOCK		lock;

	// info regarding the entire log (not segment)
	gdp_recno_t			min_recno;				// first recno in log
	gdp_recno_t			max_recno;				// last recno in log (dynamic)
	uint32_t			flags;					// see below
	int32_t				ver;					// database version

	// the underlying SQLite database
	struct sqlite3		*db;					// database handle

	// cache of prepared statements
	struct sqlite3_stmt	*insert_stmt;
	struct sqlite3_stmt	*read_by_hash_stmt;
	struct sqlite3_stmt	*read_by_recno_stmt;
	struct sqlite3_stmt	*read_by_timestamp_stmt;
};

#define LOG_POSIX_ERRORS		0x00000002	// send posix errors to syslog

#endif //_GDPLOGD_SQLITE_H_
