#include "auth.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char session_secret[] =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

    if (!auth_init("bms-test", "correct horse battery", session_secret)) {
        fputs("auth_init rejected valid test configuration\n", stderr);
        return 1;
    }

    char session_cookie[320];
    char csrf_cookie[65];

    if (!auth_create_session(
            "bms-test",
            session_cookie,
            sizeof(session_cookie),
            csrf_cookie,
            sizeof(csrf_cookie)
        )
        || session_cookie[0] == '\0'
        || strlen(csrf_cookie) != 64) {
        fputs("session token creation failed\n", stderr);
        auth_clear_secrets();
        return 1;
    }

    if (!auth_verify_credentials("bms-test", "correct horse battery")) {
        fputs("valid credentials were rejected\n", stderr);
        auth_clear_secrets();
        return 1;
    }

    if (auth_verify_credentials("bms-test", "incorrect password")
        || auth_verify_credentials("other-user", "correct horse battery")) {
        fputs("invalid credentials were accepted\n", stderr);
        auth_clear_secrets();
        return 1;
    }

    auth_clear_secrets();
    puts("authentication password tests passed");
    return 0;
}
