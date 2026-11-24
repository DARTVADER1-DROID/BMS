#ifndef BMS_BATTERY_FLEET_H
#define BMS_BATTERY_FLEET_H

#include "domain/domain_types.h"

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

#endif
