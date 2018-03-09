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

#ifndef _GDPLOGD_DISKLOG_H_
#define _GDPLOGD_DISKLOG_H_		1

#include "logd.h"
#if GDP_USE_TIDX
# include <db.h>
#endif

/*
**	Headers for the physical log implementation.
**		This is how bytes are actually laid out on the disk.
**		This module is private to the physical layer, but it is
**			used by apps/log-view since that needs to crack the
**			physical layout.
*/

// default directory for GCL storage
#define GCL_DIR				"/var/swarm/gdp/gcls"

// magic numbers and versions for on-disk structures
#define GCL_LDF_MAGIC		UINT32_C(0x47434C31)	// 'GCL1'
#define GCL_LDF_VERSION		UINT32_C(20151001)		// on-disk version
#define GCL_LDF_MINVERS		UINT32_C(20151001)		// lowest readable version
#define GCL_LDF_MAXVERS		UINT32_C(20151001)		// highest readable version
#define GCL_LDF_SUFFIX		".gdplog"

#define GCL_RIDX_MAGIC		UINT32_C(0x47434C78)	// 'GCLx'
#define GCL_RIDX_VERSION	UINT32_C(20160101)		// on-disk version
#define GCL_RIDX_MINVERS	UINT32_C(20160101)		// lowest readable version
#define GCL_RIDX_MAXVERS	UINT32_C(20160101)		// highest readable version
#define GCL_RIDX_SUFFIX		".gdpndx"

#if GDP_USE_TIDX
#define GCL_TIDX_SUFFIX		".gdptidx"
#endif

#define GCL_READ_BUFFER_SIZE 4096			// size of I/O buffers


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

// a data segment file header (on disk)
typedef struct
{
	uint32_t	magic;			// GCL_LDF_MAGIC
	uint32_t	version;		// GCL_LDF_VERSION
	uint32_t	header_size; 	// the total size of the header such that
								// the data records begin at offset header_size
	uint32_t	reserved1;		// reserved for future use
	uint16_t	n_md_entries;	// number of metadata entries
	uint16_t	log_type;		// directory, indirect, data, etc. (unused)
	uint32_t	segment;		// segment number
	uint64_t	reserved2;		// reserved for future use
	gdp_name_t	gname;			// the name of this log
	gdp_recno_t	recno_offset;	// first recno stored in this segment - 1
} segment_header_t;

// an individual record header in a segment (on disk)
typedef struct
{
	gdp_recno_t		recno;
	EP_TIME_SPEC	timestamp;
	uint16_t		sigmeta;			// signature metadata (size & hash alg)
	uint16_t		flags;				// flag bits (future use)
	uint8_t			hashalgs;			// algorithms for chain and data hashes
	uint8_t			reserved1;			// reserved for future use
	int16_t			reserved2;			// reserved for future use
	int32_t			reserved3;			// reserved for future use
	int32_t			data_length;		// in bytes
} segment_record_t;


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
**  On-disk recno index record format
**
**		Currently the recno index consists of (essentially) an array of fixed
**		size entries.  The index header contains the record offset (i.e.,
**		the lowest record number that can be accessed).
**
**		The index covers the entirety of the log (i.e., not just one
**		segment), so any records numbered below that first record number
**		are inaccessible at any location.  Each index entry contains the
**		segment number in which the record occurs and the offset into that
**		segment file.  In theory records could be interleaved between
**		segments, but that's not how things work now.
**
**		At some point it may be that the local server doesn't have all
**		the segments for a given log.  Figuring out the physical location
**		of a segment from the index is not addressed here.
**		(The current implementation assumes that all segments are local.)
**
**		The index is not intended to have unique information.  Given the
**		set of segment files, it should be possible to rebuild the index.
*/

typedef struct ridx_entry
{
	gdp_recno_t	recno;			// record number
	int64_t		offset;			// offset into segment file
	uint32_t	segment;		// id of segment (for segmented logs)
	uint32_t	reserved;		// make padding explicit
} ridx_entry_t;

typedef struct ridx_header
{
	uint32_t	magic;			// GCL_RIDX_MAGIC
	uint32_t	version;		// GCL_RIDX_VERSION
	uint32_t	header_size;	// offset to first index entry
	uint32_t	reserved1;		// must be zero
	gdp_recno_t	min_recno;		// the first record number in the log
} ridx_header_t;

#define SIZEOF_RIDX_HEADER		(sizeof(ridx_header_t))
#define SIZEOF_RIDX_RECORD		(sizeof(ridx_entry_t))


#if GDP_USE_TIDX
/*
**  On-disk timestamp index record format
**
**		This uses a Berkeley DB btree; hence, there is no header.
**		It is essentially a secondary index mapping timestamp to
**			recno (the primary key).
**		An alternative implementation could map the timestamp to
**			a ridx_entry_t, thereby eliminating the need to read
**			the ridx file at the cost of using more space.
*/

struct ts_index
{
	DB				*db;			// the Berkeley DB (btree) handle
};

typedef struct tidx_key
{
	int64_t			sec;			// seconds since Jan 1, 1970
	int32_t			nsec;			// nanoseconds
} tidx_key_t;

typedef struct tidx_value
{
	gdp_recno_t		recno;			// the primary key
} tidx_value_t;

#endif // GDP_USE_TIDX

/*
**  The in-memory cache of the physical index data.
**
**		This doesn't necessarily cover the entire index if the index
**		gets large.
**
**		This is currently a circular buffer, but that's probably
**		not the best representation since we could potentially have
**		multiple readers accessing wildly different parts of the
**		cache.  Applications trying to do historic summaries will
**		be particularly problematic.
*/

//typedef struct
//{
//	size_t				max_size;			// number of entries in data
//	size_t				current_size;		// number of filled entries in data
//	ridx_entry_t		data[];
//} ridx_cache_t;


/*
**  The in-memory representation of the on-disk log index (by recno).
**
**		Note that index.min_recno need not be the same as phys->min_recno
**		because the index may include entries for segments that have been
**		expired.
*/

struct recno_index
{
	// information about on-disk format
	FILE				*fp;					// recno -> offset file handle
	int64_t				max_offset;				// size of index file
	size_t				header_size;			// size of hdr in index file
	gdp_recno_t			min_recno;				// lowest recno in index

	// a cache of the contents
//	ridx_cache_t		cache;					// in-memory cache
};


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

	// info regarding the segment files
	uint32_t			nsegments;				// number of segments
	uint32_t			last_segment;			// segment being written
	segment_t			**segments;				// list of segment pointers
												// can be dynamically expanded

	// info regarding the indices
	struct recno_index	ridx;					// index by recno
#if GDP_USE_TIDX
	struct ts_index		tidx;					// index by timestamp
#endif
};

#define LOG_TIDX_HIDEFAILURE	0x00000001	// abandon a corrupt tidx database
#define LOG_POSIX_ERRORS		0x00000002	// send posix errors to syslog

#endif //_GDPLOGD_DISKLOG_H_
