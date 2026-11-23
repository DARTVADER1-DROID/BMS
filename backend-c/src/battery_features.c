#include "battery_features.h"

#include <math.h>

bool battery_features_calculate(
    const BatteryTest *test,
    BatteryFeatures *output
)
{
    double average_current;
    double initial_voltage_drop;
    double average_voltage_drop;
    double total_voltage_drop;

    if (test == NULL || output == NULL) {
        return false;
    }

    if (!isfinite(test->baseline_voltage_v)
        || !isfinite(test->baseline_temperature_c)
        || !isfinite(test->mean_discharge_current_a)
        || !isfinite(test->first_loaded_voltage_v)
        || !isfinite(test->mean_loaded_voltage_v)
        || !isfinite(test->minimum_loaded_voltage_v)
        || !isfinite(test->final_recovery_voltage_v)
        || !isfinite(test->temperature_after_test_c)
        || !isfinite(test->load_duration_s)
        || !isfinite(test->recovery_duration_s)) {
        return false;
    }

    average_current = test->mean_discharge_current_a;

    initial_voltage_drop =
        test->baseline_voltage_v - test->first_loaded_voltage_v;

    average_voltage_drop =
        test->baseline_voltage_v - test->mean_loaded_voltage_v;

    total_voltage_drop =
        test->baseline_voltage_v - test->minimum_loaded_voltage_v;

    if (average_current <= 0.0
        || initial_voltage_drop <= 0.0
        || average_voltage_drop <= 0.0
        || test->load_duration_s <= 0.0
        || test->recovery_duration_s <= 0.0) {
        return false;
    }

    output->step_resistance_ohm =
        initial_voltage_drop / average_current;

    output->pulse_resistance_ohm =
        average_voltage_drop / average_current;

    output->loaded_voltage_slope_v_per_s =
        (test->minimum_loaded_voltage_v - test->first_loaded_voltage_v)
        / test->load_duration_s;

    if (total_voltage_drop > 1e-6) {
        output->recovery_fraction =
            (test->final_recovery_voltage_v
                - test->minimum_loaded_voltage_v)
            / total_voltage_drop;
    } else {
        output->recovery_fraction = 0.0;
    }

    if (output->recovery_fraction < 0.0) {
        output->recovery_fraction = 0.0;
    }

    if (output->recovery_fraction > 1.5) {
        output->recovery_fraction = 1.5;
    }

    output->temperature_rise_c =
        test->temperature_after_test_c - test->baseline_temperature_c;

    output->temperature_slope_c_per_s =
        output->temperature_rise_c
        / (test->load_duration_s + test->recovery_duration_s);

    return true;
}
