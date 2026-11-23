#include "battery_features.h"
#include "battery_health.h"
#include "bms.h"
#include "diagnostic.h"

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>

static void test_null_arguments_are_rejected(void)
{
    BatteryDiagnostic diagnostic;
    BatteryTest test = {0};
    BatteryFeatures features = {0};
    BatterySohModel model = {0};
    BatterySohEstimate estimate = {0};
    double remaining_cycles = 0.0;

    diagnostic_init(&diagnostic, NULL);

    assert(!diagnostic_begin(NULL, 0.0, 4.0, 25.0));
    assert(!diagnostic_add_sample(&diagnostic, NULL));
    assert(!diagnostic_finish(&diagnostic, &test));
    assert(!diagnostic_finish(&diagnostic, NULL));
    assert(!battery_features_calculate(NULL, &features));
    assert(!battery_features_calculate(&test, NULL));
    assert(!battery_soh_estimate(NULL, &features, &estimate));
    assert(!battery_soh_estimate(&model, NULL, &estimate));
    assert(!battery_soh_estimate(&model, &features, NULL));
    assert(!battery_rul_estimate(NULL, 3, 90.0, 80.0, &remaining_cycles));
    assert(!battery_rul_estimate(NULL, 0, 90.0, 80.0, &remaining_cycles));
}

static void test_diagnostic_rejects_non_monotonic_time(void)
{
    BatteryDiagnostic diagnostic;
    BatterySample sample = {1.0, 3.9, 0.5, 25.0};

    diagnostic_init(&diagnostic, NULL);
    assert(diagnostic_begin(&diagnostic, 0.0, 4.0, 25.0));
    assert(diagnostic_add_sample(&diagnostic, &sample));

    sample.time_s = 1.0;
    assert(!diagnostic_add_sample(&diagnostic, &sample));
    assert(diagnostic.state == DIAGNOSTIC_ABORTED);
}

static void test_diagnostic_rejects_excessive_current(void)
{
    BatteryDiagnostic diagnostic;
    BatterySample sample = {1.0, 3.9, 1.01, 25.0};

    diagnostic_init(&diagnostic, NULL);
    assert(diagnostic_begin(&diagnostic, 0.0, 4.0, 25.0));
    assert(!diagnostic_add_sample(&diagnostic, &sample));
    assert(diagnostic.state == DIAGNOSTIC_ABORTED);
}

static void test_diagnostic_rejects_stale_samples(void)
{
    BatteryDiagnostic diagnostic;
    BatterySample sample = {2.1, 3.9, 0.5, 25.0};

    diagnostic_init(&diagnostic, NULL);
    assert(diagnostic_begin(&diagnostic, 0.0, 4.0, 25.0));
    assert(!diagnostic_add_sample(&diagnostic, &sample));
    assert(diagnostic.state == DIAGNOSTIC_ABORTED);
}

static void test_health_model_rejects_invalid_scale(void)
{
    BatterySohModel model = {
        .calibrated = true,
        .minimum_soh_percent = 0.0,
        .maximum_soh_percent = 100.0,
        .validation_mae_percent = 1.0,
        .intercept_percent = 90.0
    };
    BatteryFeatures features = {0};
    BatterySohEstimate estimate = {0};

    for (size_t index = 0; index < 4; index++) {
        model.feature_scale[index] = 1.0;
    }
    model.feature_scale[2] = 0.0;

    assert(!battery_soh_estimate(&model, &features, &estimate));
}

static void test_rul_rejects_non_degrading_history(void)
{
    const BatteryHealthPoint history[] = {
        {0.0, 90.0},
        {50.0, 92.0},
        {100.0, 94.0}
    };
    double remaining_cycles = -1.0;

    assert(!battery_rul_estimate(history, 3, 94.0, 80.0, &remaining_cycles));
}

static void test_rul_rejects_duplicate_cycle_counts(void)
{
    const BatteryHealthPoint history[] = {
        {0.0, 100.0},
        {50.0, 95.0},
        {50.0, 90.0}
    };
    double remaining_cycles = -1.0;

    assert(!battery_rul_estimate(history, 3, 90.0, 80.0, &remaining_cycles));
}

static void test_rul_rejects_non_finite_history(void)
{
    const BatteryHealthPoint history[] = {
        {0.0, 100.0},
        {50.0, NAN},
        {100.0, 90.0}
    };
    double remaining_cycles = -1.0;

    assert(!battery_rul_estimate(history, 3, 90.0, 80.0, &remaining_cycles));
}

static void test_bms_rejects_extreme_non_finite_and_stale_data(void)
{
    Bms bms;

    bms_init(&bms);

    assert(bms_update_telemetry(&bms, INFINITY, 0.0, 25.0, 1.0) == BMS_ERR_INVALID);
    assert(bms_update_telemetry(&bms, 1e7, 0.0, 25.0, 1.0) == BMS_ERR_INVALID);
    assert(bms_update_telemetry(&bms, 3.7, 0.0, 25.0, 1.0) == BMS_OK);

    bms_tick(&bms, 9.0);
    assert(!bms.hardware_connected);
    assert(bms.state == BMS_IDLE);

    bms_destroy(&bms);
}


typedef struct {
    Bms *bms;
    unsigned int worker_id;
} BmsWorker;

static void *write_telemetry_repeatedly(void *argument)
{
    BmsWorker *worker = argument;

    for (unsigned int index = 0; index < 5000; index++) {
        double voltage = 3.6 + (double)((index + worker->worker_id) % 50) / 100.0;

        assert(
            bms_update_telemetry(
                worker->bms,
                voltage,
                0.2,
                25.0,
                (double)index
            ) == BMS_OK
        );
    }

    return NULL;
}

static void *read_bms_state_repeatedly(void *argument)
{
    BmsWorker *worker = argument;

    for (unsigned int index = 0; index < 5000; index++) {
        assert(isfinite(bms_soc(worker->bms)));
        assert(isfinite(bms_rul(worker->bms)));
        (void)bms_is_safe(worker->bms);
    }

    return NULL;
}

static void test_concurrent_telemetry_and_readers(void)
{
    Bms bms;
    pthread_t threads[4];
    BmsWorker workers[4];

    bms_init(&bms);

    for (unsigned int index = 0; index < 4; index++) {
        workers[index].bms = &bms;
        workers[index].worker_id = index;

        void *(*worker_function)(void *) =
            index < 2
                ? write_telemetry_repeatedly
                : read_bms_state_repeatedly;

        assert(pthread_create(&threads[index], NULL, worker_function, &workers[index]) == 0);
    }

    for (unsigned int index = 0; index < 4; index++) {
        assert(pthread_join(threads[index], NULL) == 0);
    }

    assert(bms.hardware_connected);
    bms_destroy(&bms);
}

int main(void)
{
    test_null_arguments_are_rejected();
    test_diagnostic_rejects_non_monotonic_time();
    test_diagnostic_rejects_excessive_current();
    test_diagnostic_rejects_stale_samples();
    test_health_model_rejects_invalid_scale();
    test_rul_rejects_non_degrading_history();
    test_rul_rejects_duplicate_cycle_counts();
    test_rul_rejects_non_finite_history();
    test_bms_rejects_extreme_non_finite_and_stale_data();
    test_concurrent_telemetry_and_readers();

    puts("BMS edge-case tests passed");
    return 0;
}
