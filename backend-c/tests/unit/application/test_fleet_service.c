#include "application/fleet/fleet_service.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition, code) do { if (!(condition)) { fprintf(stderr, "FleetService test failure: %d\\n", (code)); return (code); } } while (0)

typedef struct {
    bool has_fleet;
    BatteryFleet fleet;
    FleetRepositoryResult find_result;
    FleetRepositoryResult create_result;
    FleetRepositoryResult update_result;
    FleetRepositoryResult delete_result;
    unsigned int find_calls;
    unsigned int create_calls;
    unsigned int update_calls;
    unsigned int delete_calls;
} FakeFleetStore;

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

static FleetRepositoryResult fake_create(
    void *context, const BmsUuid *user_id, const BatteryFleet *fleet)
{
    FakeFleetStore *store = context;
    ++store->create_calls;

    if (store->create_result != FLEET_REPOSITORY_OK) {
        return store->create_result;
    }
    if (store->has_fleet) {
        return FLEET_REPOSITORY_ALREADY_EXISTS;
    }
    if (!same_uuid(user_id, &fleet->owner_id)) {
        return FLEET_REPOSITORY_INVALID_ARGUMENT;
    }

    store->fleet = *fleet;
    store->has_fleet = true;
    return FLEET_REPOSITORY_OK;
}

static FleetRepositoryResult fake_find_by_user(
    void *context, const BmsUuid *user_id, BatteryFleet *fleet_out)
{
    FakeFleetStore *store = context;
    ++store->find_calls;

    if (store->find_result != FLEET_REPOSITORY_OK) {
        return store->find_result;
    }
    if (!store->has_fleet || !same_uuid(user_id, &store->fleet.owner_id)) {
        return FLEET_REPOSITORY_NOT_FOUND;
    }

    *fleet_out = store->fleet;
    return FLEET_REPOSITORY_OK;
}

static FleetRepositoryResult fake_update(
    void *context, const BmsUuid *user_id, const BatteryFleet *fleet)
{
    FakeFleetStore *store = context;
    ++store->update_calls;

    if (store->update_result != FLEET_REPOSITORY_OK) {
        return store->update_result;
    }
    if (!store->has_fleet || !same_uuid(user_id, &store->fleet.owner_id)) {
        return FLEET_REPOSITORY_NOT_FOUND;
    }
    if (!same_uuid(&store->fleet.fleet_id, &fleet->fleet_id)
        || !same_uuid(user_id, &fleet->owner_id)) {
        return FLEET_REPOSITORY_INVALID_ARGUMENT;
    }

    store->fleet = *fleet;
    return FLEET_REPOSITORY_OK;
}

static FleetRepositoryResult fake_delete(void *context, const BmsUuid *user_id)
{
    FakeFleetStore *store = context;
    ++store->delete_calls;

    if (store->delete_result != FLEET_REPOSITORY_OK) {
        return store->delete_result;
    }
    if (!store->has_fleet || !same_uuid(user_id, &store->fleet.owner_id)) {
        return FLEET_REPOSITORY_NOT_FOUND;
    }

    store->has_fleet = false;
    return FLEET_REPOSITORY_OK;
}

static void setup(
    FakeFleetStore *store,
    FleetRepository *repository,
    FleetService *service)
{
    store->has_fleet = false;
    store->fleet = (BatteryFleet){0};
    store->find_result = FLEET_REPOSITORY_OK;
    store->create_result = FLEET_REPOSITORY_OK;
    store->update_result = FLEET_REPOSITORY_OK;
    store->delete_result = FLEET_REPOSITORY_OK;
    store->find_calls = 0U;
    store->create_calls = 0U;
    store->update_calls = 0U;
    store->delete_calls = 0U;

    repository->context = store;
    repository->create = fake_create;
    repository->find_by_user = fake_find_by_user;
    repository->update = fake_update;
    repository->delete_by_user = fake_delete;
    (void)fleet_service_init(service, repository);
}

static int test_initialization_and_argument_validation(void)
{
    FakeFleetStore store;
    FleetRepository repository;
    FleetService service = {0};
    FleetRepository incomplete = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");

    setup(&store, &repository, &service);
    CHECK(!fleet_service_init(NULL, &repository), 1);
    CHECK(!fleet_service_init(&service, &incomplete), 2);
    CHECK(fleet_service_get_fleet(NULL, &user_id, &(BatteryFleet){0})
          == FLEET_SERVICE_INVALID_ARGUMENT, 3);
    CHECK(fleet_service_get_fleet(&service, NULL, &(BatteryFleet){0})
          == FLEET_SERVICE_INVALID_ARGUMENT, 4);
    CHECK(fleet_service_get_fleet(&service, &user_id, NULL)
          == FLEET_SERVICE_INVALID_ARGUMENT, 5);
    return 0;
}

static int test_create_and_duplicate_prevention(void)
{
    FakeFleetStore store;
    FleetRepository repository;
    FleetService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    const BmsUuid other_user =
        make_uuid("123e4567-e89b-12d3-a456-426614174002");
    BatteryFleet fleet = {0};

    setup(&store, &repository, &service);
    CHECK(battery_fleet_init(&fleet, &fleet_id, &user_id, "Primary", 10, 20), 6);
    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_OK, 7);
    CHECK(store.has_fleet, 8);
    CHECK(store.create_calls == 1U, 9);

    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_ALREADY_EXISTS, 10);
    CHECK(store.create_calls == 1U, 11);

    CHECK(fleet_service_create_fleet(&service, &other_user, &fleet)
          == FLEET_SERVICE_INVALID_ARGUMENT, 12);
    CHECK(store.create_calls == 1U, 13);

    fleet.name = " ";
    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_INVALID_ARGUMENT, 14);
    CHECK(store.create_calls == 1U, 15);
    return 0;
}

static int test_get_exists_and_summary(void)
{
    FakeFleetStore store;
    FleetRepository repository;
    FleetService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    BatteryFleet fleet = {0};
    BatteryFleet fetched = {0};
    FleetSummary summary = {0};
    bool exists = true;

    setup(&store, &repository, &service);
    CHECK(fleet_service_fleet_exists(&service, &user_id, &exists)
          == FLEET_SERVICE_OK, 16);
    CHECK(!exists, 17);
    CHECK(fleet_service_get_fleet(&service, &user_id, &fetched)
          == FLEET_SERVICE_NOT_FOUND, 18);
    CHECK(fleet_service_get_fleet_summary(&service, &user_id, &summary)
          == FLEET_SERVICE_NOT_FOUND, 19);

    CHECK(battery_fleet_init(&fleet, &fleet_id, &user_id, "Primary", 10, 20), 20);
    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_OK, 21);
    CHECK(fleet_service_fleet_exists(&service, &user_id, &exists)
          == FLEET_SERVICE_OK, 22);
    CHECK(exists, 23);
    CHECK(fleet_service_get_fleet(&service, &user_id, &fetched)
          == FLEET_SERVICE_OK, 24);
    CHECK(same_uuid(&fetched.fleet_id, &fleet_id), 25);
    CHECK(strcmp(fetched.name, "Primary") == 0, 26);
    CHECK(fleet_service_get_fleet_summary(&service, &user_id, &summary)
          == FLEET_SERVICE_OK, 27);
    CHECK(same_uuid(&summary.fleet_id, &fleet_id), 28);
    CHECK(strcmp(summary.name, "Primary") == 0, 29);
    CHECK(summary.created_at == 10 && summary.updated_at == 20, 30);
    return 0;
}

static int test_update_preserves_identity_and_rejects_replacement(void)
{
    FakeFleetStore store;
    FleetRepository repository;
    FleetService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    const BmsUuid replacement_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174011");
    BatteryFleet fleet = {0};
    BatteryFleet replacement = {0};

    setup(&store, &repository, &service);
    CHECK(battery_fleet_init(&fleet, &fleet_id, &user_id, "Before", 10, 20), 31);
    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_OK, 32);

    CHECK(battery_fleet_init(&fleet, &fleet_id, &user_id, "After", 10, 30), 33);
    CHECK(fleet_service_update_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_OK, 34);
    CHECK(store.update_calls == 1U, 35);
    CHECK(strcmp(store.fleet.name, "After") == 0, 36);
    CHECK(store.fleet.created_at == 10 && store.fleet.updated_at == 30, 37);

    CHECK(battery_fleet_init(
              &replacement, &replacement_id, &user_id, "Replacement", 10, 40), 38);
    CHECK(fleet_service_update_fleet(&service, &user_id, &replacement)
          == FLEET_SERVICE_INVALID_ARGUMENT, 39);
    CHECK(store.update_calls == 1U, 40);
    CHECK(strcmp(store.fleet.name, "After") == 0, 41);
    return 0;
}

static int test_delete_and_repository_error_mapping(void)
{
    FakeFleetStore store;
    FleetRepository repository;
    FleetService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    BatteryFleet fleet = {0};
    BatteryFleet fetched = {0};

    setup(&store, &repository, &service);
    CHECK(fleet_service_delete_fleet(&service, &user_id)
          == FLEET_SERVICE_NOT_FOUND, 42);
    CHECK(battery_fleet_init(&fleet, &fleet_id, &user_id, "Primary", 10, 20), 43);
    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_OK, 44);
    CHECK(fleet_service_delete_fleet(&service, &user_id)
          == FLEET_SERVICE_OK, 45);
    CHECK(!store.has_fleet && store.delete_calls == 2U, 46);
    CHECK(fleet_service_get_fleet(&service, &user_id, &fetched)
          == FLEET_SERVICE_NOT_FOUND, 47);

    store.find_result = FLEET_REPOSITORY_ERROR;
    CHECK(fleet_service_get_fleet(&service, &user_id, &fetched)
          == FLEET_SERVICE_REPOSITORY_ERROR, 48);
    store.delete_result = FLEET_REPOSITORY_ERROR;
    CHECK(fleet_service_delete_fleet(&service, &user_id)
          == FLEET_SERVICE_REPOSITORY_ERROR, 49);
    store.find_result = FLEET_REPOSITORY_OK;
    store.create_result = FLEET_REPOSITORY_ALREADY_EXISTS;
    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_ALREADY_EXISTS, 50);
    return 0;
}

static int test_repository_cannot_return_another_users_fleet(void)
{
    FakeFleetStore store;
    FleetRepository repository;
    FleetService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid other_user =
        make_uuid("123e4567-e89b-12d3-a456-426614174002");
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    BatteryFleet fleet = {0};
    BatteryFleet fetched = {0};

    setup(&store, &repository, &service);
    CHECK(battery_fleet_init(&fleet, &fleet_id, &other_user, "Private", 10, 20), 51);
    store.fleet = fleet;
    store.has_fleet = true;

    CHECK(fleet_service_get_fleet(&service, &user_id, &fetched)
          == FLEET_SERVICE_NOT_FOUND, 52);
    CHECK(fleet_service_get_fleet(&service, &other_user, &fetched)
          == FLEET_SERVICE_OK, 53);
    return 0;
}


static int test_all_uuid_validation_and_null_outputs(void)
{
    FakeFleetStore store;
    FleetRepository repository;
    FleetService service = {0};
    BmsUuid invalid_id = make_uuid("not-a-valid-uuid");
    BmsUuid user_id = make_uuid("123e4567-e89b-12d3-a456-426614174001");
    BatteryFleet fleet = {0};
    FleetSummary summary = {0};
    bool exists = true;

    setup(&store, &repository, &service);
    CHECK(fleet_service_create_fleet(&service, &invalid_id, &fleet)
          == FLEET_SERVICE_INVALID_ARGUMENT, 54);
    CHECK(fleet_service_get_fleet(&service, &invalid_id, &fleet)
          == FLEET_SERVICE_INVALID_ARGUMENT, 55);
    CHECK(fleet_service_update_fleet(&service, &invalid_id, &fleet)
          == FLEET_SERVICE_INVALID_ARGUMENT, 56);
    CHECK(fleet_service_delete_fleet(&service, &invalid_id)
          == FLEET_SERVICE_INVALID_ARGUMENT, 57);
    CHECK(fleet_service_fleet_exists(&service, &invalid_id, &exists)
          == FLEET_SERVICE_INVALID_ARGUMENT, 58);
    CHECK(fleet_service_fleet_exists(&service, &user_id, NULL)
          == FLEET_SERVICE_INVALID_ARGUMENT, 59);
    CHECK(fleet_service_get_fleet_summary(&service, &invalid_id, &summary)
          == FLEET_SERVICE_INVALID_ARGUMENT, 60);
    CHECK(fleet_service_get_fleet_summary(&service, &user_id, NULL)
          == FLEET_SERVICE_INVALID_ARGUMENT, 61);
    CHECK(store.find_calls == 0U && store.create_calls == 0U
          && store.update_calls == 0U && store.delete_calls == 0U, 62);
    return 0;
}

static int test_repository_error_mapping_for_each_operation(void)
{
    FakeFleetStore store;
    FleetRepository repository;
    FleetService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    BatteryFleet fleet = {0};
    BatteryFleet output = {0};
    bool exists = true;
    FleetSummary summary = {0};

    setup(&store, &repository, &service);
    CHECK(battery_fleet_init(&fleet, &fleet_id, &user_id, "Primary", 10, 20), 63);

    store.find_result = FLEET_REPOSITORY_INVALID_ARGUMENT;
    CHECK(fleet_service_get_fleet(&service, &user_id, &output)
          == FLEET_SERVICE_INVALID_ARGUMENT, 64);
    CHECK(fleet_service_fleet_exists(&service, &user_id, &exists)
          == FLEET_SERVICE_INVALID_ARGUMENT, 65);
    CHECK(exists, 66);
    CHECK(fleet_service_get_fleet_summary(&service, &user_id, &summary)
          == FLEET_SERVICE_INVALID_ARGUMENT, 67);
    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_INVALID_ARGUMENT, 68);

    store.find_result = FLEET_REPOSITORY_NOT_FOUND;
    store.create_result = FLEET_REPOSITORY_ERROR;
    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_REPOSITORY_ERROR, 69);

    store.create_result = FLEET_REPOSITORY_INVALID_ARGUMENT;
    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_INVALID_ARGUMENT, 70);

    store.create_result = FLEET_REPOSITORY_ALREADY_EXISTS;
    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_ALREADY_EXISTS, 71);

    store.create_result = FLEET_REPOSITORY_OK;
    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_OK, 72);

    store.update_result = FLEET_REPOSITORY_ERROR;
    fleet.name = "Changed";
    fleet.updated_at = 30;
    CHECK(fleet_service_update_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_REPOSITORY_ERROR, 73);
    store.update_result = FLEET_REPOSITORY_INVALID_ARGUMENT;
    CHECK(fleet_service_update_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_INVALID_ARGUMENT, 74);
    store.update_result = FLEET_REPOSITORY_OK;
    CHECK(fleet_service_update_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_OK, 75);

    store.delete_result = FLEET_REPOSITORY_INVALID_ARGUMENT;
    CHECK(fleet_service_delete_fleet(&service, &user_id)
          == FLEET_SERVICE_INVALID_ARGUMENT, 76);
    store.delete_result = FLEET_REPOSITORY_OK;
    CHECK(fleet_service_delete_fleet(&service, &user_id)
          == FLEET_SERVICE_OK, 77);
    return 0;
}

static int test_repository_corruption_is_rejected_without_output_changes(void)
{
    FakeFleetStore store;
    FleetRepository repository;
    FleetService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid other_user =
        make_uuid("123e4567-e89b-12d3-a456-426614174002");
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    BatteryFleet corrupt = {0};
    BatteryFleet output = {0};
    FleetSummary summary = {0};
    bool exists = true;

    setup(&store, &repository, &service);
    CHECK(battery_fleet_init(&corrupt, &fleet_id, &user_id, "Valid", 10, 20), 78);
    corrupt.name = "   ";
    store.fleet = corrupt;
    store.has_fleet = true;

    CHECK(fleet_service_get_fleet(&service, &user_id, &output)
          == FLEET_SERVICE_REPOSITORY_ERROR, 79);
    CHECK(fleet_service_fleet_exists(&service, &user_id, &exists)
          == FLEET_SERVICE_REPOSITORY_ERROR, 80);
    CHECK(exists, 81);
    summary.created_at = 999;
    CHECK(fleet_service_get_fleet_summary(&service, &user_id, &summary)
          == FLEET_SERVICE_REPOSITORY_ERROR, 82);
    CHECK(summary.created_at == 999, 83);

    CHECK(battery_fleet_init(&corrupt, &fleet_id, &other_user, "Wrong owner", 10, 20), 84);
    store.fleet = corrupt;
    CHECK(fleet_service_get_fleet(&service, &user_id, &output)
          == FLEET_SERVICE_NOT_FOUND, 85);
    return 0;
}

static int test_update_requires_existing_fleet_and_immutable_creation_time(void)
{
    FakeFleetStore store;
    FleetRepository repository;
    FleetService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid fleet_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174010");
    BatteryFleet fleet = {0};

    setup(&store, &repository, &service);
    CHECK(battery_fleet_init(&fleet, &fleet_id, &user_id, "Primary", 10, 20), 86);
    CHECK(fleet_service_update_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_NOT_FOUND, 87);
    CHECK(store.update_calls == 0U, 88);

    CHECK(fleet_service_create_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_OK, 89);
    fleet.name = "Changed";
    fleet.created_at = 11;
    fleet.updated_at = 30;
    CHECK(fleet_service_update_fleet(&service, &user_id, &fleet)
          == FLEET_SERVICE_INVALID_ARGUMENT, 90);
    CHECK(store.update_calls == 0U, 91);
    CHECK(strcmp(store.fleet.name, "Primary") == 0, 92);
    return 0;
}

int main(void)
{
    int result;

    result = test_initialization_and_argument_validation();
    if (result != 0) return result;
    result = test_create_and_duplicate_prevention();
    if (result != 0) return result;
    result = test_get_exists_and_summary();
    if (result != 0) return result;
    result = test_update_preserves_identity_and_rejects_replacement();
    if (result != 0) return result;
    result = test_delete_and_repository_error_mapping();
    if (result != 0) return result;
    result = test_repository_cannot_return_another_users_fleet();
    if (result != 0) return result;
    result = test_all_uuid_validation_and_null_outputs();
    if (result != 0) return result;
    result = test_repository_error_mapping_for_each_operation();
    if (result != 0) return result;
    result = test_repository_corruption_is_rejected_without_output_changes();
    if (result != 0) return result;
    result = test_update_requires_existing_fleet_and_immutable_creation_time();
    if (result != 0) return result;

    return 0;
}
