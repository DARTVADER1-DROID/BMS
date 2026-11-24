#include "domain/entities/battery.h"

#include <stddef.h>
#include <string.h>

static BmsUuid make_uuid(const char *value)
{
    BmsUuid uuid = {{0}};
    size_t index = 0U;

    while (value[index] != '\0' && index < sizeof(uuid.value)) {
        uuid.value[index] = value[index];
        ++index;
    }

    return uuid;
}

int main(void)
{
    const BmsUuid battery_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid invalid_id = make_uuid("not-a-valid-uuid");
    Battery battery = {0};
    Battery unchanged = {0};

    if (!battery_init(
            &battery, &battery_id, &fleet_id, "Pack 01", "ACTIVE", "{}")) {
        return 1;
    }

    if (!battery_validate(&battery)) {
        return 2;
    }

    if (battery.battery_id.value[36] != '\0'
        || battery.fleet_id.value[36] != '\0'
        || strcmp(battery.name, "Pack 01") != 0
        || strcmp(battery.status, "ACTIVE") != 0
        || strcmp(battery.configuration_json, "{}") != 0) {
        return 3;
    }

    if (!battery_rename(&battery, "Backup Pack")
        || strcmp(battery.name, "Backup Pack") != 0) {
        return 4;
    }

    if (!battery_set_status(&battery, "MAINTENANCE")
        || strcmp(battery.status, "MAINTENANCE") != 0) {
        return 5;
    }

    if (!battery_update_configuration(&battery, "{\"cells\": 4}")
        || strcmp(battery.configuration_json, "{\"cells\": 4}") != 0) {
        return 6;
    }

    unchanged = battery;
    if (battery_rename(&battery, "   ") || battery.name != unchanged.name) {
        return 7;
    }

    if (battery_set_status(&battery, "") || battery.status != unchanged.status) {
        return 8;
    }

    if (battery_update_configuration(&battery, "[1, 2]")
        || battery.configuration_json != unchanged.configuration_json) {
        return 9;
    }

    if (battery_update_configuration(&battery, "{malformed")
        || battery.configuration_json != unchanged.configuration_json) {
        return 10;
    }

    if (battery_init(
            &battery, &invalid_id, &fleet_id, "Invalid", "ACTIVE", "{}")) {
        return 11;
    }

    if (battery_init(
            &battery, &battery_id, &fleet_id, "   ", "ACTIVE", "{}")) {
        return 12;
    }

    if (battery_init(
            &battery, &battery_id, &fleet_id, "Pack", "ACTIVE", "null")) {
        return 13;
    }

    if (battery_validate(NULL) || battery_rename(NULL, "Pack")
        || battery_set_status(NULL, "ACTIVE")
        || battery_update_configuration(NULL, "{}")) {
        return 14;
    }

    return 0;
}
