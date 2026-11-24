#ifndef BMS_DOMAIN_ENTITIES_BATTERY_FLEET_H
#define BMS_DOMAIN_ENTITIES_BATTERY_FLEET_H

#include "domain/value_objects/domain_types.h"

#include <stdbool.h>

/*
 * Domain representation of a user's battery fleet.
 *
 * A user may own zero or one fleet. String fields are non-owning references:
 * the caller is responsible for keeping the referenced name alive.
 */
typedef struct {
    BmsUuid fleet_id;
    BmsUuid owner_id;
    const char *name;
    BmsDateTime created_at;
    BmsDateTime updated_at;
} BatteryFleet;

/* Initialize a fleet; a failed initialization leaves the target unchanged. */
bool battery_fleet_init(
    BatteryFleet *fleet,
    const BmsUuid *fleet_id,
    const BmsUuid *owner_id,
    const char *name,
    BmsDateTime created_at,
    BmsDateTime updated_at);

/* Validate the fleet's identifiers and name. */
bool battery_fleet_validate(const BatteryFleet *fleet);

/* Rename a valid fleet without changing its other attributes. */
bool battery_fleet_rename(BatteryFleet *fleet, const char *name);

#endif
