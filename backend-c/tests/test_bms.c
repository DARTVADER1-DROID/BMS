#define _POSIX_C_SOURCE 200809L

#include "bms.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void test_invalid_timestamps_are_rejected_without_mutation(void)
{
    Bms bms;

    bms_init(&bms);

    assert(bms_update_telemetry(&bms, 3.8, 0.1, 25.0, 10.0) == BMS_OK);
    assert(bms_update_telemetry(&bms, 4.0, 0.2, 26.0, NAN) == BMS_ERR_INVALID);
    assert(bms_update_telemetry(&bms, 4.0, 0.2, 26.0, INFINITY) == BMS_ERR_INVALID);
    assert(bms_update_telemetry(&bms, 4.0, 0.2, 26.0, -1.0) == BMS_ERR_INVALID);

    assert(bms.voltage == 3.8);
    assert(bms.current == 0.1);
    assert(bms.temperature == 25.0);
    assert(bms.last_seen == 10.0);

    bms_destroy(&bms);
}

static void test_telemetry_numeric_limits_and_cycle_upper_bound(void)
{
    Bms bms;

    bms_init(&bms);

    assert(bms_update_telemetry(&bms, 1e6, 1e6, 1e4, 0.0) == BMS_OK);
    assert(bms_update_telemetry(&bms, 1e6 + 1.0, 0.0, 25.0, 1.0) == BMS_ERR_INVALID);
    assert(bms_set_cycles(&bms, 1000000000LL) == BMS_OK);
    assert(bms_set_cycles(&bms, 1000000001LL) == BMS_ERR_INVALID);

    bms_destroy(&bms);
}

static void test_state_name_handles_every_enum_value(void)
{
    assert(strcmp(bms_state_name(BMS_IDLE), "idle") == 0);
    assert(strcmp(bms_state_name(BMS_CHARGING), "charging") == 0);
    assert(strcmp(bms_state_name(BMS_DISCHARGING), "discharging") == 0);
    assert(strcmp(bms_state_name((BmsState)99), "idle") == 0);
}

static void test_soc_clamps_to_valid_percentage(void)
{
    Bms bms;

    bms_init(&bms);

    assert(bms_update_telemetry(&bms, -1.0, 0.0, 25.0, 1.0) == BMS_OK);
    assert(bms_soc(&bms) == 0.0);

    assert(bms_update_telemetry(&bms, 6.0, 0.0, 25.0, 2.0) == BMS_OK);
    assert(bms_soc(&bms) == 100.0);

    bms_destroy(&bms);
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
    test_invalid_timestamps_are_rejected_without_mutation();
    test_telemetry_numeric_limits_and_cycle_upper_bound();
    test_state_name_handles_every_enum_value();
    test_soc_clamps_to_valid_percentage();

    puts("BMS core tests passed");
    return 0;
}
