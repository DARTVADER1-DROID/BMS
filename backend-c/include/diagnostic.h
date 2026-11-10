#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H

#include "battery_types.h"

typedef enum {
    DIAGNOSTIC_IDLE,
    DIAGNOSTIC_DISCHARGING,
    DIAGNOSTIC_RECOVERING,
    DIAGNOSTIC_COMPLETE,
    DIAGNOSTIC_ABORTED
} DiagnosticState;

typedef struct {
    double minimum_voltage_v;
    double maximum_voltage_v;
    double maximum_temperature_c;
    double minimum_discharge_current_a;
    double maximum_discharge_current_a;
    double maximum_sample_gap_s;
    double maximum_test_duration_s;
} DiagnosticLimits;

typedef struct {
    DiagnosticState state;
    DiagnosticLimits limits;
    BatteryTest test;
    double started_at_s;
    double last_sample_at_s;
    double load_started_at_s;
    double recovery_started_at_s;
    double first_loaded_voltage_v;
    double loaded_voltage_sum;
    double loaded_current_sum;
    double minimum_loaded_voltage_v;
    size_t loaded_samples;
} BatteryDiagnostic;

void diagnostic_default_limits(DiagnosticLimits *limits);
void diagnostic_init(BatteryDiagnostic *diagnostic, const DiagnosticLimits *limits);
bool diagnostic_begin(BatteryDiagnostic *diagnostic, double now_s,
                      double resting_voltage_v, double temperature_c);
bool diagnostic_add_sample(BatteryDiagnostic *diagnostic,
                           const BatterySample *sample);
bool diagnostic_finish(BatteryDiagnostic *diagnostic, BatteryTest *test);
void diagnostic_abort(BatteryDiagnostic *diagnostic);

#endif
