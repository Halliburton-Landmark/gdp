/* vim: set ai sw=4 sts=4 ts=4 : */

/*  To compile:
cc -I. t_multimultiread.c -Lep -Lgdp -lgdp -lep -levent -levent_pthreads -pthread -lcrypto -lavahi-client -lavahi-common
*/

#include <gdp/gdp.h>

#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <ep/ep_time.h>

#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

static EP_DBG	Dbg = EP_DBG_INIT("t_multimultiread", "GDP multiple multireader test");


void
print_event(gdp_event_t *gev)
{
	printf("%s>>> Event type %d, udata %p%s\n",
			EpVid->vidbgblue,
			gdp_event_gettype(gev), gdp_event_getudata(gev),
			EpVid->vidnorm);

	// decode it
	switch (gdp_event_gettype(gev))
	{
	  case GDP_EVENT_DATA:
		// this event contains a data return
		gdp_datum_print(gdp_event_getdatum(gev), stdout, GDP_DATUM_PRTEXT);
		break;

	  case GDP_EVENT_EOS:
		// "end of subscription": no more data will be returned
		fprintf(stderr, "End of multiread\n");
		break;

	  case GDP_EVENT_SHUTDOWN:
		// log daemon has shut down, meaning we lose our subscription
		fprintf(stderr, "log daemon shutdown\n");
		break;

	  default:
		// should be ignored, but we print it since this is a test program
		fprintf(stderr, "Unknown event type %d\n", gdp_event_gettype(gev));

		// just in case we get into some crazy loop.....
		sleep(1);
		break;
	}
}



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
	char ebuf[60];
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

	estat = gdp_init(NULL);
	ep_app_info("gdp_init: %s", ep_stat_tostr(estat, ebuf, sizeof ebuf));

	ep_time_nanosleep(INT64_C(100000000));

	estat = gdp_parse_name("x00", gclname);
	ep_app_info("gdp_parse_name: %s", ep_stat_tostr(estat, ebuf, sizeof ebuf));

	estat = gdp_gcl_open(gclname, GDP_MODE_RO, NULL, &gcl);
	ep_app_info("gdp_gcl_open: %s", ep_stat_tostr(estat, ebuf, sizeof ebuf));

	estat = do_multiread(gcl, 1, 0, (void *) 1);
	ep_app_info("1: %s", ep_stat_tostr(estat, ebuf, sizeof ebuf));
	estat = do_multiread(gcl, 1, 0, (void *) 2);
	ep_app_info("2: %s", ep_stat_tostr(estat, ebuf, sizeof ebuf));

	// hang for 5 seconds waiting for events
	ep_app_info("sleeping");
	sleep(5);

	return 0;
}
