/* vim: set ai sw=4 sts=4 ts=4 : */

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
show_record(gcl_log_record *rec, FILE *dfp, size_t *foffp, int plev)
{
	rec->recno = ep_net_ntoh64(rec->recno);
	ep_net_ntoh_timespec(&rec->timestamp);
	rec->sigmeta = ep_net_ntoh16(rec->sigmeta);
	rec->data_length = ep_net_ntoh64(rec->data_length);

	fprintf(stdout, "\nRecno: %" PRIgdp_recno
			", offset = %zd (0x%zx), dlen = %" PRIi64
			", sigmeta = %x (mdalg = %d, len = %d)\n",
			rec->recno, *foffp, *foffp, rec->data_length, rec->sigmeta,
			(rec->sigmeta >> 12) & 0x000f, rec->sigmeta & 0x0fff);
	fprintf(stdout, "\tTimestamp: ");
	ep_time_print(&rec->timestamp, stdout, EP_TIME_FMT_HUMAN);
	fprintf(stdout, " (sec=%" PRIi64 ")\n", rec->timestamp.tv_sec);

	if (plev > GDP_PR_DETAILED)
	{
		fprintf(stdout, "\tRaw:\n");
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


int
show_gcl(const char *gcl_dir_name, gdp_name_t gcl_name, int plev)
{
	gdp_pname_t gcl_pname;
	struct stat st;
	gdp_recno_t max_recno;
	int istat;

	(void) gdp_printable_name(gcl_name, gcl_pname);

	// Add 5 in the middle for '/_xx/'
	int filename_size = strlen(gcl_dir_name) + 5 + strlen(gcl_pname) +
			strlen(GCL_INDEX_SUFFIX) + 1;
	char *data_filename = alloca(filename_size);

	snprintf(data_filename, filename_size,
			"%s/_%02x/%s%s",
			gcl_dir_name, gcl_name[0], gcl_pname, GCL_DATA_SUFFIX);
	ep_dbg_cprintf(Dbg, 6, "Reading %s\n\n", data_filename);

	FILE *data_fp = fopen(data_filename, "r");
	size_t file_offset = 0;
	gcl_log_header header;
	gcl_log_record record;

	if (data_fp == NULL)
	{
		fprintf(stderr, "Could not open %s, errno = %d\n", data_filename, errno);
		return EX_NOINPUT;
	}

	snprintf(data_filename, filename_size,
			"%s/_%02x/%s%s",
			gcl_dir_name, gcl_name[0], gcl_pname, GCL_INDEX_SUFFIX);
	if (stat(data_filename, &st) != 0)
	{
		fprintf(stderr, "could not stat %s (%s)\n",
				data_filename, strerror(errno));
		max_recno = -1;
		// keep on trying
	}
	else
	{
		max_recno = (st.st_size - SIZEOF_INDEX_HEADER) / SIZEOF_INDEX_RECORD;
	}
	printf("%s (%" PRIgdp_recno " recs)\n", gcl_pname, max_recno);

	if (fread(&header, sizeof header, 1, data_fp) != 1)
	{
		fprintf(stderr, "fread() failed while reading header, ferror = %d\n",
				ferror(data_fp));
		return EX_DATAERR;
	}
	header.magic = ep_net_ntoh32(header.magic);
	header.version = ep_net_ntoh32(header.version);
	header.header_size = ep_net_ntoh32(header.header_size);
	header.num_metadata_entries = ep_net_ntoh16(header.num_metadata_entries);
	header.log_type = ep_net_ntoh16(header.log_type);
	header.extent = ep_net_ntoh16(header.log_type);
	header.recno_offset = ep_net_ntoh64(header.recno_offset);

	if (plev >= GDP_PR_BASIC)
	{
		gdp_pname_t pname;

		printf("Header: magic = 0x%08" PRIx32
				", version = %" PRIi32
				", type = %" PRIi16 "\n",
				header.magic, header.version, header.log_type);
		printf("\tname = %s\n",
				gdp_printable_name(header.gname, pname));
		printf("\textent = %" PRIu32 ", recno_offset = %" PRIu64 "\n",
				header.extent, header.recno_offset);
		printf("\theader size = %" PRId32 " (0x%" PRIx32 ")"
				", metadata entries = %d\n",
				header.header_size, header.header_size,
				header.num_metadata_entries);
		if (plev >= GDP_PR_DETAILED)
		{
			ep_hexdump(&header, sizeof header, stdout,
					EP_HEXDUMP_HEX, file_offset);
		}
	}
	file_offset += sizeof header;
	CHECK_FILE_OFFSET(data_fp, file_offset);

	if (header.num_metadata_entries > 0)
	{
		istat = show_metadata(header.num_metadata_entries, data_fp,
					&file_offset, plev);
		if (istat != 0)
			return istat;
	}
	else if (plev >= GDP_PR_BASIC)
	{
		fprintf(stdout, "\n<No metadata>\n");
	}

	if (plev <= GDP_PR_BASIC)
		return EX_OK;

	fprintf(stdout, "\n");
	fprintf(stdout, "Data records:\n");

	while (fread(&record, sizeof record, 1, data_fp) == 1)
	{
		int istat = show_record(&record, data_fp, &file_offset, plev);
		if (istat != 0)
			return istat;
	}
	
	return EX_OK;
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
			if (p == NULL || strcmp(p, GCL_DATA_SUFFIX) != 0)
				continue;

			// strip off the ".data"
			*p = '\0';

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

	default:
		plev = GDP_PR_DETAILED;
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
