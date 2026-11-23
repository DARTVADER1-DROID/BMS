#define _POSIX_C_SOURCE 200809L

#include "bms.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void configure_safe_test_limits(void)
{
    setenv("BMS_ENABLE_CONTROL", "1", 1);
    setenv("BMS_VOLTAGE_MIN", "0", 1);
    setenv("BMS_VOLTAGE_MAX", "5", 1);
    setenv("BMS_CURRENT_MIN", "-2", 1);
    setenv("BMS_CURRENT_MAX", "2", 1);
    setenv("BMS_TEMPERATURE_MIN", "-40", 1);
    setenv("BMS_TEMPERATURE_MAX", "85", 1);
}

static void test_control_state_transitions(void)
{
    Bms bms;

    bms_init(&bms);

    assert(bms.control_enabled);
    assert(bms.state == BMS_IDLE);

    assert(bms_update_telemetry(&bms, 3.7, 0.2, 25.0, 100.0) == BMS_OK);

    assert(bms_start_charging(&bms) == BMS_OK);
    assert(bms.state == BMS_CHARGING);

    assert(bms_start_discharging(&bms) == BMS_ERR_TRANSITION);
    assert(bms_stop(&bms) == BMS_OK);
    assert(bms.state == BMS_IDLE);

    assert(bms_start_discharging(&bms) == BMS_OK);
    assert(bms.state == BMS_DISCHARGING);

    assert(bms_stop(&bms) == BMS_OK);
    bms_destroy(&bms);
}

static void test_unsafe_measurements_block_control(void)
{
    Bms bms;

    bms_init(&bms);

    assert(bms_update_telemetry(&bms, 9.0, 0.0, 25.0, 101.0) == BMS_OK);
    assert(!bms_is_safe(&bms));
    assert(bms_start_charging(&bms) == BMS_ERR_UNSAFE);
    assert(bms.state == BMS_IDLE);

    bms_destroy(&bms);
}

static void test_cycle_count_validation_and_estimates(void)
{
    Bms bms;

    bms_init(&bms);

    assert(bms_set_cycles(&bms, -1) == BMS_ERR_INVALID);
    assert(bms_set_cycles(&bms, 500) == BMS_OK);
    assert(bms_rul(&bms) == 500.0);
    assert(isfinite(bms_soc(&bms)));

    bms_destroy(&bms);
}

static void test_non_finite_telemetry_is_rejected(void)
{
    Bms bms;

    bms_init(&bms);

    assert(bms_update_telemetry(&bms, NAN, 0.0, 25.0, 102.0) == BMS_ERR_INVALID);
    assert(bms_update_telemetry(&bms, INFINITY, 0.0, 25.0, 102.0) == BMS_ERR_INVALID);

    bms_destroy(&bms);
}

static void test_stale_hardware_heartbeat_stops_bms(void)
{
    Bms bms;

    bms_init(&bms);
    assert(bms_update_telemetry(&bms, 3.7, 0.2, 25.0, 100.0) == BMS_OK);
    assert(bms.hardware_connected);

    bms_tick(&bms, 108.0);

    assert(!bms.hardware_connected);
    assert(bms.state == BMS_IDLE);

    bms_destroy(&bms);
}

static void test_invalid_limits_disable_control(void)
{
    Bms bms;

    setenv("BMS_VOLTAGE_MIN", "5", 1);
    setenv("BMS_VOLTAGE_MAX", "0", 1);

    bms_init(&bms);

    assert(!bms.control_enabled);

    bms_destroy(&bms);
    configure_safe_test_limits();
}

int main(void)
{
    configure_safe_test_limits();

    test_control_state_transitions();
    test_unsafe_measurements_block_control();
    test_cycle_count_validation_and_estimates();
    test_non_finite_telemetry_is_rejected();
    test_stale_hardware_heartbeat_stops_bms();
    test_invalid_limits_disable_control();

    puts("BMS core tests passed");
    return 0;
}
