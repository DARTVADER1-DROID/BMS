#include "application/battery/battery_service.h"

#include <stdlib.h>
#include <string.h>

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

static bool same_uuid(const BmsUuid *left, const BmsUuid *right)
{
    return left != NULL && right != NULL
        && memcmp(left->value, right->value, sizeof(left->value)) == 0;
}

static BatteryServiceResult map_repository_result(BatteryRepositoryResult result)
{
    switch (result) {
    case BATTERY_REPOSITORY_OK:
        return BATTERY_SERVICE_OK;
    case BATTERY_REPOSITORY_NOT_FOUND:
        return BATTERY_SERVICE_NOT_FOUND;
    case BATTERY_REPOSITORY_ALREADY_EXISTS:
        return BATTERY_SERVICE_ALREADY_EXISTS;
    case BATTERY_REPOSITORY_INVALID_ARGUMENT:
        return BATTERY_SERVICE_INVALID_ARGUMENT;
    case BATTERY_REPOSITORY_BUFFER_TOO_SMALL:
        return BATTERY_SERVICE_BUFFER_TOO_SMALL;
    case BATTERY_REPOSITORY_ERROR:
    default:
        return BATTERY_SERVICE_REPOSITORY_ERROR;
    }
}

static bool service_is_valid(const BatteryService *service)
{
    return service != NULL
        && battery_repository_is_valid(service->repository);
}

bool battery_service_init(
    BatteryService *service, const BatteryRepository *repository)
{
    BatteryService candidate;

    if (service == NULL || !battery_repository_is_valid(repository)) {
        return false;
    }

    candidate.repository = repository;
    *service = candidate;
    return true;
}

BatteryServiceResult battery_service_add_battery(
    const BatteryService *service,
    const BmsUuid *user_id,
    const Battery *battery)
{
    BatteryRepositoryResult repository_result;

    if (!service_is_valid(service) || !is_valid_uuid(user_id)
        || battery == NULL) {
        return BATTERY_SERVICE_INVALID_ARGUMENT;
    }
    if (!battery_validate(battery)) {
        return BATTERY_SERVICE_VALIDATION_ERROR;
    }

    repository_result = service->repository->create(
        service->repository->context, user_id, battery);
    return map_repository_result(repository_result);
}

BatteryServiceResult battery_service_get_batteries(
    const BatteryService *service,
    const BmsUuid *user_id,
    Battery *batteries_out,
    size_t capacity,
    size_t *count_out)
{
    Battery *temporary = NULL;
    size_t count = 0U;
    size_t index;
    BatteryRepositoryResult repository_result;

    if (!service_is_valid(service) || !is_valid_uuid(user_id)
        || count_out == NULL || (capacity > 0U && batteries_out == NULL)) {
        return BATTERY_SERVICE_INVALID_ARGUMENT;
    }

    if (capacity > 0U) {
        if (capacity > ((size_t)-1) / sizeof(*temporary)) {
            return BATTERY_SERVICE_INVALID_ARGUMENT;
        }
        temporary = malloc(capacity * sizeof(*temporary));
        if (temporary == NULL) {
            return BATTERY_SERVICE_REPOSITORY_ERROR;
        }
    }

    repository_result = service->repository->find_all(
        service->repository->context, user_id, temporary, capacity, &count);

    if (repository_result == BATTERY_REPOSITORY_OK) {
        if (count > capacity) {
            free(temporary);
            return BATTERY_SERVICE_REPOSITORY_ERROR;
        }
        for (index = 0U; index < count; ++index) {
            if (!battery_validate(&temporary[index])) {
                free(temporary);
                return BATTERY_SERVICE_REPOSITORY_ERROR;
            }
        }
        if (count > 0U) {
            memcpy(batteries_out, temporary, count * sizeof(*temporary));
        }
        *count_out = count;
        free(temporary);
        return BATTERY_SERVICE_OK;
    }

    if (repository_result == BATTERY_REPOSITORY_BUFFER_TOO_SMALL) {
        if (count <= capacity) {
            free(temporary);
            return BATTERY_SERVICE_REPOSITORY_ERROR;
        }
        *count_out = count;
        free(temporary);
        return BATTERY_SERVICE_BUFFER_TOO_SMALL;
    }
    free(temporary);
    return map_repository_result(repository_result);
}

BatteryServiceResult battery_service_get_battery(
    const BatteryService *service,
    const BmsUuid *user_id,
    const BmsUuid *battery_id,
    Battery *battery_out)
{
    Battery candidate = {0};
    BatteryRepositoryResult repository_result;

    if (!service_is_valid(service) || !is_valid_uuid(user_id)
        || !is_valid_uuid(battery_id) || battery_out == NULL) {
        return BATTERY_SERVICE_INVALID_ARGUMENT;
    }

    repository_result = service->repository->find_by_id(
        service->repository->context, user_id, battery_id, &candidate);
    if (repository_result != BATTERY_REPOSITORY_OK) {
        return map_repository_result(repository_result);
    }
    if (!battery_validate(&candidate)
        || !same_uuid(&candidate.battery_id, battery_id)) {
        return BATTERY_SERVICE_REPOSITORY_ERROR;
    }

    *battery_out = candidate;
    return BATTERY_SERVICE_OK;
}

BatteryServiceResult battery_service_update_battery(
    const BatteryService *service,
    const BmsUuid *user_id,
    const Battery *battery)
{
    Battery existing = {0};
    BatteryRepositoryResult repository_result;

    if (!service_is_valid(service) || !is_valid_uuid(user_id)
        || battery == NULL || !is_valid_uuid(&battery->battery_id)) {
        return BATTERY_SERVICE_INVALID_ARGUMENT;
    }
    if (!battery_validate(battery)) {
        return BATTERY_SERVICE_VALIDATION_ERROR;
    }

    repository_result = service->repository->find_by_id(
        service->repository->context, user_id, &battery->battery_id, &existing);
    if (repository_result != BATTERY_REPOSITORY_OK) {
        return map_repository_result(repository_result);
    }
    if (!battery_validate(&existing)
        || !same_uuid(&existing.battery_id, &battery->battery_id)) {
        return BATTERY_SERVICE_REPOSITORY_ERROR;
    }

    /* Battery identity and fleet membership cannot be changed by an update. */
    if (!same_uuid(&existing.fleet_id, &battery->fleet_id)) {
        return BATTERY_SERVICE_INVALID_ARGUMENT;
    }

    repository_result = service->repository->update(
        service->repository->context, user_id, battery);
    return map_repository_result(repository_result);
}

BatteryServiceResult battery_service_remove_battery(
    const BatteryService *service,
    const BmsUuid *user_id,
    const BmsUuid *battery_id)
{
    BatteryRepositoryResult repository_result;

    if (!service_is_valid(service) || !is_valid_uuid(user_id)
        || !is_valid_uuid(battery_id)) {
        return BATTERY_SERVICE_INVALID_ARGUMENT;
    }

    repository_result = service->repository->delete(
        service->repository->context, user_id, battery_id);
    return map_repository_result(repository_result);
}

BatteryServiceResult battery_service_get_battery_count(
    const BatteryService *service,
    const BmsUuid *user_id,
    size_t *count_out)
{
    size_t count = 0U;
    BatteryRepositoryResult repository_result;

    if (!service_is_valid(service) || !is_valid_uuid(user_id)
        || count_out == NULL) {
        return BATTERY_SERVICE_INVALID_ARGUMENT;
    }

    /*
     * A zero-capacity find_all is the repository's count-only operation:
     * it returns OK for an empty fleet, otherwise BUFFER_TOO_SMALL and the
     * required count. No battery records are copied.
     */
    repository_result = service->repository->find_all(
        service->repository->context, user_id, NULL, 0U, &count);

    if (repository_result == BATTERY_REPOSITORY_BUFFER_TOO_SMALL) {
        if (count == 0U) {
            return BATTERY_SERVICE_REPOSITORY_ERROR;
        }
        *count_out = count;
        return BATTERY_SERVICE_OK;
    }
    if (repository_result == BATTERY_REPOSITORY_OK) {
        if (count != 0U) {
            return BATTERY_SERVICE_REPOSITORY_ERROR;
        }
        *count_out = 0U;
        return BATTERY_SERVICE_OK;
    }

    return map_repository_result(repository_result);
}
