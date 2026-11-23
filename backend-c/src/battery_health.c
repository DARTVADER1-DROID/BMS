#include "battery_health.h"

#include <math.h>

#define SOH_FEATURE_COUNT 4
#define MINIMUM_CYCLE_VARIANCE 1e-12
#define MINIMUM_DEGRADATION_PER_CYCLE 1e-6

static double clamp_value(double value, double minimum, double maximum)
{
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

bool battery_soh_estimate(
    const BatterySohModel *model,
    const BatteryFeatures *features,
    BatterySohEstimate *output
)
{
    const double feature_values[SOH_FEATURE_COUNT] = {
        features != NULL ? features->step_resistance_ohm : NAN,
        features != NULL ? features->pulse_resistance_ohm : NAN,
        features != NULL ? features->recovery_fraction : NAN,
        features != NULL ? features->temperature_rise_c : NAN
    };
    double prediction;

    if (model == NULL || features == NULL || output == NULL) {
        return false;
    }

    output->available = false;

    if (!model->calibrated) {
        return true;
    }

    if (model->minimum_soh_percent < 0.0
        || model->maximum_soh_percent > 100.0
        || model->minimum_soh_percent >= model->maximum_soh_percent
        || model->validation_mae_percent < 0.0
        || !isfinite(model->intercept_percent)) {
        return false;
    }

    prediction = model->intercept_percent;

    for (size_t index = 0; index < SOH_FEATURE_COUNT; index++) {
        double feature_scale;
        double standardized_feature;

        if (!isfinite(feature_values[index])
            || !isfinite(model->feature_mean[index])
            || !isfinite(model->feature_scale[index])
            || !isfinite(model->coefficient[index])) {
            return false;
        }

        feature_scale = model->feature_scale[index];

        if (feature_scale <= 0.0) {
            return false;
        }

        standardized_feature =
            (feature_values[index] - model->feature_mean[index])
            / feature_scale;

        prediction +=
            model->coefficient[index] * standardized_feature;
    }

    output->soh_percent = clamp_value(
        prediction,
        model->minimum_soh_percent,
        model->maximum_soh_percent
    );

    output->validation_mae_percent = model->validation_mae_percent;
    output->available = true;

    return true;
}

bool battery_rul_estimate(
    const BatteryHealthPoint *history,
    size_t history_count,
    double current_soh,
    double end_of_life_soh,
    double *remaining_cycles
)
{
    double mean_cycles = 0.0;
    double mean_soh = 0.0;
    double covariance = 0.0;
    double cycle_variance = 0.0;
    double degradation_per_cycle;

    if (history == NULL
        || remaining_cycles == NULL
        || history_count < 3
        || !isfinite(current_soh)
        || !isfinite(end_of_life_soh)
        || end_of_life_soh < 0.0
        || end_of_life_soh >= current_soh) {
        return false;
    }

    for (size_t index = 0; index < history_count; index++) {
        const BatteryHealthPoint *point = &history[index];

        if (!isfinite(point->equivalent_cycles)
            || !isfinite(point->soh_percent)
            || point->equivalent_cycles < 0.0
            || point->soh_percent < 0.0
            || point->soh_percent > 100.0) {
            return false;
        }

        if (index > 0
            && point->equivalent_cycles
                <= history[index - 1].equivalent_cycles) {
            return false;
        }

        mean_cycles += point->equivalent_cycles;
        mean_soh += point->soh_percent;
    }

    mean_cycles /= (double)history_count;
    mean_soh /= (double)history_count;

    for (size_t index = 0; index < history_count; index++) {
        const double cycle_difference =
            history[index].equivalent_cycles - mean_cycles;

        const double soh_difference =
            history[index].soh_percent - mean_soh;

        covariance += cycle_difference * soh_difference;
        cycle_variance += cycle_difference * cycle_difference;
    }

    if (cycle_variance <= MINIMUM_CYCLE_VARIANCE) {
        return false;
    }

    degradation_per_cycle = covariance / cycle_variance;

    if (!isfinite(degradation_per_cycle)
        || degradation_per_cycle >= -MINIMUM_DEGRADATION_PER_CYCLE) {
        return false;
    }

    *remaining_cycles = fmax(
        0.0,
        (current_soh - end_of_life_soh) / -degradation_per_cycle
    );

    return true;
}
