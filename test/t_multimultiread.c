/* vim: set ai sw=4 sts=4 ts=4 : */

/*  To compile:
cc -I. t_multimultiread.c -Lep -Lgdp -lgdp -lep -levent -levent_pthreads -pthread -lcrypto -lavahi-client -lavahi-common
*/

#include "t_common_support.h"

#include <getopt.h>

static EP_DBG	Dbg = EP_DBG_INIT("t_multimultiread", "GDP multiple multireader test");



/*
**  DO_MULTIREAD --- subscribe or multiread
**
**		This routine handles calls that return multiple values via the
**		event interface.  They might include subscriptions.
*/

EP_STAT
do_multiread(gdp_gcl_t *gcl,
		gdp_recno_t firstrec,
		int32_t numrecs,
		void *udata)
{
	EP_STAT estat;
	void (*cbfunc)(gdp_event_t *) = NULL;

	cbfunc = print_event;

	// make the flags more user-friendly
	if (firstrec == 0)
		firstrec = 1;

	// start up a multiread
	estat = gdp_gcl_multiread(gcl, firstrec, numrecs, cbfunc, udata);

	// check to make sure the subscribe/multiread succeeded; if not, bail
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[200];

		ep_app_fatal("Cannot multiread:\n\t%s",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	// this sleep will allow multiple results to appear before we start reading
	if (ep_dbg_test(Dbg, 100))
		ep_time_nanosleep(500000000);	//DEBUG: one half second

	return estat;
}

int
main(int argc, char **argv)
{
	gdp_gcl_t *gcl;
	gdp_name_t gclname;
	EP_STAT estat;
	int opt;

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

	char *log_xname;
	if (argc < 1)
            log_xname = strdup("x00");
        else
            log_xname = argv[0];

	estat = gdp_init(NULL);
	test_message(estat, "gdp_init");

	ep_time_nanosleep(INT64_C(100000000));

	estat = gdp_parse_name(log_xname, gclname);
	test_message(estat, "gdp_parse_name");

	estat = gdp_gcl_open(gclname, GDP_MODE_RO, NULL, &gcl);
	test_message(estat, "gdp_gcl_open");

	estat = do_multiread(gcl, 1, 0, (void *) 1);
	test_message(estat, "1");
	estat = do_multiread(gcl, 1, 0, (void *) 2);
	test_message(estat, "2");

	// hang for 5 seconds waiting for events
	ep_app_info("sleeping");
	sleep(5);

	return 0;
}
