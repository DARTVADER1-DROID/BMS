#include "application/telemetry/telemetry_repository.h"

bool telemetry_repository_is_valid(const TelemetryRepository *repository)
{
    return repository != NULL
        && repository->get_latest != NULL
        && repository->get_battery_telemetry != NULL
        && repository->get_history != NULL
        && repository->get_fleet_telemetry != NULL;
}
