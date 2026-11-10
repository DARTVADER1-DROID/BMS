#define _POSIX_C_SOURCE 200809L

#include "auth.h"

#include <microhttpd.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AUTH_PASSWORD_ITERATIONS 600000
#define AUTH_SESSION_SECONDS (8 * 60 * 60)
#define AUTH_COOKIE_NAME "bms_session"
#define AUTH_CSRF_COOKIE_NAME "bms_csrf"
#define AUTH_TOKEN_BYTES 32
#define AUTH_HEX_SIZE (AUTH_TOKEN_BYTES * 2 + 1)

static char configured_username[129];
static unsigned char password_salt[16];
static unsigned char password_digest[EVP_MAX_MD_SIZE];
static unsigned int password_digest_length;
static unsigned char session_secret[64];
static size_t session_secret_length;
static bool auth_ready;

static bool constant_time_equal(
    const unsigned char *left,
    const unsigned char *right,
    size_t length
)
{
    return CRYPTO_memcmp(left, right, length) == 0;
}

static bool to_hex(
    const unsigned char *input,
    size_t input_length,
    char *output,
    size_t output_size
)
{
    static const char digits[] = "0123456789abcdef";

    if (output_size < input_length * 2 + 1) {
        return false;
    }

    for (size_t index = 0; index < input_length; index++) {
        output[index * 2] = digits[input[index] >> 4];
        output[index * 2 + 1] = digits[input[index] & 0x0f];
    }

    output[input_length * 2] = '\0';
    return true;
}

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }

    value = (char)tolower((unsigned char)value);

    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }

    return -1;
}

static bool from_hex(
    const char *input,
    unsigned char *output,
    size_t output_length
)
{
    if (strlen(input) != output_length * 2) {
        return false;
    }

    for (size_t index = 0; index < output_length; index++) {
        int high = hex_value(input[index * 2]);
        int low = hex_value(input[index * 2 + 1]);

        if (high < 0 || low < 0) {
            return false;
        }

        output[index] = (unsigned char)((high << 4) | low);
    }

    return true;
}

static bool read_cookie(
    struct MHD_Connection *connection,
    const char *cookie_name,
    char *output,
    size_t output_size
)
{
    const char *cookie_header;
    size_t name_length;

    cookie_header = MHD_lookup_connection_value(
        connection,
        MHD_COOKIE_KIND,
        cookie_name
    );

    if (cookie_header == NULL || output_size == 0) {
        return false;
    }

    name_length = strlen(cookie_name);

    if (strncmp(cookie_header, cookie_name, name_length) != 0
        || cookie_header[name_length] != '=') {
        return false;
    }

    cookie_header += name_length + 1;

    size_t value_length = strcspn(cookie_header, ";");

    if (value_length == 0 || value_length >= output_size) {
        return false;
    }

    memcpy(output, cookie_header, value_length);
    output[value_length] = '\0';

    return true;
}

static bool session_signature(
    const char *payload,
    unsigned char signature[EVP_MAX_MD_SIZE],
    unsigned int *signature_length
)
{
    return HMAC(
        EVP_sha256(),
        session_secret,
        (int)session_secret_length,
        (const unsigned char *)payload,
        strlen(payload),
        signature,
        signature_length
    ) != NULL;
}

bool auth_init(const char *username, const char *password, const char *secret)
{
    if (username == NULL || password == NULL || secret == NULL
        || username[0] == '\0' || password[0] == '\0'
        || strlen(username) > 128 || strlen(password) < 12
        || strlen(secret) < 32 || strlen(secret) > sizeof(session_secret)) {
        return false;
    }

    if (RAND_bytes(password_salt, (int)sizeof(password_salt)) != 1) {
        return false;
    }

    if (PKCS5_PBKDF2_HMAC(
            password,
            (int)strlen(password),
            password_salt,
            (int)sizeof(password_salt),
            AUTH_PASSWORD_ITERATIONS,
            EVP_sha256(),
            (int)sizeof(password_digest),
            password_digest
        ) != 1) {
        return false;
    }

    memcpy(configured_username, username, strlen(username) + 1);
    memcpy(session_secret, secret, strlen(secret));
    session_secret_length = strlen(secret);
    password_digest_length = (unsigned int)sizeof(password_digest);
    auth_ready = true;

    return true;
}

bool auth_create_session(
    const char *username,
    char *session_cookie,
    size_t session_cookie_size,
    char *csrf_cookie,
    size_t csrf_cookie_size
)
{
    unsigned char nonce[AUTH_TOKEN_BYTES];
    unsigned char signature[EVP_MAX_MD_SIZE];
    unsigned int signature_length = 0;
    char nonce_hex[AUTH_HEX_SIZE];
    char payload[160];
    char signature_hex[EVP_MAX_MD_SIZE * 2 + 1];
    int payload_length;
    time_t expires_at;

    if (!auth_ready || username == NULL
        || strcmp(username, configured_username) != 0
        || RAND_bytes(nonce, (int)sizeof(nonce)) != 1
        || RAND_bytes((unsigned char *)csrf_cookie, AUTH_TOKEN_BYTES) != 1) {
        return false;
    }

    if (csrf_cookie_size < AUTH_HEX_SIZE
        || session_cookie_size < 256
        || !to_hex(nonce, sizeof(nonce), nonce_hex, sizeof(nonce_hex))
        || !to_hex(
            (const unsigned char *)csrf_cookie,
            AUTH_TOKEN_BYTES,
            csrf_cookie,
            csrf_cookie_size
        )) {
        return false;
    }

    expires_at = time(NULL) + AUTH_SESSION_SECONDS;
    payload_length = snprintf(
        payload,
        sizeof(payload),
        "%s.%lld.%s",
        configured_username,
        (long long)expires_at,
        nonce_hex
    );

    if (payload_length < 0 || (size_t)payload_length >= sizeof(payload)
        || !session_signature(payload, signature, &signature_length)
        || !to_hex(
            signature,
            signature_length,
            signature_hex,
            sizeof(signature_hex)
        )) {
        return false;
    }

    if (snprintf(
            session_cookie,
            session_cookie_size,
            "%s.%s",
            payload,
            signature_hex
        ) >= (int)session_cookie_size) {
        return false;
    }

    OPENSSL_cleanse(nonce, sizeof(nonce));
    OPENSSL_cleanse(signature, sizeof(signature));
    return true;
}

bool auth_read_session(
    struct MHD_Connection *connection,
    char *username,
    size_t username_size
)
{
    char token[320];
    char payload[160];
    char provided_signature_hex[EVP_MAX_MD_SIZE * 2 + 1];
    char expected_signature_hex[EVP_MAX_MD_SIZE * 2 + 1];
    unsigned char expected_signature[EVP_MAX_MD_SIZE];
    unsigned char provided_signature[EVP_MAX_MD_SIZE];
    unsigned int expected_length = 0;
    char *last_separator;
    char *expiry_separator;
    char *nonce_separator;
    long long expires_at;
    char *end = NULL;
    size_t payload_length;

    if (!auth_ready || !read_cookie(
            connection,
            AUTH_COOKIE_NAME,
            token,
            sizeof(token)
        )) {
        return false;
    }

    last_separator = strrchr(token, '.');

    if (last_separator == NULL) {
        return false;
    }

    *last_separator = '\0';

    if (strlen(last_separator + 1) != 64
        || !from_hex(
            last_separator + 1,
            provided_signature,
            32
        )) {
        return false;
    }

    payload_length = strlen(token);

    if (payload_length >= sizeof(payload)) {
        return false;
    }

    memcpy(payload, token, payload_length + 1);

    expiry_separator = strrchr(payload, '.');

    if (expiry_separator == NULL) {
        return false;
    }

    nonce_separator = expiry_separator - 64;

    if (nonce_separator <= payload
        || nonce_separator[0] != '.') {
        return false;
    }

    *expiry_separator = '\0';
    *nonce_separator = '\0';

    if (strlen(payload) == 0
        || strlen(payload) >= username_size
        || strlen(nonce_separator + 1) != 64) {
        return false;
    }

    expires_at = strtoll(expiry_separator + 1, &end, 10);

    if (end == expiry_separator + 1 || *end != '\0'
        || expires_at <= (long long)time(NULL)
        || expires_at > (long long)time(NULL) + AUTH_SESSION_SECONDS) {
        return false;
    }

    if (strcmp(payload, configured_username) != 0
        || !session_signature(token, expected_signature, &expected_length)
        || expected_length != 32
        || !constant_time_equal(
            expected_signature,
            provided_signature,
            32
        )) {
        return false;
    }

    memcpy(username, payload, strlen(payload) + 1);
    OPENSSL_cleanse(expected_signature, sizeof(expected_signature));
    return true;
}

bool auth_validate_request(struct MHD_Connection *connection)
{
    char username[129];
    return auth_read_session(connection, username, sizeof(username));
}

bool auth_validate_csrf(struct MHD_Connection *connection)
{
    char cookie_token[AUTH_HEX_SIZE];
    const char *header_token;

    if (!auth_validate_request(connection)
        || !read_cookie(
            connection,
            AUTH_CSRF_COOKIE_NAME,
            cookie_token,
            sizeof(cookie_token)
        )) {
        return false;
    }

    header_token = MHD_lookup_connection_value(
        connection,
        MHD_HEADER_KIND,
        "X-CSRF-Token"
    );

    if (header_token == NULL || strlen(header_token) != 64) {
        return false;
    }

    return CRYPTO_memcmp(cookie_token, header_token, 64) == 0;
}

void auth_clear_secrets(void)
{
    OPENSSL_cleanse(configured_username, sizeof(configured_username));
    OPENSSL_cleanse(password_salt, sizeof(password_salt));
    OPENSSL_cleanse(password_digest, sizeof(password_digest));
    OPENSSL_cleanse(session_secret, sizeof(session_secret));
    password_digest_length = 0;
    session_secret_length = 0;
    auth_ready = false;
}
