/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	Headers for the GDP Daemon
*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_stat.h>

#include <gdp/gdp.h>
#include <gdp/gdp_log.h>
#include <gdp/gdp_priv.h>
#include <gdp/gdp_protocol.h>
#include <gdp/gdp_stat.h>

#include <unistd.h>
#include <string.h>

typedef struct lev_read_cb_continue_data
{
	gdp_pkt_hdr_t *cpktbuf;
	struct bufferevent *bev;
	void *ctx;
	EP_STAT estat;
} lev_read_cb_continue_data;
