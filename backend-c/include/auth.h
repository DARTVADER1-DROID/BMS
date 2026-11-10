#ifndef BMS_AUTH_H
#define BMS_AUTH_H

#include <stdbool.h>
#include <stddef.h>

struct MHD_Connection;

bool auth_init(const char *username, const char *password, const char *secret);
bool auth_verify_credentials(const char *username, const char *password);
bool auth_login_allowed(struct MHD_Connection *connection);
void auth_login_failed(struct MHD_Connection *connection);
void auth_login_succeeded(struct MHD_Connection *connection);
bool auth_validate_request(struct MHD_Connection *connection);
bool auth_validate_csrf(struct MHD_Connection *connection);
bool auth_create_session(
    const char *username,
    char *session_cookie,
    size_t session_cookie_size,
    char *csrf_cookie,
    size_t csrf_cookie_size
);
bool auth_read_session(
    struct MHD_Connection *connection,
    char *username,
    size_t username_size
);
void auth_clear_secrets(void);

#endif
