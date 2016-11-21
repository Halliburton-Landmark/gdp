#include "replication.h"

static void (*callbackFunction)(gdp_req_t *);

/*
This function is called when a command of a reeived message has
"GDP_CND_PING(temporary)".
*/
EP_STAT
cmd_replication(gdp_req_t * req)
{
    //Here, callbackfunction in C++ is called. 
    (callbackFunction)(req);

    //int repliedValue = 555;
    //int length = gdp_buf_write(req->pdu->datum->dbuf, &repliedValue, sizeof(int));

    return EP_STAT_OK;
}

static struct cmdfuncs CmdFuncs2[] =
{
    { GDP_CMD_PING,          cmd_replication },
    { 0,                     NULL}
};

EP_STAT
replication_proto_init(
    void (*initCallbackFunction)(gdp_req_t *))
{
    callbackFunction = initCallbackFunction;
    _gdp_register_cmdfuncs(CmdFuncs2);
    return EP_STAT_OK;
}
