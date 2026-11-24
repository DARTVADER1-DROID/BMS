#include "domain/entities/battery.h"

#include <cjson/cJSON.h>

#include <ctype.h>
#include <stddef.h>
#include <string.h>

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

static bool is_json_object(const char *json)
{
    cJSON *document;
    bool valid;

    if (!is_non_empty_text(json)) {
        return false;
    }

    document = cJSON_Parse(json);
    if (document == NULL) {
        return false;
    }

    valid = cJSON_IsObject(document);
    cJSON_Delete(document);
    return valid;
}

bool battery_validate(const Battery *battery)
{
    if (battery == NULL
        || !is_valid_uuid(&battery->battery_id)
        || !is_valid_uuid(&battery->fleet_id)
        || !is_non_empty_text(battery->name)
        || !is_non_empty_text(battery->status)
        || !is_json_object(battery->configuration_json)) {
        return false;
    }

    return true;
}

bool battery_init(
    Battery *battery,
    const BmsUuid *battery_id,
    const BmsUuid *fleet_id,
    const char *name,
    const char *status,
    const char *configuration_json)
{
    Battery candidate;

    if (battery == NULL || battery_id == NULL || fleet_id == NULL) {
        return false;
    }

    candidate.battery_id = *battery_id;
    candidate.fleet_id = *fleet_id;
    candidate.name = name;
    candidate.status = status;
    candidate.configuration_json = configuration_json;

    if (!battery_validate(&candidate)) {
        return false;
    }

    *battery = candidate;
    return true;
}

bool battery_rename(Battery *battery, const char *name)
{
    if (battery == NULL || !battery_validate(battery)
        || !is_non_empty_text(name)) {
        return false;
    }

    battery->name = name;
    return true;
}

bool battery_set_status(Battery *battery, const char *status)
{
    if (battery == NULL || !battery_validate(battery)
        || !is_non_empty_text(status)) {
        return false;
    }

    battery->status = status;
    return true;
}

bool battery_update_configuration(Battery *battery, const char *configuration_json)
{
    if (battery == NULL || !battery_validate(battery)
        || !is_json_object(configuration_json)) {
        return false;
    }

    battery->configuration_json = configuration_json;
    return true;
}
