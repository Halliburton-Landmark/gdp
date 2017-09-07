/* vim: set ai sw=4 sts=4 ts=4 : */

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
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
**  ----- END LICENSE BLOCK -----
***********************************************************************/

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_net.h>
#include <ep/ep_prflags.h>
#include <ep/ep_string.h>
#include <ep/ep_time.h>
#include <ep/ep_xlate.h>
#include <gdp/gdp.h>

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <sys/stat.h>

// following are actually private definitions
#include <gdp/gdp_priv.h>
#include <gdplogd/logd_disklog.h>


/*
**  LOG-VIEW --- display raw on-disk storage
**
**		Not for user consumption.
**		This does peek into private header files.
*/

static EP_DBG	Dbg = EP_DBG_INIT("log-view", "Dump GDP logs for debugging");


#define CHECK_FILE_OFFSET	check_file_offset

#define LIST_NO_METADATA		0x00000001	// only list logs with no metadata

void
check_file_offset(FILE *fp, long offset)
{
	if (ftell(fp) != offset)
	{
		printf("%sWARNING: file offset error (actual %ld, expected %ld)%s\n",
				EpVid->vidfgred, ftell(fp), offset, EpVid->vidnorm);
	}
}


int
show_metadata(int nmds, FILE *dfp, size_t *foffp, int plev)
{
	int i;
	struct mdhdr
	{
		uint32_t md_id;
		uint32_t md_len;
	};
	struct mdhdr *mdhdrs = alloca(nmds * sizeof *mdhdrs);

	if (plev > 0)
		printf("    --------------- Metadata ---------------\n");

	i = fread(mdhdrs, sizeof *mdhdrs, nmds, dfp);
	if (i != nmds)
	{
		fprintf(stderr,
				"fread() failed while reading metadata headers,"
				" ferror = %d, wanted %d, got %d\n",
				ferror(dfp), nmds, i);
		return EX_DATAERR;
	}

	if (plev >= 4)
	{
		ep_hexdump(mdhdrs, nmds * sizeof *mdhdrs,
				stdout, EP_HEXDUMP_ASCII, *foffp);
	}

	*foffp += nmds * sizeof *mdhdrs;
	CHECK_FILE_OFFSET(dfp, *foffp);

	for (i = 0; i < nmds; ++i)
	{
		uint8_t *mdata;

		mdhdrs[i].md_id = ep_net_ntoh32(mdhdrs[i].md_id);
		mdhdrs[i].md_len = ep_net_ntoh32(mdhdrs[i].md_len);

		mdata = ep_mem_malloc(mdhdrs[i].md_len + 1);
											// +1 for null-terminator
		if (fread(mdata, mdhdrs[i].md_len, 1, dfp) != 1)
		{
			fprintf(stderr,
					"fread() failed while reading metadata string,"
					" ferror = %d\n",
					ferror(dfp));
			return EX_DATAERR;
		}
		mdata[mdhdrs[i].md_len] = '\0';
		*foffp += mdhdrs[i].md_len;
		CHECK_FILE_OFFSET(dfp, *foffp);
		if (plev > 1)
		{
			fprintf(stdout,
					"\nMetadata entry %d: name 0x%08" PRIx32
					", len %" PRId32,
					i, mdhdrs[i].md_id, mdhdrs[i].md_len);
			switch (mdhdrs[i].md_id)
			{
				case GDP_GCLMD_XID:
					printf(" (external id)\n    %s\n", mdata);
					break;

				case GDP_GCLMD_CTIME:
					printf(" (creation time)\n    %s\n", mdata);
					break;

				case GDP_GCLMD_CID:
					printf(" (creator)\n    %s\n", mdata);
					break;

				case GDP_GCLMD_PUBKEY:
					printf(" (public key)\n");
					int keylen = mdata[2] << 8 | mdata[3];
					printf("\tmd_alg %s (%d), keytype %s (%d), keylen %d\n",
							ep_crypto_md_alg_name(mdata[0]), mdata[0],
							ep_crypto_keytype_name(mdata[1]), mdata[1],
							keylen);
					if (plev > 1)
					{
						EP_CRYPTO_KEY *key;

						key = ep_crypto_key_read_mem(mdata + 4,
								mdhdrs[i].md_len - 4,
								EP_CRYPTO_KEYFORM_DER,
								EP_CRYPTO_F_PUBLIC);
						ep_crypto_key_print(key, stdout, EP_CRYPTO_F_PUBLIC);
						ep_crypto_key_free(key);
					}
					if (plev >= 4)
						ep_hexdump(mdata + 4, mdhdrs[i].md_len - 4,
								stdout, EP_HEXDUMP_HEX, 0);
					continue;
				default:
					printf("\n");
					break;
			}
			if (plev < 3)
			{
				printf("\t%s", EpChar->lquote);
				ep_xlate_out(mdata, mdhdrs[i].md_len,
						stdout, "", EP_XLATE_PLUS | EP_XLATE_NPRINT);
				fprintf(stdout, "%s\n", EpChar->rquote);
			}
		}
		else if (mdhdrs[i].md_id == GDP_GCLMD_XID)
		{
			fprintf(stdout, "\tExternal name: %s\n", mdata);
		}
		else if (mdhdrs[i].md_id == GDP_GCLMD_CID)
		{
			fprintf(stdout, "\tCreator:       %s\n", mdata);
		}
		else if (mdhdrs[i].md_id == GDP_GCLMD_CTIME)
		{
			fprintf(stdout, "\tCreation Time: %s\n", mdata);
		}

		if (plev >= 4)
		{
			ep_hexdump(mdata, mdhdrs[i].md_len,
					stdout, EP_HEXDUMP_ASCII, *foffp);
		}
		ep_mem_free(mdata);
	}
	return EX_OK;
}


EP_PRFLAGS_DESC	RecordFlags[] =
{
	{ 0,					0,					NULL				}
};


int
show_record(segment_record_t *rec, FILE *dfp, size_t *foffp, int plev)
{
	rec->recno = ep_net_ntoh64(rec->recno);
	ep_net_ntoh_timespec(&rec->timestamp);
	rec->sigmeta = ep_net_ntoh16(rec->sigmeta);
	rec->flags = ep_net_ntoh16(rec->flags);
	rec->data_length = ep_net_ntoh32(rec->data_length);

	fprintf(stdout, "\n    Recno %" PRIgdp_recno
			", offset %zd (0x%zx), dlen %" PRIi32
			", sigmeta %x (mdalg %d, len %d)\n",
			rec->recno, *foffp, *foffp, rec->data_length, rec->sigmeta,
			(rec->sigmeta >> 12) & 0x000f, rec->sigmeta & 0x0fff);
	fprintf(stdout, "\thashalgs %x, flags ", rec->hashalgs);
	ep_prflags(rec->flags, RecordFlags, stdout);
	fprintf(stdout, "\n\tTimestamp ");
	ep_time_print(&rec->timestamp, stdout, EP_TIME_FMT_HUMAN);
	fprintf(stdout, " (sec %" PRIi64 ")\n", rec->timestamp.tv_sec);

	if (plev >= 4)
	{
		ep_hexdump(&*rec, sizeof *rec, stdout, EP_HEXDUMP_HEX, *foffp);
	}
	*foffp += sizeof *rec;
	CHECK_FILE_OFFSET(dfp, *foffp);

	if (rec->data_length > 0)
	{
		char *data_buffer = ep_mem_malloc(rec->data_length);
		if (fread(data_buffer, rec->data_length, 1, dfp) != 1)
		{
			fprintf(stderr, "fread() failed while reading data @ %jd, "
							"len %" PRId32 " (%d)\n",
					(intmax_t) *foffp, rec->data_length, ferror(dfp));
			ep_mem_free(data_buffer);
			return EX_DATAERR;
		}

		if (plev >= 4)
		{
			ep_hexdump(data_buffer, rec->data_length,
					stdout, EP_HEXDUMP_ASCII,
					plev < 3 ? 0 : *foffp);
		}
		*foffp += rec->data_length;
		CHECK_FILE_OFFSET(dfp, *foffp);
		ep_mem_free(data_buffer);
	}

	// print the signature
	if ((rec->sigmeta & 0x0fff) > 0)
	{
		uint8_t sigbuf[0x1000];		// maximum size of signature
		int siglen = rec->sigmeta & 0x0fff;

		if (fread(sigbuf, siglen, 1, dfp) != 1)
		{
			fprintf(stderr, "fread() failed while reading signature (%d)\n",
					ferror(dfp));
			return EX_DATAERR;
		}

		if (plev >= 4)
		{
			ep_hexdump(sigbuf, siglen, stdout, EP_HEXDUMP_ASCII,
				plev < 3 ? 0 : *foffp);
		}

		*foffp += siglen;
		CHECK_FILE_OFFSET(dfp, *foffp);
	}

	return EX_OK;
}


FILE *
open_index(const char *ridx_filename, struct stat *st, ridx_header_t *phdr)
{
	FILE *ridx_fp = NULL;
	ridx_header_t ridx_header;

	if (stat(ridx_filename, st) != 0)
	{
		fprintf(stderr, "could not stat %s (%s)\n",
				ridx_filename, strerror(errno));
		return NULL;
	}

	ridx_fp = fopen(ridx_filename, "r");
	if (ridx_fp == NULL)
	{
		fprintf(stderr, "Could not open %s (%s)\n",
				ridx_filename, strerror(errno));
		return NULL;
	}

	if (st->st_size < SIZEOF_RIDX_HEADER)
	{
		phdr->magic = 0;
		return ridx_fp;
	}
	if (fread(&ridx_header, sizeof ridx_header, 1, ridx_fp) != 1)
	{
		fprintf(stderr, "Could not read hdr for %s (%s)\n",
				ridx_filename, strerror(errno));
	}
	else
	{
		phdr->magic = ep_net_ntoh32(ridx_header.magic);
		phdr->version = ep_net_ntoh32(ridx_header.version);
		phdr->header_size = ep_net_ntoh32(ridx_header.header_size);
		phdr->reserved1 = ep_net_ntoh32(ridx_header.reserved1);
		phdr->min_recno = ep_net_ntoh64(ridx_header.min_recno);
	}

	return ridx_fp;
}


gdp_recno_t
show_ridx_header(const char *ridx_filename,
		int plev,
		int *min_segment,
		int *max_segment)
{
	struct stat st;
	ridx_entry_t xent;
	ridx_header_t ridx_header;
	FILE *ridx_fp = open_index(ridx_filename, &st, &ridx_header);

	*min_segment = 0;
	*max_segment = 0;

	if (ridx_fp == NULL)
		return -1;

	if (ridx_header.magic == 0)
		goto no_header;
	else if (ridx_header.magic != GCL_RIDX_MAGIC)
	{
		fprintf(stderr, "Bad index magic %04x\n", ridx_header.magic);
	}

	// get info from the first record
	if (st.st_size <= ridx_header.header_size)
	{
		// no records yet
		if (plev > 1)
			printf("\tno index records\n");
		goto done;
	}
	else if (fseek(ridx_fp, ridx_header.header_size, SEEK_SET) < 0)
	{
		printf("show_ridx_header: cannot seek (1)\n");
	}
	else if (fread(&xent, SIZEOF_RIDX_RECORD, 1, ridx_fp) != 1)
	{
		printf("show_ridx_header: fread failure (1)\n");
	}
	else
	{
		*min_segment = ep_net_ntoh32(xent.segment);
	}

	// get info from the last record
	*max_segment = *min_segment;
	if (fseek(ridx_fp, st.st_size - SIZEOF_RIDX_RECORD, SEEK_SET) < 0)
	{
		printf("show_ridx_header: cannot seek (2)\n");
	}
	else if (fread(&xent, SIZEOF_RIDX_RECORD, 1, ridx_fp) != 1)
	{
		printf("show_ridx_header: fread failure (2)\n");
	}
	else
	{
		*max_segment = ep_net_ntoh32(xent.segment);
	}

	if (plev > 1)
	{
		printf("    Index: magic=%04" PRIx32 ", vers=%" PRId32
				", header_size=%" PRId32 ", min_recno=%" PRIgdp_recno "\n",
				ridx_header.magic, ridx_header.version,
				ridx_header.header_size, ridx_header.min_recno);

		printf("\tmin_segment=%d, max_segment=%d max_recno=%" PRIgdp_recno
				"\n\toffset=%jd segment=%d reserved=%x\n",
				*min_segment,
				*max_segment,
				ep_net_ntoh64(xent.recno),
				(intmax_t) ep_net_ntoh64(xent.offset),
				ep_net_ntoh32(xent.segment),
				ep_net_ntoh32(xent.reserved));
	}
done:
	fclose(ridx_fp);
	return ((st.st_size - ridx_header.header_size) / SIZEOF_RIDX_RECORD)
				+ ridx_header.min_recno;

no_header:
	fclose(ridx_fp);
	if (plev > 1)
		printf("Old-style headerless index\n");
	return st.st_size / SIZEOF_RIDX_RECORD;
	return -1;
}


int
show_index_contents(const char *gcl_dir_name, gdp_name_t gcl_name, int plev)
{
	struct stat st;
	ridx_header_t ridx_header;
	gdp_pname_t gcl_pname;

	(void) gdp_printable_name(gcl_name, gcl_pname);

	// Add 5 in the middle for '/_xx/'
	int filename_size = strlen(gcl_dir_name) + 5 + strlen(gcl_pname) +
			strlen(GCL_RIDX_SUFFIX) + 1;
	char *ridx_filename = alloca(filename_size);
	snprintf(ridx_filename, filename_size,
			"%s/_%02x/%s%s",
			gcl_dir_name, gcl_name[0], gcl_pname, GCL_RIDX_SUFFIX);

	FILE *ridx_fp = open_index(ridx_filename, &st, &ridx_header);

	if (ridx_fp == NULL)
	{
		fprintf(stderr, "Could not open %s (%s)\n",
				ridx_filename, strerror(errno));
		return EX_NOINPUT;
	}

	printf("\n    =============== Index ===============\n");

	while (true)
	{
		ridx_entry_t ridx_entry;
		if (fread(&ridx_entry, sizeof ridx_entry, 1, ridx_fp) != 1)
			break;
		ridx_entry.recno = ep_net_ntoh64(ridx_entry.recno);
		ridx_entry.offset = ep_net_ntoh64(ridx_entry.offset);
		ridx_entry.segment = ep_net_ntoh32(ridx_entry.segment);
		ridx_entry.reserved = ep_net_ntoh32(ridx_entry.reserved);

		printf("\trecno %" PRIgdp_recno ", segment %" PRIu32
				", offset %" PRIu64 ", reserved %" PRIu32 "\n",
				ridx_entry.recno, ridx_entry.segment,
				ridx_entry.offset, ridx_entry.reserved);
	}
	fclose(ridx_fp);
	return EX_OK;
}


int
read_segment_header(FILE *logfp, segment_header_t *loghdr)
{
	if (fread(loghdr, sizeof *loghdr, 1, logfp) != 1)
	{
		fprintf(stderr, "fread() failed while reading log_header, ferror = %d\n",
				ferror(logfp));
		return EX_DATAERR;
	}
	loghdr->magic = ep_net_ntoh32(loghdr->magic);
	loghdr->version = ep_net_ntoh32(loghdr->version);
	loghdr->header_size = ep_net_ntoh32(loghdr->header_size);
	loghdr->reserved1 = ep_net_ntoh32(loghdr->reserved1);
	loghdr->n_md_entries = ep_net_ntoh16(loghdr->n_md_entries);
	loghdr->log_type = ep_net_ntoh16(loghdr->log_type);
	loghdr->segment = ep_net_ntoh32(loghdr->segment);
	loghdr->reserved2 = ep_net_ntoh64(loghdr->reserved2);
	loghdr->recno_offset = ep_net_ntoh64(loghdr->recno_offset);

	return 0;
}


int
show_segment(const char *gcl_dir_name,
		gdp_name_t gcl_name,
		int extno,
		bool shadow,
		int plev)
{
	gdp_pname_t gcl_pname;
	int istat = 0;
	char segment_str[20];

	(void) gdp_printable_name(gcl_name, gcl_pname);
	snprintf(segment_str, sizeof segment_str, "-%06d", extno);

	// Add 5 in the middle for '/_xx/'
	int filename_size = strlen(gcl_dir_name) + 5 + strlen(gcl_pname) +
			strlen(segment_str) + strlen(GCL_LDF_SUFFIX) + 1;
	char *filename = alloca(filename_size);

	snprintf(filename, filename_size,
			"%s/_%02x/%s%s%s",
			gcl_dir_name, gcl_name[0], gcl_pname, segment_str, GCL_LDF_SUFFIX);
	ep_dbg_cprintf(Dbg, 6, "Reading %s\n\n", filename);

	FILE *data_fp = fopen(filename, "r");
	if (data_fp == NULL && extno == 0)
	{
		// try again without segment
		snprintf(filename, filename_size,
				"%s/_%02x/%s%s",
				gcl_dir_name, gcl_name[0], gcl_pname, GCL_LDF_SUFFIX);
		ep_dbg_cprintf(Dbg, 6, "Reading %s\n\n", filename);
		data_fp = fopen(filename, "r");
	}
	if (data_fp == NULL)
	{
		fflush(stdout);
		if (!shadow)
			fprintf(stderr, "Could not open %s, errno = %d\n", filename, errno);
		return EX_NOINPUT;
	}

	size_t file_offset = 0;
	segment_header_t log_header;
	segment_record_t record;
	istat = read_segment_header(data_fp, &log_header);
	if (istat != 0)
		goto fail0;

	if (plev >= 1)
	{
		gdp_pname_t pname;

		printf("\n    =============== Segment %d ===============\n", extno);
		printf("\tsegment %d magic 0x%08" PRIx32
				", version %" PRIi32
				", type %" PRIi16 "\n",
				log_header.segment, log_header.magic,
				log_header.version, log_header.log_type);
		printf("\tname %s\n",
				gdp_printable_name(log_header.gname, pname));
		printf("\theader size %" PRId32 " (0x%" PRIx32 ")"
				", metadata entries %d, recno_offset %" PRIgdp_recno "\n",
				log_header.header_size, log_header.header_size,
				log_header.n_md_entries, log_header.recno_offset);
		if (plev >= 4)
		{
			ep_hexdump(&log_header, sizeof log_header, stdout,
					EP_HEXDUMP_HEX, file_offset);
		}
	}
	file_offset += sizeof log_header;
	CHECK_FILE_OFFSET(data_fp, file_offset);

	if (log_header.n_md_entries > 0)
	{
		istat = show_metadata(log_header.n_md_entries, data_fp,
					&file_offset, plev);
		if (istat != 0)
			goto fail0;
	}
	else if (plev >= 1)
	{
		fprintf(stdout, "\n<No metadata>\n");
	}

	if (plev <= 2)
		goto success;

	fprintf(stdout, "    --------------- Data ---------------\n");

	while (fread(&record, sizeof record, 1, data_fp) == 1)
	{
		istat = show_record(&record, data_fp, &file_offset, plev);
		if (istat != 0)
			break;
	}

fail0:
success:
	fclose(data_fp);
	return istat;
}


int
show_gcl(const char *gcl_dir_name, gdp_name_t gcl_name, int plev)
{
	gdp_pname_t gcl_pname;
	gdp_recno_t max_recno;
	int segment;
	int min_segment;
	int max_segment;
	int istat = 0;

	(void) gdp_printable_name(gcl_name, gcl_pname);
	if (plev <= 0)
	{
		printf("%s\n", gcl_pname);
		return 0;
	}
	printf("\nLog %s:\n", gcl_pname);

	// Add 5 in the middle for '/_xx/'
	int filename_size = strlen(gcl_dir_name) + 5 + strlen(gcl_pname) +
			strlen(GCL_RIDX_SUFFIX) + 1;
	char *filename = alloca(filename_size);

	snprintf(filename, filename_size,
			"%s/_%02x/%s%s",
			gcl_dir_name, gcl_name[0], gcl_pname, GCL_RIDX_SUFFIX);
	max_recno = show_ridx_header(filename, plev,
						&min_segment, &max_segment);
	printf("\t%" PRIgdp_recno " recs\n", max_recno - 1);

	if (plev <= 1)
	{
		plev = 0;			// arrange to get external ID only
		for (segment = min_segment; segment <= max_segment; segment++)
		{
			istat = show_segment(gcl_dir_name, gcl_name, segment, true, plev);
			if (istat != EX_NOINPUT)
				break;
		}
		return 0;
	}

	for (segment = min_segment; segment <= max_segment; segment++)
		istat = show_segment(gcl_dir_name, gcl_name, segment, false, plev);

	if (plev >= 5)
	{
		show_index_contents(gcl_dir_name, gcl_name, plev);
	}

	return istat;
}


bool
test_metadata(const char *filename, int list_flags)
{
	FILE *dfp;
	int istat;

	if (!EP_UT_BITSET(LIST_NO_METADATA, list_flags))
		return true;

	dfp = fopen(filename, "r");
	if (dfp == NULL)
		return true;

	segment_header_t seghdr;
	istat = read_segment_header(dfp, &seghdr);
	fclose(dfp);
	if (istat != 0)
		return true;

	return seghdr.n_md_entries == 0;
}


int
list_gcls(const char *gcl_dir_name, int plev, int list_flags)
{
	DIR *dir;
	int subdir;
	gdp_name_t gcl_iname;

	dir = opendir(gcl_dir_name);
	if (dir == NULL)
	{
		fprintf(stderr, "Could not open %s, errno = %d\n",
				gcl_dir_name, errno);
		return EX_NOINPUT;
	}
	closedir(dir);

	for (subdir = 0; subdir < 0x100; subdir++)
	{
		char dbuf[400];
		EP_HASH *seenhash = ep_hash_new("seenhash", NULL, 0);

		snprintf(dbuf, sizeof dbuf, "%s/_%02x", gcl_dir_name, subdir);
		dir = opendir(dbuf);
		if (dir == NULL)
			continue;

		for (;;)
		{
			struct dirent dentbuf;
			struct dirent *dent;

			// read the next directory entry
			int i = readdir_r(dir, &dentbuf, &dent);
			if (i != 0)
			{
				ep_log(ep_stat_from_errno(i),
						"list_gcls: readdir_r failed");
				break;
			}
			if (dent == NULL)
				break;

			// we're only interested in .data files
			char *p = strrchr(dent->d_name, '.');
			if (p == NULL || strcmp(p, GCL_LDF_SUFFIX) != 0)
				continue;

			// save the full pathname in case we need it
			snprintf(dbuf, sizeof dbuf, "%s/_%02x/%s",
					gcl_dir_name, subdir, dent->d_name);

			// strip off the ".data"
			*p = '\0';

			// strip off segment number if it exists
			if (strlen(dent->d_name) > GDP_GCL_PNAME_LEN &&
					dent->d_name[GDP_GCL_PNAME_LEN] == '-')
				dent->d_name[GDP_GCL_PNAME_LEN] = '\0';

			// see if we've already printed this
			if (ep_hash_insert(seenhash, GDP_GCL_PNAME_LEN, dent->d_name, "")
					!= NULL)
				continue;

			// we may want to select by metadata
			if (test_metadata(dbuf, list_flags))
			{
				// print the name
				gdp_parse_name(dent->d_name, gcl_iname);
				show_gcl(gcl_dir_name, gcl_iname, plev);
			}
		}
		closedir(dir);
		ep_hash_free(seenhash);
		seenhash = NULL;
	}

	return EX_OK;
}


void
usage(const char *msg)
{
	fprintf(stderr,
			"Usage error: %s\n"
			"Usage: log-view [-d dir] [-D dbgspec ] [-l] [-r] [-v] [gcl_name ...]\n"
			"\t-d dir -- set log database root directory\n"
			"\t-D spec -- set debug flags\n"
			"\t-l -- list all local GCLs\n"
			"\t-n -- only list GCLs with no metadata\n"
			"\t-v -- print verbose information (-vv for more detail)\n",
				msg);

	exit(EX_USAGE);
}

int
main(int argc, char *argv[])
{
	int opt;
	int verbosity = 0;
	bool list_gcl = false;
	char *gcl_xname = NULL;
	const char *gcl_dir_name = NULL;
	uint32_t list_flags = 0;
	int istat;

	ep_lib_init(0);

	while ((opt = getopt(argc, argv, "d:D:lnv")) > 0)
	{
		switch (opt)
		{
		case 'd':
			gcl_dir_name = optarg;
			break;

		case 'D':
			ep_dbg_set(optarg);
			break;

		case 'l':
			list_gcl = true;
			break;

		case 'n':
			list_flags |= LIST_NO_METADATA;
			break;

		case 'v':
			verbosity++;
			break;

		default:
			usage("unknown flag");
		}
	}
	argc -= optind;
	argv += optind;

	// arrange to get runtime parameters
	ep_adm_readparams("gdp");
	ep_adm_readparams("gdplogd");

	// set up GCL directory path
	if (gcl_dir_name == NULL)
		gcl_dir_name = ep_adm_getstrparam("swarm.gdplogd.gcl.dir", GCL_DIR);

	if (list_gcl)
	{
		if (argc > 0)
			usage("cannot use a GCL name with -l");
		return list_gcls(gcl_dir_name, verbosity, list_flags);
	}

	if (argc <= 0)
		usage("GCL name required");
	while (argc > 0)
	{
		gcl_xname = argv[0];
		argc--;
		argv++;

		gdp_name_t gcl_name;

		EP_STAT estat = gdp_parse_name(gcl_xname, gcl_name);
		if (!EP_STAT_ISOK(estat))
		{
			ep_app_message(estat, "unparsable GCL name");
			continue;
		}
		istat = show_gcl(gcl_dir_name, gcl_name, verbosity);
	}
	exit(istat);
}
