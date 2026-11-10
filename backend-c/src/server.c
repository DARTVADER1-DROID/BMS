#define _POSIX_C_SOURCE 200809L

#include "bms.h"

#include <microhttpd.h>
#include <cjson/cJSON.h>

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define REQUEST_BODY_LIMIT 8192

static Bms g_bms;
static volatile sig_atomic_t server_running = 1;

static const char *api_token;
static const char *device_token;
static const char *cors_origin;

typedef struct {
    char body[REQUEST_BODY_LIMIT + 1];
    size_t used;
} HttpRequest;

static double monotonic_time_seconds(void)
{
    struct timespec timestamp;

    clock_gettime(CLOCK_MONOTONIC, &timestamp);

    return (double)timestamp.tv_sec
        + (double)timestamp.tv_nsec / 1e9;
}

static void handle_shutdown_signal(int signal_number)
{
    (void)signal_number;
    server_running = 0;
}

static bool tokens_match(const char *provided, const char *expected)
{
    size_t provided_length;
    size_t expected_length;
    unsigned char difference = 0;

    if (expected == NULL || *expected == '\0') {
        return true;
    }

    if (provided == NULL) {
        return false;
    }

    provided_length = strlen(provided);
    expected_length = strlen(expected);

    if (provided_length != expected_length) {
        return false;
    }

    for (size_t index = 0; index < provided_length; index++) {
        difference |= (unsigned char)(provided[index] ^ expected[index]);
    }

    return difference == 0;
}

static bool request_is_authorized(
    struct MHD_Connection *connection,
    const char *expected_token
)
{
    const char *authorization_header;

    if (expected_token == NULL || *expected_token == '\0') {
        return true;
    }

    authorization_header = MHD_lookup_connection_value(
        connection,
        MHD_HEADER_KIND,
        "Authorization"
    );

    if (authorization_header == NULL
        || strncmp(authorization_header, "Bearer ", 7) != 0) {
        return false;
    }

    return tokens_match(authorization_header + 7, expected_token);
}

static enum MHD_Result send_response(
    struct MHD_Connection *connection,
    unsigned int status_code,
    const char *body
)
{
    struct MHD_Response *http_response;
    enum MHD_Result result;

    http_response = MHD_create_response_from_buffer(
        strlen(body),
        (void *)body,
        MHD_RESPMEM_MUST_COPY
    );

    if (http_response == NULL) {
        return MHD_NO;
    }

    MHD_add_response_header(
        http_response,
        "Content-Type",
        "application/json; charset=utf-8"
    );
    MHD_add_response_header(
        http_response,
        "Cache-Control",
        "no-store"
    );
    MHD_add_response_header(
        http_response,
        "X-Content-Type-Options",
        "nosniff"
    );
    MHD_add_response_header(
        http_response,
        "Access-Control-Allow-Origin",
        cors_origin != NULL ? cors_origin : "*"
    );
    MHD_add_response_header(
        http_response,
        "Access-Control-Allow-Headers",
        "Content-Type, Authorization"
    );
    MHD_add_response_header(
        http_response,
        "Access-Control-Allow-Methods",
        "GET, POST, OPTIONS"
    );

    result = MHD_queue_response(
        connection,
        status_code,
        http_response
    );

    MHD_destroy_response(http_response);
    return result;
}

static enum MHD_Result send_json_response(
    struct MHD_Connection *connection,
    unsigned int status_code,
    cJSON *json
)
{
    char *serialized_json;
    enum MHD_Result result;

    serialized_json = cJSON_PrintUnformatted(json);

    if (serialized_json == NULL) {
        return send_response(
            connection,
            500,
            "{\"error\":\"serialization_failed\"}"
        );
    }

    result = send_response(connection, status_code, serialized_json);
    free(serialized_json);

    return result;
}

static cJSON *create_json_object(void)
{
    return cJSON_CreateObject();
}

static void add_current_state(cJSON *json)
{
    pthread_mutex_lock(&g_bms.lock);

    cJSON_AddStringToObject(
        json,
        "state",
        bms_state_name(g_bms.state)
    );

    pthread_mutex_unlock(&g_bms.lock);
}

static bool read_json_number(
    cJSON *json,
    const char *key,
    double *value
)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(json, key);

    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble)) {
        return false;
    }

    *value = item->valuedouble;
    return true;
}

static enum MHD_Result handle_health_request(
    struct MHD_Connection *connection
)
{
    cJSON *json = create_json_object();
    enum MHD_Result result;

    cJSON_AddStringToObject(json, "message", "connected");
    cJSON_AddStringToObject(json, "implementation", "c");
    cJSON_AddBoolToObject(
        json,
        "control_enabled",
        g_bms.control_enabled
    );

    result = send_json_response(connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result handle_data_request(
    struct MHD_Connection *connection
)
{
    cJSON *json = create_json_object();
    enum MHD_Result result;

    pthread_mutex_lock(&g_bms.lock);

    cJSON_AddNumberToObject(json, "voltage", g_bms.voltage);
    cJSON_AddNumberToObject(json, "current", g_bms.current);
    cJSON_AddNumberToObject(json, "temperature", g_bms.temperature);
    cJSON_AddNumberToObject(json, "time", g_bms.last_seen);
    cJSON_AddBoolToObject(
        json,
        "hardware_connection",
        g_bms.hardware_connected
    );

    pthread_mutex_unlock(&g_bms.lock);

    result = send_json_response(connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result handle_state_request(
    struct MHD_Connection *connection,
    bool include_warning
)
{
    cJSON *json = create_json_object();
    enum MHD_Result result;

    pthread_mutex_lock(&g_bms.lock);

    cJSON_AddStringToObject(
        json,
        "state",
        bms_state_name(g_bms.state)
    );

    if (include_warning) {
        cJSON_AddStringToObject(
            json,
            "warning_messages",
            g_bms.warning
        );
    }

    pthread_mutex_unlock(&g_bms.lock);

    result = send_json_response(connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result handle_hardware_connection_request(
    struct MHD_Connection *connection
)
{
    cJSON *json = create_json_object();
    enum MHD_Result result;

    pthread_mutex_lock(&g_bms.lock);

    cJSON_AddBoolToObject(
        json,
        "hardware_connection",
        g_bms.hardware_connected
    );

    pthread_mutex_unlock(&g_bms.lock);

    result = send_json_response(connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result handle_estimate_request(
    struct MHD_Connection *connection,
    const char *url
)
{
    cJSON *json = create_json_object();
    enum MHD_Result result;

    if (strcmp(url, "/s_o_c") == 0) {
        double state_of_charge = bms_soc(&g_bms);

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
        double state_of_health = bms_soh(&g_bms);

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
            bms_rul(&g_bms)
        );
    }

    result = send_json_response(connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result handle_get_request(
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

    return send_response(
        connection,
        404,
        "{\"error\":\"not_found\"}"
    );
}

static enum MHD_Result handle_telemetry_update(
    struct MHD_Connection *connection,
    const char *body
)
{
    cJSON *json;
    double voltage;
    double current;
    double temperature;
    bool valid_telemetry;

    if (!request_is_authorized(connection, device_token)) {
        return send_response(
            connection,
            401,
            "{\"error\":\"unauthorized_device\"}"
        );
    }

    json = cJSON_Parse(body != NULL ? body : "");

    valid_telemetry =
        json != NULL
        && read_json_number(json, "voltage", &voltage)
        && read_json_number(json, "current", &current)
        && read_json_number(json, "temperature", &temperature);

    cJSON_Delete(json);

    if (!valid_telemetry) {
        return send_response(
            connection,
            400,
            "{\"error\":\"invalid_telemetry\"}"
        );
    }

    if (bms_update_telemetry(
            &g_bms,
            voltage,
            current,
            temperature,
            monotonic_time_seconds()
        ) != BMS_OK) {
        return send_response(
            connection,
            400,
            "{\"error\":\"telemetry_out_of_range\"}"
        );
    }

    return send_response(
        connection,
        200,
        "{\"message\":\"Battery data updated\"}"
    );
}

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

    json = create_json_object();
    cJSON_AddStringToObject(json, "error", error_message);
    add_current_state(json);

    response_result = send_json_response(
        connection,
        status_code,
        json
    );

    cJSON_Delete(json);
    return response_result;
}

static enum MHD_Result handle_control_request(
    struct MHD_Connection *connection,
    const char *url
)
{
    BmsResult operation_result;
    const char *success_message;
    cJSON *json;
    enum MHD_Result response_result;

    if (!request_is_authorized(connection, api_token)) {
        return send_response(
            connection,
            401,
            "{\"error\":\"unauthorized\"}"
        );
    }

    if (strcmp(url, "/charge") == 0) {
        operation_result = bms_start_charging(&g_bms);
        success_message = "Charging started";
    } else if (strcmp(url, "/discharge") == 0) {
        operation_result = bms_start_discharging(&g_bms);
        success_message = "Discharging started";
    } else if (strcmp(url, "/stop") == 0) {
        operation_result = bms_stop(&g_bms);
        success_message = "Battery stopped";
    } else if (strcmp(url, "/cycles") == 0) {
        int64_t cycle_count;
        const char *count_parameter = MHD_lookup_connection_value(
            connection,
            MHD_GET_ARGUMENT_KIND,
            "count"
        );

        if (count_parameter == NULL || *count_parameter == '\0') {
            return send_response(
                connection,
                400,
                "{\"error\":\"count_required\"}"
            );
        }

        if (!read_cycle_count(connection, &cycle_count)) {
            return send_response(
                connection,
                400,
                "{\"error\":\"invalid_cycle_count\"}"
            );
        }

        operation_result = bms_set_cycles(&g_bms, cycle_count);
        success_message = "Cycle count updated";
    } else {
        return send_response(
            connection,
            404,
            "{\"error\":\"not_found\"}"
        );
    }

    if (operation_result != BMS_OK) {
        return handle_control_error(connection, operation_result);
    }

    json = create_json_object();
    add_current_state(json);
    cJSON_AddStringToObject(json, "message", success_message);

    response_result = send_json_response(connection, 200, json);
    cJSON_Delete(json);

    return response_result;
}

static enum MHD_Result handle_request(
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *body
)
{
    if (strcmp(method, MHD_HTTP_METHOD_OPTIONS) == 0) {
        return send_response(connection, 204, "");
    }

    if (strcmp(url, "/") == 0 || strcmp(url, "/health") == 0) {
        return handle_health_request(connection);
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
        return handle_get_request(connection, url);
    }

    if (strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
        return send_response(
            connection,
            405,
            "{\"error\":\"method_not_allowed\"}"
        );
    }

    if (strcmp(url, "/update") == 0) {
        return handle_telemetry_update(connection, body);
    }

    return handle_control_request(connection, url);
}

static enum MHD_Result access_callback(
    void *closure,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *http_version,
    const char *upload_data,
    size_t *upload_data_size,
    void **request_context
)
{
    HttpRequest *request;

    (void)closure;
    (void)http_version;

    request = *request_context;

    if (request == NULL) {
        request = calloc(1, sizeof(*request));

        if (request == NULL) {
            return MHD_NO;
        }

        *request_context = request;
        return MHD_YES;
    }

    if (*upload_data_size > 0) {
        if (request->used + *upload_data_size > REQUEST_BODY_LIMIT) {
            return send_response(
                connection,
                413,
                "{\"error\":\"body_too_large\"}"
            );
        }

        memcpy(
            request->body + request->used,
            upload_data,
            *upload_data_size
        );

        request->used += *upload_data_size;
        request->body[request->used] = '\0';
        *upload_data_size = 0;

        return MHD_YES;
    }

    return handle_request(
        connection,
        url,
        method,
        request->body
    );
}

static void request_completed_callback(
    void *closure,
    struct MHD_Connection *connection,
    void **request_context,
    enum MHD_RequestTerminationCode termination_code
)
{
    (void)closure;
    (void)connection;
    (void)termination_code;

    free(*request_context);
    *request_context = NULL;
}

static void *safety_monitor_thread(void *unused)
{
    const struct timespec interval = {
        .tv_sec = 0,
        .tv_nsec = 250000000L
    };

    (void)unused;

    while (server_running) {
        bms_tick(&g_bms, monotonic_time_seconds());
        nanosleep(&interval, NULL);
    }

    return NULL;
}

static unsigned int configured_port(void)
{
    const char *port_environment = getenv("PORT");
    char *end = NULL;
    unsigned long parsed_port;

    if (port_environment == NULL || *port_environment == '\0') {
        return 8080;
    }

    parsed_port = strtoul(port_environment, &end, 10);

    if (end == port_environment
        || *end != '\0'
        || parsed_port == 0
        || parsed_port > 65535) {
        return 8080;
    }

    return (unsigned int)parsed_port;
}

int main(void)
{
    unsigned int port;
    pthread_t safety_thread;
    struct MHD_Daemon *daemon;

    signal(SIGINT, handle_shutdown_signal);
    signal(SIGTERM, handle_shutdown_signal);

    api_token = getenv("BMS_API_TOKEN");
    device_token = getenv("BMS_DEVICE_TOKEN");
    cors_origin = getenv("BMS_CORS_ORIGIN");

    bms_init(&g_bms);
    port = configured_port();

    if (pthread_create(
            &safety_thread,
            NULL,
            safety_monitor_thread,
            NULL
        ) != 0) {
        bms_destroy(&g_bms);
        return 1;
    }

    daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD,
        (uint16_t)port,
        NULL,
        NULL,
        &access_callback,
        NULL,
        MHD_OPTION_NOTIFY_COMPLETED,
        &request_completed_callback,
        NULL,
        MHD_OPTION_CONNECTION_TIMEOUT,
        (unsigned int)5,
        MHD_OPTION_THREAD_POOL_SIZE,
        (unsigned int)4,
        MHD_OPTION_END
    );

    if (daemon == NULL) {
        fprintf(stderr, "HTTP server failed on port %u\n", port);

        server_running = 0;
        pthread_join(safety_thread, NULL);
        bms_destroy(&g_bms);

        return 1;
    }

    fprintf(
        stdout,
        "BMS C backend :%u; control %s\n",
        port,
        g_bms.control_enabled
            ? "enabled"
            : "DISABLED - not commissioned"
    );

    while (server_running) {
        const struct timespec interval = {
            .tv_sec = 0,
            .tv_nsec = 200000000L
        };

        nanosleep(&interval, NULL);
    }

    MHD_stop_daemon(daemon);
    pthread_join(safety_thread, NULL);
    bms_destroy(&g_bms);

    return 0;
}
