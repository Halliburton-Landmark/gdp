/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  GDP-WRITER --- writes records to a GCL
**
**		This reads the records one line at a time from standard input
**		and assumes they are text, but there is no text requirement
**		implied by the GDP.
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

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_string.h>
#include <gdp/gdp.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <sysexits.h>
#include <sys/stat.h>

bool	AsyncIo = false;		// use asynchronous I/O
bool	Quiet = false;			// be silent (no chatty messages)
bool	Hexdump = false;		// echo input in hex instead of ASCII
bool	KeepGoing = false;		// keep going on append errors

static EP_DBG	Dbg = EP_DBG_INIT("gdp-writer", "gdp-writer");


#define LOG(tag)	{ if (LogFile != NULL) do_log(tag); }


static char	*EventTypes[] =
{
	"Free (internal use)",
	"Data",
	"End of Subscription",
	"Shutdown",
	"Asynchronous Status",
};

void
showstat(gdp_event_t *gev)
{
	int evtype = gdp_event_gettype(gev);
	EP_STAT estat = gdp_event_getstat(gev);
	gdp_datum_t *d = gdp_event_getdatum(gev);
	char ebuf[100];
	char tbuf[20];
	char *evname;

	if (evtype < 0 || evtype >= sizeof EventTypes / sizeof EventTypes[0])
	{
		snprintf(tbuf, sizeof tbuf, "%d", evtype);
		evname = tbuf;
	}
	else
	{
		evname = EventTypes[evtype];
	}

	printf("Asynchronous event type %s:\n"
			"\trecno %" PRIgdp_recno ", stat %s\n",
			evname,
			gdp_datum_getrecno(d),
			ep_stat_tostr(estat, ebuf, sizeof ebuf));

	gdp_event_free(gev);
}


EP_STAT
write_record(gdp_datum_t *datum, gdp_gcl_t *gcl)
{
	EP_STAT estat;

	// echo the input for that warm fuzzy feeling
	if (!Quiet)
	{
		gdp_buf_t *dbuf = gdp_datum_getbuf(datum);
		int l = gdp_buf_getlength(dbuf);
		unsigned char *buf = gdp_buf_getptr(dbuf, l);

		if (!Hexdump)
			fprintf(stdout, "Got input %s%.*s%s\n",
					EpChar->lquote, l, buf, EpChar->rquote);
		else
			ep_hexdump(buf, l, stdout, EP_HEXDUMP_ASCII, 0);
	}

	if (ep_dbg_test(Dbg, 60))
		gdp_datum_print(datum, ep_dbg_getfile(), GDP_DATUM_PRDEBUG);

	// then send the buffer to the GDP
	LOG("W");
	if (AsyncIo)
	{
		estat = gdp_gcl_append_async(gcl, datum, showstat, NULL);
		EP_STAT_CHECK(estat, return estat);

		// return value will be printed asynchronously
	}
	else
	{
		estat = gdp_gcl_append(gcl, datum);

		if (EP_STAT_ISOK(estat))
		{
			// print the return value (shows the record number assigned)
			if (!Quiet)
				gdp_datum_print(datum, stdout, 0);
		}
		else if (!Quiet)
		{
			char ebuf[100];
			ep_app_error("Append error: %s",
						ep_stat_tostr(estat, ebuf, sizeof ebuf));
		}
	}
	return estat;
}


int
main(int argc, char **argv)
{
	gdp_gcl_t *gcl;
	gdp_name_t gcliname;
	int opt;
	EP_STAT estat;
	char *gdpd_addr = NULL;
	bool show_usage = false;
	bool one_record = false;
	char *log_file_name = NULL;
	char *signing_key_file = NULL;
	gdp_gcl_open_info_t *info;





	if (!Quiet)
	{
		gdp_pname_t pname;

		// dump the internal version of the GCL to facilitate testing
		printf("GDPname: %s (%" PRIu64 " recs)\n",
				gdp_printable_name(*gdp_gcl_getname(gcl), pname),
				gdp_gcl_getnrecs(gcl));

		// OK, ready to go!
		fprintf(stdout, "\nStarting to read input\n");
	}

	// we need a place to buffer the input
	gdp_datum_t *datum = gdp_datum_new();

	if (one_record)
	{
		// read the entire stdin into a single datum
		char buf[8 * 1024];
		int l;

		while ((l = fread(buf, 1, sizeof buf, stdin)) > 0)
			gdp_buf_write(gdp_datum_getbuf(datum), buf, l);

		estat = write_record(datum, gcl);
	}
	else
	{
		// write lines into multiple datums
		char buf[200];

		while (fgets(buf, sizeof buf, stdin) != NULL)
		{
			// strip off newlines
			char *p = strchr(buf, '\n');
			if (p != NULL)
				*p++ = '\0';

			// first copy the text buffer into the datum buffer
			gdp_buf_write(gdp_datum_getbuf(datum), buf, strlen(buf));

			// write the record to the log
			estat = write_record(datum, gcl);
			if (!EP_STAT_ISOK(estat) && !KeepGoing)
				break;
		}
	}

	// OK, all done.  Free our resources and exit
	gdp_datum_free(datum);

	// give a chance to collect async results
	if (AsyncIo)
		sleep(1);

	// tell the GDP that we are done
	gdp_gcl_close(gcl);

fail1:
	if (info != NULL)
		gdp_gcl_open_info_free(info);

fail0:
	;			// avoid compiler error
	int exitstat;

	if (EP_STAT_ISOK(estat))
		exitstat = EX_OK;
	else if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOROUTE))
		exitstat = EX_CANTCREAT;
	else if (EP_STAT_ISABORT(estat))
		exitstat = EX_SOFTWARE;
	else
		exitstat = EX_UNAVAILABLE;

	// OK status can have values; hide that from the user
	if (EP_STAT_ISOK(estat))
		estat = EP_STAT_OK;
	if (!EP_STAT_ISOK(estat))
		ep_app_message(estat, "exiting with status");
	else if (!Quiet)
		fprintf(stderr, "Exiting with status OK\n");
	return exitstat;
}
