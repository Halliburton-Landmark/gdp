/* vim: set ai sw=4 sts=4 ts=4 : */

#include <gdp/gdp.h>

#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <ep/ep_time.h>

#include <stdio.h>
#include <unistd.h>

extern void	print_event(gdp_event_t *gev);

extern void EP_TYPE_PRINTFLIKE(2, 3)
		test_message(EP_STAT estat, char *fmt, ...);
