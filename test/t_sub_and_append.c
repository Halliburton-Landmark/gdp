/* vim: set ai sw=4 sts=4 ts=4 : */

/*  To compile:
cc -I. t_sub_and_append.c -Lep -Lgdp -lgdp -lep -levent -levent_pthreads -pthread -lcrypto -lavahi-client -lavahi-common
*/

#include "t_common_support.h"

#include <getopt.h>

//static EP_DBG	Dbg = EP_DBG_INIT("t_sub_and_append", "Subscribe and Append in one process test");


int
main(int argc, char **argv)
{
	gdp_gcl_t *gcl;
	gdp_name_t gclname;
	gdp_datum_t *d;
	EP_STAT estat;
	int opt;
	char *gclxname = "x00";

	while ((opt = getopt(argc, argv, "D:")) > 0)
	{
		switch (opt)
		{
		  case 'D':
			ep_dbg_set(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		gclxname = argv[0];

	estat = gdp_init(NULL);
	test_message(estat, "gdp_init");

	ep_time_nanosleep(INT64_C(100000000));

	estat = gdp_parse_name(gclxname, gclname);
	test_message(estat, "gdp_parse_name");

	estat = gdp_gcl_open(gclname, GDP_MODE_RA, NULL, &gcl);
	test_message(estat, "gdp_gcl_open");

	estat = gdp_gcl_subscribe(gcl, 0, 0, NULL, print_event, NULL);
	test_message(estat, "gdp_gcl_subscribe");

	d = gdp_datum_new();
	gdp_buf_printf(gdp_datum_getbuf(d), "one");
	estat = gdp_gcl_append(gcl, d);
	test_message(estat, "gdp_gcl_append1");
	gdp_buf_reset(gdp_datum_getbuf(d));

	gdp_buf_printf(gdp_datum_getbuf(d), "two");
	estat = gdp_gcl_append(gcl, d);
	test_message(estat, "gdp_gcl_append2");

	// hang for a minute waiting for events
	ep_app_info("sleeping");
	sleep(60);

	return 0;
}
