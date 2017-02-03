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

int
parse_hex(const char *s, gdp_name_t gdpiname)
{
	int i;

	if (strlen(s) != 64)
		return 1;

	for (i = 0; i < 32; i++)
	{
		if (!isxdigit(s[0]) || !isxdigit(s[1]))
			return 1;
		gdpiname[i] = (Xlations[s[0] - 0x30]) << 4 | (Xlations[s[1] - 0x30]);
		s += 2;
	}

	return 0;
}

void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-D dbgspec] gdp_name\n"
			"    -D  set debugging flags\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	int opt;
	int i;
	bool show_usage = false;
	gdp_name_t gdpiname;
	gdp_pname_t gdppname;

	while ((opt = getopt(argc, argv, "D:")) > 0)
	{
		switch (opt)
		{
			case 'D':
				ep_dbg_set(optarg);
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

	// don't really need to initialize the GDP library for this
	if (parse_hex(argv[0], gdpiname) != 0)
		gdp_parse_name(argv[0], gdpiname);
	gdp_printable_name(gdpiname, gdppname);
	fprintf(stdout,
			"printable: %s\n"
			"hex:       ",
			gdppname);
	for (i = 0; i < sizeof gdpiname; i++)
		fprintf(stdout, "%02x", gdpiname[i]);
	fprintf(stdout, "\n");
	exit(EX_OK);
}
