/* vim: set ai sw=4 sts=4 ts=4 : */

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
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
***********************************************************************/

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_net.h>
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
#include <gdplogd/logd_physlog.h>


/*
**  LOG-VIEW --- display raw on-disk storage
**
**		Not for user consumption.
**		This does peek into private header files.
*/

static EP_DBG	Dbg = EP_DBG_INIT("log-view", "Dump GDP logs for debugging");


#define CHECK_FILE_OFFSET	check_file_offset

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

	i = fread(mdhdrs, sizeof *mdhdrs, nmds, dfp);
	if (i != nmds)
	{
		fprintf(stderr,
				"fread() failed while reading metadata headers,"
				" ferror = %d, wanted %d, got %d\n",
				ferror(dfp), nmds, i);
		return EX_DATAERR;
	}

	if (plev >= GDP_PR_DETAILED)
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

		mdata = malloc(mdhdrs[i].md_len + 1);
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
		if (plev > GDP_PR_BASIC)
		{
			fprintf(stdout,
					"\nMetadata entry %d: name = 0x%08" PRIx32
					", len = %" PRId32,
					i, mdhdrs[i].md_id, mdhdrs[i].md_len);
			switch (mdhdrs[i].md_id)
			{
				case GDP_GCLMD_XID:
					printf(" (external id)\n");
					break;

				case GDP_GCLMD_CTIME:
					printf(" (creation time)\n");
					break;

				case GDP_GCLMD_PUBKEY:
					printf(" (public key)\n");
					int keylen = mdata[2] << 8 | mdata[3];
					printf("\tmd_alg %s (%d), keytype %s (%d), keylen %d\n",
							ep_crypto_md_alg_name(mdata[0]), mdata[0],
							ep_crypto_keytype_name(mdata[1]), mdata[1],
							keylen);
					if (plev > GDP_PR_BASIC)
					{
						EP_CRYPTO_KEY *key;

						key = ep_crypto_key_read_mem(mdata + 4,
								mdhdrs[i].md_len - 4,
								EP_CRYPTO_KEYFORM_DER,
								EP_CRYPTO_F_PUBLIC);
						ep_crypto_key_print(key, stdout, EP_CRYPTO_F_PUBLIC);
						ep_crypto_key_free(key);
					}
					if (plev >= GDP_PR_DETAILED)
						ep_hexdump(mdata + 4, mdhdrs[i].md_len - 4,
								stdout, EP_HEXDUMP_HEX, 0);
					continue;
				default:
					printf("\n");
					break;
			}
			if (plev < GDP_PR_DETAILED)
			{
				printf("\t%s", EpChar->lquote);
				ep_xlate_out(mdata, mdhdrs[i].md_len,
						stdout, "", EP_XLATE_PLUS | EP_XLATE_NPRINT);
				fprintf(stdout, "%s\n", EpChar->rquote);
			}
		}
		else if (mdhdrs[i].md_id == GDP_GCLMD_XID)
		{
			fprintf(stdout,
					"\tExternal name: %s\n", mdata);
		}

		if (plev >= GDP_PR_DETAILED)
		{
			ep_hexdump(mdata, mdhdrs[i].md_len,
					stdout, EP_HEXDUMP_ASCII, *foffp);
		}
		free(mdata);
	}
	return EX_OK;
}


int
show_record(extent_record_t *rec, FILE *dfp, size_t *foffp, int plev)
{
	rec->recno = ep_net_ntoh64(rec->recno);
	ep_net_ntoh_timespec(&rec->timestamp);
	rec->sigmeta = ep_net_ntoh16(rec->sigmeta);
	rec->data_length = ep_net_ntoh64(rec->data_length);

	fprintf(stdout, "\n    Recno: %" PRIgdp_recno
			", offset = %zd (0x%zx), dlen = %" PRIi64
			", sigmeta = %x (mdalg = %d, len = %d)\n",
			rec->recno, *foffp, *foffp, rec->data_length, rec->sigmeta,
			(rec->sigmeta >> 12) & 0x000f, rec->sigmeta & 0x0fff);
	fprintf(stdout, "\tTimestamp: ");
	ep_time_print(&rec->timestamp, stdout, EP_TIME_FMT_HUMAN);
	fprintf(stdout, " (sec=%" PRIi64 ")\n", rec->timestamp.tv_sec);

	if (plev > GDP_PR_DETAILED)
	{
		ep_hexdump(&*rec, sizeof *rec, stdout, EP_HEXDUMP_HEX, *foffp);
	}
	*foffp += sizeof *rec;
	CHECK_FILE_OFFSET(dfp, *foffp);

	char *data_buffer = malloc(rec->data_length);
	if (fread(data_buffer, rec->data_length, 1, dfp) != 1)
	{
		fprintf(stderr, "fread() failed while reading data (%d)\n",
				ferror(dfp));
		free(data_buffer);
		return EX_DATAERR;
	}

	if (plev >= GDP_PR_BASIC + 2)
	{
		ep_hexdump(data_buffer, rec->data_length,
				stdout, EP_HEXDUMP_ASCII,
				plev < GDP_PR_DETAILED ? 0 : *foffp);
	}
	*foffp += rec->data_length;
	CHECK_FILE_OFFSET(dfp, *foffp);
	free(data_buffer);

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

		if (plev >= GDP_PR_BASIC + 2)
		{
			ep_hexdump(sigbuf, siglen, stdout, EP_HEXDUMP_ASCII,
				plev < GDP_PR_DETAILED ? 0 : *foffp);
		}

		*foffp += siglen;
		CHECK_FILE_OFFSET(dfp, *foffp);
	}

	return EX_OK;
}


FILE *
open_index(const char *index_filename, struct stat *st, index_header_t *phdr)
{
	FILE *index_fp = NULL;
	index_header_t index_header;

	if (stat(index_filename, st) != 0)
	{
		fprintf(stderr, "could not stat %s (%s)\n",
				index_filename, strerror(errno));
		return NULL;
	}

	index_fp = fopen(index_filename, "r");
	if (index_fp == NULL)
	{
		fprintf(stderr, "Could not open %s (%s)\n",
				index_filename, strerror(errno));
		return NULL;
	}

	if (st->st_size < SIZEOF_INDEX_HEADER)
	{
		phdr->magic = 0;
		return index_fp;
	}
	if (fread(&index_header, sizeof index_header, 1, index_fp) != 1)
	{
		fprintf(stderr, "Could not read hdr for %s (%s)\n",
				index_filename, strerror(errno));
	}
	else
	{
		phdr->magic = ep_net_ntoh32(index_header.magic);
		phdr->version = ep_net_ntoh32(index_header.version);
		phdr->header_size = ep_net_ntoh32(index_header.header_size);
		phdr->reserved1 = ep_net_ntoh32(index_header.reserved1);
		phdr->min_recno = ep_net_ntoh64(index_header.min_recno);
	}

	return index_fp;
}


gdp_recno_t
show_index_header(const char *index_filename, int plev)
{
	struct stat st;
	index_header_t index_header;
	FILE *index_fp = open_index(index_filename, &st, &index_header);

	if (index_fp == NULL)
		return -1;

	if (index_header.magic == 0)
		goto no_header;
	else if (index_header.magic != GCL_LXF_MAGIC)
	{
		fprintf(stderr, "Bad index magic %04x\n", index_header.magic);
	}

	if (plev > GDP_PR_BASIC)
	{
		printf("    Index: magic=%04" PRIx32 ", vers=%" PRId32
				", header_size=%" PRId32 ", min_recno=%" PRIgdp_recno "\n",
				index_header.magic, index_header.version,
				index_header.header_size, index_header.min_recno);

		// get info from the last record
		index_entry_t xent;
		if (st.st_size <= index_header.header_size)
		{
			// no records yet
			printf("\tno records\n");
		}
		else if (fseek(index_fp, st.st_size - SIZEOF_INDEX_RECORD,
						SEEK_SET) < 0)
		{
			printf("show_index_header: cannot seek\n");
		}
		else if (fread(&xent, SIZEOF_INDEX_RECORD, 1, index_fp) != 1)
		{
			printf("show_index_header: fread failure\n");
		}
		else
		{
			printf("\tlast recno %" PRIgdp_recno
					" offset %jd extent %d reserved %x\n",
					ep_net_ntoh64(xent.recno),
					(intmax_t) ep_net_ntoh64(xent.offset),
					ep_net_ntoh32(xent.extent),
					ep_net_ntoh32(xent.reserved));
		}
	}
	fclose(index_fp);
	return ((st.st_size - index_header.header_size) / SIZEOF_INDEX_RECORD)
				+ index_header.min_recno;

no_header:
	fclose(index_fp);
	if (plev > GDP_PR_BASIC)
		printf("Old-style headerless index\n");
	return st.st_size / SIZEOF_INDEX_RECORD;
	return -1;
}


int
show_index_contents(const char *gcl_dir_name, gdp_name_t gcl_name, int plev)
{
	struct stat st;
	index_header_t index_header;
	gdp_pname_t gcl_pname;

	(void) gdp_printable_name(gcl_name, gcl_pname);

	// Add 5 in the middle for '/_xx/'
	int filename_size = strlen(gcl_dir_name) + 5 + strlen(gcl_pname) +
			strlen(GCL_LXF_SUFFIX) + 1;
	char *index_filename = alloca(filename_size);
	snprintf(index_filename, filename_size,
			"%s/_%02x/%s%s",
			gcl_dir_name, gcl_name[0], gcl_pname, GCL_LXF_SUFFIX);

	FILE *index_fp = open_index(index_filename, &st, &index_header);

	if (index_fp == NULL)
	{
		fprintf(stderr, "Could not open %s (%s)\n",
				index_filename, strerror(errno));
		return EX_NOINPUT;
	}

	printf("\n    Index Contents:\n");

	while (true)
	{
		index_entry_t index_entry;
		if (fread(&index_entry, sizeof index_entry, 1, index_fp) != 1)
			break;
		index_entry.recno = ep_net_ntoh64(index_entry.recno);
		index_entry.offset = ep_net_ntoh64(index_entry.offset);
		index_entry.extent = ep_net_ntoh32(index_entry.extent);
		index_entry.reserved = ep_net_ntoh32(index_entry.reserved);

		printf("\trecno %" PRIgdp_recno ", extent %" PRIu32
				", offset %" PRIu64 ", reserved %" PRIu32 "\n",
				index_entry.recno, index_entry.extent,
				index_entry.offset, index_entry.reserved);
	}
	fclose(index_fp);
	return EX_OK;
}


int
show_extent(const char *gcl_dir_name, gdp_name_t gcl_name, int extno, int plev)
{
	gdp_pname_t gcl_pname;
	int istat;
	char extent_str[20];

	(void) gdp_printable_name(gcl_name, gcl_pname);
	snprintf(extent_str, sizeof extent_str, "-%06d", extno);

	// Add 5 in the middle for '/_xx/'
	int filename_size = strlen(gcl_dir_name) + 5 + strlen(gcl_pname) +
			strlen(extent_str) + strlen(GCL_LDF_SUFFIX) + 1;
	char *filename = alloca(filename_size);

	snprintf(filename, filename_size,
			"%s/_%02x/%s%s%s",
			gcl_dir_name, gcl_name[0], gcl_pname, extent_str, GCL_LDF_SUFFIX);
	ep_dbg_cprintf(Dbg, 6, "Reading %s\n\n", filename);

	FILE *data_fp = fopen(filename, "r");
	if (data_fp == NULL && extno == 0)
	{
		// try again without extent
		snprintf(filename, filename_size,
				"%s/_%02x/%s%s",
				gcl_dir_name, gcl_name[0], gcl_pname, GCL_LDF_SUFFIX);
		ep_dbg_cprintf(Dbg, 6, "Reading %s\n\n", filename);
		data_fp = fopen(filename, "r");
	}
	if (data_fp == NULL)
	{
		fprintf(stderr, "Could not open %s, errno = %d\n", filename, errno);
		return EX_NOINPUT;
	}

	size_t file_offset = 0;
	extent_header_t log_header;
	extent_record_t record;
	if (fread(&log_header, sizeof log_header, 1, data_fp) != 1)
	{
		fprintf(stderr, "fread() failed while reading log_header, ferror = %d\n",
				ferror(data_fp));
		istat = EX_DATAERR;
		goto fail0;
	}
	log_header.magic = ep_net_ntoh32(log_header.magic);
	log_header.version = ep_net_ntoh32(log_header.version);
	log_header.header_size = ep_net_ntoh32(log_header.header_size);
	log_header.reserved1 = ep_net_ntoh32(log_header.reserved1);
	log_header.n_md_entries = ep_net_ntoh16(log_header.n_md_entries);
	log_header.log_type = ep_net_ntoh16(log_header.log_type);
	log_header.extent = ep_net_ntoh32(log_header.log_type);
	log_header.reserved2 = ep_net_ntoh64(log_header.reserved2);
	log_header.recno_offset = ep_net_ntoh64(log_header.recno_offset);

	if (plev >= GDP_PR_BASIC)
	{
		gdp_pname_t pname;

		printf("    Extent %d: magic = 0x%08" PRIx32
				", version = %" PRIi32
				", type = %" PRIi16 "\n",
				log_header.extent, log_header.magic, log_header.version,
				log_header.log_type);
		printf("\tname = %s\n",
				gdp_printable_name(log_header.gname, pname));
		printf("\theader size = %" PRId32 " (0x%" PRIx32 ")"
				", metadata entries = %d, recno_offset = %" PRIgdp_recno "\n",
				log_header.header_size, log_header.header_size,
				log_header.n_md_entries, log_header.recno_offset);
		if (plev >= GDP_PR_DETAILED)
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
	else if (plev >= GDP_PR_BASIC)
	{
		fprintf(stdout, "\n<No metadata>\n");
	}

	if (plev <= GDP_PR_BASIC)
		goto success;

	fprintf(stdout, "\n    Data records:\n");

	while (fread(&record, sizeof record, 1, data_fp) == 1)
	{
		int istat = show_record(&record, data_fp, &file_offset, plev);
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
	int istat;

	(void) gdp_printable_name(gcl_name, gcl_pname);
	printf("\nLog %s:\n", gcl_pname);

	// Add 5 in the middle for '/_xx/'
	int filename_size = strlen(gcl_dir_name) + 5 + strlen(gcl_pname) +
			strlen(GCL_LXF_SUFFIX) + 1;
	char *filename = alloca(filename_size);

	snprintf(filename, filename_size,
			"%s/_%02x/%s%s",
			gcl_dir_name, gcl_name[0], gcl_pname, GCL_LXF_SUFFIX);
	max_recno = show_index_header(filename, plev);
	printf("\t%" PRIgdp_recno " recs\n", max_recno - 1);

	istat = show_extent(gcl_dir_name, gcl_name, 0, plev);

	if (plev > GDP_PR_DETAILED)
	{
		show_index_contents(gcl_dir_name, gcl_name, plev);
	}

	return istat;
}


int
list_gcls(const char *gcl_dir_name, int plev)
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

			// strip off the ".data"
			*p = '\0';

			// strip off extent number if it exists
			if (strlen(dent->d_name) > GDP_GCL_PNAME_LEN &&
					dent->d_name[GDP_GCL_PNAME_LEN] == '-')
				dent->d_name[GDP_GCL_PNAME_LEN] = '\0';

			// print the name
			gdp_parse_name(dent->d_name, gcl_iname);
			show_gcl(gcl_dir_name, gcl_iname, plev);
		}
		closedir(dir);
	}

	return EX_OK;
}


void
usage(const char *msg)
{
	fprintf(stderr,
			"Usage error: %s\n"
			"Usage: log-view [-d dir] [-l] [-r] [gcl_name]\n"
			"\t-d dir -- set log database root directory\n"
			"\t-D spec -- set debug flags\n"
			"\t-l -- list all local GCLs\n"
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

	ep_lib_init(0);

	while ((opt = getopt(argc, argv, "d:D:lv")) > 0)
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

	int plev;
	switch (verbosity)
	{
	case 0:
		plev = GDP_PR_PRETTY;
		break;

	case 1:
		plev = GDP_PR_BASIC;
		break;

	case 2:
		plev = GDP_PR_BASIC + 1;
		break;

	case 3:
		plev = GDP_PR_DETAILED;
		break;

	default:
		plev = GDP_PR_DETAILED + 1;
		break;
	}

	if (list_gcl)
	{
		if (argc > 0)
			usage("cannot use a GCL name with -l");
		return list_gcls(gcl_dir_name, plev);
	}

	if (argc > 0)
	{
		gcl_xname = argv[0];
		argc--;
		argv++;
		if (argc > 0)
			usage("extra arguments");
	}
	else
	{
		usage("GCL name required");
	}

	gdp_name_t gcl_name;

	EP_STAT estat = gdp_parse_name(gcl_xname, gcl_name);
	if (!EP_STAT_ISOK(estat))
	{
		usage("unparsable GCL name");
	}
	exit(show_gcl(gcl_dir_name, gcl_name, plev));
}
