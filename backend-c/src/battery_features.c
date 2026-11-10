#include "battery_features.h"
#include <math.h>

bool battery_features_calculate(const BatteryTest *test, BatteryFeatures *out) {
    if (!test || !out) return false;
    if (!isfinite(test->baseline_voltage_v) ||
        !isfinite(test->baseline_temperature_c) ||
        !isfinite(test->mean_discharge_current_a) ||
        !isfinite(test->first_loaded_voltage_v) ||
        !isfinite(test->mean_loaded_voltage_v) ||
        !isfinite(test->minimum_loaded_voltage_v) ||
        !isfinite(test->final_recovery_voltage_v) ||
        !isfinite(test->temperature_after_test_c) ||
        !isfinite(test->load_duration_s) ||
        !isfinite(test->recovery_duration_s)) return false;

    const double current = test->mean_discharge_current_a;
    const double step_drop = test->baseline_voltage_v - test->first_loaded_voltage_v;
    const double pulse_drop = test->baseline_voltage_v - test->mean_loaded_voltage_v;
    const double total_drop = test->baseline_voltage_v - test->minimum_loaded_voltage_v;
    if (current <= 0.0 || step_drop <= 0.0 || pulse_drop <= 0.0 ||
        test->load_duration_s <= 0.0 || test->recovery_duration_s <= 0.0)
        return false;

    out->step_resistance_ohm = step_drop / current;
    out->pulse_resistance_ohm = pulse_drop / current;
    out->loaded_voltage_slope_v_per_s =
        (test->minimum_loaded_voltage_v - test->first_loaded_voltage_v) /
        test->load_duration_s;
    out->recovery_fraction = total_drop > 1e-6
        ? (test->final_recovery_voltage_v - test->minimum_loaded_voltage_v) / total_drop
        : 0.0;
    if (out->recovery_fraction < 0.0) out->recovery_fraction = 0.0;
    if (out->recovery_fraction > 1.5) out->recovery_fraction = 1.5;
    out->temperature_rise_c =
        test->temperature_after_test_c - test->baseline_temperature_c;
    out->temperature_slope_c_per_s =
        out->temperature_rise_c / (test->load_duration_s + test->recovery_duration_s);
    return true;
}
