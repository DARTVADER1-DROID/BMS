#include "application/telemetry/telemetry_service.h"

#include <stddef.h>

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

static bool service_is_valid(const TelemetryService *service)
{
    return service != NULL
        && telemetry_repository_is_valid(service->repository);
}

static TelemetryServiceResult map_repository_result(
    TelemetryRepositoryResult result)
{
    switch (result) {
    case TELEMETRY_REPOSITORY_OK:
        return TELEMETRY_SERVICE_OK;
    case TELEMETRY_REPOSITORY_NOT_FOUND:
        return TELEMETRY_SERVICE_NOT_FOUND;
    case TELEMETRY_REPOSITORY_INVALID_ARGUMENT:
        return TELEMETRY_SERVICE_INVALID_ARGUMENT;
    case TELEMETRY_REPOSITORY_BUFFER_TOO_SMALL:
        return TELEMETRY_SERVICE_BUFFER_TOO_SMALL;
    case TELEMETRY_REPOSITORY_ERROR:
    default:
        return TELEMETRY_SERVICE_REPOSITORY_ERROR;
    }
}

static bool valid_record_buffer(
    const TelemetryRecord *records_out,
    size_t capacity,
    const size_t *count_out)
{
    return count_out != NULL
        && ((capacity == 0U && records_out == NULL)
            || (capacity > 0U && records_out != NULL));
}

bool telemetry_service_init(
    TelemetryService *service,
    const TelemetryRepository *repository)
{
    TelemetryService candidate;

    if (service == NULL || !telemetry_repository_is_valid(repository)) {
        return false;
    }

    candidate.repository = repository;
    *service = candidate;
    return true;
}

TelemetryServiceResult telemetry_service_get_latest_telemetry(
    const TelemetryService *service,
    const BmsUuid *user_id,
    const BmsUuid *battery_id,
    TelemetryRecord *record_out)
{
    if (!service_is_valid(service) || !is_valid_uuid(user_id)
        || !is_valid_uuid(battery_id) || record_out == NULL) {
        return TELEMETRY_SERVICE_INVALID_ARGUMENT;
    }

    return map_repository_result(service->repository->get_latest(
        service->repository->context, user_id, battery_id, record_out));
}

TelemetryServiceResult telemetry_service_get_battery_telemetry(
    const TelemetryService *service,
    const BmsUuid *user_id,
    const BmsUuid *battery_id,
    TelemetryRecord *records_out,
    size_t capacity,
    size_t *count_out)
{
    if (!service_is_valid(service) || !is_valid_uuid(user_id)
        || !is_valid_uuid(battery_id)
        || !valid_record_buffer(records_out, capacity, count_out)) {
        return TELEMETRY_SERVICE_INVALID_ARGUMENT;
    }

    return map_repository_result(service->repository->get_battery_telemetry(
        service->repository->context, user_id, battery_id,
        records_out, capacity, count_out));
}

TelemetryServiceResult telemetry_service_get_telemetry_history(
    const TelemetryService *service,
    const BmsUuid *user_id,
    const BmsUuid *battery_id,
    BmsDateTime from,
    BmsDateTime to,
    TelemetryRecord *records_out,
    size_t capacity,
    size_t *count_out)
{
    if (!service_is_valid(service) || !is_valid_uuid(user_id)
        || !is_valid_uuid(battery_id) || from > to
        || !valid_record_buffer(records_out, capacity, count_out)) {
        return TELEMETRY_SERVICE_INVALID_ARGUMENT;
    }

    return map_repository_result(service->repository->get_history(
        service->repository->context, user_id, battery_id, from, to,
        records_out, capacity, count_out));
}

TelemetryServiceResult telemetry_service_get_fleet_telemetry(
    const TelemetryService *service,
    const BmsUuid *user_id,
    TelemetryRecord *records_out,
    size_t capacity,
    size_t *count_out)
{
    if (!service_is_valid(service) || !is_valid_uuid(user_id)
        || !valid_record_buffer(records_out, capacity, count_out)) {
        return TELEMETRY_SERVICE_INVALID_ARGUMENT;
    }

    return map_repository_result(service->repository->get_fleet_telemetry(
        service->repository->context, user_id,
        records_out, capacity, count_out));
}
