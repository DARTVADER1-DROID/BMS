#include "application/battery/battery_repository.h"

#include <stddef.h>

static BatteryRepositoryResult fake_create(
    void *context, const BmsUuid *user_id, const Battery *battery)
{
    (void)context;
    (void)user_id;
    (void)battery;
    return BATTERY_REPOSITORY_ERROR;
}

static BatteryRepositoryResult fake_find_all(
    void *context,
    const BmsUuid *user_id,
    Battery *batteries_out,
    size_t capacity,
    size_t *count_out)
{
    (void)context;
    (void)user_id;
    (void)batteries_out;
    (void)capacity;
    (void)count_out;
    return BATTERY_REPOSITORY_ERROR;
}

static BatteryRepositoryResult fake_find_by_id(
    void *context,
    const BmsUuid *user_id,
    const BmsUuid *battery_id,
    Battery *battery_out)
{
    (void)context;
    (void)user_id;
    (void)battery_id;
    (void)battery_out;
    return BATTERY_REPOSITORY_ERROR;
}

static BatteryRepositoryResult fake_update(
    void *context, const BmsUuid *user_id, const Battery *battery)
{
    (void)context;
    (void)user_id;
    (void)battery;
    return BATTERY_REPOSITORY_ERROR;
}

static BatteryRepositoryResult fake_delete(
    void *context, const BmsUuid *user_id, const BmsUuid *battery_id)
{
    (void)context;
    (void)user_id;
    (void)battery_id;
    return BATTERY_REPOSITORY_ERROR;
}

static int test_null_and_empty_repositories_are_invalid(void)
{
    const BatteryRepository empty = {0};

    if (battery_repository_is_valid(NULL)) {
        return 1;
    }
    if (battery_repository_is_valid(&empty)) {
        return 2;
    }
    return 0;
}

static int test_each_operation_is_required(void)
{
    BatteryRepository repository = {
        .context = NULL,
        .create = fake_create,
        .find_all = fake_find_all,
        .find_by_id = fake_find_by_id,
        .update = fake_update,
        .delete = fake_delete
    };

    if (!battery_repository_is_valid(&repository)) {
        return 3;
    }

    repository.create = NULL;
    if (battery_repository_is_valid(&repository)) {
        return 4;
    }
    repository.create = fake_create;

    repository.find_all = NULL;
    if (battery_repository_is_valid(&repository)) {
        return 5;
    }
    repository.find_all = fake_find_all;

    repository.find_by_id = NULL;
    if (battery_repository_is_valid(&repository)) {
        return 6;
    }
    repository.find_by_id = fake_find_by_id;

    repository.update = NULL;
    if (battery_repository_is_valid(&repository)) {
        return 7;
    }
    repository.update = fake_update;

    repository.delete = NULL;
    if (battery_repository_is_valid(&repository)) {
        return 8;
    }

    return 0;
}

int main(void)
{
    int result = test_null_and_empty_repositories_are_invalid();

    if (result != 0) {
        return result;
    }

    return test_each_operation_is_required();
}
