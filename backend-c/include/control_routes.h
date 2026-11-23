#ifndef BMS_CONTROL_ROUTES_H
#define BMS_CONTROL_ROUTES_H

#include "server_context.h"

#include <microhttpd.h>

enum MHD_Result control_routes_handle(
    const BmsServerContext *context,
    struct MHD_Connection *connection,
    const char *url
);

#endif
