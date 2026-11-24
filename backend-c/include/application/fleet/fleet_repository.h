#ifndef BMS_APPLICATION_FLEET_FLEET_REPOSITORY_H
#define BMS_APPLICATION_FLEET_FLEET_REPOSITORY_H

#include "domain/entities/battery_fleet.h"

#include <stdbool.h>

/*
 * Storage boundary used by FleetService.
 *
 * Implementations must scope every operation to user_id. create() must
 * enforce the one-fleet-per-user invariant atomically; the service's
 * existence check is only an early, user-friendly guard against duplicates.
 */
typedef enum {
    FLEET_REPOSITORY_OK = 0,
    FLEET_REPOSITORY_NOT_FOUND,
    FLEET_REPOSITORY_ALREADY_EXISTS,
    FLEET_REPOSITORY_INVALID_ARGUMENT,
    FLEET_REPOSITORY_ERROR
} FleetRepositoryResult;

typedef struct {
    void *context;
    FleetRepositoryResult (*create)(
        void *context, const BmsUuid *user_id, const BatteryFleet *fleet);
    FleetRepositoryResult (*find_by_user)(
        void *context, const BmsUuid *user_id, BatteryFleet *fleet_out);
    FleetRepositoryResult (*update)(
        void *context, const BmsUuid *user_id, const BatteryFleet *fleet);
    FleetRepositoryResult (*delete_by_user)(
        void *context, const BmsUuid *user_id);
} FleetRepository;

bool fleet_repository_is_valid(const FleetRepository *repository);

#endif
