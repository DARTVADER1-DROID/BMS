#ifndef BATTERY_FEATURES_H
#define BATTERY_FEATURES_H

#include "battery_types.h"

/* Convert a completed, validated pulse test into diagnostic features. */
bool battery_features_calculate(const BatteryTest *test, BatteryFeatures *features);

#endif
