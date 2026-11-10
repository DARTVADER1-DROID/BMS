#define _POSIX_C_SOURCE 200809L

#include "bms.h"
#include "auth.h"
#include <openssl/crypto.h>

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

static const char *device_token;
static const char *auth_username;
static const char *cors_origin;

static const char OPENAPI_DOCUMENT[] = "{\"openapi\":\"3.0.3\",\"info\":{\"title\":\"BMS C Backend API\",\"version\":\"1.0.0\",\"description\":\"Interactive API reference for the native C BMS backend. Dashboard API routes require a signed session cookie. POST /update requires the configured device bearer token. Control remains disabled until explicitly commissioned.\"},\"tags\":[{\"name\":\"System\",\"description\":\"Health and connectivity\"},{\"name\":\"Telemetry\",\"description\":\"Live battery measurements\"},{\"name\":\"Estimation\",\"description\":\"Legacy SOC, SOH and RUL indicators\"},{\"name\":\"Control\",\"description\":\"Supervisory control commands\"},{\"name\":\"Device\",\"description\":\"ESP32 telemetry ingestion\"}],\"components\":{\"securitySchemes\":{\"SessionCookie\":{\"type\":\"apiKey\",\"in\":\"cookie\",\"name\":\"__Host-bms_session\",\"description\":\"Signed dashboard session cookie. Obtain through POST /auth/login.\"},\"DeviceBearer\":{\"type\":\"http\",\"scheme\":\"bearer\",\"bearerFormat\":\"Token\",\"description\":\"Required BMS_DEVICE_TOKEN for POST /update.\"}},\"schemas\":{\"HealthResponse\":{\"type\":\"object\",\"properties\":{\"message\":{\"type\":\"string\",\"example\":\"connected\"},\"implementation\":{\"type\":\"string\",\"example\":\"c\"},\"control_enabled\":{\"type\":\"boolean\",\"example\":false}}},\"TelemetryResponse\":{\"type\":\"object\",\"properties\":{\"voltage\":{\"type\":\"number\",\"format\":\"double\",\"example\":3.92},\"current\":{\"type\":\"number\",\"format\":\"double\",\"example\":0.45},\"temperature\":{\"type\":\"number\",\"format\":\"double\",\"example\":28.4},\"time\":{\"type\":\"number\",\"format\":\"double\",\"description\":\"Monotonic timestamp of last telemetry update\"},\"hardware_connection\":{\"type\":\"boolean\",\"example\":true}}},\"StateResponse\":{\"type\":\"object\",\"properties\":{\"state\":{\"type\":\"string\",\"example\""
    ":\"idle\"},\"warning_messages\":{\"type\":\"string\",\"example\":\"\"}}},\"HardwareResponse\":{\"type\":\"object\",\"properties\":{\"hardware_connection\":{\"type\":\"boolean\",\"example\":false}}},\"EstimateResponse\":{\"type\":\"object\",\"additionalProperties\":{\"type\":\"number\",\"nullable\":true}},\"ControlResponse\":{\"type\":\"object\",\"properties\":{\"state\":{\"type\":\"string\",\"example\":\"idle\"},\"message\":{\"type\":\"string\",\"example\":\"Battery stopped\"}}},\"TelemetryUpdate\":{\"type\":\"object\",\"required\":[\"voltage\",\"current\",\"temperature\"],\"properties\":{\"voltage\":{\"type\":\"number\",\"format\":\"double\",\"example\":3.92},\"current\":{\"type\":\"number\",\"format\":\"double\",\"example\":0.45},\"temperature\":{\"type\":\"number\",\"format\":\"double\",\"example\":28.4}}}}},\"paths\":{\"/auth/login\":{\"post\":{\"tags\":[\"Authentication\"],\"summary\":\"Sign in and create a browser session\",\"requestBody\":{\"required\":true,\"content\":{\"application/json\":{\"schema\":{\"type\":\"object\",\"required\":[\"username\",\"password\"],\"properties\":{\"username\":{\"type\":\"string\"},\"password\":{\"type\":\"string\",\"format\":\"password\"}}}}}},\"responses\":{\"200\":{\"description\":\"Session cookies set\"},\"401\":{\"description\":\"Invalid credentials\"},\"429\":{\"description\":\"Too many failed attempts\"}}}},\"/auth/me\":{\"get\":{\"security\":[{\"SessionCookie\":[]}],\"tags\":[\"Authentication\"],\"summary\":\"Get current authenticated account\",\"responses\":{\"200\":{\"description\":\"Authenticated\"},\"401\":{\"description\":\"Not signed in\"}}}},\"/auth/logout\":{\"post\":{\"tags\":[\"Authentication\"],\"summary\":\"Clear current browser session\",\"security\":[{\"SessionCookie\":[]}],\"parameters\":[{\"name\":\"X-CSRF-Token\",\"in\":\"header\",\"required\":true,\"schema\":{\"type\":\"string\"}}],\"responses\":{\"200\":{\"description\":\"Signed out\"},\"403\":{\"description\":\"CSRF validation failed\"}}}},\"/\":{\"get\":{\"tags\":[\"System\"],\"summary\":\"Root health check\",\"responses\":{\"200\":{\"description\":\"Backend is responding\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/schemas/HealthResponse\"}}}}}}},\"/health\":{\"get\":{\"tags\":[\"System\"],\"summary\":\"Health check\",\"responses\":{\"200\":{\"description\":\"Backend is responding\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/schemas/HealthResponse\"}}}}}}},\"/data\":{\"get\":{\"security\":[{\"SessionCookie\":[]}],\"tags\":[\"Telemetry\"],\"summary\":\"Read latest battery telemetry\",\"responses\":{\"200\":{\"description\":\"Latest telemetry\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/schemas/TelemetryResponse\"}}}}}}},\"/state\":{\"get\":{\"security\":[{\"SessionCookie\":[]}],\"tags\":[\"Telemetry\"],\"summary\":\"Read state and warning message\",\"responses\":{\"200\":{\"description\":\"Current BMS state\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/schemas/StateResponse\"}}}}}}},\"/state-hardware\":{\"get\":{\"security\":[{\"SessionCookie\":[]}],\"tags\":[\"Telemetry\"],\"summary\":\"Read hardware-facing state\",\"responses\":{\"200\":{\"description\":\"Current state\",\"content\":{\"application/json\":{\"schema\":{\"type\":\"object\",\"properties\":{\"stat"
    "e\":{\"type\":\"string\",\"example\":\"idle\"}}}}}}}}},\"/hardware_connection\":{\"get\":{\"security\":[{\"SessionCookie\":[]}],\"tags\":[\"Telemetry\"],\"summary\":\"Check ESP32 heartbeat status\",\"responses\":{\"200\":{\"description\":\"Hardware connection status\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/schemas/HardwareResponse\"}}}}}}},\"/s_o_c\":{\"get\":{\"security\":[{\"SessionCookie\":[]}],\"tags\":[\"Estimation\"],\"summary\":\"Read state-of-charge estimate\",\"description\":\"Legacy voltage-interpolation estimate; not a calibrated safety value.\",\"responses\":{\"200\":{\"description\":\"SOC estimate (null if unavailable)\",\"content\":{\"application/json\":{\"schema\":{\"type\":\"object\",\"properties\":{\"s_o_c\":{\"type\":\"number\",\"nullable\":true,\"example\":76.5}}}}}}}}},\"/s_o_h\":{\"get\":{\"security\":[{\"SessionCookie\":[]}],\"tags\":[\"Estimation\"],\"summary\":\"Read state-of-health estimate\",\"description\":\"Legacy empirical estimate; not validated for the target cell chemistry.\",\"responses\":{\"200\":{\"description\":\"SOH estimate (null if unavailable)\",\"content\":{\"application/json\":{\"schema\":{\"type\":\"object\",\"properties\":{\"s_o_h\":{\"type\":\"number\",\"nullable\":true,\"example\":94.2}}}}}}}}},\"/r_u_l\":{\"get\":{\"security\":[{\"SessionCookie\":[]}],\"tags\":[\"Estimation\"],\"summary\":\"Read remaining-useful-life estimate\",\"description\":\"Basic rated-cycle subtraction; provisional and not a safety function.\",\"responses\":{\"200\":{\"description\":\"RUL estimate\",\"content\":{\"application/json\":{\"schema\":{\"type\":\"object\",\"properties\":{\"r_u_l\":{\"type\":\"number\",\"example\":850}}}}}}}}},\"/charge\":{\"post\":{\"tags\":[\"Control\"],\"summary\":\"Request charging mode\",\"description\":\"Requires a signed dashboard session. Control remains disabled unless commissioned with explicit limits.\",\"security\":[{\"SessionCookie\":[]}],\"responses\":{\"200\":{\"description\":\"Command accepted\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/schemas/ControlResponse\"}}}},\"401\":{\"description\":\"Missing or invali"
    "d bearer token\"},\"409\":{\"description\":\"Unsafe condition or invalid state transition\"},\"503\":{\"description\":\"Control is not commissioned\"}}}},\"/discharge\":{\"post\":{\"tags\":[\"Control\"],\"summary\":\"Request discharging mode\",\"description\":\"Requires a signed dashboard session. Control remains disabled unless commissioned with explicit limits.\",\"security\":[{\"SessionCookie\":[]}],\"responses\":{\"200\":{\"description\":\"Command accepted\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/schemas/ControlResponse\"}}}},\"401\":{\"description\":\"Missing or invalid bearer token\"},\"409\":{\"description\":\"Unsafe condition or invalid state transition\"},\"503\":{\"description\":\"Control is not commissioned\"}}}},\"/stop\":{\"post\":{\"tags\":[\"Control\"],\"summary\":\"Request idle/stop state\",\"security\":[{\"SessionCookie\":[]}],\"responses\":{\"200\":{\"description\":\"Stop command accepted\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/schemas/ControlResponse\"}}}},\"401\":{\"description\":\"Missing or invalid bearer token\"},\"409\":{\"description\":\"Invalid state transition\"},\"503\":{\"description\":\"Control is not commissioned\"}}}},\"/cycles\":{\"post\":{\"tags\":[\"Control\"],\"summary\":\"Set equivalent cycle count\",\"description\":\"Requires a signed dashboard session. Count must be an integer from 0 to 1,000,000,000.\",\"security\":[{\"SessionCookie\":[]}],\"parameters\":[{\"name\":\"count\",\"in\":\"query\",\"required\":true,\"schema\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":1000000000},\"example\":112}],\"responses\":{\"200\":{\"description\":\"Cycle count updated\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/schemas/ControlResponse\"}}}},\"400\":{\"description\":\"Missing or invalid cycle count\"},\"401\":{\"description\":\"Missing or invalid bearer token\"},\"503\":{\"description\":\"Control is not commissioned\"}}}},\"/update\":{\"post\":{"
    "\"tags\":[\"Device\"],\"summary\":\"Submit ESP32 telemetry\",\"description\":\"Requires BMS_DEVICE_TOKEN. All three values must be finite and within configured backend limits.\",\"security\":[{\"DeviceBearer\":[]}],\"requestBody\":{\"required\":true,\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/schemas/TelemetryUpdate\"}}}},\"responses\":{\"200\":{\"description\":\"Telemetry accepted\",\"content\":{\"application/json\":{\"schema\":{\"type\":\"object\",\"properties\":{\"message\":{\"type\":\"string\",\"example\":\"Battery data updated\"}}}}}},\"400\":{\"description\":\"Malformed telemetry or a measurement outside configured limits\"},\"401\":{\"description\":\"Missing or invalid device bearer token\"}}}}}}";

static const char SWAGGER_UI_HTML[] = "<!doctype html>\n<html lang=\"en\">\n<head>\n  <meta charset=\"utf-8\">\n  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n  <title>BMS API — Swagger UI</title>\n  <link rel=\"stylesheet\" href=\"https://cdn.jsdelivr.net/npm/swagger-ui-dist@5/swagger-ui.css\">\n  <style>\n    body { margin: 0; background: #fafafa; }\n    .topbar { display: none; }\n    .swagger-ui .info { margin: 28px 0; }\n    .swagger-ui .scheme-container { box-shadow: none; }\n  </style>\n</head>\n<body>\n  <div id=\"swagger-ui\"></div>\n  <script src=\"https://cdn.jsdelivr.net/npm/swagger-ui-dist@5/swagger-ui-bundle.js\"></script>\n  <script>\n    window.addEventListener(\"load\", function () {\n      window.ui = SwaggerUIBundle({\n        url: \"/openapi.json\",\n        dom_id: \"#swagger-ui\",\n        deepLinking: true,\n        displayRequestDuration: true,\n        persistAuthorization: true,\n        tryItOutEnabled: true,\n        requestInterceptor: function (request) {\n          if (request.method && request.method.toLowerCase() === \"post\"\n              && !request.url.endsWith(\"/auth/login\")\n              && !request.url.endsWith(\"/update\")) {\n            var match = document.cookie.match(/(?:^|; )__Host-bms_csrf=([^;]+)/);\n            if (match) request.headers[\"X-CSRF-Token\"] = decodeURIComponent(match[1]);\n          }\n          return request;\n        },\n        validatorUrl: null\n      });\n    });\n  </script>\n</body>\n</html>";

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


static enum MHD_Result send_auth_response(
    struct MHD_Connection *connection,
    unsigned int status_code,
    const char *body,
    const char *session_cookie,
    const char *csrf_cookie
)
{
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(body), (void *)body, MHD_RESPMEM_MUST_COPY
    );
    enum MHD_Result result;

    if (response == NULL) {
        return MHD_NO;
    }

    MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
    MHD_add_response_header(response, "Cache-Control", "no-store");
    MHD_add_response_header(response, "X-Content-Type-Options", "nosniff");
    MHD_add_response_header(response, "Set-Cookie", session_cookie);
    if (csrf_cookie != NULL) {
        MHD_add_response_header(response, "Set-Cookie", csrf_cookie);
    }
    if (cors_origin != NULL && cors_origin[0] != '\0') {
        MHD_add_response_header(response, "Access-Control-Allow-Origin", cors_origin);
        MHD_add_response_header(response, "Access-Control-Allow-Credentials", "true");
        MHD_add_response_header(response, "Vary", "Origin");
    }

    result = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return result;
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
    if (cors_origin != NULL && cors_origin[0] != '\0') {
        MHD_add_response_header(http_response, "Access-Control-Allow-Origin", cors_origin);
        MHD_add_response_header(http_response, "Access-Control-Allow-Credentials", "true");
        MHD_add_response_header(http_response, "Vary", "Origin");
    }
    MHD_add_response_header(
        http_response,
        "Access-Control-Allow-Headers",
        "Content-Type, Authorization, X-CSRF-Token"
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

static enum MHD_Result send_html_response(
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
        "text/html; charset=utf-8"
    );
    MHD_add_response_header(http_response, "Cache-Control", "no-store");
    MHD_add_response_header(http_response, "X-Content-Type-Options", "nosniff");
    MHD_add_response_header(
        http_response,
        "Access-Control-Allow-Origin",
        cors_origin != NULL ? cors_origin : "*"
    );

    result = MHD_queue_response(connection, status_code, http_response);
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

    if (device_token == NULL || device_token[0] == '\0' || !request_is_authorized(connection, device_token)) {
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


static enum MHD_Result handle_auth_request(
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *body
)
{
    if (strcmp(url, "/auth/me") == 0 && strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
        char username[129];
        cJSON *json;
        enum MHD_Result result;

        if (!auth_read_session(connection, username, sizeof(username))) {
            return send_response(connection, 401, "{\"error\":\"unauthenticated\"}");
        }

        json = create_json_object();
        cJSON_AddStringToObject(json, "username", username);
        result = send_json_response(connection, 200, json);
        cJSON_Delete(json);
        return result;
    }

    if (strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
        return send_response(connection, 405, "{\"error\":\"method_not_allowed\"}");
    }

    if (strcmp(url, "/auth/login") == 0) {
        cJSON *json = cJSON_Parse(body != NULL ? body : "");
        cJSON *user = json != NULL ? cJSON_GetObjectItemCaseSensitive(json, "username") : NULL;
        cJSON *pass = json != NULL ? cJSON_GetObjectItemCaseSensitive(json, "password") : NULL;
        char session_value[320];
        char csrf_value[65];
        char session_header[420];
        char csrf_header[180];
        char response_body[512];
        enum MHD_Result result;

        if (!auth_login_allowed(connection)) {
            cJSON_Delete(json);
            return send_response(connection, 429, "{\"error\":\"too_many_attempts\"}");
        }

        if (!cJSON_IsString(user) || !cJSON_IsString(pass)
            || user->valuestring == NULL || pass->valuestring == NULL
            || strlen(user->valuestring) > 128 || strlen(pass->valuestring) > 1024) {
            auth_login_failed(connection);
            cJSON_Delete(json);
            return send_response(connection, 400, "{\"error\":\"invalid_credentials\"}");
        }

        if (!auth_verify_credentials(user->valuestring, pass->valuestring)) {
            auth_login_failed(connection);
            cJSON_Delete(json);
            return send_response(connection, 401, "{\"error\":\"invalid_credentials\"}");
        }
        auth_login_succeeded(connection);
        cJSON_Delete(json);

        if (!auth_create_session(auth_username, session_value, sizeof(session_value),
                                 csrf_value, sizeof(csrf_value))) {
            return send_response(connection, 500, "{\"error\":\"session_creation_failed\"}");
        }

        (void)snprintf(session_header, sizeof(session_header),
            "__Host-bms_session=%s; Path=/; Max-Age=28800; HttpOnly; Secure; SameSite=None",
            session_value);
        (void)snprintf(csrf_header, sizeof(csrf_header),
            "__Host-bms_csrf=%s; Path=/; Max-Age=28800; Secure; SameSite=None", csrf_value);
        (void)snprintf(response_body, sizeof(response_body),
            "{\"authenticated\":true,\"username\":\"%s\",\"csrf_token\":\"%s\"}", auth_username, csrf_value);

        result = send_auth_response(connection, 200, response_body, session_header, csrf_header);
        OPENSSL_cleanse(session_value, sizeof(session_value));
        OPENSSL_cleanse(csrf_value, sizeof(csrf_value));
        return result;
    }

    if (strcmp(url, "/auth/logout") == 0) {
        static const char clear_session[] =
            "__Host-bms_session=; Path=/; Max-Age=0; HttpOnly; Secure; SameSite=None";
        static const char clear_csrf[] =
            "__Host-bms_csrf=; Path=/; Max-Age=0; Secure; SameSite=None";

        if (!auth_validate_csrf(connection)) {
            return send_response(connection, 403, "{\"error\":\"csrf_failed\"}");
        }
        return send_auth_response(connection, 200, "{\"authenticated\":false}",
                                  clear_session, clear_csrf);
    }

    return send_response(connection, 404, "{\"error\":\"not_found\"}");
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

    if (strncmp(url, "/auth/", 6) == 0) {
        return handle_auth_request(connection, url, method, body);
    }

    if (strcmp(url, "/") == 0 || strcmp(url, "/health") == 0) {
        return handle_health_request(connection);
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0
        && strcmp(url, "/docs") == 0) {
        return send_html_response(connection, 200, SWAGGER_UI_HTML);
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0
        && strcmp(url, "/openapi.json") == 0) {
        return send_response(connection, 200, OPENAPI_DOCUMENT);
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
        if (!auth_validate_request(connection)) {
            return send_response(connection, 401, "{\"error\":\"unauthenticated\"}");
        }
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

    if (!auth_validate_csrf(connection)) {
        return send_response(connection, 403, "{\"error\":\"csrf_failed\"}");
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

    device_token = getenv("BMS_DEVICE_TOKEN");
    cors_origin = getenv("BMS_CORS_ORIGIN");
    auth_username = getenv("BMS_AUTH_USERNAME");

    if (!auth_init(auth_username, getenv("BMS_AUTH_PASSWORD"), getenv("BMS_SESSION_SECRET"))
        || device_token == NULL || device_token[0] == '\0') {
        fprintf(stderr,
            "Missing auth configuration. Set BMS_AUTH_USERNAME, BMS_AUTH_PASSWORD (12+ chars), "
            "BMS_SESSION_SECRET (32+ chars), and BMS_DEVICE_TOKEN.\n");
        return 1;
    }

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
    auth_clear_secrets();

    return 0;
}
