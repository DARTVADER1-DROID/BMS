#include "http_response.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static enum MHD_Result http_send_auth_response(
    const BmsServerContext *context,
    
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
    if (context->cors_origin != NULL && context->cors_origin[0] != '\0') {
        MHD_add_response_header(response, "Access-Control-Allow-Origin", context->cors_origin);
        MHD_add_response_header(response, "Access-Control-Allow-Credentials", "true");
        MHD_add_response_header(response, "Vary", "Origin");
    }

    result = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return result;
}

static enum MHD_Result http_send_response(
    const BmsServerContext *context,
    
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
    if (context->cors_origin != NULL && context->cors_origin[0] != '\0') {
        MHD_add_response_header(http_response, "Access-Control-Allow-Origin", context->cors_origin);
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

static enum MHD_Result http_send_html_response(
    const BmsServerContext *context,
    
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
        context->cors_origin != NULL ? context->cors_origin : "*"
    );

    result = MHD_queue_response(connection, status_code, http_response);
    MHD_destroy_response(http_response);

    return result;
}

static enum MHD_Result http_send_json_response(
    const BmsServerContext *context,
    
    struct MHD_Connection *connection,
    unsigned int status_code,
    cJSON *json
)
{
    char *serialized_json;
    enum MHD_Result result;

    serialized_json = cJSON_PrintUnformatted(json);

    if (serialized_json == NULL) {
        return http_send_response(
            context,
            connection,
            500,
            "{\"error\":\"serialization_failed\"}"
        );
    }

    result = http_send_response(context, connection, status_code, serialized_json);
    free(serialized_json);

    return result;
}

static cJSON *http_create_json_object(void)
{
    return cJSON_CreateObject();
}

static void http_add_current_state(
    const BmsServerContext *context,
    cJSON *json)
{
    pthread_mutex_lock(&(*context->bms).lock);

    cJSON_AddStringToObject(
        json,
        "state",
        bms_state_name((*context->bms).state)
    );

    pthread_mutex_unlock(&(*context->bms).lock);
}

static bool http_read_json_number(
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

static bool static tokens_match(const char *provided, const char *expected)
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

static bool http_request_is_authorized(
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

static double http_monotonic_time_seconds(void)
{
    struct timespec timestamp;

    clock_gettime(CLOCK_MONOTONIC, &timestamp);

    return (double)timestamp.tv_sec
        + (double)timestamp.tv_nsec / 1e9;
}
