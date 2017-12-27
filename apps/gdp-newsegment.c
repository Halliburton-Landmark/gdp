/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  GDP-NEWSEGMENT --- create a new segment in an existing GCL
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

#include <sysexits.h>

void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-D dbg_spec] [-G router_addr] target_log\n"
			"    -D  set debugging flags\n"
			"    -G  IP host to contact for gdp_router\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	int opt;
	bool show_usage = false;
	EP_STAT estat;
	gdp_gcl_t *gcl;
	char *gdpd_addr = NULL;
	const char *lname;
	gdp_name_t gcliname;
	const char *phase;

	while ((opt = getopt(argc, argv, "D:G:")) > 0)
	{
		switch (opt)
		{
		 case 'D':
			ep_dbg_set(optarg);
			break;

		 case 'G':
			gdpd_addr = optarg;
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

	// initialize GDP library
	estat = gdp_init(gdpd_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	// allow thread to settle to avoid interspersed debug output
	ep_time_nanosleep(INT64_C(100000000));

	// open target GCL (must already exist)
	lname = argv[0];
	gdp_parse_name(lname, gcliname);
	phase = "open";
	estat = gdp_gcl_open(gcliname, GDP_MODE_AO, NULL, &gcl);
	EP_STAT_CHECK(estat, goto fail1);

	// create new segment
	phase = "create new segment for";
	estat = gdp_gcl_newsegment(gcl);
	EP_STAT_CHECK(estat, goto fail1);

	// close GCLs / release resources
	estat = gdp_gcl_close(gcl);

	if (!EP_STAT_ISOK(estat))
	{
fail1:
		ep_app_error("could not %s %s", phase, lname);
	}

fail0:
	ep_app_message(estat, "exiting with status");
	return EP_STAT_ISOK(estat) ? EX_OK : EX_UNAVAILABLE;
}
