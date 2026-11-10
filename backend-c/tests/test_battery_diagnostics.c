#include "diagnostic.h"
#include "battery_features.h"
#include "battery_health.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    BatteryDiagnostic diagnostic;
    diagnostic_init(&diagnostic, NULL);
    assert(diagnostic_begin(&diagnostic, 0.0, 4.00, 25.0));

    const BatterySample samples[] = {
        {1.0, 3.98, 0.50, 25.0},
        {2.0, 3.97, 0.50, 25.0},
        {3.0, 3.96, 0.50, 25.1},
        {4.0, 3.95, 0.50, 25.1},
        {5.0, 3.98, 0.00, 25.1},
        {6.0, 3.99, 0.00, 25.1}
    };
    for (size_t i = 0; i < sizeof(samples)/sizeof(samples[0]); ++i)
        assert(diagnostic_add_sample(&diagnostic, &samples[i]));

    BatteryTest test;
    BatteryFeatures features;
    assert(diagnostic_finish(&diagnostic, &test));
    assert(battery_features_calculate(&test, &features));
    assert(features.step_resistance_ohm > 0.0);
    assert(features.pulse_resistance_ohm > 0.0);

    BatterySohModel uncalibrated = {0};
    BatterySohEstimate estimate;
    assert(battery_soh_estimate(&uncalibrated, &features, &estimate));
    assert(!estimate.available);

    BatteryHealthPoint history[] = {{0,100},{50,96},{100,91}};
    double remaining = 0.0;
    assert(battery_rul_estimate(history, 3, 91, 80, &remaining));
    assert(remaining > 0.0 && isfinite(remaining));

    diagnostic_init(&diagnostic, NULL);
    assert(diagnostic_begin(&diagnostic, 0.0, 4.00, 25.0));
    const BatterySample unsafe = {1.0, 2.9, 0.5, 25.0};
    assert(!diagnostic_add_sample(&diagnostic, &unsafe));
    assert(diagnostic.state == DIAGNOSTIC_ABORTED);

    puts("Battery diagnostic tests passed");
    return 0;
}
