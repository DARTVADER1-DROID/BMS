#include "domain/entities/battery.h"

#include <stddef.h>
#include <string.h>

#define CHECK(condition, code) do { if (!(condition)) return (code); } while (0)

static BmsUuid make_uuid(const char *value)
{
    BmsUuid uuid = {{0}};
    size_t index = 0U;

    if (value == NULL) {
        return uuid;
    }

    while (value[index] != '\0' && index < sizeof(uuid.value)) {
        uuid.value[index] = value[index];
        ++index;
    }

    return uuid;
}

static bool same_uuid(const BmsUuid *left, const BmsUuid *right)
{
    return memcmp(left->value, right->value, sizeof(left->value)) == 0;
}

static bool same_battery(const Battery *left, const Battery *right)
{
    return same_uuid(&left->battery_id, &right->battery_id)
        && same_uuid(&left->fleet_id, &right->fleet_id)
        && left->name == right->name
        && left->status == right->status
        && left->configuration_json == right->configuration_json;
}

static int test_valid_initialization(void)
{
    const BmsUuid battery_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid uppercase_id =
        make_uuid("123E4567-E89B-12D3-A456-4266141740AB");
    Battery battery = {0};

    CHECK(battery_init(&battery, &battery_id, &fleet_id,
                      "Pack 01", "ACTIVE", "{}"), 1);
    CHECK(battery_validate(&battery), 2);
    CHECK(same_uuid(&battery.battery_id, &battery_id), 3);
    CHECK(same_uuid(&battery.fleet_id, &fleet_id), 4);
    CHECK(strcmp(battery.name, "Pack 01") == 0, 5);
    CHECK(strcmp(battery.status, "ACTIVE") == 0, 6);
    CHECK(strcmp(battery.configuration_json, "{}") == 0, 7);

    CHECK(battery_init(&battery, &uppercase_id, &fleet_id,
                       "Unicode name - 电池", "custom-status", " { \"x\" : [1, true, null] } "),
          8);
    CHECK(battery_validate(&battery), 9);
    return 0;
}

static int test_initialization_rejects_invalid_arguments(void)
{
    const BmsUuid valid_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    const BmsUuid invalid_id = make_uuid("not-a-valid-uuid");
    const BmsUuid malformed_separator =
        make_uuid("123e4567xe89b-12d3-a456-426614174000");
    const BmsUuid invalid_hex =
        make_uuid("123e4567-e89b-12d3-a456-42661417400g");
    const BmsUuid short_id =
        make_uuid("123e4567-e89b-12d3-a456-42661417400");
    Battery battery = {0};
    Battery original = {0};

    CHECK(battery_init(&battery, &valid_id, &valid_id,
                       "Original", "ACTIVE", "{}"), 10);
    original = battery;

    CHECK(!battery_init(NULL, &valid_id, &valid_id, "x", "y", "{}"), 11);
    CHECK(!battery_init(&battery, NULL, &valid_id, "x", "y", "{}"), 12);
    CHECK(!battery_init(&battery, &valid_id, NULL, "x", "y", "{}"), 13);
    CHECK(!battery_init(&battery, &invalid_id, &valid_id, "x", "y", "{}"), 14);
    CHECK(!battery_init(&battery, &valid_id, &invalid_id, "x", "y", "{}"), 15);
    CHECK(!battery_init(&battery, &malformed_separator, &valid_id, "x", "y", "{}"), 16);
    CHECK(!battery_init(&battery, &invalid_hex, &valid_id, "x", "y", "{}"), 17);
    CHECK(!battery_init(&battery, &short_id, &valid_id, "x", "y", "{}"), 18);
    CHECK(!battery_init(&battery, &valid_id, &valid_id, NULL, "y", "{}"), 19);
    CHECK(!battery_init(&battery, &valid_id, &valid_id, " \t\r\n ", "y", "{}"), 20);
    CHECK(!battery_init(&battery, &valid_id, &valid_id, "x", NULL, "{}"), 21);
    CHECK(!battery_init(&battery, &valid_id, &valid_id, "x", " \t\r\n ", "{}"), 22);
    CHECK(!battery_init(&battery, &valid_id, &valid_id, "x", "y", NULL), 23);
    CHECK(same_battery(&battery, &original), 24);
    return 0;
}

static int test_uuid_validation(void)
{
    Battery battery = {0};
    const BmsUuid valid_id =
        make_uuid("00000000-0000-0000-0000-000000000000");

    CHECK(battery_init(&battery, &valid_id, &valid_id, "Pack", "ACTIVE", "{}"), 25);
    CHECK(battery_validate(&battery), 26);

    battery.battery_id.value[8] = 'x';
    CHECK(!battery_validate(&battery), 27);
    battery.battery_id = valid_id;
    battery.fleet_id.value[35] = 'z';
    CHECK(!battery_validate(&battery), 28);
    return 0;
}

static int test_name_updates(void)
{
    const BmsUuid id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    Battery battery = {0};
    Battery before = {0};

    CHECK(battery_init(&battery, &id, &id, "Original", "ACTIVE", "{}"), 29);
    before = battery;
    CHECK(!battery_rename(NULL, "New name"), 30);
    CHECK(!battery_rename(&battery, NULL), 31);
    CHECK(!battery_rename(&battery, ""), 32);
    CHECK(!battery_rename(&battery, " \t\n "), 33);
    CHECK(same_battery(&battery, &before), 34);

    CHECK(battery_rename(&battery, "Original"), 35);
    CHECK(strcmp(battery.name, "Original") == 0, 36);
    CHECK(battery_rename(&battery, "  Leading and trailing  "), 37);
    CHECK(strcmp(battery.name, "  Leading and trailing  ") == 0, 38);
    CHECK(same_uuid(&battery.battery_id, &before.battery_id), 39);
    CHECK(same_uuid(&battery.fleet_id, &before.fleet_id), 40);
    CHECK(strcmp(battery.status, before.status) == 0, 41);
    CHECK(strcmp(battery.configuration_json, before.configuration_json) == 0, 42);

    battery.name = NULL;
    CHECK(!battery_rename(&battery, "Cannot repair invalid entity"), 43);
    return 0;
}

static int test_status_updates(void)
{
    const BmsUuid id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    Battery battery = {0};
    Battery before = {0};

    CHECK(battery_init(&battery, &id, &id, "Pack", "ACTIVE", "{}"), 44);
    before = battery;
    CHECK(!battery_set_status(NULL, "INACTIVE"), 45);
    CHECK(!battery_set_status(&battery, NULL), 46);
    CHECK(!battery_set_status(&battery, ""), 47);
    CHECK(!battery_set_status(&battery, " \t\n "), 48);
    CHECK(same_battery(&battery, &before), 49);

    CHECK(battery_set_status(&battery, "ACTIVE"), 50);
    CHECK(battery_set_status(&battery, "MAINTENANCE"), 51);
    CHECK(strcmp(battery.status, "MAINTENANCE") == 0, 52);
    CHECK(same_uuid(&battery.battery_id, &before.battery_id), 53);
    CHECK(same_uuid(&battery.fleet_id, &before.fleet_id), 54);
    CHECK(strcmp(battery.name, before.name) == 0, 55);
    CHECK(strcmp(battery.configuration_json, before.configuration_json) == 0, 56);

    battery.configuration_json = "not-json";
    CHECK(!battery_set_status(&battery, "INACTIVE"), 57);
    return 0;
}

static int test_configuration_json_validation(void)
{
    const BmsUuid id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    static const char *const invalid_json[] = {
        NULL,
        "",
        " ",
        "{",
        "{malformed}",
        "{\"key\":}",
        "{\"key\": 1,}",
        "{\"key\": 1} trailing",
        "{}{}",
        "[]",
        "[1, 2]",
        "null",
        "true",
        "false",
        "0",
        "42",
        "\"string\""
    };
    Battery battery = {0};
    Battery before = {0};
    size_t index;

    CHECK(battery_init(&battery, &id, &id, "Pack", "ACTIVE", "{}"), 58);
    before = battery;
    CHECK(!battery_update_configuration(NULL, "{}"), 59);

    for (index = 0U; index < sizeof(invalid_json) / sizeof(invalid_json[0]); ++index) {
        CHECK(!battery_update_configuration(&battery, invalid_json[index]), 60);
        CHECK(same_battery(&battery, &before), 61);
    }

    CHECK(battery_update_configuration(&battery, "{}"), 62);
    CHECK(battery_update_configuration(&battery, " { \"cells\" : 4 } "), 63);
    CHECK(battery_update_configuration(
              &battery,
              "{\"nested\":{\"array\":[1,2,3],\"enabled\":true},\"text\":\"quote: \\\"\"}"),
          64);
    CHECK(battery_validate(&battery), 65);
    CHECK(same_uuid(&battery.battery_id, &before.battery_id), 66);
    CHECK(same_uuid(&battery.fleet_id, &before.fleet_id), 67);
    CHECK(strcmp(battery.name, before.name) == 0, 68);
    CHECK(strcmp(battery.status, before.status) == 0, 69);

    battery.status = NULL;
    CHECK(!battery_update_configuration(&battery, "{}"), 70);
    return 0;
}

static int test_validate_each_field_independently(void)
{
    const BmsUuid id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    Battery battery = {0};

    CHECK(battery_init(&battery, &id, &id, "Pack", "ACTIVE", "{}"), 71);
    CHECK(battery_validate(&battery), 72);

    battery.battery_id.value[0] = 'x';
    CHECK(!battery_validate(&battery), 73);
    battery.battery_id = id;
    battery.fleet_id.value[0] = 'x';
    CHECK(!battery_validate(&battery), 74);
    battery.fleet_id = id;
    battery.name = "";
    CHECK(!battery_validate(&battery), 75);
    battery.name = "Pack";
    battery.status = " \t ";
    CHECK(!battery_validate(&battery), 76);
    battery.status = "ACTIVE";
    battery.configuration_json = "[]";
    CHECK(!battery_validate(&battery), 77);
    battery.configuration_json = "{}";
    CHECK(battery_validate(&battery), 78);
    CHECK(!battery_validate(NULL), 79);
    return 0;
}

int main(void)
{
    int result;

    result = test_valid_initialization();
    if (result != 0) return result;
    result = test_initialization_rejects_invalid_arguments();
    if (result != 0) return result;
    result = test_uuid_validation();
    if (result != 0) return result;
    result = test_name_updates();
    if (result != 0) return result;
    result = test_status_updates();
    if (result != 0) return result;
    result = test_configuration_json_validation();
    if (result != 0) return result;
    result = test_validate_each_field_independently();
    if (result != 0) return result;

    return 0;
}
