#ifndef BMS_APPLICATION_TELEMETRY_TELEMETRY_REPOSITORY_H
#define BMS_APPLICATION_TELEMETRY_TELEMETRY_REPOSITORY_H

#include "domain/entities/telemetry.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Storage boundary for user-scoped telemetry queries.
 *
 * Implementations must verify battery ownership for battery-specific
 * queries and scope fleet queries to user_id. Buffer operations write at
 * most capacity records and set count_out to the number written.
 */
typedef enum {
    TELEMETRY_REPOSITORY_OK = 0,
    TELEMETRY_REPOSITORY_NOT_FOUND,
    TELEMETRY_REPOSITORY_INVALID_ARGUMENT,
    TELEMETRY_REPOSITORY_BUFFER_TOO_SMALL,
    TELEMETRY_REPOSITORY_ERROR
} TelemetryRepositoryResult;

typedef struct {
    void *context;

    TelemetryRepositoryResult (*get_latest)(
        void *context,
        const BmsUuid *user_id,
        const BmsUuid *battery_id,
        TelemetryRecord *record_out);

    TelemetryRepositoryResult (*get_battery_telemetry)(
        void *context,
        const BmsUuid *user_id,
        const BmsUuid *battery_id,
        TelemetryRecord *records_out,
        size_t capacity,
        size_t *count_out);

    TelemetryRepositoryResult (*get_history)(
        void *context,
        const BmsUuid *user_id,
        const BmsUuid *battery_id,
        BmsDateTime from,
        BmsDateTime to,
        TelemetryRecord *records_out,
        size_t capacity,
        size_t *count_out);

    TelemetryRepositoryResult (*get_fleet_telemetry)(
        void *context,
        const BmsUuid *user_id,
        TelemetryRecord *records_out,
        size_t capacity,
        size_t *count_out);
} TelemetryRepository;

bool telemetry_repository_is_valid(const TelemetryRepository *repository);

#endif
