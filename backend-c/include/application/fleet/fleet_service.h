#ifndef BMS_APPLICATION_FLEET_FLEET_SERVICE_H
#define BMS_APPLICATION_FLEET_FLEET_SERVICE_H

#include "application/fleet/fleet_repository.h"

#include <stdbool.h>

typedef enum {
    FLEET_SERVICE_OK = 0,
    FLEET_SERVICE_NOT_FOUND,
    FLEET_SERVICE_ALREADY_EXISTS,
    FLEET_SERVICE_INVALID_ARGUMENT,
    FLEET_SERVICE_REPOSITORY_ERROR
} FleetServiceResult;

/*
 * The first summary intentionally contains only fields available from the
 * BatteryFleet entity. Battery counts and health metrics belong to later
 * BatteryService/analytics work and are not fabricated here.
 */
typedef struct {
    BmsUuid fleet_id;
    const char *name;
    BmsDateTime created_at;
    BmsDateTime updated_at;
} FleetSummary;

typedef struct {
    const FleetRepository *repository;
} FleetService;

bool fleet_service_init(
    FleetService *service, const FleetRepository *repository);

FleetServiceResult fleet_service_create_fleet(
    const FleetService *service,
    const BmsUuid *user_id,
    const BatteryFleet *fleet);

FleetServiceResult fleet_service_get_fleet(
    const FleetService *service,
    const BmsUuid *user_id,
    BatteryFleet *fleet_out);

FleetServiceResult fleet_service_update_fleet(
    const FleetService *service,
    const BmsUuid *user_id,
    const BatteryFleet *fleet);

FleetServiceResult fleet_service_delete_fleet(
    const FleetService *service,
    const BmsUuid *user_id);

FleetServiceResult fleet_service_fleet_exists(
    const FleetService *service,
    const BmsUuid *user_id,
    bool *exists_out);

FleetServiceResult fleet_service_get_fleet_summary(
    const FleetService *service,
    const BmsUuid *user_id,
    FleetSummary *summary_out);

#endif
