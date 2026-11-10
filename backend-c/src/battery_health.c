#include "battery_health.h"
#include <math.h>

static double clamp(double value, double low, double high) {
    return value < low ? low : (value > high ? high : value);
}

bool battery_soh_estimate(const BatterySohModel *model,
                          const BatteryFeatures *f,
                          BatterySohEstimate *out) {
    if (!model || !f || !out) return false;
    out->available = false;
    if (!model->calibrated) return true;

    const double values[4] = {
        f->step_resistance_ohm,
        f->pulse_resistance_ohm,
        f->recovery_fraction,
        f->temperature_rise_c
    };
    if (model->minimum_soh_percent < 0.0 ||
        model->maximum_soh_percent > 100.0 ||
        model->minimum_soh_percent >= model->maximum_soh_percent ||
        model->validation_mae_percent < 0.0 ||
        !isfinite(model->intercept_percent)) return false;

    double prediction = model->intercept_percent;
    for (size_t i = 0; i < 4; ++i) {
        if (!isfinite(values[i]) || !isfinite(model->feature_mean[i]) ||
            !isfinite(model->feature_scale[i]) ||
            model->feature_scale[i] <= 0.0 ||
            !isfinite(model->coefficient[i])) return false;
        prediction += model->coefficient[i] *
            ((values[i] - model->feature_mean[i]) / model->feature_scale[i]);
    }
    out->soh_percent = clamp(prediction, model->minimum_soh_percent,
                             model->maximum_soh_percent);
    out->validation_mae_percent = model->validation_mae_percent;
    out->available = true;
    return true;
}

bool battery_rul_estimate(const BatteryHealthPoint *history, size_t count,
                          double current_soh, double eol_soh, double *remaining) {
    if (!history || !remaining || count < 3 || !isfinite(current_soh) ||
        !isfinite(eol_soh) || eol_soh < 0.0 || eol_soh >= current_soh)
        return false;

    double mean_x = 0.0, mean_y = 0.0;
    for (size_t i = 0; i < count; ++i) {
        if (!isfinite(history[i].equivalent_cycles) ||
            !isfinite(history[i].soh_percent) ||
            history[i].equivalent_cycles < 0.0 ||
            history[i].soh_percent < 0.0 || history[i].soh_percent > 100.0)
            return false;
        if (i && history[i].equivalent_cycles <= history[i-1].equivalent_cycles)
            return false;
        mean_x += history[i].equivalent_cycles;
        mean_y += history[i].soh_percent;
    }
    mean_x /= (double)count;
    mean_y /= (double)count;

    double covariance = 0.0, variance = 0.0;
    for (size_t i = 0; i < count; ++i) {
        const double dx = history[i].equivalent_cycles - mean_x;
        covariance += dx * (history[i].soh_percent - mean_y);
        variance += dx * dx;
    }
    if (variance <= 1e-12) return false;
    const double degradation_per_cycle = covariance / variance;
    if (!isfinite(degradation_per_cycle) || degradation_per_cycle >= -1e-6)
        return false;

    *remaining = fmax(0.0, (current_soh - eol_soh) / -degradation_per_cycle);
    return true;
}
