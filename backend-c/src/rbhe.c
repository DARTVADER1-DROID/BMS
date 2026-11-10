#include "rbhe.h"

#include <math.h>
#include <string.h>

static bool finite_positive(double x) { return isfinite(x) && x > 0.0; }
static double clampd(double x, double lo, double hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

void rbhe_default_limits(RbheLimits *l) {
    if (!l) return;
    l->min_test_current_a = 0.10;
    l->max_test_current_a = 1.00; /* Conservative diagnostic default, not a cell rating. */
    l->min_voltage_v = 3.0;       /* Provisional test abort boundary; verify datasheet. */
    l->max_voltage_v = 4.20;      /* Provisional; manufacturer limit takes precedence. */
    l->max_temperature_c = 45.0;  /* Conservative diagnostic abort threshold. */
    l->max_test_seconds = 120.0;
    l->max_sample_gap_seconds = 2.0;
}

void rbhe_init(Rbhe *e, const RbheLimits *limits) {
    if (!e) return;
    memset(e, 0, sizeof(*e));
    if (limits) e->limits = *limits;
    else rbhe_default_limits(&e->limits);
}

RbheResult rbhe_set_soh_calibration(Rbhe *e, const RbheSohCalibration *c) {
    if (!e || !c) return RBHE_ERR_ARGUMENT;
    if (!c->valid) { memset(&e->soh, 0, sizeof(e->soh)); return RBHE_OK; }
    if (!isfinite(c->intercept_percent) ||
        !finite_positive(c->min_soh_percent) ||
        c->max_soh_percent <= c->min_soh_percent ||
        !finite_positive(c->max_validation_error_percent)) return RBHE_ERR_CALIBRATION;
    for (size_t i = 0; i < 4; ++i) {
        if (!isfinite(c->mean[i]) || !finite_positive(c->scale[i]) ||
            !isfinite(c->weight[i])) return RBHE_ERR_CALIBRATION;
    }
    e->soh = *c;
    return RBHE_OK;
}

RbheResult rbhe_begin(Rbhe *e, double t, double v, double temp) {
    if (!e || !isfinite(t) || !isfinite(v) || !isfinite(temp)) return RBHE_ERR_ARGUMENT;
    if (e->active) return RBHE_ERR_STATE;
    if (v < e->limits.min_voltage_v || v > e->limits.max_voltage_v ||
        temp >= e->limits.max_temperature_c) return RBHE_ERR_INSUFFICIENT_DATA;
    e->sample_count = 0;
    e->active = true;
    e->baseline_voltage_v = v;
    e->baseline_temperature_c = temp;
    e->start_time_s = t;
    e->load_start_time_s = 0.0;
    e->recovery_start_time_s = 0.0;
    e->first_load_voltage_v = 0.0;
    e->min_load_voltage_v = v;
    e->sum_load_current_a = 0.0;
    e->sum_load_voltage_v = 0.0;
    e->load_count = 0;
    e->recovery_voltage_v = 0.0;
    e->recovery_time_s = 0.0;
    e->saw_load = false;
    e->saw_recovery = false;
    return RBHE_OK;
}

RbheResult rbhe_sample(Rbhe *e, const RbheSample *s) {
    if (!e || !s) return RBHE_ERR_ARGUMENT;
    if (!e->active) return RBHE_ERR_STATE;
    if (!isfinite(s->timestamp_s) || !isfinite(s->voltage_v) ||
        !isfinite(s->current_a) || !isfinite(s->temperature_c)) return RBHE_ERR_ARGUMENT;
    if (e->sample_count >= RBHE_MAX_SAMPLES) return RBHE_ERR_CAPACITY;
    if (s->timestamp_s <= e->start_time_s ||
        (e->sample_count && s->timestamp_s <= e->samples[e->sample_count-1].timestamp_s))
        return RBHE_ERR_ARGUMENT;
    if (s->timestamp_s - e->start_time_s > e->limits.max_test_seconds ||
        (e->sample_count && s->timestamp_s - e->samples[e->sample_count-1].timestamp_s >
         e->limits.max_sample_gap_seconds)) return RBHE_ERR_INSUFFICIENT_DATA;
    if (s->voltage_v < e->limits.min_voltage_v || s->voltage_v > e->limits.max_voltage_v ||
        s->temperature_c >= e->limits.max_temperature_c) return RBHE_ERR_INSUFFICIENT_DATA;

    e->samples[e->sample_count++] = *s;
    const double discharge = s->current_a;
    if (discharge >= e->limits.min_test_current_a) {
        if (!e->saw_load) {
            e->saw_load = true;
            e->load_start_time_s = s->timestamp_s;
            e->first_load_voltage_v = s->voltage_v;
        }
        if (e->saw_recovery) return RBHE_ERR_INSUFFICIENT_DATA;
        if (discharge > e->limits.max_test_current_a) return RBHE_ERR_INSUFFICIENT_DATA;
        if (s->voltage_v < e->min_load_voltage_v) e->min_load_voltage_v = s->voltage_v;
        e->sum_load_current_a += discharge;
        e->sum_load_voltage_v += s->voltage_v;
        ++e->load_count;
    } else if (fabs(discharge) < e->limits.min_test_current_a && e->saw_load) {
        /* Keep the latest unloaded voltage so the feature reflects recovery,
           not merely the first sample after the load was removed. */
        if (!e->saw_recovery) {
            e->saw_recovery = true;
            e->recovery_start_time_s = s->timestamp_s;
        }
        e->recovery_voltage_v = s->voltage_v;
        e->recovery_time_s = s->timestamp_s - e->load_start_time_s;
    } else if (e->saw_load) {
        /* A charging current or an ambiguous transition invalidates this pulse. */
        return RBHE_ERR_INSUFFICIENT_DATA;
    }
    return RBHE_OK;
}

RbheResult rbhe_finish(Rbhe *e, RbheResultData *r) {
    if (!e || !r) return RBHE_ERR_ARGUMENT;
    if (!e->active) return RBHE_ERR_STATE;
    e->active = false;
    memset(r, 0, sizeof(*r));
    if (!e->saw_load || !e->saw_recovery || e->load_count < 3) return RBHE_ERR_INSUFFICIENT_DATA;

    const double mean_i = e->sum_load_current_a / (double)e->load_count;
    const double mean_v = e->sum_load_voltage_v / (double)e->load_count;
    const double delta_v_step = e->baseline_voltage_v - e->first_load_voltage_v;
    const double delta_v_steady = e->baseline_voltage_v - mean_v;
    const double duration = e->samples[e->sample_count-1].timestamp_s - e->start_time_s;
    if (!finite_positive(mean_i) || delta_v_step <= 0.0 || delta_v_steady <= 0.0 ||
        duration <= 0.0) return RBHE_ERR_INSUFFICIENT_DATA;

    r->duration_s = duration;
    r->baseline_voltage_v = e->baseline_voltage_v;
    r->minimum_load_voltage_v = e->min_load_voltage_v;
    r->mean_load_current_a = mean_i;
    r->step_resistance_ohm = delta_v_step / mean_i;
    r->steady_resistance_ohm = delta_v_steady / mean_i;
    r->voltage_slope_v_per_s =
        (e->min_load_voltage_v - e->first_load_voltage_v) /
        fmax(1.0, e->recovery_start_time_s - e->load_start_time_s);
    const double total_drop = e->baseline_voltage_v - e->min_load_voltage_v;
    r->recovery_fraction = total_drop > 1e-6
        ? clampd((e->recovery_voltage_v - e->min_load_voltage_v) / total_drop, 0.0, 1.5)
        : 0.0;
    r->recovery_elapsed_s = e->recovery_time_s;
    r->temperature_rise_c = e->samples[e->sample_count-1].temperature_c -
                            e->baseline_temperature_c;
    r->temperature_slope_c_per_s = r->temperature_rise_c / duration;
    r->valid = true;

    if (e->soh.valid) {
        const double f[4] = {r->step_resistance_ohm, r->steady_resistance_ohm,
                             r->recovery_fraction, r->temperature_rise_c};
        double prediction = e->soh.intercept_percent;
        for (size_t i = 0; i < 4; ++i)
            prediction += e->soh.weight[i] * ((f[i] - e->soh.mean[i]) / e->soh.scale[i]);
        r->estimated_soh_percent = clampd(prediction, e->soh.min_soh_percent,
                                           e->soh.max_soh_percent);
        r->soh_validation_error_percent = e->soh.max_validation_error_percent;
        r->soh_confidence_percent = clampd(100.0 -
            2.0 * e->soh.max_validation_error_percent, 0.0, 100.0);
        r->soh_available = true;
    }
    if (r->soh_available && e->history_count >= 3) {
        double rul = 0.0;
        if (rbhe_estimate_rul(e, r->estimated_soh_percent, 80.0, &rul) == RBHE_OK) {
            r->rul_cycles = rul;
            r->rul_available = true;
        }
    }
    return RBHE_OK;
}

RbheResult rbhe_add_health_history(Rbhe *e, double cycle, double soh) {
    if (!e || !isfinite(cycle) || !isfinite(soh) || cycle < 0.0 ||
        soh < 0.0 || soh > 100.0) return RBHE_ERR_ARGUMENT;
    if (e->history_count && cycle <= e->history_cycle[e->history_count-1])
        return RBHE_ERR_ARGUMENT;
    if (e->history_count == RBHE_HISTORY) {
        memmove(e->history_soh, e->history_soh+1, (RBHE_HISTORY-1)*sizeof(double));
        memmove(e->history_cycle, e->history_cycle+1, (RBHE_HISTORY-1)*sizeof(double));
        --e->history_count;
    }
    e->history_cycle[e->history_count] = cycle;
    e->history_soh[e->history_count] = soh;
    ++e->history_count;
    return RBHE_OK;
}

RbheResult rbhe_estimate_rul(const Rbhe *e, double current_soh,
                             double eol_soh, double *rul) {
    if (!e || !rul || !isfinite(current_soh) || !isfinite(eol_soh) ||
        eol_soh < 0.0 || eol_soh >= current_soh) return RBHE_ERR_ARGUMENT;
    if (e->history_count < 3) return RBHE_ERR_INSUFFICIENT_DATA;

    /* Least-squares SOH-vs-equivalent-cycle slope. */
    double mx = 0.0, my = 0.0;
    for (size_t i = 0; i < e->history_count; ++i) {
        mx += e->history_cycle[i];
        my += e->history_soh[i];
    }
    mx /= (double)e->history_count;
    my /= (double)e->history_count;
    double cov = 0.0, var = 0.0;
    for (size_t i = 0; i < e->history_count; ++i) {
        const double dx = e->history_cycle[i] - mx;
        cov += dx * (e->history_soh[i] - my);
        var += dx * dx;
    }
    if (var <= 1e-12) return RBHE_ERR_INSUFFICIENT_DATA;
    const double slope = cov / var;
    if (!isfinite(slope) || slope >= -1e-6) return RBHE_ERR_INSUFFICIENT_DATA;
    *rul = fmax(0.0, (current_soh - eol_soh) / -slope);
    return RBHE_OK;
}
