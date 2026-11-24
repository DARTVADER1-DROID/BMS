#ifndef BMS_DOMAIN_ENTITIES_BATTERY_H
#define BMS_DOMAIN_ENTITIES_BATTERY_H

#include "domain/value_objects/domain_types.h"

/*
 * Domain representation of a battery belonging to exactly one fleet.
 *
 * String fields are non-owning references. configuration_json points to a
 * serialized JSON value; parsing, validation and ownership are intentionally
 * left to the application/persistence layer at this scaffold stage.
 */
typedef struct {
    BmsUuid battery_id;
    BmsUuid fleet_id;
    const char *name;
    const char *status;
    const char *configuration_json;
} Battery;

#endif
