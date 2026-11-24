#ifndef BMS_APPLICATION_BATTERY_BATTERY_REPOSITORY_H
#define BMS_APPLICATION_BATTERY_BATTERY_REPOSITORY_H

#include "domain/entities/battery.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Storage boundary used by BatteryService.
 *
 * Every operation is scoped by the authenticated user_id. Implementations
 * must resolve the user's fleet internally and must never trust a fleet_id
 * supplied by a caller as proof of ownership.
 *
 * create() must reject duplicate battery IDs and ensure the battery belongs
 * to the user's fleet. update() must not move a battery to another fleet.
 * find_all() returns BATTERY_REPOSITORY_OK with count_out == 0 when the user
 * has no batteries. Buffer operations must not write beyond capacity; when
 * the buffer is too small they return BUFFER_TOO_SMALL and report the
 * required record count through count_out.
 */
typedef enum {
    BATTERY_REPOSITORY_OK = 0,
    BATTERY_REPOSITORY_NOT_FOUND,
    BATTERY_REPOSITORY_ALREADY_EXISTS,
    BATTERY_REPOSITORY_INVALID_ARGUMENT,
    BATTERY_REPOSITORY_BUFFER_TOO_SMALL,
    BATTERY_REPOSITORY_ERROR
} BatteryRepositoryResult;

typedef struct {
    void *context;

    BatteryRepositoryResult (*create)(
        void *context,
        const BmsUuid *user_id,
        const Battery *battery);

    BatteryRepositoryResult (*find_all)(
        void *context,
        const BmsUuid *user_id,
        Battery *batteries_out,
        size_t capacity,
        size_t *count_out);

    BatteryRepositoryResult (*find_by_id)(
        void *context,
        const BmsUuid *user_id,
        const BmsUuid *battery_id,
        Battery *battery_out);

    BatteryRepositoryResult (*update)(
        void *context,
        const BmsUuid *user_id,
        const Battery *battery);

    BatteryRepositoryResult (*delete)(
        void *context,
        const BmsUuid *user_id,
        const BmsUuid *battery_id);
} BatteryRepository;

/* A valid repository supplies every operation in the contract. */
bool battery_repository_is_valid(const BatteryRepository *repository);

#endif
