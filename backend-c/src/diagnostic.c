#include "diagnostic.h"
#include <math.h>
#include <string.h>

void diagnostic_default_limits(DiagnosticLimits *l) {
    if (!l) return;
    l->minimum_voltage_v = 3.0; /* provisional software guard, verify cell datasheet */
    l->maximum_voltage_v = 4.20;
    l->maximum_temperature_c = 45.0;
    l->minimum_discharge_current_a = 0.10;
    l->maximum_discharge_current_a = 1.00; /* test cap, NOT cell discharge rating */
    l->maximum_sample_gap_s = 2.0;
    l->maximum_test_duration_s = 120.0;
}

void diagnostic_init(BatteryDiagnostic *d, const DiagnosticLimits *limits) {
    if (!d) return;
    memset(d, 0, sizeof(*d));
    if (limits) d->limits = *limits;
    else diagnostic_default_limits(&d->limits);
    d->state = DIAGNOSTIC_IDLE;
}

bool diagnostic_begin(BatteryDiagnostic *d, double now, double voltage, double temp) {
    if (!d || !isfinite(now) || !isfinite(voltage) || !isfinite(temp) ||
        d->state == DIAGNOSTIC_DISCHARGING || d->state == DIAGNOSTIC_RECOVERING)
        return false;
    if (voltage < d->limits.minimum_voltage_v ||
        voltage > d->limits.maximum_voltage_v ||
        temp >= d->limits.maximum_temperature_c) return false;

    memset(&d->test, 0, sizeof(d->test));
    d->test.baseline_voltage_v = voltage;
    d->test.baseline_temperature_c = temp;
    d->started_at_s = now;
    d->last_sample_at_s = now;
    d->loaded_voltage_sum = 0.0;
    d->loaded_current_sum = 0.0;
    d->loaded_samples = 0;
    d->state = DIAGNOSTIC_DISCHARGING;
    return true;
}

bool diagnostic_add_sample(BatteryDiagnostic *d, const BatterySample *s) {
    if (!d || !s || !isfinite(s->time_s) || !isfinite(s->voltage_v) ||
        !isfinite(s->current_a) || !isfinite(s->temperature_c)) return false;
    if (d->state != DIAGNOSTIC_DISCHARGING &&
        d->state != DIAGNOSTIC_RECOVERING) return false;
    if (s->time_s <= d->last_sample_at_s ||
        s->time_s - d->last_sample_at_s > d->limits.maximum_sample_gap_s ||
        s->time_s - d->started_at_s > d->limits.maximum_test_duration_s ||
        s->voltage_v < d->limits.minimum_voltage_v ||
        s->voltage_v > d->limits.maximum_voltage_v ||
        s->temperature_c >= d->limits.maximum_temperature_c) {
        diagnostic_abort(d);
        return false;
    }
    d->last_sample_at_s = s->time_s;

    if (d->state == DIAGNOSTIC_DISCHARGING &&
        s->current_a >= d->limits.minimum_discharge_current_a) {
        if (s->current_a > d->limits.maximum_discharge_current_a) {
            diagnostic_abort(d);
            return false;
        }
        if (!d->loaded_samples) {
            d->load_started_at_s = s->time_s;
            d->first_loaded_voltage_v = s->voltage_v;
            d->minimum_loaded_voltage_v = s->voltage_v;
        }
        if (s->voltage_v < d->minimum_loaded_voltage_v)
            d->minimum_loaded_voltage_v = s->voltage_v;
        d->loaded_voltage_sum += s->voltage_v;
        d->loaded_current_sum += s->current_a;
        ++d->loaded_samples;
        return true;
    }

    if (d->state == DIAGNOSTIC_DISCHARGING && d->loaded_samples &&
        fabs(s->current_a) < d->limits.minimum_discharge_current_a) {
        d->state = DIAGNOSTIC_RECOVERING;
        d->recovery_started_at_s = s->time_s;
        d->test.final_recovery_voltage_v = s->voltage_v;
        d->test.temperature_after_test_c = s->temperature_c;
        return true;
    }

    if (d->state == DIAGNOSTIC_RECOVERING) {
        if (fabs(s->current_a) >= d->limits.minimum_discharge_current_a) {
            diagnostic_abort(d);
            return false;
        }
        d->test.final_recovery_voltage_v = s->voltage_v;
        d->test.temperature_after_test_c = s->temperature_c;
    }
    return true;
}

bool diagnostic_finish(BatteryDiagnostic *d, BatteryTest *out) {
    if (!d || !out || d->state != DIAGNOSTIC_RECOVERING ||
        d->loaded_samples < 3) return false;
    d->test.mean_discharge_current_a =
        d->loaded_current_sum / (double)d->loaded_samples;
    d->test.first_loaded_voltage_v = d->first_loaded_voltage_v;
    d->test.mean_loaded_voltage_v =
        d->loaded_voltage_sum / (double)d->loaded_samples;
    d->test.minimum_loaded_voltage_v = d->minimum_loaded_voltage_v;
    d->test.load_duration_s = d->recovery_started_at_s - d->load_started_at_s;
    d->test.recovery_duration_s = d->last_sample_at_s - d->recovery_started_at_s;
    if (d->test.load_duration_s <= 0.0 || d->test.recovery_duration_s <= 0.0)
        return false;
    *out = d->test;
    d->state = DIAGNOSTIC_COMPLETE;
    return true;
}

void diagnostic_abort(BatteryDiagnostic *d) {
    if (d) d->state = DIAGNOSTIC_ABORTED;
}
