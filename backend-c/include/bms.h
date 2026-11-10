#ifndef BMS_H
#define BMS_H
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
typedef enum { BMS_IDLE=0, BMS_CHARGING, BMS_DISCHARGING } BmsState;
typedef enum { BMS_OK=0, BMS_ERR_INVALID, BMS_ERR_UNSAFE, BMS_ERR_TRANSITION, BMS_ERR_DISABLED } BmsResult;
typedef struct {
 double voltage,current,temperature;
 double voltage_min,voltage_max,current_min,current_max,temperature_min,temperature_max;
 double soc_voltage_min,soc_voltage_max,rated_useful_life;
 int64_t cycle_count;
 double last_seen;
 bool hardware_connected,control_enabled;
 BmsState state;
 char warning[96];
 pthread_mutex_t lock;
} Bms;
void bms_init(Bms*); void bms_destroy(Bms*);
BmsResult bms_update_telemetry(Bms*,double,double,double,double);
BmsResult bms_start_charging(Bms*); BmsResult bms_start_discharging(Bms*);
BmsResult bms_stop(Bms*); void bms_tick(Bms*,double);
BmsResult bms_set_cycles(Bms*,int64_t);
const char *bms_state_name(BmsState);
double bms_soc(Bms*); double bms_soh(Bms*); double bms_rul(Bms*);
bool bms_is_safe(Bms*);
#endif
