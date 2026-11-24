#include "domain/entities/battery.h"
#include "domain/entities/battery_fleet.h"

#include <stdbool.h>
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

/*
 * The UML models the relationship through Battery.fleet_id. There is no
 * collection or service API yet, so this test exercises that association
 * contract without introducing behavior outside either entity's interface.
 */
static bool battery_belongs_to_fleet(
    const Battery *battery,
    const BatteryFleet *fleet)
{
    return battery_fleet_validate(fleet)
        && battery_validate(battery)
        && same_uuid(&battery->fleet_id, &fleet->fleet_id);
}

static int test_multiple_batteries_share_one_valid_fleet(void)
{
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    const BmsUuid owner_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid battery_one_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    const BmsUuid battery_two_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174011");
    BatteryFleet fleet = {0};
    Battery battery_one = {0};
    Battery battery_two = {0};

    CHECK(battery_fleet_init(
              &fleet, &fleet_id, &owner_id, "Main fleet", 1000, 2000), 1);
    CHECK(battery_init(
              &battery_one, &battery_one_id, &fleet_id,
              "Pack 01", "ACTIVE", "{\"capacity_ah\": 100}"), 2);
    CHECK(battery_init(
              &battery_two, &battery_two_id, &fleet_id,
              "Pack 02", "STANDBY", "{\"capacity_ah\": 120}"), 3);

    CHECK(battery_belongs_to_fleet(&battery_one, &fleet), 4);
    CHECK(battery_belongs_to_fleet(&battery_two, &fleet), 5);
    CHECK(battery_fleet_validate(&fleet), 6);
    CHECK(battery_validate(&battery_one), 7);
    CHECK(battery_validate(&battery_two), 8);
    return 0;
}

static int test_battery_with_different_fleet_id_is_not_associated(void)
{
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    const BmsUuid other_fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174002");
    const BmsUuid owner_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid battery_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    BatteryFleet fleet = {0};
    Battery battery = {0};

    CHECK(battery_fleet_init(
              &fleet, &fleet_id, &owner_id, "Main fleet", 1000, 2000), 9);
    CHECK(battery_init(
              &battery, &battery_id, &other_fleet_id,
              "Foreign pack", "ACTIVE", "{}"), 10);

    /* Both entities are individually valid; only their relationship differs. */
    CHECK(battery_fleet_validate(&fleet), 11);
    CHECK(battery_validate(&battery), 12);
    CHECK(!battery_belongs_to_fleet(&battery, &fleet), 13);
    return 0;
}

static int test_entity_updates_preserve_association_and_independence(void)
{
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    const BmsUuid owner_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid battery_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    BatteryFleet fleet = {0};
    Battery battery = {0};
    const char *original_fleet_name;

    CHECK(battery_fleet_init(
              &fleet, &fleet_id, &owner_id, "Original fleet", 1000, 2000), 14);
    CHECK(battery_init(
              &battery, &battery_id, &fleet_id,
              "Original pack", "ACTIVE", "{}"), 15);
    original_fleet_name = fleet.name;

    CHECK(battery_fleet_rename(&fleet, "Renamed fleet"), 16);
    CHECK(strcmp(fleet.name, "Renamed fleet") == 0, 17);
    CHECK(battery_belongs_to_fleet(&battery, &fleet), 18);

    CHECK(battery_rename(&battery, "Renamed pack"), 19);
    CHECK(battery_set_status(&battery, "MAINTENANCE"), 20);
    CHECK(battery_update_configuration(&battery, "{\"cells\": 16}"), 21);
    CHECK(battery_belongs_to_fleet(&battery, &fleet), 22);
    CHECK(strcmp(fleet.name, "Renamed fleet") == 0, 23);
    CHECK(strcmp(battery.name, "Renamed pack") == 0, 24);
    CHECK(strcmp(battery.status, "MAINTENANCE") == 0, 25);
    CHECK(strcmp(battery.configuration_json, "{\"cells\": 16}") == 0, 26);
    CHECK(original_fleet_name != NULL, 27);
    return 0;
}

static int test_invalid_entities_cannot_form_association(void)
{
    const BmsUuid valid_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    const BmsUuid battery_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    BatteryFleet fleet = {0};
    Battery battery = {0};

    CHECK(battery_fleet_init(
              &fleet, &valid_id, &valid_id, "Fleet", 0, 0), 28);
    CHECK(battery_init(
              &battery, &battery_id, &valid_id, "Pack", "ACTIVE", "{}"), 29);

    fleet.name = "   ";
    CHECK(!battery_belongs_to_fleet(&battery, &fleet), 30);
    fleet.name = "Fleet";
    battery.configuration_json = "[]";
    CHECK(!battery_belongs_to_fleet(&battery, &fleet), 31);
    CHECK(!battery_belongs_to_fleet(NULL, &fleet), 32);
    CHECK(!battery_belongs_to_fleet(&battery, NULL), 33);
    return 0;
}

int main(void)
{
    int result;

    result = test_multiple_batteries_share_one_valid_fleet();
    if (result != 0) return result;
    result = test_battery_with_different_fleet_id_is_not_associated();
    if (result != 0) return result;
    result = test_entity_updates_preserve_association_and_independence();
    if (result != 0) return result;
    result = test_invalid_entities_cannot_form_association();
    if (result != 0) return result;

    return 0;
}
