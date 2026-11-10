#define _POSIX_C_SOURCE 200809L
#include "bms.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static double env_double(const char*n,double d){const char*v=getenv(n);if(!v||!*v)return d;char*e=NULL;double x=strtod(v,&e);return e&&*e=='\0'&&isfinite(x)?x:d;}
static bool env_true(const char*n){const char*v=getenv(n);return v&&(!strcmp(v,"1")||!strcmp(v,"true")||!strcmp(v,"TRUE")||!strcmp(v,"yes"));}
static double mono(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(double)t.tv_sec+(double)t.tv_nsec/1e9;}
static bool limits_valid(const Bms*b){return b->voltage_min<b->voltage_max&&b->current_min<b->current_max&&b->temperature_min<b->temperature_max;}
static bool safe_unlocked(Bms*b){
 if(!isfinite(b->voltage)||!isfinite(b->current)||!isfinite(b->temperature)){snprintf(b->warning,sizeof b->warning,"Invalid sensor data");return false;}
 if(b->voltage>b->voltage_max){snprintf(b->warning,sizeof b->warning,"Over voltage!");return false;}
 if(b->voltage<b->voltage_min){snprintf(b->warning,sizeof b->warning,"Under voltage!");return false;}
 if(b->current>b->current_max){snprintf(b->warning,sizeof b->warning,"Over current!");return false;}
 if(b->current<b->current_min){snprintf(b->warning,sizeof b->warning,"Under current!");return false;}
 if(b->temperature>b->temperature_max){snprintf(b->warning,sizeof b->warning,"Over temperature!");return false;}
 if(b->temperature<b->temperature_min){snprintf(b->warning,sizeof b->warning,"Under temperature!");return false;}
 snprintf(b->warning,sizeof b->warning,"Nominal");return true;
}
void bms_init(Bms*b){
 memset(b,0,sizeof *b);pthread_mutex_init(&b->lock,NULL);
 b->voltage=3.7;b->temperature=27.0;
 b->voltage_min=env_double("BMS_VOLTAGE_MIN",-10);b->voltage_max=env_double("BMS_VOLTAGE_MAX",10);
 b->current_min=env_double("BMS_CURRENT_MIN",-10);b->current_max=env_double("BMS_CURRENT_MAX",10);
 b->temperature_min=env_double("BMS_TEMPERATURE_MIN",-100);b->temperature_max=env_double("BMS_TEMPERATURE_MAX",100);
 b->soc_voltage_min=env_double("BMS_SOC_VOLTAGE_MIN",0);b->soc_voltage_max=env_double("BMS_SOC_VOLTAGE_MAX",5);
 b->rated_useful_life=env_double("BMS_RATED_USEFUL_LIFE",1000);b->cycle_count=112;
 b->last_seen=mono();b->state=BMS_IDLE;
 b->control_enabled=env_true("BMS_ENABLE_CONTROL")&&limits_valid(b);
 snprintf(b->warning,sizeof b->warning,"Nominal");
}
void bms_destroy(Bms*b){pthread_mutex_destroy(&b->lock);}
BmsResult bms_update_telemetry(Bms*b,double v,double a,double t,double now){
 if(!isfinite(v)||!isfinite(a)||!isfinite(t)||fabs(v)>1e6||fabs(a)>1e6||fabs(t)>1e4)return BMS_ERR_INVALID;
 pthread_mutex_lock(&b->lock);b->voltage=v;b->current=a;b->temperature=t;b->last_seen=now;b->hardware_connected=true;
 if(!safe_unlocked(b))b->state=BMS_IDLE;pthread_mutex_unlock(&b->lock);return BMS_OK;
}
static BmsResult start_state(Bms*b,BmsState target){
 pthread_mutex_lock(&b->lock);
 if(!b->control_enabled){pthread_mutex_unlock(&b->lock);return BMS_ERR_DISABLED;}
 if(!b->hardware_connected){pthread_mutex_unlock(&b->lock);return BMS_ERR_UNSAFE;}
 if(!safe_unlocked(b)){b->state=BMS_IDLE;pthread_mutex_unlock(&b->lock);return BMS_ERR_UNSAFE;}
 if(b->state!=BMS_IDLE){pthread_mutex_unlock(&b->lock);return BMS_ERR_TRANSITION;}
 b->state=target;pthread_mutex_unlock(&b->lock);return BMS_OK;
}
BmsResult bms_start_charging(Bms*b){return start_state(b,BMS_CHARGING);}
BmsResult bms_start_discharging(Bms*b){return start_state(b,BMS_DISCHARGING);}
BmsResult bms_stop(Bms*b){pthread_mutex_lock(&b->lock);b->state=BMS_IDLE;pthread_mutex_unlock(&b->lock);return BMS_OK;}
void bms_tick(Bms*b,double now){pthread_mutex_lock(&b->lock);if(now-b->last_seen>7.0){b->hardware_connected=false;b->state=BMS_IDLE;snprintf(b->warning,sizeof b->warning,"Hardware heartbeat timeout");}else if(!safe_unlocked(b))b->state=BMS_IDLE;pthread_mutex_unlock(&b->lock);}
BmsResult bms_set_cycles(Bms*b,int64_t n){if(n<0||n>1000000000LL)return BMS_ERR_INVALID;pthread_mutex_lock(&b->lock);b->cycle_count=n;pthread_mutex_unlock(&b->lock);return BMS_OK;}
const char*bms_state_name(BmsState s){switch(s){case BMS_CHARGING:return"charging";case BMS_DISCHARGING:return"discharging";default:return"idle";}}
bool bms_is_safe(Bms*b){pthread_mutex_lock(&b->lock);bool x=safe_unlocked(b);pthread_mutex_unlock(&b->lock);return x;}
double bms_soc(Bms*b){pthread_mutex_lock(&b->lock);double span=b->soc_voltage_max-b->soc_voltage_min;double x=span>0?100*(b->voltage-b->soc_voltage_min)/span:NAN;if(isfinite(x)){if(x<0)x=0;if(x>100)x=100;}pthread_mutex_unlock(&b->lock);return x;}
/* Legacy empirical curve retained for compatibility, not a validated estimator. */
double bms_soh(Bms*b){pthread_mutex_lock(&b->lock);int64_t n=b->cycle_count;pthread_mutex_unlock(&b->lock);if(n<=0)return NAN;double x=(double)n/3.97379;double y=0.000088875*pow((double)n-760.58,2)+pow(log(x),2)+47.917;if(!isfinite(y))return NAN;if(y<0)y=0;if(y>100)y=100;return y;}
double bms_rul(Bms*b){pthread_mutex_lock(&b->lock);double x=b->rated_useful_life-(double)b->cycle_count;pthread_mutex_unlock(&b->lock);return x>0?x:0;}
