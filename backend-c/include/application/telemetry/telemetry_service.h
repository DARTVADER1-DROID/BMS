#ifndef BMS_APPLICATION_TELEMETRY_TELEMETRY_SERVICE_H
#define BMS_APPLICATION_TELEMETRY_TELEMETRY_SERVICE_H

#include "application/telemetry/telemetry_repository.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TELEMETRY_SERVICE_OK = 0,
    TELEMETRY_SERVICE_NOT_FOUND,
    TELEMETRY_SERVICE_INVALID_ARGUMENT,
    TELEMETRY_SERVICE_BUFFER_TOO_SMALL,
    TELEMETRY_SERVICE_REPOSITORY_ERROR
} TelemetryServiceResult;

typedef struct {
    const TelemetryRepository *repository;
} TelemetryService;

bool telemetry_service_init(
    TelemetryService *service,
    const TelemetryRepository *repository);

TelemetryServiceResult telemetry_service_get_latest_telemetry(
    const TelemetryService *service,
    const BmsUuid *user_id,
    const BmsUuid *battery_id,
    TelemetryRecord *record_out);

TelemetryServiceResult telemetry_service_get_battery_telemetry(
    const TelemetryService *service,
    const BmsUuid *user_id,
    const BmsUuid *battery_id,
    TelemetryRecord *records_out,
    size_t capacity,
    size_t *count_out);

TelemetryServiceResult telemetry_service_get_telemetry_history(
    const TelemetryService *service,
    const BmsUuid *user_id,
    const BmsUuid *battery_id,
    BmsDateTime from,
    BmsDateTime to,
    TelemetryRecord *records_out,
    size_t capacity,
    size_t *count_out);

TelemetryServiceResult telemetry_service_get_fleet_telemetry(
    const TelemetryService *service,
    const BmsUuid *user_id,
    TelemetryRecord *records_out,
    size_t capacity,
    size_t *count_out);

/*
 * UML also lists get_active_alerts(). It is intentionally not exposed yet:
 * the architecture has no Alert entity, fields, or repository contract.
 */

#endif
