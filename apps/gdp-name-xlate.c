/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  GDP-NAME-XLATE --- show GDP name in various forms
**
**		Given an external name, shows the internal name in base64
**		and as a hex number.
**
**	----- BEGIN LICENSE BLOCK -----
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
**	----- END LICENSE BLOCK -----
*/

#include <gdp/gdp.h>

#include <ep/ep_app.h>
#include <ep/ep_dbg.h>

#include <ctype.h>
#include <getopt.h>
#include <string.h>
#include <sysexits.h>

uint8_t	Xlations[] =
	{
		0,	1,	2,	3,	4,	5,	6,	7,
		8,	9,	0,	0,	0,	0,	0,	0,
		0,	10, 11, 12, 13, 14, 15, 0,
		0,	0,	0,	0,	0,	0,	0,	0,
		0,	0,	0,	0,	0,	0,	0,	0,
		0,	0,	0,	0,	0,	0,	0,	0,
		0,	10, 11, 12, 13, 14, 15, 0,
	};

EP_STAT
parse_hex(const char *s, gdp_name_t gdpiname)
{
	int i;

	if (strlen(s) != 64)
		return EP_STAT_ERROR;

	for (i = 0; i < 32; i++)
	{
		if (!isxdigit(s[0]) || !isxdigit(s[1]))
			return EP_STAT_ERROR;
		gdpiname[i] = (Xlations[s[0] - '0']) << 4 | (Xlations[s[1] - '0']);
		s += 2;
	}

	return EP_STAT_OK;
}

void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-b] [-D dbgspec] [-h] gdp_name\n"
			"    -b  print printable base64 name\n"
			"    -D  set debugging flags\n"
			"    -f  show file name root\n"
			"    -h  print hexadecimal name\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	int opt;
	EP_STAT estat;
	bool show_usage = false;
	gdp_name_t gdpiname;
	gdp_pname_t gdppname;
	bool show_b64 = false;
	bool show_hex = false;
	bool show_file_name = false;

	while ((opt = getopt(argc, argv, "bD:fh")) > 0)
	{
		switch (opt)
		{
			case 'b':
				show_b64 = true;
				break;

			case 'D':
				ep_dbg_set(optarg);
				break;

			case 'f':
				show_file_name = true;
				break;

			case 'h':
				show_hex = true;
				break;

			default:
				show_usage = true;
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage || argc != 1)
		usage();

	// don't really need to initialize the GDP library for this, but
	// we do need the name lookup part of this --- cheat and use
	// internal interfaces.
	// DON'T TRY THIS AT HOME, KIDS!!!
	gdp_init_phase_0(NULL);
	extern void _gdp_name_init(void);
	_gdp_name_init();

	char *xname = argv[0];
	estat = parse_hex(argv[0], gdpiname);
	if (!EP_STAT_ISOK(estat))
		estat = gdp_name_parse(argv[0], gdpiname, &xname);
	if (EP_STAT_ISFAIL(estat))
	{
		ep_app_message(estat, "Cannot parse name \"%s\"", argv[0]);
		exit(EX_USAGE);
	}
	gdp_printable_name(gdpiname, gdppname);
	if (show_b64)
	{
		fprintf(stdout, "%s\n", gdppname);
	}
	else if (show_hex)
	{
		unsigned int i;
		for (i = 0; i < sizeof gdpiname; i++)
			fprintf(stdout, "%02x", gdpiname[i]);
		fprintf(stdout, "\n");
	}
	else if (show_file_name)
	{
		fprintf(stdout, "_%02x/%s\n", gdpiname[0], gdppname);
	}
	else
	{
		fprintf(stdout,
				"human:     %s\n"
				"printable: %s\n"
				"hex:       ",
				xname, gdppname);
		unsigned int i;
		for (i = 0; i < sizeof gdpiname; i++)
			fprintf(stdout, "%02x", gdpiname[i]);
		fprintf(stdout, "\n");
	}
	exit(EX_OK);
}
