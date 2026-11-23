#include "control_routes.h"

#include "http_response.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

static bool read_cycle_count(
    struct MHD_Connection *connection,
    int64_t *cycle_count
)
{
    const char *query_value;
    char *end = NULL;
    long long parsed_value;

    query_value = MHD_lookup_connection_value(
        connection,
        MHD_GET_ARGUMENT_KIND,
        "count"
    );

    if (query_value == NULL || *query_value == '\0') {
        return false;
    }

    errno = 0;
    parsed_value = strtoll(query_value, &end, 10);

    if (errno != 0
        || end == query_value
        || *end != '\0'
        || parsed_value < 0
        || parsed_value > 1000000000LL) {
        return false;
    }

    *cycle_count = (int64_t)parsed_value;
    return true;
}

static enum MHD_Result handle_control_error(
    const BmsServerContext *context,
    
    struct MHD_Connection *connection,
    BmsResult result
)
{
    unsigned int status_code;
    const char *error_message;
    cJSON *json;
    enum MHD_Result response_result;

    status_code =
        result == BMS_ERR_DISABLED
            ? 503
            : (result == BMS_ERR_INVALID ? 400 : 409);

    if (result == BMS_ERR_DISABLED) {
        error_message = "control_not_commissioned";
    } else if (result == BMS_ERR_UNSAFE) {
        error_message = "safety_interlock";
    } else if (result == BMS_ERR_TRANSITION) {
        error_message = "invalid_state_transition";
    } else {
        error_message = "invalid_request";
    }

    json = http_create_json_object();
    cJSON_AddStringToObject(json, "error", error_message);
    http_add_current_state(context, json);

    response_result = http_send_json_response(
        context,
        connection,
        status_code,
        json
    );

    cJSON_Delete(json);
    return response_result;
}

enum MHD_Result control_routes_handle(
    const BmsServerContext *context,
    
    struct MHD_Connection *connection,
    const char *url
)
{
    BmsResult operation_result;
    const char *success_message;
    cJSON *json;
    enum MHD_Result response_result;

    if (strcmp(url, "/charge") == 0) {
        operation_result = bms_start_charging(context->bms);
        success_message = "Charging started";
    } else if (strcmp(url, "/discharge") == 0) {
        operation_result = bms_start_discharging(context->bms);
        success_message = "Discharging started";
    } else if (strcmp(url, "/stop") == 0) {
        operation_result = bms_stop(context->bms);
        success_message = "Battery stopped";
    } else if (strcmp(url, "/cycles") == 0) {
        int64_t cycle_count;
        const char *count_parameter = MHD_lookup_connection_value(
            connection,
            MHD_GET_ARGUMENT_KIND,
            "count"
        );

        if (count_parameter == NULL || *count_parameter == '\0') {
            return http_send_response(
                context,
                connection,
                400,
                "{\"error\":\"count_required\"}"
            );
        }

        if (!read_cycle_count(connection, &cycle_count)) {
            return http_send_response(
                context,
                connection,
                400,
                "{\"error\":\"invalid_cycle_count\"}"
            );
        }

        operation_result = bms_set_cycles(context->bms, cycle_count);
        success_message = "Cycle count updated";
    } else {
        return http_send_response(
            context,
            connection,
            404,
            "{\"error\":\"not_found\"}"
        );
    }

    if (operation_result != BMS_OK) {
        return handle_control_error(context, connection, operation_result);
    }

    json = http_create_json_object();
    http_add_current_state(context, json);
    cJSON_AddStringToObject(json, "message", success_message);

    response_result = http_send_json_response(context, connection, 200, json);
    cJSON_Delete(json);

    return response_result;
}
