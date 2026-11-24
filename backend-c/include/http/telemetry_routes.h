#ifndef BMS_TELEMETRY_ROUTES_H
#define BMS_TELEMETRY_ROUTES_H

#include "http/server_context.h"

#include <microhttpd.h>

enum MHD_Result telemetry_routes_update(
    const BmsServerContext *context,
    struct MHD_Connection *connection,
    const char *body
);

#endif
