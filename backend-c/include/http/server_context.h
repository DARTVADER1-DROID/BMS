#ifndef BMS_SERVER_CONTEXT_H
#define BMS_SERVER_CONTEXT_H

#include "core/bms.h"

typedef struct {
    Bms *bms;
    const char *device_token;
    const char *auth_username;
    const char *cors_origin;
} BmsServerContext;

#endif
