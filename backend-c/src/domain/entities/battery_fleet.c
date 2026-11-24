#include "domain/entities/battery_fleet.h"

#include <ctype.h>
#include <stddef.h>

#include <stdbool.h>

static bool is_non_empty_text(const char *value)
{
    const unsigned char *cursor;

    if (value == NULL) {
        return false;
    }

    cursor = (const unsigned char *)value;
    while (*cursor != '\0') {
        if (isspace(*cursor) == 0) {
            return true;
        }
        ++cursor;
    }

    return false;
}

static bool is_valid_uuid(const BmsUuid *uuid)
{
    size_t index;

    if (uuid == NULL || uuid->value[36] != '\0') {
        return false;
    }

    for (index = 0U; index < 36U; ++index) {
        const char character = uuid->value[index];
        const bool separator_position =
            index == 8U || index == 13U || index == 18U || index == 23U;

        if (separator_position) {
            if (character != '-') {
                return false;
            }
        } else if (!((character >= '0' && character <= '9')
                     || (character >= 'a' && character <= 'f')
                     || (character >= 'A' && character <= 'F'))) {
            return false;
        }
    }

    return true;
}

bool battery_fleet_validate(const BatteryFleet *fleet)
{
    return fleet != NULL
        && is_valid_uuid(&fleet->fleet_id)
        && is_valid_uuid(&fleet->owner_id)
        && is_non_empty_text(fleet->name);
}

bool battery_fleet_init(
    BatteryFleet *fleet,
    const BmsUuid *fleet_id,
    const BmsUuid *owner_id,
    const char *name,
    BmsDateTime created_at,
    BmsDateTime updated_at)
{
    BatteryFleet candidate;

    if (fleet == NULL || fleet_id == NULL || owner_id == NULL) {
        return false;
    }

    candidate.fleet_id = *fleet_id;
    candidate.owner_id = *owner_id;
    candidate.name = name;
    candidate.created_at = created_at;
    candidate.updated_at = updated_at;

    if (!battery_fleet_validate(&candidate)) {
        return false;
    }

    *fleet = candidate;
    return true;
}

bool battery_fleet_rename(BatteryFleet *fleet, const char *name)
{
    if (fleet == NULL || !battery_fleet_validate(fleet)
        || !is_non_empty_text(name)) {
        return false;
    }

    fleet->name = name;
    return true;
}
