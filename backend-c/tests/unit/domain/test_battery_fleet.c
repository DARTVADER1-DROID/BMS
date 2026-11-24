#include "domain/entities/battery_fleet.h"

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

static bool same_fleet(const BatteryFleet *left, const BatteryFleet *right)
{
    return same_uuid(&left->fleet_id, &right->fleet_id)
        && same_uuid(&left->owner_id, &right->owner_id)
        && left->name == right->name
        && left->created_at == right->created_at
        && left->updated_at == right->updated_at;
}

static int test_valid_initialization(void)
{
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    const BmsUuid owner_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    BatteryFleet fleet = {0};

    CHECK(battery_fleet_init(
              &fleet, &fleet_id, &owner_id, "Primary fleet", 1000, 2000),
          1);
    CHECK(battery_fleet_validate(&fleet), 2);
    CHECK(same_uuid(&fleet.fleet_id, &fleet_id), 3);
    CHECK(same_uuid(&fleet.owner_id, &owner_id), 4);
    CHECK(strcmp(fleet.name, "Primary fleet") == 0, 5);
    CHECK(fleet.created_at == 1000, 6);
    CHECK(fleet.updated_at == 2000, 7);
    return 0;
}

static int test_initialization_rejects_invalid_arguments(void)
{
    const BmsUuid valid_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    const BmsUuid invalid_id = make_uuid("not-a-valid-uuid");
    BatteryFleet fleet = {0};
    BatteryFleet original = {0};

    CHECK(battery_fleet_init(
              &fleet, &valid_id, &valid_id, "Original", 10, 20),
          8);
    original = fleet;

    CHECK(!battery_fleet_init(NULL, &valid_id, &valid_id, "x", 0, 0), 9);
    CHECK(!battery_fleet_init(&fleet, NULL, &valid_id, "x", 0, 0), 10);
    CHECK(!battery_fleet_init(&fleet, &valid_id, NULL, "x", 0, 0), 11);
    CHECK(!battery_fleet_init(&fleet, &invalid_id, &valid_id, "x", 0, 0), 12);
    CHECK(!battery_fleet_init(&fleet, &valid_id, &invalid_id, "x", 0, 0), 13);
    CHECK(!battery_fleet_init(&fleet, &valid_id, &valid_id, NULL, 0, 0), 14);
    CHECK(!battery_fleet_init(&fleet, &valid_id, &valid_id, "", 0, 0), 15);
    CHECK(!battery_fleet_init(&fleet, &valid_id, &valid_id, " \t\r\n ", 0, 0), 16);
    CHECK(same_fleet(&fleet, &original), 17);
    return 0;
}

static int test_rename(void)
{
    const BmsUuid id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    BatteryFleet fleet = {0};
    BatteryFleet original = {0};

    CHECK(battery_fleet_init(&fleet, &id, &id, "Before", 10, 20), 18);
    original = fleet;

    CHECK(!battery_fleet_rename(NULL, "After"), 19);
    CHECK(!battery_fleet_rename(&fleet, NULL), 20);
    CHECK(!battery_fleet_rename(&fleet, ""), 21);
    CHECK(!battery_fleet_rename(&fleet, " \t "), 22);
    CHECK(same_fleet(&fleet, &original), 23);

    CHECK(battery_fleet_rename(&fleet, "After"), 24);
    CHECK(strcmp(fleet.name, "After") == 0, 25);
    CHECK(same_uuid(&fleet.fleet_id, &original.fleet_id), 26);
    CHECK(same_uuid(&fleet.owner_id, &original.owner_id), 27);
    CHECK(fleet.created_at == original.created_at, 28);
    CHECK(fleet.updated_at == original.updated_at, 29);

    fleet.name = NULL;
    CHECK(!battery_fleet_rename(&fleet, "Cannot repair invalid fleet"), 30);
    return 0;
}

static int test_validation(void)
{
    const BmsUuid id =
        make_uuid("123e4567-e89b-12d3-a456-426614174000");
    BatteryFleet fleet = {0};

    CHECK(battery_fleet_init(&fleet, &id, &id, "Fleet", 0, 0), 31);
    CHECK(battery_fleet_validate(&fleet), 32);

    fleet.fleet_id.value[8] = 'x';
    CHECK(!battery_fleet_validate(&fleet), 33);
    fleet.fleet_id = id;
    fleet.owner_id.value[35] = 'z';
    CHECK(!battery_fleet_validate(&fleet), 34);
    fleet.owner_id = id;
    fleet.name = " \t ";
    CHECK(!battery_fleet_validate(&fleet), 35);
    CHECK(!battery_fleet_validate(NULL), 36);
    return 0;
}

int main(void)
{
    int result;

    result = test_valid_initialization();
    if (result != 0) return result;
    result = test_initialization_rejects_invalid_arguments();
    if (result != 0) return result;
    result = test_rename();
    if (result != 0) return result;
    result = test_validation();
    if (result != 0) return result;

    return 0;
}
