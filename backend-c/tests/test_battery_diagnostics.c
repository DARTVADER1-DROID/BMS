#include "battery_features.h"
#include "battery_health.h"
#include "diagnostic.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_pulse_diagnostic(void)
{
    BatteryDiagnostic diagnostic;
    BatteryTest test;
    BatteryFeatures features;

    const BatterySample samples[] = {
        {1.0, 3.98, 0.50, 25.0},
        {2.0, 3.97, 0.50, 25.0},
        {3.0, 3.96, 0.50, 25.1},
        {4.0, 3.95, 0.50, 25.1},
        {5.0, 3.98, 0.00, 25.1},
        {6.0, 3.99, 0.00, 25.1}
    };

    diagnostic_init(&diagnostic, NULL);

    assert(diagnostic_begin(&diagnostic, 0.0, 4.00, 25.0));

    for (size_t index = 0;
         index < sizeof(samples) / sizeof(samples[0]);
         index++) {
        assert(diagnostic_add_sample(&diagnostic, &samples[index]));
    }

    assert(diagnostic_finish(&diagnostic, &test));
    assert(battery_features_calculate(&test, &features));

    assert(features.step_resistance_ohm > 0.0);
    assert(features.pulse_resistance_ohm > 0.0);
}

static void test_uncalibrated_soh_is_unavailable(void)
{
    BatterySohModel model = {0};
    BatteryFeatures features = {0};
    BatterySohEstimate estimate;

    assert(battery_soh_estimate(&model, &features, &estimate));
    assert(!estimate.available);
}

static void test_rul_requires_degradation_history(void)
{
    const BatteryHealthPoint history[] = {
        {0.0, 100.0},
        {50.0, 96.0},
        {100.0, 91.0}
    };

    double remaining_cycles = 0.0;

    assert(
        battery_rul_estimate(
            history,
            sizeof(history) / sizeof(history[0]),
            91.0,
            80.0,
            &remaining_cycles
        )
    );

    assert(remaining_cycles > 0.0);
    assert(isfinite(remaining_cycles));
}

static void test_unsafe_voltage_aborts_diagnostic(void)
{
    BatteryDiagnostic diagnostic;
    const BatterySample unsafe_sample = {
        1.0,
        2.9,
        0.5,
        25.0
    };

    diagnostic_init(&diagnostic, NULL);

    assert(diagnostic_begin(&diagnostic, 0.0, 4.00, 25.0));
    assert(!diagnostic_add_sample(&diagnostic, &unsafe_sample));
    assert(diagnostic.state == DIAGNOSTIC_ABORTED);
}

int main(void)
{
    test_pulse_diagnostic();
    test_uncalibrated_soh_is_unavailable();
    test_rul_requires_degradation_history();
    test_unsafe_voltage_aborts_diagnostic();

    puts("Battery diagnostic tests passed");
    return 0;
}
