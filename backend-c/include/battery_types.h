#ifndef BATTERY_TYPES_H
#define BATTERY_TYPES_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    double time_s;
    double voltage_v;
    double current_a;      /* Positive = discharge. */
    double temperature_c;
} BatterySample;

typedef struct {
    double baseline_voltage_v;
    double baseline_temperature_c;
    double mean_discharge_current_a;
    double first_loaded_voltage_v;
    double mean_loaded_voltage_v;
    double minimum_loaded_voltage_v;
    double final_recovery_voltage_v;
    double temperature_after_test_c;
    double load_duration_s;
    double recovery_duration_s;
} BatteryTest;

typedef struct {
    double step_resistance_ohm;
    double pulse_resistance_ohm;
    double loaded_voltage_slope_v_per_s;
    double recovery_fraction;
    double temperature_rise_c;
    double temperature_slope_c_per_s;
} BatteryFeatures;

#endif
