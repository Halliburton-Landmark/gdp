#ifndef _LOGD_RPL_H_
#define _LOGD_RPL_H_

#include <limits.h>

/*
**  Replication Services
*/

extern void     _rpl_init(gdp_gcl_t *pgcl);
extern EP_STAT  _rpl_fwd_append(gdp_req_t *req);
extern EP_STAT  _rpl_fetch_entry(
                    gdp_gcl_t *gcl,
                    gdp_datum_t *datum,
                    gdp_recno_t recno,
                    gdp_name_t to_server,
                    gdp_event_cbfunc_t cbfunc,
                    void *cbarg,
                    gdp_chan_t *chan);
extern EP_STAT  _rpl_periodic_sync_init();
extern void     _rpl_periodic_sync_reply(
                    gdp_gcl_t *gcl,
                    gdp_recno_t recno,
                    gdp_name_t to_server);
extern EP_STAT  _rpl_periodic_beacon(
                    gdp_gcl_t *gcl,
                    gdp_name_t to_server,
                    gdp_event_cbfunc_t cbfunc,
                    void *cbarg,
                    gdp_chan_t *chan);
extern void     _rpl_find_missing_log_entries(
                    gdp_gcl_t *gcl,
                    gdp_name_t to_server,
                    gdp_recno_t log_chain[][2],
                    int32_t the_num_chains);
extern int      _rpl_num_log_chain(
                    gdp_gcl_t *gcl,
                    gdp_recno_t log_chain[][2]);
extern EP_STAT  _rpl_reactive_sync(
                    gdp_recno_t start_recno,
                    int32_t numrecs,
                    gdp_gcl_t *pgcl,
                    gdp_datum_t *datum);
extern int32_t  _rpl_check_missing_entries(
                    gdp_recno_t *missing,
                    gdp_recno_t start_recno,
                    int32_t numrecs,
                    gdp_gcl_t *pgcl);
extern EP_STAT  _rpl_fetch_missing_entries(
                    gdp_recno_t *missing,
                    int32_t total_missing,
                    gdp_gcl_t *pgcl,
                    gdp_datum_t *datum);
extern void     _rpl_resp_cb(gdp_event_t *gev);
extern void     _rpl_resp_proc(gdp_event_t *gev);
extern void     _rpl_reply_ack(gdp_req_t *t, uint8_t cmd);
extern void     _rpl_add_ackedsvr(
                    gdp_req_t *req,
                    const gdp_name_t svrname,
                    const int type);
extern void     _rpl_rplsvr_freeall(struct rplsvr_head *rplsvr);
extern void     _rpl_ackedsvr_freeall(struct rplsvr_head *acksvr);
extern void     _rpl_rplcbarg_free(gdp_rplcb_t *rplcbarg);
extern uint16_t _rpl_get_number_ackedsvr(
                    const gdp_req_t *req);
extern uint16_t _rpl_get_number_acksuccess(
                    const gdp_req_t *req);

#endif // _LOGD_RPL_H_
