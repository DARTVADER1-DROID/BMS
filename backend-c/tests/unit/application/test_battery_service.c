#include "application/battery/battery_service.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, code) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "BatteryService test failure: %d\\n", (code)); \
            return (code); \
        } \
    } while (0)

#define STORE_CAPACITY 8U

typedef struct {
    Battery batteries[STORE_CAPACITY];
    size_t count;
    BatteryRepositoryResult create_result;
    BatteryRepositoryResult find_all_result;
    BatteryRepositoryResult find_by_id_result;
    BatteryRepositoryResult update_result;
    BatteryRepositoryResult delete_result;
    unsigned int create_calls;
    unsigned int find_all_calls;
    unsigned int find_by_id_calls;
    unsigned int update_calls;
    unsigned int delete_calls;
} FakeBatteryStore;

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

static bool same_uuid(const BmsUuid *left, const BmsUuid *right)
{
    return memcmp(left->value, right->value, sizeof(left->value)) == 0;
}

static BatteryRepositoryResult fake_create(
    void *context, const BmsUuid *user_id, const Battery *battery)
{
    FakeBatteryStore *store = context;
    size_t index;
    (void)user_id;
    ++store->create_calls;

    if (store->create_result != BATTERY_REPOSITORY_OK) {
        return store->create_result;
    }
    for (index = 0U; index < store->count; ++index) {
        if (same_uuid(&store->batteries[index].battery_id, &battery->battery_id)) {
            return BATTERY_REPOSITORY_ALREADY_EXISTS;
        }
    }
    if (store->count >= STORE_CAPACITY) {
        return BATTERY_REPOSITORY_ERROR;
    }
    store->batteries[store->count++] = *battery;
    return BATTERY_REPOSITORY_OK;
}

static BatteryRepositoryResult fake_find_all(
    void *context, const BmsUuid *user_id, Battery *batteries_out,
    size_t capacity, size_t *count_out)
{
    FakeBatteryStore *store = context;
    size_t index;
    (void)user_id;
    ++store->find_all_calls;

    if (store->find_all_result != BATTERY_REPOSITORY_OK) {
        return store->find_all_result;
    }
    *count_out = store->count;
    if (store->count > capacity) {
        return BATTERY_REPOSITORY_BUFFER_TOO_SMALL;
    }
    for (index = 0U; index < store->count; ++index) {
        batteries_out[index] = store->batteries[index];
    }
    return BATTERY_REPOSITORY_OK;
}

static BatteryRepositoryResult fake_find_by_id(
    void *context, const BmsUuid *user_id, const BmsUuid *battery_id,
    Battery *battery_out)
{
    FakeBatteryStore *store = context;
    size_t index;
    (void)user_id;
    ++store->find_by_id_calls;

    if (store->find_by_id_result != BATTERY_REPOSITORY_OK) {
        return store->find_by_id_result;
    }
    for (index = 0U; index < store->count; ++index) {
        if (same_uuid(&store->batteries[index].battery_id, battery_id)) {
            *battery_out = store->batteries[index];
            return BATTERY_REPOSITORY_OK;
        }
    }
    return BATTERY_REPOSITORY_NOT_FOUND;
}

static BatteryRepositoryResult fake_update(
    void *context, const BmsUuid *user_id, const Battery *battery)
{
    FakeBatteryStore *store = context;
    size_t index;
    (void)user_id;
    ++store->update_calls;

    if (store->update_result != BATTERY_REPOSITORY_OK) {
        return store->update_result;
    }
    for (index = 0U; index < store->count; ++index) {
        if (same_uuid(&store->batteries[index].battery_id, &battery->battery_id)) {
            store->batteries[index] = *battery;
            return BATTERY_REPOSITORY_OK;
        }
    }
    return BATTERY_REPOSITORY_NOT_FOUND;
}

static BatteryRepositoryResult fake_delete(
    void *context, const BmsUuid *user_id, const BmsUuid *battery_id)
{
    FakeBatteryStore *store = context;
    size_t index;
    (void)user_id;
    ++store->delete_calls;

    if (store->delete_result != BATTERY_REPOSITORY_OK) {
        return store->delete_result;
    }
    for (index = 0U; index < store->count; ++index) {
        if (same_uuid(&store->batteries[index].battery_id, battery_id)) {
            size_t next;
            for (next = index + 1U; next < store->count; ++next) {
                store->batteries[next - 1U] = store->batteries[next];
            }
            --store->count;
            return BATTERY_REPOSITORY_OK;
        }
    }
    return BATTERY_REPOSITORY_NOT_FOUND;
}

static void setup(
    FakeBatteryStore *store, BatteryRepository *repository, BatteryService *service)
{
    *store = (FakeBatteryStore){0};
    store->create_result = BATTERY_REPOSITORY_OK;
    store->find_all_result = BATTERY_REPOSITORY_OK;
    store->find_by_id_result = BATTERY_REPOSITORY_OK;
    store->update_result = BATTERY_REPOSITORY_OK;
    store->delete_result = BATTERY_REPOSITORY_OK;
    repository->context = store;
    repository->create = fake_create;
    repository->find_all = fake_find_all;
    repository->find_by_id = fake_find_by_id;
    repository->update = fake_update;
    repository->delete = fake_delete;
    (void)battery_service_init(service, repository);
}

static Battery make_battery(
    const char *battery_id, const char *fleet_id, const char *name)
{
    Battery battery = {0};
    const BmsUuid id = make_uuid(battery_id);
    const BmsUuid fleet = make_uuid(fleet_id);
    (void)battery_init(&battery, &id, &fleet, name, "active", "{}");
    return battery;
}

static int test_initialization_and_invalid_arguments(void)
{
    FakeBatteryStore store;
    BatteryRepository repository;
    BatteryService service = {0};
    BatteryRepository incomplete = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    Battery battery = make_battery(
        "123e4567-e89b-12d3-a456-426614174101",
        "123e4567-e89b-12d3-a456-426614174010", "Pack A");
    size_t count = 99U;

    setup(&store, &repository, &service);
    CHECK(!battery_service_init(NULL, &repository), 1);
    CHECK(!battery_service_init(&service, &incomplete), 2);
    CHECK(battery_service_add_battery(NULL, &user_id, &battery)
          == BATTERY_SERVICE_INVALID_ARGUMENT, 3);
    CHECK(battery_service_add_battery(&service, NULL, &battery)
          == BATTERY_SERVICE_INVALID_ARGUMENT, 4);
    CHECK(battery_service_get_batteries(&service, &user_id, NULL, 1U, &count)
          == BATTERY_SERVICE_INVALID_ARGUMENT, 5);
    CHECK(battery_service_get_batteries(&service, &user_id, NULL, 0U, NULL)
          == BATTERY_SERVICE_INVALID_ARGUMENT, 6);
    CHECK(battery_service_get_battery_count(&service, &user_id, NULL)
          == BATTERY_SERVICE_INVALID_ARGUMENT, 7);
    CHECK(store.create_calls == 0U && store.find_all_calls == 0U, 8);
    return 0;
}

static int test_add_validation_and_repository_result_mapping(void)
{
    FakeBatteryStore store;
    BatteryRepository repository;
    BatteryService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    Battery battery = make_battery(
        "123e4567-e89b-12d3-a456-426614174101",
        "123e4567-e89b-12d3-a456-426614174010", "Pack A");

    setup(&store, &repository, &service);
    CHECK(battery_service_add_battery(&service, &user_id, &battery)
          == BATTERY_SERVICE_OK, 9);
    CHECK(store.count == 1U && store.create_calls == 1U, 10);
    CHECK(battery_service_add_battery(&service, &user_id, &battery)
          == BATTERY_SERVICE_ALREADY_EXISTS, 11);

    battery.name = " ";
    CHECK(battery_service_add_battery(&service, &user_id, &battery)
          == BATTERY_SERVICE_VALIDATION_ERROR, 12);
    CHECK(store.create_calls == 2U, 13);

    battery = make_battery(
        "123e4567-e89b-12d3-a456-426614174102",
        "123e4567-e89b-12d3-a456-426614174010", "Pack B");
    store.create_result = BATTERY_REPOSITORY_ERROR;
    CHECK(battery_service_add_battery(&service, &user_id, &battery)
          == BATTERY_SERVICE_REPOSITORY_ERROR, 14);
    store.create_result = BATTERY_REPOSITORY_INVALID_ARGUMENT;
    CHECK(battery_service_add_battery(&service, &user_id, &battery)
          == BATTERY_SERVICE_INVALID_ARGUMENT, 15);
    return 0;
}

static int test_get_all_buffer_semantics_and_output_preservation(void)
{
    FakeBatteryStore store;
    BatteryRepository repository;
    BatteryService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    Battery first = make_battery(
        "123e4567-e89b-12d3-a456-426614174101",
        "123e4567-e89b-12d3-a456-426614174010", "Pack A");
    Battery second = make_battery(
        "123e4567-e89b-12d3-a456-426614174102",
        "123e4567-e89b-12d3-a456-426614174010", "Pack B");
    Battery output[2] = {0};
    size_t count = 77U;

    setup(&store, &repository, &service);
    CHECK(battery_service_get_batteries(&service, &user_id, output, 2U, &count)
          == BATTERY_SERVICE_OK, 16);
    CHECK(count == 0U, 17);

    CHECK(battery_service_add_battery(&service, &user_id, &first)
          == BATTERY_SERVICE_OK, 18);
    CHECK(battery_service_add_battery(&service, &user_id, &second)
          == BATTERY_SERVICE_OK, 19);
    CHECK(battery_service_get_batteries(&service, &user_id, output, 1U, &count)
          == BATTERY_SERVICE_BUFFER_TOO_SMALL, 20);
    CHECK(count == 2U, 21);
    CHECK(output[0].name == NULL, 22);

    CHECK(battery_service_get_batteries(&service, &user_id, output, 2U, &count)
          == BATTERY_SERVICE_OK, 23);
    CHECK(count == 2U, 24);
    CHECK(strcmp(output[0].name, "Pack A") == 0, 25);
    CHECK(strcmp(output[1].name, "Pack B") == 0, 26);
    return 0;
}

static int test_get_one_and_corrupt_repository_data(void)
{
    FakeBatteryStore store;
    BatteryRepository repository;
    BatteryService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid battery_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174101");
    Battery battery = make_battery(
        "123e4567-e89b-12d3-a456-426614174101",
        "123e4567-e89b-12d3-a456-426614174010", "Pack A");
    Battery output = make_battery(
        "123e4567-e89b-12d3-a456-426614174199",
        "123e4567-e89b-12d3-a456-426614174010", "Sentinel");

    setup(&store, &repository, &service);
    CHECK(battery_service_get_battery(&service, &user_id, &battery_id, &output)
          == BATTERY_SERVICE_NOT_FOUND, 27);
    CHECK(strcmp(output.name, "Sentinel") == 0, 28);
    CHECK(battery_service_add_battery(&service, &user_id, &battery)
          == BATTERY_SERVICE_OK, 29);
    CHECK(battery_service_get_battery(&service, &user_id, &battery_id, &output)
          == BATTERY_SERVICE_OK, 30);
    CHECK(strcmp(output.name, "Pack A") == 0, 31);

    store.batteries[0].name = " ";
    CHECK(battery_service_get_battery(&service, &user_id, &battery_id, &output)
          == BATTERY_SERVICE_REPOSITORY_ERROR, 32);
    CHECK(strcmp(output.name, "Pack A") == 0, 33);
    return 0;
}

static int test_update_identity_and_delete(void)
{
    FakeBatteryStore store;
    BatteryRepository repository;
    BatteryService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    const BmsUuid other_fleet =
        make_uuid("123e4567-e89b-12d3-a456-426614174099");
    const BmsUuid battery_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174101");
    Battery battery = make_battery(
        "123e4567-e89b-12d3-a456-426614174101",
        "123e4567-e89b-12d3-a456-426614174010", "Pack A");
    Battery changed = battery;

    setup(&store, &repository, &service);
    CHECK(battery_service_update_battery(&service, &user_id, &battery)
          == BATTERY_SERVICE_NOT_FOUND, 34);
    CHECK(battery_service_add_battery(&service, &user_id, &battery)
          == BATTERY_SERVICE_OK, 35);

    changed.name = "Renamed";
    changed.status = "maintenance";
    changed.configuration_json = "{\"series\":4}";
    CHECK(battery_service_update_battery(&service, &user_id, &changed)
          == BATTERY_SERVICE_OK, 36);
    CHECK(store.update_calls == 1U, 37);
    CHECK(strcmp(store.batteries[0].name, "Renamed") == 0, 38);

    changed.fleet_id = other_fleet;
    CHECK(battery_service_update_battery(&service, &user_id, &changed)
          == BATTERY_SERVICE_INVALID_ARGUMENT, 39);
    CHECK(store.update_calls == 1U, 40);

    changed = store.batteries[0];
    changed.configuration_json = "[]";
    CHECK(battery_service_update_battery(&service, &user_id, &changed)
          == BATTERY_SERVICE_VALIDATION_ERROR, 41);

    CHECK(battery_service_remove_battery(&service, &user_id, &battery_id)
          == BATTERY_SERVICE_OK, 42);
    CHECK(store.count == 0U, 43);
    CHECK(battery_service_remove_battery(&service, &user_id, &battery_id)
          == BATTERY_SERVICE_NOT_FOUND, 44);
    return 0;
}

static int test_count_and_error_mapping(void)
{
    FakeBatteryStore store;
    BatteryRepository repository;
    BatteryService service = {0};
    const BmsUuid user_id =
        make_uuid("123e4567-e89b-12d3-a456-426614174001");
    Battery battery = make_battery(
        "123e4567-e89b-12d3-a456-426614174101",
        "123e4567-e89b-12d3-a456-426614174010", "Pack A");
    size_t count = 99U;

    setup(&store, &repository, &service);
    CHECK(battery_service_get_battery_count(&service, &user_id, &count)
          == BATTERY_SERVICE_OK, 45);
    CHECK(count == 0U, 46);
    CHECK(battery_service_add_battery(&service, &user_id, &battery)
          == BATTERY_SERVICE_OK, 47);
    CHECK(battery_service_get_battery_count(&service, &user_id, &count)
          == BATTERY_SERVICE_OK, 48);
    CHECK(count == 1U, 49);

    store.find_all_result = BATTERY_REPOSITORY_ERROR;
    CHECK(battery_service_get_battery_count(&service, &user_id, &count)
          == BATTERY_SERVICE_REPOSITORY_ERROR, 50);
    store.find_all_result = BATTERY_REPOSITORY_INVALID_ARGUMENT;
    CHECK(battery_service_get_battery_count(&service, &user_id, &count)
          == BATTERY_SERVICE_INVALID_ARGUMENT, 51);
    store.delete_result = BATTERY_REPOSITORY_ERROR;
    CHECK(battery_service_remove_battery(
              &service, &user_id, &battery.battery_id)
          == BATTERY_SERVICE_REPOSITORY_ERROR, 52);
    return 0;
}

int main(void)
{
    int result;

    result = test_initialization_and_invalid_arguments();
    if (result != 0) return result;
    result = test_add_validation_and_repository_result_mapping();
    if (result != 0) return result;
    result = test_get_all_buffer_semantics_and_output_preservation();
    if (result != 0) return result;
    result = test_get_one_and_corrupt_repository_data();
    if (result != 0) return result;
    result = test_update_identity_and_delete();
    if (result != 0) return result;
    result = test_count_and_error_mapping();
    if (result != 0) return result;
    return 0;
}
