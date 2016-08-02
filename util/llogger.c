/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  LLOGGER --- like logger(1), but to a local file
**
**  	Also allows for log rotation.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include <sys/errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>


void
reopen(const char *fname, FILE **fpp, ino_t *inop)
{
	FILE *fp = *fpp;
	struct stat st;

	if (fp != NULL)
		fclose(fp);

	fp = fopen(fname, "a");
	if (fp == NULL)
	{
		fprintf(stderr, "Cannot open output file \"%s\": %s\n",
				fname, strerror(errno));
		exit(EX_CANTCREAT);
	}
	*inop = -1;
	if (fstat(fileno(fp), &st) == 0)
		*inop = st.st_ino;
	*fpp = fp;
}

int
main(int argc, char **argv)
{
	int opt;
	char in_buf[10240];
	FILE *in_fp;
	FILE *out_fp;
	const char *out_fname;
	ino_t prev_ino = -1;

	while ((opt = getopt(argc, argv, "")) > 0)
	{
		switch (opt)
		{
		case 'x':
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
	{
		out_fname = NULL;
		out_fp = stdout;
	}
	else
	{
		out_fname = argv[0];
		out_fp = NULL;
		reopen(out_fname, &out_fp, &prev_ino);
		argc--;
		argv++;
	}

	in_fp = stdin;


	while (fgets(in_buf, sizeof in_buf, in_fp) != NULL)
	{
		struct stat st;
		struct timeval tv;
		struct tm *tm;

		// if the file has been rotated, open the new version
		if (out_fname != NULL)
		{
			if (stat(out_fname, &st) != 0 || st.st_ino != prev_ino)
			{
				reopen(out_fname, &out_fp, &prev_ino);
			}
		}

		gettimeofday(&tv, NULL);
		tm = gmtime(&tv.tv_sec);
		long usec = tv.tv_usec;
		fprintf(out_fp, "%04d-%02d-%02d %02d:%02d:%02d.%06ld %s",
				tm->tm_year + 1900,
				tm->tm_mon + 1,
				tm->tm_mday,
				tm->tm_hour,
				tm->tm_min,
				tm->tm_sec,
				usec,
				in_buf);
	}
}
