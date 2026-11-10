#ifndef BATTERY_FEATURES_H
#define BATTERY_FEATURES_H

#include "battery_types.h"

/*
 * Convert a completed pulse test into diagnostic features.
 * The input test must contain validated measurement data.
 */
bool battery_features_calculate(
    const BatteryTest *test,
    BatteryFeatures *features
);

#endif
