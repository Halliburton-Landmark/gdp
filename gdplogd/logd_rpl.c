#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>
#include <gdplogd/logd.h>
#include <gdplogd/logd_disklog.h>
#include <gdplogd/logd_rpl.h>

#include <ep/ep_dbg.h>

#include <string.h>

static EP_DBG Dbg = EP_DBG_INIT("gdplogd.rplservice",
       						"GDP GCL Replication Service");

EP_THR       _GdpRplSyncLoopThread;

void
_rpl_init(gdp_gcl_t *gcl) //This function is tentatively prepared to set a set of inital replica servers until a reconfiguration interface is developed.
{
    const char *svrnames = NULL;
    svrnames = ep_adm_getstrparam("swarm.gdp.rpl.svr", NULL);

    if (svrnames == NULL)
    {
        //There is no parameter on swarm.gdp.rpl.svr.
        return;
    }

    char rbuf[500] = "";
    strlcat(rbuf,
            svrnames,
            sizeof rbuf); //Copy data into another pointer not to modify original data.

    char *nameptr;
    char *nextptr;

    for (nameptr = rbuf; nameptr != NULL; nameptr = nextptr)
    {
        nextptr = strchr(nameptr, ';');
        if (nextptr != NULL)
            *nextptr++ = '\0';

        nameptr = &nameptr[strspn(nameptr, " \t")];

        gdp_rplsvr_t *svr = ep_mem_zalloc(sizeof (*svr));
        gdp_parse_name(nameptr, svr->svrname);
        if (memcmp(svr->svrname, _GdpMyRoutingName, sizeof _GdpMyRoutingName) != 0)
        {
            //Add log server names without myself.
            gdp_printable_name(svr->svrname, svr->svrpname);
            ep_thr_mutex_lock(&gcl->mutex);
            svr->stat = GDP_RPL_INIT;
            LIST_INSERT_HEAD(&gcl->rplsvr, svr, svrlist);
            ep_thr_mutex_unlock(&gcl->mutex);
        }
    }

    return;
}

int
_rpl_num_log_chain(
    gdp_gcl_t *gcl,
    gdp_recno_t log_chain[][2])
{
    gcl_physinfo_t *phys = gcl->x->physinfo;
    int num_chains = 1;

    if (phys->max_recno == 0) {
        return num_chains;
    }


    int chain_start = 0;
    int initialize = 1;
    gdp_recno_t recno;
    EP_STAT estat = EP_STAT_OK;

	//kaz//ep_thr_rwlock_rdlock(&phys->lock);
    for (recno = phys->min_recno; recno <= phys->max_recno; recno++)
    {
        EP_ASSERT_POINTER_VALID(gcl);
        gdp_datum_t *datum = gdp_datum_new();
        datum->recno = recno;
        estat = gcl->x->physimpl->read_by_recno(gcl, datum);

        if (EP_STAT_IS_SAME(estat, EP_STAT_OK)) {
            if (initialize == 1 || chain_start == 1) {

                if (chain_start == 1)
                    num_chains++;

                log_chain[num_chains-1][0] = recno;
                log_chain[num_chains-1][1] = recno;
                chain_start = 0;
                initialize = 0;
            }
            else {
                log_chain[num_chains-1][1] = recno;
            }
        }
        else if (EP_STAT_IS_SAME(estat, GDP_STAT_RECORD_MISSING)) {
            chain_start = 1;
        }
        else {
            // do nothing
        }
    }
	//kaz//ep_thr_rwlock_unlock(&phys->lock);

    return num_chains;
}

static void
_rpl_periodic_sync(gdp_name_t gclname, void *arg)
{
    gdp_rplsvr_t *currentsvr;
    gdp_rplsvr_t *nextsvr;

    gdp_gcl_t *gcl = _gdp_gcl_cache_get(gclname, 0);
    if (gcl == NULL)
        return;

    gdp_pname_t pname;
    gdp_printable_name(gclname, pname);

    // calculate the number of replica servers
    int num_svr = 0;
    for (currentsvr = LIST_FIRST(&gcl->rplsvr); currentsvr != NULL; currentsvr = nextsvr)
    {
        nextsvr = LIST_NEXT(currentsvr, svrlist);
        num_svr++;
    }

    // generate randome number with upper limit of num_svr
    int random = rand() % num_svr + 1;
    if (1 > random || num_svr < random) {
        ep_app_error("unexpected number is generated");
        exit(1);
    }

    // choose one server randomly out of replica servers
    int cnt = 1;
    for (currentsvr = LIST_FIRST(&gcl->rplsvr); currentsvr != NULL; currentsvr = nextsvr)
    {
        nextsvr = LIST_NEXT(currentsvr, svrlist);
        if (cnt == random) {
            EP_STAT estat = _rpl_periodic_beacon(gcl, currentsvr->svrname, NULL, NULL, NULL);
        }
        cnt++;
    }

    //kaz//if (gcl != NULL)
    //kaz//    _gdp_gcl_decref(&gcl);
}

static void
_rpl_periodic_sync_all()
{
    GdpDiskImpl.foreach(_rpl_periodic_sync, NULL);
}

static void *
_rpl_sync_thread()
{

	long interval = ep_adm_getlongparam("swarm.gdp.rpl.intervalsec", 10L);

    for (;;)
    {
		if (interval > 0)
			ep_time_nanosleep(interval * 1000000000LL);

        _rpl_periodic_sync_all();
    }
}


EP_STAT
_rpl_periodic_sync_init()
{
    int err = ep_thr_spawn(&_GdpRplSyncLoopThread, _rpl_sync_thread, NULL);

    if (err != 0)
    {
        ep_log(ep_stat_from_errno(err),
               "_rpl_sync_thread: cannot start synchronization thread");
        return GDP_STAT_PROTOCOL_FAIL;
    }
    else {

        return EP_STAT_OK;

    }
}

/*
** _rpl_periodic_sync_reply --- This sends missing log entries for a log server.
**
**          PERIODICSYNC_REPLY does not require an acknowledgement.
**          If some log entris fail to arrive then these log entries
**          suppose to be retried at a next round.
 */

void
_rpl_periodic_sync_reply(
    gdp_gcl_t *gcl,
    gdp_recno_t recno,
    gdp_name_t to_server)
{
    EP_STAT estat = EP_STAT_OK;
    gdp_req_t *req;
    uint32_t reqflags = 0; // no need to wait for ACK/NAK
    gdp_pname_t src_name;
    gdp_pname_t dst_name;
    gdp_printable_name(_GdpMyRoutingName, src_name);
    gdp_printable_name(to_server, dst_name);

	// create a new request and point it at the routing layer
	estat = _gdp_req_new(GDP_CMD_SYNC_REPLY, gcl, NULL, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

    // put gclname into data buffer
    gdp_buf_write(req->pdu->datum->dbuf, req->pdu->dst, sizeof (req->pdu->dst));

    // put destination into pdu dst
	memcpy(req->pdu->dst, to_server, sizeof req->pdu->dst);

    // get a log entry corresponding to recno
    gdp_datum_t *datum = gdp_datum_new();
    datum->recno = recno;
    estat = gcl->x->physimpl->read_by_recno(gcl, datum);

	// copy the existing datum, including metadata
    // read_by_recno is now calling gdp_buf_reset, which prevents from writing gclname before it.
	size_t l = gdp_buf_getlength(datum->dbuf);
	gdp_buf_write(req->pdu->datum->dbuf, gdp_buf_getptr(datum->dbuf, l), l);
	req->pdu->datum->recno = datum->recno;
	req->pdu->datum->ts = datum->ts;
	req->pdu->datum->sigmdalg = datum->sigmdalg;
	req->pdu->datum->siglen = datum->siglen;
	if (req->pdu->datum->sig != NULL)
		gdp_buf_free(req->pdu->datum->sig);
	req->pdu->datum->sig = NULL;
	if (datum->sig != NULL)
	{
		l = gdp_buf_getlength(datum->sig);
		req->pdu->datum->sig = gdp_buf_new();
		gdp_buf_write(req->pdu->datum->sig, gdp_buf_getptr(datum->sig, l), l);
	}

    // send the request
    estat = _gdp_req_send(req);

    gdp_datum_free(datum);
    req->pdu->datum = NULL;
    if (!EP_STAT_ISOK(estat))
    {
        _gdp_req_free(&req);
    }

fail0:
	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];

		ep_dbg_printf("_req_periodic_sync_reply => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	return estat;
}

/*
** _rpl_find_missing_log_entries --- find missing log entries for periodic sync
**
**          It finds missing log entries by comparing sets of log chains from
**          another log server with log entries in its local disk.
**          If there are missing log entries found, these are sent through
**          _rpl_periodic_sync_reply.
 */

void
_rpl_find_missing_log_entries(
    gdp_gcl_t *gcl,
    gdp_name_t to_server,
    gdp_recno_t log_chain[][2],
    int32_t the_num_chains)
{
    gdp_recno_t current_recno = 1;
    int32_t cnt;
    for (cnt = 1; cnt <= the_num_chains; cnt++) {
        if (cnt == 1) {
            //Check tail log entries//--->
            for (current_recno = 1; current_recno < log_chain[cnt-1][0]; current_recno++) {

                _rpl_periodic_sync_reply(gcl, current_recno, to_server); //works as async_append.

            }
            //<---
        }

        if (cnt != the_num_chains) {
            //Check hole log entries//--->
            for (current_recno = log_chain[cnt-1][1] + 1; current_recno <= log_chain[cnt][0]; current_recno++) {

                _rpl_periodic_sync_reply(gcl, current_recno, to_server); //works as async_append.

            }
            //<---
        }
        else {
            //Check head log entries//
            for (current_recno = (int32_t)log_chain[cnt-1][1] + 1; current_recno <= gcl->x->physinfo->max_recno; current_recno++) {

                _rpl_periodic_sync_reply(gcl, current_recno, to_server); //works as async_append.

            }
        }
    }
}

/*
** _rpl_fwd_append --- forward an append request to replica servers
**
 */

EP_STAT
_rpl_fwd_append(gdp_req_t *req)
{
    EP_STAT estat = EP_STAT_OK;
    gdp_rplsvr_t *currentsvr;
    gdp_rplsvr_t *nextsvr;

    req->ackcnt = ep_adm_getintparam("swarm.gdp.rpl.ackcnt", 1); //The number of acks before ack reply towards a writer. Should be under gdp_gcl in the future.
    req->flags |= GDP_REQ_PERSIST; //Keep this req till the number of acked server is satisfied. Timeout is needed in the future to avoid memory leak.
    uint32_t reqflags = 0; 
    char ebuf[200];
    void (*cbfunc)(gdp_event_t *) = NULL;
    cbfunc = _rpl_resp_cb;

    ep_thr_mutex_lock(&req->gcl->mutex);
    currentsvr = LIST_FIRST(&req->gcl->rplsvr);
    ep_thr_mutex_unlock(&req->gcl->mutex);


    for (; currentsvr != NULL; currentsvr = nextsvr)
    {
        nextsvr = LIST_NEXT(currentsvr, svrlist);
        if (memcmp(currentsvr->svrname, _GdpMyRoutingName, sizeof _GdpMyRoutingName) != 0)
        {
            gdp_rplcb_t *cbarg = ep_mem_zalloc(sizeof (*cbarg));
            cbarg->req = req;
            memcpy(cbarg->svrname, currentsvr->svrname, sizeof cbarg->svrname);
            LIST_INSERT_HEAD(&req->rplcb, cbarg, cblist);
            estat = _gdp_gcl_fwd_append(req->gcl, req->pdu->datum, currentsvr->svrname, cbfunc, cbarg, NULL, reqflags);
            if (!EP_STAT_ISOK(estat))
            {
                ep_app_fatal("Cannot rpl_fwd_append:\n\t%s",
                        ep_stat_tostr(estat, ebuf, sizeof ebuf));
            }
            else {
                req->fwdcnt++;
                ep_app_info("do_fwd_append (%d): %s",
		                req->fwdcnt, ep_stat_tostr(estat, ebuf, sizeof ebuf));
            }
        }
    }

    //No dbuf flush here because it is delivered to subscribers later. Timing to flush is obscure.
    //gd_buf_drain(datum, gdp_buf_getlength(datum));

    if (req->ackcnt == 1) {
        //Reply an ack in gdp_main.c later.
        req->acksnt = 1;
    }

    return estat;
}

/*
**  _rpl_fetch_entry --- fetch a log entry from another server
**
**          It replies success if a request is sent out.
**          This function works like _gdp_gcl_async_read for synchronization between log servers.
**
*/

EP_STAT
_rpl_fetch_entry(
    gdp_gcl_t *gcl,
    gdp_datum_t *datum,
    gdp_recno_t recno,
    gdp_name_t to_server,
    gdp_event_cbfunc_t cbfunc,
    void *cbarg,
    gdp_chan_t *chan)
{
	EP_STAT estat;
	gdp_req_t *req;

	// sanity checks
	if (memcmp(to_server, _GdpMyRoutingName, sizeof _GdpMyRoutingName) == 0)
	{
		// forwarding to ourselves: bad idea
		EP_ASSERT_FAILURE("_gdp_gcl_fetch entry: forwarding to myself");
	}

	estat = _gdp_req_new(GDP_CMD_REACTIVESYNC, gcl, chan, NULL, GDP_REQ_ASYNCIO, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// arrange for responses to appear as events or callbacks
	_gdp_event_setcb(req, cbfunc, cbarg);

	// add the actual target GDP name to the data
	gdp_buf_write(req->pdu->datum->dbuf, req->pdu->dst, sizeof req->pdu->dst);

	// change the destination to be the final server, not the GCL
	memcpy(req->pdu->dst, to_server, sizeof req->pdu->dst);

	// copy the existing datum, including metadata
	size_t l = gdp_buf_getlength(datum->dbuf);
	gdp_buf_write(req->pdu->datum->dbuf, gdp_buf_getptr(datum->dbuf, l), l);
	req->pdu->datum->recno = recno;

	// XXX should we take a callback function?

	estat = _gdp_req_send(req);

	// unlike append_async, we leave the datum intact

	// cleanup
	req->pdu->datum = NULL;			// owned by caller
	if (!EP_STAT_ISOK(estat))
	{
		_gdp_req_free(&req);
	}
	else
	{
		req->state = GDP_REQ_IDLE;
		ep_thr_cond_signal(&req->cond);
		_gdp_req_unlock(req);
	}

fail0:
	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];
		ep_dbg_printf("_rpl_fetch_entry => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}

/*
**  _rpl_reactive_sync --- synchronization with other replica servers
**
**          It replies success if all missing log entries are filled in.
**          Currently, it is supposed be perfectly worked. Otherwise,
**          the following read will fail at missing log entries.
*/

EP_STAT
_rpl_reactive_sync(
    gdp_recno_t start_recno,
    int32_t numrecs,
    gdp_gcl_t *pgcl,
    gdp_datum_t *datum)
{
    EP_STAT estat = EP_STAT_OK;
    gdp_recno_t missing[1000];
    int32_t total_missing = 0;

    // check missing log entries here//
    total_missing = _rpl_check_missing_entries(missing, start_recno, numrecs, pgcl, datum);

    // fetch missing log entries from other servers//
    if (total_missing > 0)
        estat = _rpl_fetch_missing_entries(missing, total_missing, pgcl, datum);

    // if all missing log entries are filled in, reply EP_STAT_OK.//
    return estat;
}

/*
**  _rpl_fetch_missing_entries --- fetch missing log entries from other log servers.
**
**          It replies success if all missing log entries are filled in.
**          It replies failure if all missing log entries are not filled in and timed out.
*/

EP_STAT
_rpl_fetch_missing_entries(
    gdp_recno_t *missing_recno,
    int32_t total_missing,
    gdp_gcl_t *pgcl,
    gdp_datum_t *datum)
{
    EP_STAT estat = EP_STAT_OK;
    gdp_rplsvr_t *currentsvr;
    gdp_rplsvr_t *nextsvr;

    // make gdp_req temporary for cmd_append
    gdp_req_t *req;
    estat = _gdp_req_new(GDP_CMD_APPEND, pgcl, NULL, NULL, NULL, &req); // make temporarily for cmd_append
    EP_STAT_CHECK(estat, goto fail0);

    char ebuf[200];

    ep_thr_mutex_lock(&pgcl->mutex);
    currentsvr = LIST_FIRST(&pgcl->rplsvr);
    ep_thr_mutex_unlock(&pgcl->mutex);

    const char *lname;
    int fwdcnt = 1;
    int32_t current_missing = total_missing;
    for (; currentsvr != NULL; currentsvr = nextsvr)
    {
        nextsvr = LIST_NEXT(currentsvr, svrlist);
        lname = currentsvr->svrpname;

        int cnt;
        for (cnt = 0; cnt < total_missing; cnt++) {
            //TBD//Multiread is more efficient here.
            estat = _rpl_fetch_entry(pgcl, datum, missing_recno[cnt], currentsvr->svrname, NULL, NULL, NULL);

            if (!EP_STAT_ISOK(estat))
            {
                ep_app_fatal("Cannot _gdp_gcl_fwd_read:\n\t%s",
                        ep_stat_tostr(estat, ebuf, sizeof ebuf));
            }
            else {
                ep_app_info("_gdp_gcl_fwd_read (%d): %s",
	                    fwdcnt, ep_stat_tostr(estat, ebuf, sizeof ebuf));
            }
        }

        //wait for response//
        gdp_event_t *gev;
        //TBD//EP_TIME_SPEC timeout = { 0, 100000000, 0.1 }; //Need to set timeout.
        //TBD//while ( ((gev = gdp_event_next(pgcl, &timeout)) != NULL)

        while ( (current_missing != 0)
                && ((gev = gdp_event_next(pgcl, NULL)) != NULL) )
        {
            gdp_datum_t *datum = gdp_event_getdatum(gev);
            int cnt;
            int match = 0;

            switch (gdp_event_gettype(gev))
            {
		    case GDP_EVENT_DATA:
                for (cnt = 0; cnt < current_missing; cnt++) {
                    if (missing_recno[cnt] = datum->recno)
                    {
                        missing_recno[cnt] = 0; //Set 0 because it is filled in.
                        match = 1;
                        break;
                    }
                }
                if (match) {
                    while (cnt < current_missing) {
                        missing_recno[cnt] = missing_recno[cnt + 1];
                        cnt++;
                    }
                    current_missing--;

                    // append missing log entries into the disk//--->
                    req->pdu->datum = datum;
                    req->fwdflag = 1; // not to forward this request
                    cmd_append(req);
                    //<---
                }
                break;

            case GDP_EVENT_EOS:
            case GDP_EVENT_SHUTDOWN:
                ep_app_error("unexpected end of fwd_read");
                estat = EP_STAT_END_OF_FILE;
                goto fail1;

            default:
                // ignore
                break;
            }
            gdp_event_free(gev);
        }

        if (current_missing == 0)
        {
            estat = EP_STAT_OK;
            break; //No need to continue
        }
        fwdcnt++;
    }
    //_gdp_req_free(&req); //This calls gdp_gcl_decref then does not work


fail0:
	if (ep_dbg_test(Dbg, 10))
	{
		ep_dbg_printf("_rpl_fetch_missing_entries => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
fail1:
    if (false)
    {
fail2:
        ep_app_error("could not forward read request to %s", lname);
    }

    return estat;
}

/*
**  _rpl_check_missing_entries --- return the number of missing log entries
**
**          It checks missing log entries in "numrecs" log entries starting
**           from recno of "start_recno".
*/

int32_t
_rpl_check_missing_entries(
    gdp_recno_t *missing,
    gdp_recno_t start_recno,
    int32_t numrecs,
    gdp_gcl_t *pgcl,
    gdp_datum_t *datum)
{
    EP_STAT estat;
    int32_t total_missing = 0;
    datum->recno = start_recno;

    while (numrecs >= 0) {

        estat = pgcl->x->physimpl->read_by_recno(pgcl, datum); //This function must be expensive though...

        if (EP_STAT_ISOK(estat))
        {
            //do nothing.
        }
        else if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOTFOUND))
        {
            //Store record number for a following fetching process.
            //This log entry is for data freshness.
        }
        else if (EP_STAT_IS_SAME(estat, GDP_STAT_RECORD_MISSING))
        {
            //Store record number for a following fetching process.
            //This log entry is for a log hole.
            missing[total_missing] = datum->recno;
            total_missing++;
        }
        else
        {
            //ep_log(estat, "_rpl_reactive_sync(%s): this log entry is not supposed to be. recno %" PRIgdp_recno,
            //        pgcl->pname, datum->recno);
            //Not supported now.
        }

        //required??
        gdp_buf_reset(datum->dbuf);

        numrecs--;
        datum->recno++;
    }

    return total_missing;
}

/*
**  _rpl_periodic_beacon --- notify of having log entries to a log server for periodic synchronization
**
**          It replies success if a beacon is sent out.
**          This function works like _gdp_gcl_async_read for synchronization between log servers.
**
*/

EP_STAT
_rpl_periodic_beacon(
    gdp_gcl_t *gcl,
    gdp_name_t to_server,
    gdp_event_cbfunc_t cbfunc,
    void *cbarg,
    gdp_chan_t *chan)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;

	// sanity checks
	if (memcmp(to_server, _GdpMyRoutingName, sizeof _GdpMyRoutingName) == 0)
	{
		// forwarding to ourselves: bad idea
		EP_ASSERT_FAILURE("_gdp_gcl_fetch entry: forwarding to myself");
	}

	estat = _gdp_req_new(GDP_CMD_PERIODICSYNC, gcl, chan, NULL, NULL, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// arrange for responses to appear as events or callbacks
	_gdp_event_setcb(req, cbfunc, cbarg);

	// add the actual target GDP name to the data
	gdp_buf_write(req->pdu->datum->dbuf, req->pdu->dst, sizeof req->pdu->dst);


    int the_num_chains; // The number of chains.
    gdp_recno_t log_chain[UCHAR_MAX][2]; //If INT_MAX is used, thread causes a segmentation fault. Now, it allows only 255 log chains.

    the_num_chains = _rpl_num_log_chain(gcl, log_chain);

    //Put the number of chains//
	gdp_buf_write(req->pdu->datum->dbuf, &the_num_chains, sizeof the_num_chains);

    //Put log chain information//--->
    int cnt;
    for (cnt = 1; cnt <= the_num_chains; cnt++) {
        gdp_buf_write(req->pdu->datum->dbuf, &log_chain[cnt-1][0], sizeof(gdp_recno_t));
        gdp_buf_write(req->pdu->datum->dbuf, &log_chain[cnt-1][1], sizeof(gdp_recno_t));
    }
    //<---

	// change the destination to be the final server, not the GCL
	memcpy(req->pdu->dst, to_server, sizeof req->pdu->dst);

	estat = _gdp_req_send(req);
	// unlike append_async, we leave the datum intact

	// cleanup
	req->pdu->datum = NULL;			// owned by caller
	if (!EP_STAT_ISOK(estat))
	{
		_gdp_req_free(&req);
	}
	else
	{
		req->state = GDP_REQ_IDLE;
		ep_thr_cond_signal(&req->cond);
		_gdp_req_unlock(req);
	}

fail0:
	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];
		ep_dbg_printf("_rpl_periodic_beacon => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**  _rpl_resp_cb --- start processing an incoming response for the forwarded APPEND requests.
*/

void
_rpl_resp_cb(gdp_event_t *gev)
{
    gdp_event_print(gev, stdout, 3);

    if (gdp_event_gettype(gev) == GDP_EVENT_CREATED || gdp_event_gettype(gev) == GDP_EVENT_FAILURE)
    {
        gdp_gcl_t *gcl = gdp_event_getgcl(gev);
        if (gcl != NULL) {
            _rpl_resp_proc(gev);
        }
    }
    else if (gdp_event_gettype(gev) == GDP_EVENT_EOS)
    {
        //It should show an error message.
    }

    gdp_event_free(gev);
}

void
_rpl_resp_proc(gdp_event_t *gev)
{
    gdp_rplcb_t *cbarg = gdp_event_getudata(gev);

    //Add a log server into acksvr//--->
    _rpl_add_ackedsvr(cbarg->req, cbarg->svrname, gdp_event_gettype(gev));
    //<---

    //Check whether timing is to reply an ack to a client or not//--->
    uint16_t acktotal = _rpl_get_number_ackedsvr(cbarg->req) + 1;
    uint16_t acksuccess = _rpl_get_number_acksuccess(cbarg->req) + 1;
    uint16_t ackfail = acktotal - acksuccess;
    uint16_t svrtotal = cbarg->req->fwdcnt + 1;
    uint16_t quorumwrite = cbarg->req->ackcnt;

    if (cbarg->req->acksnt != 1) {
        if (acksuccess >= quorumwrite) {
            _rpl_reply_ack(cbarg->req, GDP_ACK_CREATED);
            cbarg->req->acksnt = 1;
        }
        else if (ackfail > (svrtotal - quorumwrite)) {
            _rpl_reply_ack(cbarg->req, GDP_NAK_S_REPLICATE_FAIL);
            cbarg->req->acksnt = 1;
        }
        else {
            //Do not send anything. Keep going.
        }
    }

    if (acktotal == svrtotal)
    {
        _rpl_ackedsvr_freeall(&cbarg->req->acksvr);
        _gdp_req_free(&cbarg->req);
    }
    //<---

    _rpl_rplcbarg_free(cbarg);
}

void
_rpl_reply_ack(gdp_req_t *req, uint8_t cmd)
{
    req->pdu->cmd = cmd;
    req->stat = _gdp_pdu_out(req->pdu, req->chan, NULL);
}

void
_rpl_add_ackedsvr(gdp_req_t *req, const gdp_name_t svrname, int type)
{
    //stop-gap implementation. This should check whether incoming ack is corresponding to a sent pdu.

    if (!gdp_name_is_valid(svrname))
    {
        printf("_rpl_add_ackedsvr. Error. Unexpected svrname is chosen.\n");
        exit(1);
    }
    gdp_rplsvr_t *svr;
    svr = ep_mem_zalloc(sizeof (*svr));

    memcpy(svr->svrname, svrname, sizeof svr->svrname);
    gdp_printable_name(svr->svrname, svr->svrpname);

    _gdp_req_lock(req);
    //Need to check duplication here//--->
    gdp_rplsvr_t *current;
    gdp_rplsvr_t *next;
    int flag = 0;
    for (current = LIST_FIRST(&req->acksvr); current != NULL; current = next)
    {
        if (GDP_NAME_SAME(svr->svrname, current->svrname)) {
            flag = 1;
        }
        next = LIST_NEXT(current, acklist);
    }
    //<---
    if (flag == 0) {
        if (type == GDP_EVENT_CREATED) {
            svr->stat = GDP_RPL_SUCCESS;
        }
        else if (type == GDP_EVENT_FAILURE) {
            svr->stat = GDP_RPL_FAILURE;
        }
        else {
            if (ep_dbg_test(Dbg, 1))
            {
                ep_dbg_printf("_rpl_add_ackedsvr. Error. Unknown event type is detect.\n", svr->svrpname);
            }
            exit(1);
        }
        LIST_INSERT_HEAD(&req->acksvr, svr, acklist);
    }
    else {
        //Do nothing.
        if (ep_dbg_test(Dbg, 1))
		{
            ep_dbg_printf("_rpl_add_ackedsvr: duplicated ack from %s server is detected.\n", svr->svrpname);
        }
    }
    _gdp_req_unlock(req);
}

uint16_t
_rpl_get_number_ackedsvr(const gdp_req_t *req)
{
    //Need to check whether log servers in the list is corresponding to any log server in sending candidates.
    uint16_t count = 0;
    gdp_rplsvr_t *current;
    gdp_rplsvr_t *next;
    for (current = LIST_FIRST(&req->acksvr); current != NULL; current = next)
    {
        next = LIST_NEXT(current, acklist);
        count++;
    }
    return count;
}

uint16_t
_rpl_get_number_acksuccess(const gdp_req_t *req)
{
    uint16_t count = 0;
    gdp_rplsvr_t *current;
    gdp_rplsvr_t *next;
    for (current = LIST_FIRST(&req->acksvr); current != NULL; current = next)
    {
        next = LIST_NEXT(current, acklist);
        if (current->stat == GDP_RPL_SUCCESS)
            count++;
    }
    return count;
}

void
_rpl_rplsvr_freeall(struct rplsvr_head *rplsvr)
{
    gdp_rplsvr_t *current;
    gdp_rplsvr_t *next;
    for (current = LIST_FIRST(rplsvr); current != NULL; current = next)
    {
        next = LIST_NEXT(current, svrlist);
        LIST_REMOVE(current, svrlist);
        current = NULL;
    }
}

void
_rpl_ackedsvr_freeall(struct rplsvr_head *acksvr)
{
    gdp_rplsvr_t *current;
    gdp_rplsvr_t *next;
    for (current = LIST_FIRST(acksvr); current != NULL; current = next)
    {
        next = LIST_NEXT(current, acklist);
        LIST_REMOVE(current, acklist);
        current = NULL;
    }
}

void
_rpl_rplcbarg_free(gdp_rplcb_t *rplcbarg)
{
    LIST_REMOVE(rplcbarg, cblist);
    rplcbarg = NULL;
}
