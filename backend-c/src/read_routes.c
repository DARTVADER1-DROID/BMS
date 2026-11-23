#include "read_routes.h"

#include "http_response.h"

#include <cjson/cJSON.h>

#include <math.h>
#include <stdbool.h>
#include <string.h>

static enum MHD_Result read_routes_health(
    const BmsServerContext *context,
    
    struct MHD_Connection *connection
)
{
    cJSON *json = http_http_create_json_object();
    enum MHD_Result result;

    cJSON_AddStringToObject(json, "message", "connected");
    cJSON_AddStringToObject(json, "implementation", "c");
    cJSON_AddBoolToObject(
        json,
        "control_enabled",
        (*context->bms).control_enabled
    );

    result = http_send_json_response(&server_context, connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result static handle_data_request(
    const BmsServerContext *context,
    
    struct MHD_Connection *connection
)
{
    cJSON *json = http_http_create_json_object();
    enum MHD_Result result;

    pthread_mutex_lock(&(*context->bms).lock);

    cJSON_AddNumberToObject(json, "voltage", (*context->bms).voltage);
    cJSON_AddNumberToObject(json, "current", (*context->bms).current);
    cJSON_AddNumberToObject(json, "temperature", (*context->bms).temperature);
    cJSON_AddNumberToObject(json, "time", (*context->bms).last_seen);
    cJSON_AddBoolToObject(
        json,
        "hardware_connection",
        (*context->bms).hardware_connected
    );

    pthread_mutex_unlock(&(*context->bms).lock);

    result = http_send_json_response(&server_context, connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result static handle_state_request(
    const BmsServerContext *context,
    
    struct MHD_Connection *connection,
    bool include_warning
)
{
    cJSON *json = http_http_create_json_object();
    enum MHD_Result result;

    pthread_mutex_lock(&(*context->bms).lock);

    cJSON_AddStringToObject(
        json,
        "state",
        bms_state_name((*context->bms).state)
    );

    if (include_warning) {
        cJSON_AddStringToObject(
            json,
            "warning_messages",
            (*context->bms).warning
        );
    }

    pthread_mutex_unlock(&(*context->bms).lock);

    result = http_send_json_response(&server_context, connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result static handle_hardware_connection_request(
    const BmsServerContext *context,
    
    struct MHD_Connection *connection
)
{
    cJSON *json = http_http_create_json_object();
    enum MHD_Result result;

    pthread_mutex_lock(&(*context->bms).lock);

    cJSON_AddBoolToObject(
        json,
        "hardware_connection",
        (*context->bms).hardware_connected
    );

    pthread_mutex_unlock(&(*context->bms).lock);

    result = http_send_json_response(&server_context, connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result static handle_estimate_request(
    const BmsServerContext *context,
    
    struct MHD_Connection *connection,
    const char *url
)
{
    cJSON *json = http_http_create_json_object();
    enum MHD_Result result;

    if (strcmp(url, "/s_o_c") == 0) {
        double state_of_charge = bms_soc(&(*context->bms));

        if (isfinite(state_of_charge)) {
            cJSON_AddNumberToObject(
                json,
                "s_o_c",
                state_of_charge
            );
        } else {
            cJSON_AddNullToObject(json, "s_o_c");
        }
    } else if (strcmp(url, "/s_o_h") == 0) {
        double state_of_health = bms_soh(&(*context->bms));

        if (isfinite(state_of_health)) {
            cJSON_AddNumberToObject(
                json,
                "s_o_h",
                state_of_health
            );
        } else {
            cJSON_AddNullToObject(json, "s_o_h");
        }
    } else {
        cJSON_AddNumberToObject(
            json,
            "r_u_l",
            bms_rul(&(*context->bms))
        );
    }

    result = http_send_json_response(&server_context, connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result read_routes_handle(
    const BmsServerContext *context,
    
    struct MHD_Connection *connection,
    const char *url
)
{
    if (strcmp(url, "/data") == 0) {
        return handle_data_request(connection);
    }

    if (strcmp(url, "/state") == 0) {
        return handle_state_request(connection, true);
    }

    if (strcmp(url, "/state-hardware") == 0) {
        return handle_state_request(connection, false);
    }

    if (strcmp(url, "/hardware_connection") == 0) {
        return handle_hardware_connection_request(connection);
    }

    if (strcmp(url, "/s_o_c") == 0
        || strcmp(url, "/s_o_h") == 0
        || strcmp(url, "/r_u_l") == 0) {
        return handle_estimate_request(connection, url);
    }

    return http_http_send_response(
            context,
            &server_context,
        connection,
        404,
        "{\"error\":\"not_found\"}"
    );
}
