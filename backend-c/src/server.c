#define _POSIX_C_SOURCE 200809L

#include "bms.h"
#include "auth.h"
#include "api_docs.h"
#include "server_context.h"
#include "http_response.h"
#include "auth_routes.h"
#include "telemetry_routes.h"
#include "control_routes.h"

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

static BmsServerContext server_context;





typedef struct {
    char body[REQUEST_BODY_LIMIT + 1];
    size_t used;
    bool body_too_large;
} HttpRequest;



static void handle_shutdown_signal(int signal_number)
{
    (void)signal_number;
    server_running = 0;
}




















static enum MHD_Result handle_health_request(
    struct MHD_Connection *connection
)
{
    cJSON *json = http_create_json_object();
    enum MHD_Result result;

    cJSON_AddStringToObject(json, "message", "connected");
    cJSON_AddStringToObject(json, "implementation", "c");
    cJSON_AddBoolToObject(
        json,
        "control_enabled",
        g_bms.control_enabled
    );

    result = http_send_json_response(&server_context, connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result handle_data_request(
    struct MHD_Connection *connection
)
{
    cJSON *json = http_create_json_object();
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

    result = http_send_json_response(&server_context, connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result handle_state_request(
    struct MHD_Connection *connection,
    bool include_warning
)
{
    cJSON *json = http_create_json_object();
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

    result = http_send_json_response(&server_context, connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result handle_hardware_connection_request(
    struct MHD_Connection *connection
)
{
    cJSON *json = http_create_json_object();
    enum MHD_Result result;

    pthread_mutex_lock(&g_bms.lock);

    cJSON_AddBoolToObject(
        json,
        "hardware_connection",
        g_bms.hardware_connected
    );

    pthread_mutex_unlock(&g_bms.lock);

    result = http_send_json_response(&server_context, connection, 200, json);
    cJSON_Delete(json);

    return result;
}

static enum MHD_Result handle_estimate_request(
    struct MHD_Connection *connection,
    const char *url
)
{
    cJSON *json = http_create_json_object();
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

    result = http_send_json_response(&server_context, connection, 200, json);
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

    return http_send_response(
            &server_context,
        connection,
        404,
        "{\"error\":\"not_found\"}"
    );
}












static enum MHD_Result handle_request(
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *body
)
{
    if (strcmp(method, MHD_HTTP_METHOD_OPTIONS) == 0) {
        return http_send_response(&server_context, connection, 204, "");
    }

    if (strncmp(url, "/auth/", 6) == 0) {
        return auth_routes_handle(&server_context, connection, url, method, body);
    }

    if (strcmp(url, "/") == 0 || strcmp(url, "/health") == 0) {
        return handle_health_request(connection);
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0
        && strcmp(url, "/docs") == 0) {
        return http_send_html_response(&server_context, connection, 200, SWAGGER_UI_HTML);
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0
        && strcmp(url, "/openapi.json") == 0) {
        return http_send_response(&server_context, connection, 200, OPENAPI_DOCUMENT);
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
        if (!auth_validate_request(connection)) {
            return http_send_response(&server_context, connection, 401, "{\"error\":\"unauthenticated\"}");
        }
        return handle_get_request(connection, url);
    }

    if (strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
        return http_send_response(
            &server_context,
            connection,
            405,
            "{\"error\":\"method_not_allowed\"}"
        );
    }

    if (strcmp(url, "/update") == 0) {
        return telemetry_routes_update(&server_context, connection, body);
    }

    if (!auth_validate_csrf(connection)) {
        return http_send_response(&server_context, connection, 403, "{\"error\":\"csrf_failed\"}");
    }

    return control_routes_handle(&server_context, connection, url);
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
        if (request->body_too_large
            || *upload_data_size > REQUEST_BODY_LIMIT - request->used) {
            request->body_too_large = true;
            *upload_data_size = 0;
            return MHD_YES;
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

    if (request->body_too_large) {
        return http_send_response(
            &server_context,
            connection,
            413,
            "{\"error\":\"body_too_large\"}"
        );
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
        bms_tick(&g_bms, http_monotonic_time_seconds());
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

    server_context.device_token = getenv("BMS_DEVICE_TOKEN");
    server_context.cors_origin = getenv("BMS_CORS_ORIGIN");
    server_context.auth_username = getenv("BMS_AUTH_USERNAME");

    if (!auth_init(auth_username, getenv("BMS_AUTH_PASSWORD"), getenv("BMS_SESSION_SECRET"))
        || server_context.device_token == NULL || server_context.device_token[0] == '\0') {
        fprintf(stderr,
            "Missing auth configuration. Set BMS_AUTH_USERNAME, BMS_AUTH_PASSWORD (12+ chars), "
            "BMS_SESSION_SECRET (32+ chars), and BMS_DEVICE_TOKEN.\n");
        return 1;
    }

    bms_init(&g_bms);
    server_context.bms = &g_bms;
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
    auth_clear_secrets();

    return 0;
}
