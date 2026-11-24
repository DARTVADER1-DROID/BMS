#ifndef BMS_HTTP_RESPONSE_H
#define BMS_HTTP_RESPONSE_H

#include "http/server_context.h"

#include <cjson/cJSON.h>
#include <microhttpd.h>

#include <stdbool.h>

double http_monotonic_time_seconds(void);
bool http_request_is_authorized(
    struct MHD_Connection *connection,
    const char *expected_token
);
enum MHD_Result http_send_response(
    const BmsServerContext *context,
    struct MHD_Connection *connection,
    unsigned int status_code,
    const char *body
);
enum MHD_Result http_send_auth_response(
    const BmsServerContext *context,
    struct MHD_Connection *connection,
    unsigned int status_code,
    const char *body,
    const char *session_cookie,
    const char *csrf_cookie
);
enum MHD_Result http_send_html_response(
    const BmsServerContext *context,
    struct MHD_Connection *connection,
    unsigned int status_code,
    const char *body
);
enum MHD_Result http_send_json_response(
    const BmsServerContext *context,
    struct MHD_Connection *connection,
    unsigned int status_code,
    cJSON *json
);
cJSON *http_create_json_object(void);
void http_add_current_state(const BmsServerContext *context, cJSON *json);
bool http_read_json_number(cJSON *json, const char *key, double *value);

#endif
