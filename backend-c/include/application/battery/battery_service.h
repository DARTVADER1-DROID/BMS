#ifndef BMS_APPLICATION_BATTERY_BATTERY_SERVICE_H
#define BMS_APPLICATION_BATTERY_BATTERY_SERVICE_H

#include "application/battery/battery_repository.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    BATTERY_SERVICE_OK = 0,
    BATTERY_SERVICE_NOT_FOUND,
    BATTERY_SERVICE_ALREADY_EXISTS,
    BATTERY_SERVICE_INVALID_ARGUMENT,
    BATTERY_SERVICE_VALIDATION_ERROR,
    BATTERY_SERVICE_BUFFER_TOO_SMALL,
    BATTERY_SERVICE_REPOSITORY_ERROR
} BatteryServiceResult;

typedef struct {
    const BatteryRepository *repository;
} BatteryService;

/*
 * String fields in Battery are non-owning. Values returned by repository
 * callbacks are borrowed; their lifetime is governed by the repository
 * adapter. BatteryService copies the struct, not the strings.
 */
bool battery_service_init(
    BatteryService *service, const BatteryRepository *repository);

BatteryServiceResult battery_service_add_battery(
    const BatteryService *service,
    const BmsUuid *user_id,
    const Battery *battery);

BatteryServiceResult battery_service_get_batteries(
    const BatteryService *service,
    const BmsUuid *user_id,
    Battery *batteries_out,
    size_t capacity,
    size_t *count_out);

BatteryServiceResult battery_service_get_battery(
    const BatteryService *service,
    const BmsUuid *user_id,
    const BmsUuid *battery_id,
    Battery *battery_out);

BatteryServiceResult battery_service_update_battery(
    const BatteryService *service,
    const BmsUuid *user_id,
    const Battery *battery);

BatteryServiceResult battery_service_remove_battery(
    const BatteryService *service,
    const BmsUuid *user_id,
    const BmsUuid *battery_id);

BatteryServiceResult battery_service_get_battery_count(
    const BatteryService *service,
    const BmsUuid *user_id,
    size_t *count_out);

#endif
