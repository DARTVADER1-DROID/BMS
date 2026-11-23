#ifndef BMS_H
#define BMS_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BMS_IDLE = 0,
    BMS_CHARGING,
    BMS_DISCHARGING
} BmsState;

typedef enum {
    BMS_OK = 0,
    BMS_ERR_INVALID,
    BMS_ERR_UNSAFE,
    BMS_ERR_TRANSITION,
    BMS_ERR_DISABLED
} BmsResult;

typedef struct {
    double voltage;
    double current;
    double temperature;

    double voltage_min;
    double voltage_max;
    double current_min;
    double current_max;
    double temperature_min;
    double temperature_max;

    double soc_voltage_min;
    double soc_voltage_max;
    double rated_useful_life;

    int64_t cycle_count;
    double last_seen;

    bool hardware_connected;
    bool control_enabled;
    BmsState state;

    char warning[96];
    pthread_mutex_t lock;
} Bms;

void bms_init(Bms *bms);
void bms_destroy(Bms *bms);

BmsResult bms_update_telemetry(
    Bms *bms,
    double voltage,
    double current,
    double temperature,
    double timestamp
);

BmsResult bms_start_charging(Bms *bms);
BmsResult bms_start_discharging(Bms *bms);
BmsResult bms_stop(Bms *bms);

void bms_tick(Bms *bms, double now);
BmsResult bms_set_cycles(Bms *bms, int64_t cycle_count);

const char *bms_state_name(BmsState state);

double bms_soc(Bms *bms);
double bms_soh(Bms *bms);
double bms_rul(Bms *bms);

bool bms_is_safe(Bms *bms);

#endif
