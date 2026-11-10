#include "diagnostic.h"

#include <math.h>
#include <string.h>

void diagnostic_default_limits(DiagnosticLimits *limits)
{
    if (limits == NULL) {
        return;
    }

    /* Provisional software guards; verify them against the cell datasheet. */
    limits->minimum_voltage_v = 3.0;
    limits->maximum_voltage_v = 4.20;
    limits->maximum_temperature_c = 45.0;

    limits->minimum_discharge_current_a = 0.10;
    /* Test cap only. This is not the cell's rated discharge current. */
    limits->maximum_discharge_current_a = 1.00;

    limits->maximum_sample_gap_s = 2.0;
    limits->maximum_test_duration_s = 120.0;
}

void diagnostic_init(
    BatteryDiagnostic *diagnostic,
    const DiagnosticLimits *limits
)
{
    if (diagnostic == NULL) {
        return;
    }

    memset(diagnostic, 0, sizeof(*diagnostic));

    if (limits != NULL) {
        diagnostic->limits = *limits;
    } else {
        diagnostic_default_limits(&diagnostic->limits);
    }

    diagnostic->state = DIAGNOSTIC_IDLE;
}

bool diagnostic_begin(
    BatteryDiagnostic *diagnostic,
    double now,
    double voltage,
    double temperature
)
{
    if (diagnostic == NULL
        || !isfinite(now)
        || !isfinite(voltage)
        || !isfinite(temperature)
        || diagnostic->state == DIAGNOSTIC_DISCHARGING
        || diagnostic->state == DIAGNOSTIC_RECOVERING) {
        return false;
    }

    if (voltage < diagnostic->limits.minimum_voltage_v
        || voltage > diagnostic->limits.maximum_voltage_v
        || temperature >= diagnostic->limits.maximum_temperature_c) {
        return false;
    }

    memset(&diagnostic->test, 0, sizeof(diagnostic->test));

    diagnostic->test.baseline_voltage_v = voltage;
    diagnostic->test.baseline_temperature_c = temperature;

    diagnostic->started_at_s = now;
    diagnostic->last_sample_at_s = now;

    diagnostic->loaded_voltage_sum = 0.0;
    diagnostic->loaded_current_sum = 0.0;
    diagnostic->loaded_samples = 0;

    diagnostic->state = DIAGNOSTIC_DISCHARGING;
    return true;
}

bool diagnostic_add_sample(
    BatteryDiagnostic *diagnostic,
    const BatterySample *sample
)
{
    if (diagnostic == NULL || sample == NULL
        || !isfinite(sample->time_s)
        || !isfinite(sample->voltage_v)
        || !isfinite(sample->current_a)
        || !isfinite(sample->temperature_c)) {
        return false;
    }

    if (diagnostic->state != DIAGNOSTIC_DISCHARGING
        && diagnostic->state != DIAGNOSTIC_RECOVERING) {
        return false;
    }

    if (sample->time_s <= diagnostic->last_sample_at_s
        || sample->time_s - diagnostic->last_sample_at_s
            > diagnostic->limits.maximum_sample_gap_s
        || sample->time_s - diagnostic->started_at_s
            > diagnostic->limits.maximum_test_duration_s
        || sample->voltage_v < diagnostic->limits.minimum_voltage_v
        || sample->voltage_v > diagnostic->limits.maximum_voltage_v
        || sample->temperature_c
            >= diagnostic->limits.maximum_temperature_c) {
        diagnostic_abort(diagnostic);
        return false;
    }

    diagnostic->last_sample_at_s = sample->time_s;

    if (diagnostic->state == DIAGNOSTIC_DISCHARGING
        && sample->current_a
            >= diagnostic->limits.minimum_discharge_current_a) {
        if (sample->current_a
            > diagnostic->limits.maximum_discharge_current_a) {
            diagnostic_abort(diagnostic);
            return false;
        }

        if (diagnostic->loaded_samples == 0) {
            diagnostic->load_started_at_s = sample->time_s;
            diagnostic->first_loaded_voltage_v = sample->voltage_v;
            diagnostic->minimum_loaded_voltage_v = sample->voltage_v;
        }

        if (sample->voltage_v < diagnostic->minimum_loaded_voltage_v) {
            diagnostic->minimum_loaded_voltage_v = sample->voltage_v;
        }

        diagnostic->loaded_voltage_sum += sample->voltage_v;
        diagnostic->loaded_current_sum += sample->current_a;
        diagnostic->loaded_samples++;

        return true;
    }

    if (diagnostic->state == DIAGNOSTIC_DISCHARGING
        && diagnostic->loaded_samples > 0
        && fabs(sample->current_a)
            < diagnostic->limits.minimum_discharge_current_a) {
        diagnostic->state = DIAGNOSTIC_RECOVERING;
        diagnostic->recovery_started_at_s = sample->time_s;

        diagnostic->test.final_recovery_voltage_v = sample->voltage_v;
        diagnostic->test.temperature_after_test_c = sample->temperature_c;

        return true;
    }

    if (diagnostic->state == DIAGNOSTIC_RECOVERING) {
        if (fabs(sample->current_a)
            >= diagnostic->limits.minimum_discharge_current_a) {
            diagnostic_abort(diagnostic);
            return false;
        }

        diagnostic->test.final_recovery_voltage_v = sample->voltage_v;
        diagnostic->test.temperature_after_test_c = sample->temperature_c;
    }

    return true;
}

bool diagnostic_finish(
    BatteryDiagnostic *diagnostic,
    BatteryTest *output
)
{
    if (diagnostic == NULL
        || output == NULL
        || diagnostic->state != DIAGNOSTIC_RECOVERING
        || diagnostic->loaded_samples < 3) {
        return false;
    }

    diagnostic->test.mean_discharge_current_a =
        diagnostic->loaded_current_sum
        / (double)diagnostic->loaded_samples;

    diagnostic->test.first_loaded_voltage_v =
        diagnostic->first_loaded_voltage_v;

    diagnostic->test.mean_loaded_voltage_v =
        diagnostic->loaded_voltage_sum
        / (double)diagnostic->loaded_samples;

    diagnostic->test.minimum_loaded_voltage_v =
        diagnostic->minimum_loaded_voltage_v;

    diagnostic->test.load_duration_s =
        diagnostic->recovery_started_at_s
        - diagnostic->load_started_at_s;

    diagnostic->test.recovery_duration_s =
        diagnostic->last_sample_at_s
        - diagnostic->recovery_started_at_s;

    if (diagnostic->test.load_duration_s <= 0.0
        || diagnostic->test.recovery_duration_s <= 0.0) {
        return false;
    }

    *output = diagnostic->test;
    diagnostic->state = DIAGNOSTIC_COMPLETE;

    return true;
}

void diagnostic_abort(BatteryDiagnostic *diagnostic)
{
    if (diagnostic != NULL) {
        diagnostic->state = DIAGNOSTIC_ABORTED;
    }
}
