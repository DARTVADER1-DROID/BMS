#include "bms.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
 setenv("BMS_ENABLE_CONTROL","1",1);setenv("BMS_VOLTAGE_MIN","0",1);setenv("BMS_VOLTAGE_MAX","5",1);
 setenv("BMS_CURRENT_MIN","-2",1);setenv("BMS_CURRENT_MAX","2",1);setenv("BMS_TEMPERATURE_MIN","-40",1);setenv("BMS_TEMPERATURE_MAX","85",1);
 Bms b;bms_init(&b);assert(b.control_enabled);
 assert(bms_update_telemetry(&b,3.7,.2,25,100)==BMS_OK);
 assert(bms_start_charging(&b)==BMS_OK);assert(bms_start_discharging(&b)==BMS_ERR_TRANSITION);
 assert(bms_stop(&b)==BMS_OK);assert(bms_start_discharging(&b)==BMS_OK);assert(bms_stop(&b)==BMS_OK);
 assert(bms_update_telemetry(&b,9,0,25,101)==BMS_OK);assert(bms_start_charging(&b)==BMS_ERR_UNSAFE);
 assert(bms_set_cycles(&b,-1)==BMS_ERR_INVALID);assert(bms_set_cycles(&b,500)==BMS_OK);assert(bms_rul(&b)==500);
 assert(isfinite(bms_soc(&b)));assert(bms_update_telemetry(&b,NAN,0,25,102)==BMS_ERR_INVALID);
 bms_tick(&b,110);assert(!b.hardware_connected);assert(b.state==BMS_IDLE);bms_destroy(&b);puts("BMS core tests passed");return 0;
}
