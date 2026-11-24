#include "http/read_routes.h"

#include "http/http_response.h"

#include <cjson/cJSON.h>

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

enum MHD_Result read_routes_health(
    const BmsServerContext *context,
    struct MHD_Connection *connection
)
{
    cJSON *json = http_create_json_object();
    enum MHD_Result result;

    cJSON_AddStringToObject(json, "message", "connected");
    cJSON_AddStringToObject(json, "implementation", "c");
    cJSON_AddBoolToObject(json, "control_enabled", context->bms->control_enabled);

    result = http_send_json_response(context, connection, 200, json);
    cJSON_Delete(json);
    return result;
}

static enum MHD_Result read_data(
    const BmsServerContext *context,
    struct MHD_Connection *connection
)
{
    cJSON *json = http_create_json_object();
    enum MHD_Result result;

    pthread_mutex_lock(&context->bms->lock);
    cJSON_AddNumberToObject(json, "voltage", context->bms->voltage);
    cJSON_AddNumberToObject(json, "current", context->bms->current);
    cJSON_AddNumberToObject(json, "temperature", context->bms->temperature);
    cJSON_AddNumberToObject(json, "time", context->bms->last_seen);
    cJSON_AddBoolToObject(json, "hardware_connection", context->bms->hardware_connected);
    pthread_mutex_unlock(&context->bms->lock);

    result = http_send_json_response(context, connection, 200, json);
    cJSON_Delete(json);
    return result;
}

static enum MHD_Result read_state(
    const BmsServerContext *context,
    struct MHD_Connection *connection,
    bool include_warning
)
{
    cJSON *json = http_create_json_object();
    enum MHD_Result result;

    pthread_mutex_lock(&context->bms->lock);
    cJSON_AddStringToObject(json, "state", bms_state_name(context->bms->state));

    if (include_warning) {
        cJSON_AddStringToObject(json, "warning_messages", context->bms->warning);
    }

    pthread_mutex_unlock(&context->bms->lock);

    result = http_send_json_response(context, connection, 200, json);
    cJSON_Delete(json);
    return result;
}

static enum MHD_Result read_hardware_connection(
    const BmsServerContext *context,
    struct MHD_Connection *connection
)
{
    cJSON *json = http_create_json_object();
    enum MHD_Result result;

    pthread_mutex_lock(&context->bms->lock);
    cJSON_AddBoolToObject(json, "hardware_connection", context->bms->hardware_connected);
    pthread_mutex_unlock(&context->bms->lock);

    result = http_send_json_response(context, connection, 200, json);
    cJSON_Delete(json);
    return result;
}

static enum MHD_Result read_estimate(
    const BmsServerContext *context,
    struct MHD_Connection *connection,
    const char *url
)
{
    cJSON *json = http_create_json_object();
    enum MHD_Result result;

    if (strcmp(url, "/s_o_c") == 0) {
        double estimate = bms_soc(context->bms);

        if (isfinite(estimate)) {
            cJSON_AddNumberToObject(json, "s_o_c", estimate);
        } else {
            cJSON_AddNullToObject(json, "s_o_c");
        }
    } else if (strcmp(url, "/s_o_h") == 0) {
        double estimate = bms_soh(context->bms);

        if (isfinite(estimate)) {
            cJSON_AddNumberToObject(json, "s_o_h", estimate);
        } else {
            cJSON_AddNullToObject(json, "s_o_h");
        }
    } else {
        cJSON_AddNumberToObject(json, "r_u_l", bms_rul(context->bms));
    }

    result = http_send_json_response(context, connection, 200, json);
    cJSON_Delete(json);
    return result;
}

enum MHD_Result read_routes_handle(
    const BmsServerContext *context,
    struct MHD_Connection *connection,
    const char *url
)
{
    if (strcmp(url, "/data") == 0) {
        return read_data(context, connection);
    }

    if (strcmp(url, "/state") == 0) {
        return read_state(context, connection, true);
    }

    if (strcmp(url, "/state-hardware") == 0) {
        return read_state(context, connection, false);
    }

    if (strcmp(url, "/hardware_connection") == 0) {
        return read_hardware_connection(context, connection);
    }

    if (strcmp(url, "/s_o_c") == 0
        || strcmp(url, "/s_o_h") == 0
        || strcmp(url, "/r_u_l") == 0) {
        return read_estimate(context, connection, url);
    }

    return http_send_response(
        context,
        connection,
        404,
        "{\"error\":\"not_found\"}"
    );
}
