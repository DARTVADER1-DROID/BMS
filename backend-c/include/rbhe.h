#ifndef RBHE_H
#define RBHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RBHE_MAX_SAMPLES 256
#define RBHE_HISTORY 16

typedef enum {
    RBHE_OK = 0,
    RBHE_ERR_ARGUMENT,
    RBHE_ERR_STATE,
    RBHE_ERR_CAPACITY,
    RBHE_ERR_INSUFFICIENT_DATA,
    RBHE_ERR_CALIBRATION
} RbheResult;

typedef struct {
    double min_test_current_a;
    double max_test_current_a;
    double min_voltage_v;
    double max_voltage_v;
    double max_temperature_c;
    double max_test_seconds;
    double max_sample_gap_seconds;
} RbheLimits;

/* Coefficients are learned from cells with independently measured capacity SOH.
   Features are standardized: (feature - mean) / scale. */
typedef struct {
    bool valid;
    double mean[4];   /* R_step_ohm, R_steady_ohm, recovery_fraction, temp_rise_C */
    double scale[4];
    double weight[4];
    double intercept_percent;
    double min_soh_percent;
    double max_soh_percent;
    double max_validation_error_percent;
} RbheSohCalibration;

typedef struct {
    double timestamp_s;
    double voltage_v;
    double current_a; /* Positive means discharge; negative means charge. */
    double temperature_c;
} RbheSample;

typedef struct {
    bool valid;
    double duration_s;
    double baseline_voltage_v;
    double minimum_load_voltage_v;
    double mean_load_current_a;
    double step_resistance_ohm;
    double steady_resistance_ohm;
    double voltage_slope_v_per_s;
    double recovery_fraction;
    double recovery_elapsed_s;
    double temperature_rise_c;
    double temperature_slope_c_per_s;
    double estimated_soh_percent;
    double soh_validation_error_percent;
    double soh_confidence_percent;
    bool soh_available;
    double rul_cycles;
    bool rul_available;
} RbheResultData;

typedef struct {
    RbheLimits limits;
    RbheSohCalibration soh;
    RbheSample samples[RBHE_MAX_SAMPLES];
    size_t sample_count;
    bool active;
    double baseline_voltage_v;
    double baseline_temperature_c;
    double start_time_s;
    double load_start_time_s;
    double recovery_start_time_s;
    double first_load_voltage_v;
    double min_load_voltage_v;
    double sum_load_current_a;
    double sum_load_voltage_v;
    size_t load_count;
    double recovery_voltage_v;
    double recovery_time_s;
    bool saw_load;
    bool saw_recovery;
    double initial_capacity_ah;
    double history_soh[RBHE_HISTORY];
    double history_cycle[RBHE_HISTORY];
    size_t history_count;
} Rbhe;

void rbhe_default_limits(RbheLimits *limits);
void rbhe_init(Rbhe *estimator, const RbheLimits *limits);
RbheResult rbhe_set_soh_calibration(Rbhe *estimator, const RbheSohCalibration *calibration);
RbheResult rbhe_begin(Rbhe *estimator, double timestamp_s,
                      double resting_voltage_v, double temperature_c);
RbheResult rbhe_sample(Rbhe *estimator, const RbheSample *sample);
RbheResult rbhe_finish(Rbhe *estimator, RbheResultData *result);

/* Add a SOH estimate paired with an independently known equivalent full-cycle
   count. RUL is withheld until at least three valid points show degradation. */
RbheResult rbhe_add_health_history(Rbhe *estimator, double cycle_equivalent,
                                    double measured_soh_percent);
RbheResult rbhe_estimate_rul(const Rbhe *estimator, double current_soh_percent,
                             double end_of_life_soh_percent, double *rul_cycles);

#ifdef __cplusplus
}
#endif
#endif
