#include "application/fleet/fleet_service.h"

#include <string.h>

static bool is_valid_user_id(const BmsUuid *user_id)
{
    size_t index;

    if (user_id == NULL || user_id->value[36] != '\0') {
        return false;
    }

    for (index = 0U; index < 36U; ++index) {
        const char character = user_id->value[index];
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

static FleetServiceResult map_repository_result(FleetRepositoryResult result)
{
    switch (result) {
    case FLEET_REPOSITORY_OK:
        return FLEET_SERVICE_OK;
    case FLEET_REPOSITORY_NOT_FOUND:
        return FLEET_SERVICE_NOT_FOUND;
    case FLEET_REPOSITORY_ALREADY_EXISTS:
        return FLEET_SERVICE_ALREADY_EXISTS;
    case FLEET_REPOSITORY_INVALID_ARGUMENT:
        return FLEET_SERVICE_INVALID_ARGUMENT;
    case FLEET_REPOSITORY_ERROR:
    default:
        return FLEET_SERVICE_REPOSITORY_ERROR;
    }
}

static bool service_is_valid(const FleetService *service)
{
    return service != NULL
        && fleet_repository_is_valid(service->repository);
}

static FleetServiceResult find_owned_fleet(
    const FleetService *service,
    const BmsUuid *user_id,
    BatteryFleet *fleet_out)
{
    FleetRepositoryResult result;

    if (!service_is_valid(service) || !is_valid_user_id(user_id)
        || fleet_out == NULL) {
        return FLEET_SERVICE_INVALID_ARGUMENT;
    }

    result = service->repository->find_by_user(
        service->repository->context, user_id, fleet_out);
    if (result != FLEET_REPOSITORY_OK) {
        return map_repository_result(result);
    }

    if (!battery_fleet_validate(fleet_out)
        || !same_uuid(&fleet_out->owner_id, user_id)) {
        return FLEET_SERVICE_REPOSITORY_ERROR;
    }

    return FLEET_SERVICE_OK;
}

bool fleet_service_init(
    FleetService *service, const FleetRepository *repository)
{
    FleetService candidate;

    if (service == NULL || !fleet_repository_is_valid(repository)) {
        return false;
    }

    candidate.repository = repository;
    *service = candidate;
    return true;
}

FleetServiceResult fleet_service_create_fleet(
    const FleetService *service,
    const BmsUuid *user_id,
    const BatteryFleet *fleet)
{
    BatteryFleet existing;
    FleetRepositoryResult repository_result;
    FleetServiceResult result;

    if (!service_is_valid(service) || !is_valid_user_id(user_id)
        || !battery_fleet_validate(fleet)
        || !same_uuid(&fleet->owner_id, user_id)) {
        return FLEET_SERVICE_INVALID_ARGUMENT;
    }

    result = find_owned_fleet(service, user_id, &existing);
    if (result == FLEET_SERVICE_OK) {
        return FLEET_SERVICE_ALREADY_EXISTS;
    }
    if (result != FLEET_SERVICE_NOT_FOUND) {
        return result;
    }

    repository_result = service->repository->create(
        service->repository->context, user_id, fleet);
    return map_repository_result(repository_result);
}

FleetServiceResult fleet_service_get_fleet(
    const FleetService *service,
    const BmsUuid *user_id,
    BatteryFleet *fleet_out)
{
    if (fleet_out == NULL) {
        return FLEET_SERVICE_INVALID_ARGUMENT;
    }

    return find_owned_fleet(service, user_id, fleet_out);
}

FleetServiceResult fleet_service_update_fleet(
    const FleetService *service,
    const BmsUuid *user_id,
    const BatteryFleet *fleet)
{
    BatteryFleet existing;
    FleetServiceResult result;
    FleetRepositoryResult repository_result;

    if (!service_is_valid(service) || !is_valid_user_id(user_id)
        || !battery_fleet_validate(fleet)
        || !same_uuid(&fleet->owner_id, user_id)) {
        return FLEET_SERVICE_INVALID_ARGUMENT;
    }

    result = find_owned_fleet(service, user_id, &existing);
    if (result != FLEET_SERVICE_OK) {
        return result;
    }

    /* A user's fleet identity and creation time are immutable. */
    if (!same_uuid(&existing.fleet_id, &fleet->fleet_id)
        || existing.created_at != fleet->created_at) {
        return FLEET_SERVICE_INVALID_ARGUMENT;
    }

    repository_result = service->repository->update(
        service->repository->context, user_id, fleet);
    return map_repository_result(repository_result);
}

FleetServiceResult fleet_service_delete_fleet(
    const FleetService *service, const BmsUuid *user_id)
{
    FleetRepositoryResult repository_result;

    if (!service_is_valid(service) || !is_valid_user_id(user_id)) {
        return FLEET_SERVICE_INVALID_ARGUMENT;
    }

    repository_result = service->repository->delete_by_user(
        service->repository->context, user_id);
    return map_repository_result(repository_result);
}

FleetServiceResult fleet_service_fleet_exists(
    const FleetService *service,
    const BmsUuid *user_id,
    bool *exists_out)
{
    BatteryFleet existing;
    FleetServiceResult result;

    if (exists_out == NULL) {
        return FLEET_SERVICE_INVALID_ARGUMENT;
    }

    result = find_owned_fleet(service, user_id, &existing);
    if (result == FLEET_SERVICE_OK) {
        *exists_out = true;
        return FLEET_SERVICE_OK;
    }
    if (result == FLEET_SERVICE_NOT_FOUND) {
        *exists_out = false;
        return FLEET_SERVICE_OK;
    }

    return result;
}

FleetServiceResult fleet_service_get_fleet_summary(
    const FleetService *service,
    const BmsUuid *user_id,
    FleetSummary *summary_out)
{
    BatteryFleet fleet;
    FleetServiceResult result;

    if (summary_out == NULL) {
        return FLEET_SERVICE_INVALID_ARGUMENT;
    }

    result = find_owned_fleet(service, user_id, &fleet);
    if (result != FLEET_SERVICE_OK) {
        return result;
    }

    summary_out->fleet_id = fleet.fleet_id;
    summary_out->name = fleet.name;
    summary_out->created_at = fleet.created_at;
    summary_out->updated_at = fleet.updated_at;
    return FLEET_SERVICE_OK;
}
