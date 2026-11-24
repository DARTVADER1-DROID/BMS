#include "http/telemetry_routes.h"

#include "http/http_response.h"

#include <cjson/cJSON.h>

#include <math.h>
#include <stdbool.h>

enum MHD_Result telemetry_routes_update(
    const BmsServerContext *context,
    
    struct MHD_Connection *connection,
    const char *body
)
{
    cJSON *json;
    double voltage;
    double current;
    double temperature;
    bool valid_telemetry;

    if (context->device_token == NULL || context->device_token[0] == '\0' || !http_request_is_authorized(connection, context->device_token)) {
        return http_send_response(
            context,
            connection,
            401,
            "{\"error\":\"unauthorized_device\"}"
        );
    }

    json = cJSON_Parse(body != NULL ? body : "");

    valid_telemetry =
        json != NULL
        && http_read_json_number(json, "voltage", &voltage)
        && http_read_json_number(json, "current", &current)
        && http_read_json_number(json, "temperature", &temperature);

    cJSON_Delete(json);

    if (!valid_telemetry) {
        return http_send_response(
            context,
            connection,
            400,
            "{\"error\":\"invalid_telemetry\"}"
        );
    }

    if (bms_update_telemetry(
            context->bms,
            voltage,
            current,
            temperature,
            http_monotonic_time_seconds()
        ) != BMS_OK) {
        return http_send_response(
            context,
            connection,
            400,
            "{\"error\":\"telemetry_out_of_range\"}"
        );
    }

    return http_send_response(
        context,
        connection,
        200,
        "{\"message\":\"Battery data updated\"}"
    );
}
