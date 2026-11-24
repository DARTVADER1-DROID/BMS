#define _POSIX_C_SOURCE 200809L

#include "core/bms.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double read_environment_double(const char *name, double default_value)
{
    const char *value = getenv(name);
    char *end = NULL;
    double parsed_value;

    if (value == NULL || value[0] == '\0') {
        return default_value;
    }

    parsed_value = strtod(value, &end);

    if (end == value || *end != '\0' || !isfinite(parsed_value)) {
        return default_value;
    }

    return parsed_value;
}

static bool environment_value_is_set(const char *name)
{
    const char *value = getenv(name);
    return value != NULL && value[0] != '\0';
}

static bool environment_flag_is_enabled(const char *name)
{
    const char *value = getenv(name);

    if (value == NULL) {
        return false;
    }

    return strcmp(value, "1") == 0
        || strcmp(value, "true") == 0
        || strcmp(value, "TRUE") == 0
        || strcmp(value, "yes") == 0;
}

static double monotonic_time_seconds(void)
{
    struct timespec timestamp;

    clock_gettime(CLOCK_MONOTONIC, &timestamp);

    return (double)timestamp.tv_sec
        + (double)timestamp.tv_nsec / 1e9;
}

static bool configured_limits_are_valid(const Bms *bms)
{
    return bms->voltage_min < bms->voltage_max
        && bms->current_min < bms->current_max
        && bms->temperature_min < bms->temperature_max;
}

/* Caller must hold bms->lock. */
static bool measurements_are_safe(Bms *bms)
{
    if (!isfinite(bms->voltage)
        || !isfinite(bms->current)
        || !isfinite(bms->temperature)) {
        snprintf(bms->warning, sizeof(bms->warning), "Invalid sensor data");
        return false;
    }

    if (bms->voltage > bms->voltage_max) {
        snprintf(bms->warning, sizeof(bms->warning), "Over voltage!");
        return false;
    }

    if (bms->voltage < bms->voltage_min) {
        snprintf(bms->warning, sizeof(bms->warning), "Under voltage!");
        return false;
    }

    if (bms->current > bms->current_max) {
        snprintf(bms->warning, sizeof(bms->warning), "Over current!");
        return false;
    }

    if (bms->current < bms->current_min) {
        snprintf(bms->warning, sizeof(bms->warning), "Under current!");
        return false;
    }

    if (bms->temperature > bms->temperature_max) {
        snprintf(bms->warning, sizeof(bms->warning), "Over temperature!");
        return false;
    }

    if (bms->temperature < bms->temperature_min) {
        snprintf(bms->warning, sizeof(bms->warning), "Under temperature!");
        return false;
    }

    snprintf(bms->warning, sizeof(bms->warning), "Nominal");
    return true;
}

void bms_init(Bms *bms)
{
    memset(bms, 0, sizeof(*bms));
    pthread_mutex_init(&bms->lock, NULL);

    bms->voltage = 3.7;
    bms->temperature = 27.0;

    bms->voltage_min = read_environment_double("BMS_VOLTAGE_MIN", -10.0);
    bms->voltage_max = read_environment_double("BMS_VOLTAGE_MAX", 10.0);

    bms->current_min = read_environment_double("BMS_CURRENT_MIN", -10.0);
    bms->current_max = read_environment_double("BMS_CURRENT_MAX", 10.0);

    bms->temperature_min = read_environment_double("BMS_TEMPERATURE_MIN", -100.0);
    bms->temperature_max = read_environment_double("BMS_TEMPERATURE_MAX", 100.0);

    bms->soc_voltage_min = read_environment_double("BMS_SOC_VOLTAGE_MIN", 0.0);
    bms->soc_voltage_max = read_environment_double("BMS_SOC_VOLTAGE_MAX", 5.0);

    bms->rated_useful_life = read_environment_double("BMS_RATED_USEFUL_LIFE", 1000.0);
    bms->cycle_count = 112;
    bms->last_seen = monotonic_time_seconds();
    bms->state = BMS_IDLE;

    bms->control_enabled =
        environment_flag_is_enabled("BMS_ENABLE_CONTROL")
        && environment_value_is_set("BMS_VOLTAGE_MIN")
        && environment_value_is_set("BMS_VOLTAGE_MAX")
        && environment_value_is_set("BMS_CURRENT_MIN")
        && environment_value_is_set("BMS_CURRENT_MAX")
        && environment_value_is_set("BMS_TEMPERATURE_MIN")
        && environment_value_is_set("BMS_TEMPERATURE_MAX")
        && configured_limits_are_valid(bms);

    snprintf(bms->warning, sizeof(bms->warning), "Nominal");
}

void bms_destroy(Bms *bms)
{
    pthread_mutex_destroy(&bms->lock);
}

BmsResult bms_update_telemetry(
    Bms *bms,
    double voltage,
    double current,
    double temperature,
    double timestamp
)
{
    if (!isfinite(voltage)
        || !isfinite(current)
        || !isfinite(temperature)
        || !isfinite(timestamp)
        || timestamp < 0.0
        || fabs(voltage) > 1e6
        || fabs(current) > 1e6
        || fabs(temperature) > 1e4) {
        return BMS_ERR_INVALID;
    }

    pthread_mutex_lock(&bms->lock);

    bms->voltage = voltage;
    bms->current = current;
    bms->temperature = temperature;
    bms->last_seen = timestamp;
    bms->hardware_connected = true;

    if (!measurements_are_safe(bms)) {
        bms->state = BMS_IDLE;
    }

    pthread_mutex_unlock(&bms->lock);
    return BMS_OK;
}

static BmsResult start_operation(Bms *bms, BmsState requested_state)
{
    pthread_mutex_lock(&bms->lock);

    if (!bms->control_enabled) {
        pthread_mutex_unlock(&bms->lock);
        return BMS_ERR_DISABLED;
    }

    if (!bms->hardware_connected) {
        pthread_mutex_unlock(&bms->lock);
        return BMS_ERR_UNSAFE;
    }

    if (!measurements_are_safe(bms)) {
        bms->state = BMS_IDLE;
        pthread_mutex_unlock(&bms->lock);
        return BMS_ERR_UNSAFE;
    }

    if (bms->state != BMS_IDLE) {
        pthread_mutex_unlock(&bms->lock);
        return BMS_ERR_TRANSITION;
    }

    bms->state = requested_state;

    pthread_mutex_unlock(&bms->lock);
    return BMS_OK;
}

BmsResult bms_start_charging(Bms *bms)
{
    return start_operation(bms, BMS_CHARGING);
}

BmsResult bms_start_discharging(Bms *bms)
{
    return start_operation(bms, BMS_DISCHARGING);
}

BmsResult bms_stop(Bms *bms)
{
    pthread_mutex_lock(&bms->lock);
    bms->state = BMS_IDLE;
    pthread_mutex_unlock(&bms->lock);

    return BMS_OK;
}

void bms_tick(Bms *bms, double now)
{
    pthread_mutex_lock(&bms->lock);

    if (now - bms->last_seen > 7.0) {
        bms->hardware_connected = false;
        bms->state = BMS_IDLE;
        snprintf(
            bms->warning,
            sizeof(bms->warning),
            "Hardware heartbeat timeout"
        );
    } else if (!measurements_are_safe(bms)) {
        bms->state = BMS_IDLE;
    }

    pthread_mutex_unlock(&bms->lock);
}

BmsResult bms_set_cycles(Bms *bms, int64_t cycle_count)
{
    if (cycle_count < 0 || cycle_count > 1000000000LL) {
        return BMS_ERR_INVALID;
    }

    pthread_mutex_lock(&bms->lock);
    bms->cycle_count = cycle_count;
    pthread_mutex_unlock(&bms->lock);

    return BMS_OK;
}

const char *bms_state_name(BmsState state)
{
    switch (state) {
        case BMS_CHARGING:
            return "charging";

        case BMS_DISCHARGING:
            return "discharging";

        case BMS_IDLE:
        default:
            return "idle";
    }
}

bool bms_is_safe(Bms *bms)
{
    bool safe;

    pthread_mutex_lock(&bms->lock);
    safe = measurements_are_safe(bms);
    pthread_mutex_unlock(&bms->lock);

    return safe;
}

double bms_soc(Bms *bms)
{
    double voltage_span;
    double state_of_charge;

    pthread_mutex_lock(&bms->lock);

    voltage_span = bms->soc_voltage_max - bms->soc_voltage_min;

    if (voltage_span > 0.0) {
        state_of_charge =
            100.0 * (bms->voltage - bms->soc_voltage_min) / voltage_span;
    } else {
        state_of_charge = NAN;
    }

    if (isfinite(state_of_charge)) {
        if (state_of_charge < 0.0) {
            state_of_charge = 0.0;
        } else if (state_of_charge > 100.0) {
            state_of_charge = 100.0;
        }
    }

    pthread_mutex_unlock(&bms->lock);
    return state_of_charge;
}

/* Legacy empirical curve retained for compatibility, not a validated estimator. */
double bms_soh(Bms *bms)
{
    int64_t cycle_count;
    double cycle_value;
    double logarithm_input;
    double state_of_health;

    pthread_mutex_lock(&bms->lock);
    cycle_count = bms->cycle_count;
    pthread_mutex_unlock(&bms->lock);

    if (cycle_count <= 0) {
        return NAN;
    }

    cycle_value = (double)cycle_count;
    logarithm_input = cycle_value / 3.97379;

    state_of_health =
        0.000088875 * pow(cycle_value - 760.58, 2.0)
        + pow(log(logarithm_input), 2.0)
        + 47.917;

    if (!isfinite(state_of_health)) {
        return NAN;
    }

    if (state_of_health < 0.0) {
        return 0.0;
    }

    if (state_of_health > 100.0) {
        return 100.0;
    }

    return state_of_health;
}

double bms_rul(Bms *bms)
{
    double remaining_cycles;

    pthread_mutex_lock(&bms->lock);
    remaining_cycles = bms->rated_useful_life - (double)bms->cycle_count;
    pthread_mutex_unlock(&bms->lock);

    return remaining_cycles > 0.0 ? remaining_cycles : 0.0;
}
