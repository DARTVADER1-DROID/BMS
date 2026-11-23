#ifndef BMS_READ_ROUTES_H
#define BMS_READ_ROUTES_H

#include "server_context.h"

#include <microhttpd.h>

enum MHD_Result read_routes_health(const BmsServerContext *context, struct MHD_Connection *connection);
enum MHD_Result read_routes_handle(const BmsServerContext *context, struct MHD_Connection *connection, const char *url);

#endif
