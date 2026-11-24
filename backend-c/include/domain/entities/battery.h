#ifndef BMS_DOMAIN_ENTITIES_BATTERY_H
#define BMS_DOMAIN_ENTITIES_BATTERY_H

#include "domain/value_objects/domain_types.h"

#include <stdbool.h>

/*
 * Domain representation of one battery belonging to exactly one fleet.
 *
 * String fields are non-owning references. The caller must keep the supplied
 * strings alive for as long as the Battery uses them. configuration_json
 * must contain a JSON object.
 */
typedef struct {
    BmsUuid battery_id;
    BmsUuid fleet_id;
    const char *name;
    const char *status;
    const char *configuration_json;
} Battery;

/*
 * Initialize a battery from caller-owned values.
 * Returns false and leaves battery unchanged when an argument is invalid.
 */
bool battery_init(
    Battery *battery,
    const BmsUuid *battery_id,
    const BmsUuid *fleet_id,
    const char *name,
    const char *status,
    const char *configuration_json);

/* Validate all fields and their basic invariants. */
bool battery_validate(const Battery *battery);

/* Update one field after validating it; other fields remain unchanged. */
bool battery_rename(Battery *battery, const char *name);
bool battery_set_status(Battery *battery, const char *status);
bool battery_update_configuration(Battery *battery, const char *configuration_json);

#endif
