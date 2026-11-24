#ifndef BMS_DOMAIN_VALUE_OBJECTS_DOMAIN_TYPES_H
#define BMS_DOMAIN_VALUE_OBJECTS_DOMAIN_TYPES_H

#include <stdint.h>

/*
 * Shared value types used by domain entities.
 *
 * UUID values use the canonical 36-character textual representation plus
 * the terminating null byte. Date/time values are Unix timestamps in
 * milliseconds (UTC).
 */
typedef struct {
    char value[37];
} BmsUuid;

typedef int64_t BmsDateTime;

#endif
