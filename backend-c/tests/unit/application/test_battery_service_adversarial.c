#include "application/battery/battery_service.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, code) do { \
    if (!(condition)) { \
        fprintf(stderr, "BatteryService adversarial test failed: %d\\n", (code)); \
        return (code); \
    } \
} while (0)

#define USERS 2U
#define RECORDS 4U

typedef struct {
    BmsUuid user_ids[USERS];
    Battery records[USERS][RECORDS];
    size_t counts[USERS];
    BatteryRepositoryResult result;
    int mode;
    unsigned int calls;
} AdversarialStore;

enum {
    MODE_NORMAL = 0,
    MODE_PARTIAL_ERROR,
    MODE_WRONG_ID,
    MODE_COUNT_OVERFLOW,
    MODE_COUNT_ONLY_LIES,
    MODE_UNKNOWN_RESULT
};

static BmsUuid uuid(const char *value)
{
    BmsUuid result = {{0}};
    (void)snprintf(result.value, sizeof(result.value), "%s", value);
    return result;
}

static int user_index(const AdversarialStore *store, const BmsUuid *user)
{
    size_t i;
    for (i = 0U; i < USERS; ++i) {
        if (memcmp(store->user_ids[i].value, user->value,
                   sizeof(user->value)) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static BatteryRepositoryResult create_record(
    void *context, const BmsUuid *user, const Battery *battery)
{
    AdversarialStore *store = context;
    const int owner = user_index(store, user);
    size_t i;
    ++store->calls;
    if (owner < 0) return BATTERY_REPOSITORY_NOT_FOUND;
    if (store->result != BATTERY_REPOSITORY_OK) return store->result;
    for (i = 0U; i < store->counts[(size_t)owner]; ++i) {
        if (memcmp(store->records[(size_t)owner][i].battery_id.value,
                   battery->battery_id.value, sizeof(battery->battery_id.value)) == 0) {
            return BATTERY_REPOSITORY_ALREADY_EXISTS;
        }
    }
    if (store->counts[(size_t)owner] >= RECORDS) return BATTERY_REPOSITORY_ERROR;
    store->records[(size_t)owner][store->counts[(size_t)owner]++] = *battery;
    return BATTERY_REPOSITORY_OK;
}

static BatteryRepositoryResult find_all_records(
    void *context, const BmsUuid *user, Battery *out, size_t capacity,
    size_t *count_out)
{
    AdversarialStore *store = context;
    const int owner = user_index(store, user);
    size_t count, copy_count, i;
    ++store->calls;
    if (owner < 0) return BATTERY_REPOSITORY_NOT_FOUND;
    if (store->mode == MODE_PARTIAL_ERROR) {
        if (out != NULL && capacity > 0U) out[0].name = "partial-corruption";
        return BATTERY_REPOSITORY_ERROR;
    }
    if (store->mode == MODE_UNKNOWN_RESULT) return (BatteryRepositoryResult)999;
    count = store->counts[(size_t)owner];
    if (store->mode == MODE_COUNT_ONLY_LIES && out == NULL) {
        *count_out = 1U;
        return BATTERY_REPOSITORY_OK;
    }
    if (store->mode == MODE_COUNT_OVERFLOW && out != NULL) {
        *count_out = capacity + 1U;
        return BATTERY_REPOSITORY_OK;
    }
    *count_out = count;
    if (count > capacity) return BATTERY_REPOSITORY_BUFFER_TOO_SMALL;
    copy_count = count;
    for (i = 0U; i < copy_count; ++i) out[i] = store->records[(size_t)owner][i];
    return BATTERY_REPOSITORY_OK;
}

static BatteryRepositoryResult find_one_record(
    void *context, const BmsUuid *user, const BmsUuid *id, Battery *out)
{
    AdversarialStore *store = context;
    const int owner = user_index(store, user);
    size_t i;
    ++store->calls;
    if (owner < 0) return BATTERY_REPOSITORY_NOT_FOUND;
    if (store->result != BATTERY_REPOSITORY_OK) return store->result;
    for (i = 0U; i < store->counts[(size_t)owner]; ++i) {
        if (memcmp(store->records[(size_t)owner][i].battery_id.value,
                   id->value, sizeof(id->value)) == 0) {
            *out = store->records[(size_t)owner][i];
            if (store->mode == MODE_WRONG_ID) {
                out->battery_id = uuid("123e4567-e89b-12d3-a456-426614174199");
            }
            return BATTERY_REPOSITORY_OK;
        }
    }
    return BATTERY_REPOSITORY_NOT_FOUND;
}

static BatteryRepositoryResult update_record(
    void *context, const BmsUuid *user, const Battery *battery)
{
    AdversarialStore *store = context;
    const int owner = user_index(store, user);
    size_t i;
    ++store->calls;
    if (owner < 0) return BATTERY_REPOSITORY_NOT_FOUND;
    if (store->result != BATTERY_REPOSITORY_OK) return store->result;
    for (i = 0U; i < store->counts[(size_t)owner]; ++i) {
        if (memcmp(store->records[(size_t)owner][i].battery_id.value,
                   battery->battery_id.value, sizeof(battery->battery_id.value)) == 0) {
            store->records[(size_t)owner][i] = *battery;
            return BATTERY_REPOSITORY_OK;
        }
    }
    return BATTERY_REPOSITORY_NOT_FOUND;
}

static BatteryRepositoryResult delete_record(
    void *context, const BmsUuid *user, const BmsUuid *id)
{
    AdversarialStore *store = context;
    const int owner = user_index(store, user);
    size_t i;
    ++store->calls;
    if (owner < 0) return BATTERY_REPOSITORY_NOT_FOUND;
    if (store->result != BATTERY_REPOSITORY_OK) return store->result;
    for (i = 0U; i < store->counts[(size_t)owner]; ++i) {
        if (memcmp(store->records[(size_t)owner][i].battery_id.value,
                   id->value, sizeof(id->value)) == 0) {
            size_t j;
            for (j = i + 1U; j < store->counts[(size_t)owner]; ++j)
                store->records[(size_t)owner][j - 1U] = store->records[(size_t)owner][j];
            --store->counts[(size_t)owner];
            return BATTERY_REPOSITORY_OK;
        }
    }
    return BATTERY_REPOSITORY_NOT_FOUND;
}

static Battery make_battery(const char *id, const char *fleet, const char *name)
{
    Battery value = {0};
    const BmsUuid battery_id = uuid(id);
    const BmsUuid fleet_id = uuid(fleet);
    (void)battery_init(&value, &battery_id, &fleet_id, name, "active", "{}");
    return value;
}

static void setup(
    AdversarialStore *store, BatteryRepository *repository, BatteryService *service)
{
    *store = (AdversarialStore){0};
    store->user_ids[0] = uuid("123e4567-e89b-12d3-a456-426614174001");
    store->user_ids[1] = uuid("123e4567-e89b-12d3-a456-426614174002");
    store->result = BATTERY_REPOSITORY_OK;
    repository->context = store;
    repository->create = create_record;
    repository->find_all = find_all_records;
    repository->find_by_id = find_one_record;
    repository->update = update_record;
    repository->delete = delete_record;
    (void)battery_service_init(service, repository);
}

static int test_user_partition_and_cross_user_access(void)
{
    AdversarialStore store;
    BatteryRepository repository;
    BatteryService service = {0};
    Battery a = make_battery("123e4567-e89b-12d3-a456-426614174101",
        "123e4567-e89b-12d3-a456-426614174010", "User A pack");
    Battery b = make_battery("123e4567-e89b-12d3-a456-426614174102",
        "123e4567-e89b-12d3-a456-426614174020", "User B pack");
    Battery output = {0};
    Battery list[2] = {{0}};
    size_t count = 0U;

    setup(&store, &repository, &service);
    CHECK(battery_service_add_battery(&service, &store.user_ids[0], &a) == BATTERY_SERVICE_OK, 1);
    CHECK(battery_service_add_battery(&service, &store.user_ids[1], &b) == BATTERY_SERVICE_OK, 2);
    CHECK(battery_service_get_battery(&service, &store.user_ids[0], &b.battery_id, &output)
        == BATTERY_SERVICE_NOT_FOUND, 3);
    CHECK(battery_service_remove_battery(&service, &store.user_ids[0], &b.battery_id)
        == BATTERY_SERVICE_NOT_FOUND, 4);
    CHECK(battery_service_update_battery(&service, &store.user_ids[0], &b)
        == BATTERY_SERVICE_NOT_FOUND, 5);
    CHECK(battery_service_get_batteries(&service, &store.user_ids[0], list, 2U, &count)
        == BATTERY_SERVICE_OK, 6);
    CHECK(count == 1U && strcmp(list[0].name, "User A pack") == 0, 7);
    CHECK(battery_service_get_batteries(&service, &store.user_ids[1], list, 2U, &count)
        == BATTERY_SERVICE_OK, 8);
    CHECK(count == 1U && strcmp(list[0].name, "User B pack") == 0, 9);
    return 0;
}

static int test_invalid_uuid_and_no_repository_call(void)
{
    AdversarialStore store;
    BatteryRepository repository;
    BatteryService service = {0};
    BmsUuid malformed = uuid("123e4567xe89b-12d3-a456-426614174001");
    Battery battery = make_battery("123e4567-e89b-12d3-a456-426614174101",
        "123e4567-e89b-12d3-a456-426614174010", "Valid");
    unsigned int before;

    setup(&store, &repository, &service);
    before = store.calls;
    CHECK(battery_service_add_battery(&service, &malformed, &battery)
        == BATTERY_SERVICE_INVALID_ARGUMENT, 10);
    CHECK(battery_service_get_battery_count(&service, &malformed, &(size_t){0U})
        == BATTERY_SERVICE_INVALID_ARGUMENT, 11);
    CHECK(store.calls == before, 12);
    return 0;
}

static int test_repository_faults_and_output_atomicity(void)
{
    AdversarialStore store;
    BatteryRepository repository;
    BatteryService service = {0};
    Battery record = make_battery("123e4567-e89b-12d3-a456-426614174101",
        "123e4567-e89b-12d3-a456-426614174010", "Original");
    Battery output[2] = {record, record};
    size_t count = 777U;
    Battery single = record;

    setup(&store, &repository, &service);
    CHECK(battery_service_add_battery(&service, &store.user_ids[0], &record)
        == BATTERY_SERVICE_OK, 13);

    store.mode = MODE_PARTIAL_ERROR;
    CHECK(battery_service_get_batteries(&service, &store.user_ids[0], output, 2U, &count)
        == BATTERY_SERVICE_REPOSITORY_ERROR, 14);
    CHECK(strcmp(output[0].name, "Original") == 0 && count == 777U, 15);

    store.mode = MODE_COUNT_OVERFLOW;
    CHECK(battery_service_get_batteries(&service, &store.user_ids[0], output, 1U, &count)
        == BATTERY_SERVICE_REPOSITORY_ERROR, 16);
    CHECK(strcmp(output[0].name, "Original") == 0, 17);

    store.mode = MODE_WRONG_ID;
    single = record;
    CHECK(battery_service_get_battery(&service, &store.user_ids[0], &record.battery_id, &single)
        == BATTERY_SERVICE_REPOSITORY_ERROR, 18);
    CHECK(strcmp(single.name, "Original") == 0, 19);

    store.mode = MODE_COUNT_ONLY_LIES;
    count = 888U;
    CHECK(battery_service_get_battery_count(&service, &store.user_ids[0], &count)
        == BATTERY_SERVICE_REPOSITORY_ERROR, 20);
    CHECK(count == 888U, 21);

    store.mode = MODE_UNKNOWN_RESULT;
    CHECK(battery_service_get_batteries(&service, &store.user_ids[0], output, 2U, &count)
        == BATTERY_SERVICE_REPOSITORY_ERROR, 22);
    return 0;
}

static int test_repository_error_paths_for_mutations(void)
{
    AdversarialStore store;
    BatteryRepository repository;
    BatteryService service = {0};
    Battery record = make_battery("123e4567-e89b-12d3-a456-426614174101",
        "123e4567-e89b-12d3-a456-426614174010", "Original");
    Battery changed = record;

    setup(&store, &repository, &service);
    CHECK(battery_service_add_battery(&service, &store.user_ids[0], &record)
        == BATTERY_SERVICE_OK, 23);
    store.result = BATTERY_REPOSITORY_ERROR;
    CHECK(battery_service_add_battery(&service, &store.user_ids[0], &changed)
        == BATTERY_SERVICE_REPOSITORY_ERROR, 24);
    CHECK(battery_service_update_battery(&service, &store.user_ids[0], &changed)
        == BATTERY_SERVICE_REPOSITORY_ERROR, 25);
    CHECK(battery_service_remove_battery(&service, &store.user_ids[0], &record.battery_id)
        == BATTERY_SERVICE_REPOSITORY_ERROR, 26);
    return 0;
}

int main(void)
{
    int result = test_user_partition_and_cross_user_access();
    if (result != 0) return result;
    result = test_invalid_uuid_and_no_repository_call();
    if (result != 0) return result;
    result = test_repository_faults_and_output_atomicity();
    if (result != 0) return result;
    return test_repository_error_paths_for_mutations();
}
