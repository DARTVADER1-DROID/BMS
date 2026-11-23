#include "auth_routes.h"

#include "auth.h"
#include "http_response.h"

#include <openssl/crypto.h>
#include <cjson/cJSON.h>

#include <stdio.h>
#include <string.h>

enum MHD_Result auth_routes_handle(
    const BmsServerContext *context,
    
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
            return http_send_response(context, connection, 401, "{\"error\":\"unauthenticated\"}");
        }

        json = http_create_json_object();
        cJSON_AddStringToObject(json, "username", username);
        result = http_send_json_response(context, connection, 200, json);
        cJSON_Delete(json);
        return result;
    }

    if (strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
        return http_send_response(context, connection, 405, "{\"error\":\"method_not_allowed\"}");
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
            return http_send_response(context, connection, 429, "{\"error\":\"too_many_attempts\"}");
        }

        if (!cJSON_IsString(user) || !cJSON_IsString(pass)
            || user->valuestring == NULL || pass->valuestring == NULL
            || strlen(user->valuestring) > 128 || strlen(pass->valuestring) > 1024) {
            auth_login_failed(connection);
            cJSON_Delete(json);
            return http_send_response(context, connection, 400, "{\"error\":\"invalid_credentials\"}");
        }

        if (!auth_verify_credentials(user->valuestring, pass->valuestring)) {
            auth_login_failed(connection);
            cJSON_Delete(json);
            return http_send_response(context, connection, 401, "{\"error\":\"invalid_credentials\"}");
        }
        auth_login_succeeded(connection);
        cJSON_Delete(json);

        if (!auth_create_session(context->auth_username, session_value, sizeof(session_value),
                                 csrf_value, sizeof(csrf_value))) {
            return http_send_response(context, connection, 500, "{\"error\":\"session_creation_failed\"}");
        }

        (void)snprintf(session_header, sizeof(session_header),
            "__Host-bms_session=%s; Path=/; Max-Age=28800; HttpOnly; Secure; SameSite=None",
            session_value);
        (void)snprintf(csrf_header, sizeof(csrf_header),
            "__Host-bms_csrf=%s; Path=/; Max-Age=28800; Secure; SameSite=None", csrf_value);
        (void)snprintf(response_body, sizeof(response_body),
            "{\"authenticated\":true,\"username\":\"%s\",\"csrf_token\":\"%s\"}", context->auth_username, csrf_value);

        result = http_send_auth_response(context, connection, 200, response_body, session_header, csrf_header);
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
            return http_send_response(context, connection, 403, "{\"error\":\"csrf_failed\"}");
        }
        return http_send_auth_response(context, connection, 200, "{\"authenticated\":false}",
                                  clear_session, clear_csrf);
    }

    return http_send_response(context, connection, 404, "{\"error\":\"not_found\"}");
}
