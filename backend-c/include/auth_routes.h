#ifndef BMS_AUTH_ROUTES_H
#define BMS_AUTH_ROUTES_H

#include "server_context.h"

#include <microhttpd.h>

enum MHD_Result auth_routes_handle(
    const BmsServerContext *context,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *body
);

#endif
