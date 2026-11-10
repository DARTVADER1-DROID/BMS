#ifndef BATTERY_HEALTH_H
#define BATTERY_HEALTH_H

#include "battery_types.h"

typedef struct {
    bool calibrated;
    double feature_mean[4];
    double feature_scale[4];
    double coefficient[4];
    double intercept_percent;
    double minimum_soh_percent;
    double maximum_soh_percent;
    double validation_mae_percent;
} BatterySohModel;

typedef struct {
    bool available;
    double soh_percent;
    double validation_mae_percent;
} BatterySohEstimate;

typedef struct {
    double equivalent_cycles;
    double soh_percent;
} BatteryHealthPoint;

bool battery_soh_estimate(const BatterySohModel *model,
                          const BatteryFeatures *features,
                          BatterySohEstimate *estimate);

bool battery_rul_estimate(const BatteryHealthPoint *history,
                          size_t history_count,
                          double current_soh_percent,
                          double end_of_life_soh_percent,
                          double *remaining_cycles);

#endif
