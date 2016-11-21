#ifndef GDPREPLICATION_H_
#define GDPREPLICATION_H_

#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>
#include <gdp/gdp_pdu.h>
#include <gdp/gdp_stat.h>
#include <ep/ep_app.h>

extern EP_STAT replication_proto_init(void (*initCallbackFunction)(gdp_req_t *));

#endif //GDPREPLICATION_H_
