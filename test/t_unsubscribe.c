/* vim: set ai sw=4 sts=4 ts=4 : */

/*  To compile:
cc -I. t_unsubscribe.c -Lep -Lgdp -lgdp -lep -levent -levent_pthreads -pthread -lcrypto -lavahi-client -lavahi-common
*/

#include "t_common_support.h"

#include <getopt.h>

//static EP_DBG	Dbg = EP_DBG_INIT("t_unsubscribe", "Unsubscribe test");


int
main(int argc, char **argv)
{
	gdp_gcl_t *gcl;
	gdp_name_t gclname;
	gdp_datum_t *d;
	EP_STAT estat;
	int opt;
	char *gclxname = "x00";
	bool test_wildcard = false;
	int pausesec = 1;

	while ((opt = getopt(argc, argv, "D:p:w")) > 0)
	{
		switch (opt)
		{
		  case 'D':
			ep_dbg_set(optarg);
			break;

		  case 'p':
			pausesec = atoi(optarg);
			break;

		  case 'w':
			test_wildcard = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		gclxname = argv[0];

	estat = gdp_init(NULL);
	test_message(estat, "gdp_init");

	ep_time_nanosleep(100 MILLISECONDS);

	estat = gdp_parse_name(gclxname, gclname);
	test_message(estat, "gdp_parse_name");

	estat = gdp_gcl_open(gclname, GDP_MODE_RA, NULL, &gcl);
	test_message(estat, "gdp_gcl_open");

	estat = gdp_gcl_subscribe(gcl, 0, 0, NULL, print_event, (void *) 1);
	test_message(estat, "gdp_gcl_subscribe");
	ep_app_info("You should see subscription results");

	d = gdp_datum_new();
	gdp_buf_printf(gdp_datum_getbuf(d), "one");
	estat = gdp_gcl_append(gcl, d);
	test_message(estat, "gdp_gcl_append1");
	gdp_buf_reset(gdp_datum_getbuf(d));
	ep_app_info("sleeping1");
	ep_time_nanosleep(pausesec SECONDS);


	// use different cbarg: subscription should persist
	estat = gdp_gcl_unsubscribe(gcl, NULL, (void *) 2);
	test_message(estat, "gdp_gcl_unsubscribe1");

	gdp_buf_printf(gdp_datum_getbuf(d), "two");
	estat = gdp_gcl_append(gcl, d);
	test_message(estat, "gdp_gcl_append2");
	ep_app_info("sleeping2");
	ep_time_nanosleep(pausesec SECONDS);

	if (test_wildcard)
	{
		// use wildcard cbarg: subscription should go away
		estat = gdp_gcl_unsubscribe(gcl, NULL, NULL);
		test_message(estat, "gdp_gcl_unsubscribe3");
	}
	else
	{
		// use correct cbarg: subscription should go away
		estat = gdp_gcl_unsubscribe(gcl, NULL, (void *) 1);
		test_message(estat, "gdp_gcl_unsubscribe2");
	}

	ep_app_info("Subscription should be gone");

	gdp_buf_printf(gdp_datum_getbuf(d), "three");
	estat = gdp_gcl_append(gcl, d);
	test_message(estat, "gdp_gcl_append3");
	ep_app_info("sleeping3");
	ep_time_nanosleep(pausesec SECONDS);

	ep_app_info("closing");
	estat = gdp_gcl_close(gcl);
	test_message(estat, "gdp_gcl_close");

	return 0;
}
