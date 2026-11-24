#ifndef BMS_DOMAIN_ENTITIES_TELEMETRY_H
#define BMS_DOMAIN_ENTITIES_TELEMETRY_H

#include "domain/value_objects/domain_types.h"

/*
 * A persisted telemetry sample as described by the architecture ERD.
 * Values are measurements; unit conventions and validity ranges must be
 * established by the ingestion/domain policy before production use.
 */
typedef struct {
    BmsUuid telemetry_id;
    BmsUuid battery_id;
    double voltage;
    double current;
    double temperature;
    double soc;
    BmsDateTime recorded_at;
} TelemetryRecord;

#endif
