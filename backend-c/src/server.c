#define _POSIX_C_SOURCE 200809L
#include "bms.h"
#include <microhttpd.h>
#include <cjson/cJSON.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define BODY_LIMIT 8192
static Bms g_bms;static volatile sig_atomic_t running=1;
static const char*api_token,*device_token,*cors_origin;
typedef struct{char body[BODY_LIMIT+1];size_t used;}Request;
static double mono_now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(double)t.tv_sec+(double)t.tv_nsec/1e9;}
static void stop_signal(int s){(void)s;running=0;}
static bool token_ok(const char*p,const char*e){if(!e||!*e)return true;if(!p)return false;size_t a=strlen(p),b=strlen(e);if(a!=b)return false;unsigned char d=0;for(size_t i=0;i<a;i++)d|=(unsigned char)(p[i]^e[i]);return d==0;}
static bool authorized(struct MHD_Connection*c,const char*expected){if(!expected||!*expected)return true;const char*h=MHD_lookup_connection_value(c,MHD_HEADER_KIND,"Authorization");return h&&!strncmp(h,"Bearer ",7)&&token_ok(h+7,expected);}
static enum MHD_Result response(struct MHD_Connection*c,unsigned status,const char*body){
 struct MHD_Response*r=MHD_create_response_from_buffer(strlen(body),(void*)body,MHD_RESPMEM_MUST_COPY);if(!r)return MHD_NO;
 MHD_add_response_header(r,"Content-Type","application/json; charset=utf-8");MHD_add_response_header(r,"Cache-Control","no-store");MHD_add_response_header(r,"X-Content-Type-Options","nosniff");
 MHD_add_response_header(r,"Access-Control-Allow-Origin",cors_origin?cors_origin:"*");MHD_add_response_header(r,"Access-Control-Allow-Headers","Content-Type, Authorization");MHD_add_response_header(r,"Access-Control-Allow-Methods","GET, POST, OPTIONS");
 enum MHD_Result x=MHD_queue_response(c,status,r);MHD_destroy_response(r);return x;
}
static enum MHD_Result json_response(struct MHD_Connection*c,unsigned status,cJSON*j){char*s=cJSON_PrintUnformatted(j);if(!s)return response(c,500,"{\"error\":\"serialization_failed\"}");enum MHD_Result x=response(c,status,s);free(s);return x;}
static cJSON*obj(void){return cJSON_CreateObject();}
static void add_state(cJSON*j){pthread_mutex_lock(&g_bms.lock);cJSON_AddStringToObject(j,"state",bms_state_name(g_bms.state));pthread_mutex_unlock(&g_bms.lock);}
static bool number(cJSON*j,const char*k,double*out){cJSON*x=cJSON_GetObjectItemCaseSensitive(j,k);if(!cJSON_IsNumber(x)||!isfinite(x->valuedouble))return false;*out=x->valuedouble;return true;}
static enum MHD_Result handle(struct MHD_Connection*c,const char*url,const char*method,const char*body){
 if(!strcmp(method,MHD_HTTP_METHOD_OPTIONS))return response(c,204,"");
 if(!strcmp(url,"/")||!strcmp(url,"/health")){cJSON*j=obj();cJSON_AddStringToObject(j,"message","connected");cJSON_AddStringToObject(j,"implementation","c");cJSON_AddBoolToObject(j,"control_enabled",g_bms.control_enabled);enum MHD_Result x=json_response(c,200,j);cJSON_Delete(j);return x;}
 if(!strcmp(method,MHD_HTTP_METHOD_GET)){
  cJSON*j=obj();
  if(!strcmp(url,"/data")){pthread_mutex_lock(&g_bms.lock);cJSON_AddNumberToObject(j,"voltage",g_bms.voltage);cJSON_AddNumberToObject(j,"current",g_bms.current);cJSON_AddNumberToObject(j,"temperature",g_bms.temperature);cJSON_AddNumberToObject(j,"time",g_bms.last_seen);cJSON_AddBoolToObject(j,"hardware_connection",g_bms.hardware_connected);pthread_mutex_unlock(&g_bms.lock);}
  else if(!strcmp(url,"/state")||!strcmp(url,"/state-hardware")){pthread_mutex_lock(&g_bms.lock);cJSON_AddStringToObject(j,"state",bms_state_name(g_bms.state));if(!strcmp(url,"/state"))cJSON_AddStringToObject(j,"warning_messages",g_bms.warning);pthread_mutex_unlock(&g_bms.lock);}
  else if(!strcmp(url,"/hardware_connection")){pthread_mutex_lock(&g_bms.lock);cJSON_AddBoolToObject(j,"hardware_connection",g_bms.hardware_connected);pthread_mutex_unlock(&g_bms.lock);}
  else if(!strcmp(url,"/s_o_c")){double x=bms_soc(&g_bms);if(isfinite(x))cJSON_AddNumberToObject(j,"s_o_c",x);else cJSON_AddNullToObject(j,"s_o_c");}
  else if(!strcmp(url,"/s_o_h")){double x=bms_soh(&g_bms);if(isfinite(x))cJSON_AddNumberToObject(j,"s_o_h",x);else cJSON_AddNullToObject(j,"s_o_h");}
  else if(!strcmp(url,"/r_u_l"))cJSON_AddNumberToObject(j,"r_u_l",bms_rul(&g_bms));
  else{cJSON_Delete(j);return response(c,404,"{\"error\":\"not_found\"}");}
  enum MHD_Result x=json_response(c,200,j);cJSON_Delete(j);return x;
 }
 if(strcmp(method,MHD_HTTP_METHOD_POST))return response(c,405,"{\"error\":\"method_not_allowed\"}");
 if(!strcmp(url,"/update")){
  if(!authorized(c,device_token))return response(c,401,"{\"error\":\"unauthorized_device\"}");
  cJSON*j=cJSON_Parse(body?body:"");double v,a,t;bool ok=j&&number(j,"voltage",&v)&&number(j,"current",&a)&&number(j,"temperature",&t);cJSON_Delete(j);
  if(!ok)return response(c,400,"{\"error\":\"invalid_telemetry\"}");
  if(bms_update_telemetry(&g_bms,v,a,t,mono_now())!=BMS_OK)return response(c,400,"{\"error\":\"telemetry_out_of_range\"}");
  return response(c,200,"{\"message\":\"Battery data updated\"}");
 }
 if(!authorized(c,api_token))return response(c,401,"{\"error\":\"unauthorized\"}");
 BmsResult result;const char*msg;
 if(!strcmp(url,"/charge")){result=bms_start_charging(&g_bms);msg="Charging started";}
 else if(!strcmp(url,"/discharge")){result=bms_start_discharging(&g_bms);msg="Discharging started";}
 else if(!strcmp(url,"/stop")){result=bms_stop(&g_bms);msg="Battery stopped";}
 else if(!strcmp(url,"/cycles")){const char*n=MHD_lookup_connection_value(c,MHD_GET_ARGUMENT_KIND,"count");if(!n||!*n)return response(c,400,"{\"error\":\"count_required\"}");errno=0;char*e=NULL;long long v=strtoll(n,&e,10);if(errno||!e||*e||v<0||v>1000000000LL)return response(c,400,"{\"error\":\"invalid_cycle_count\"}");result=bms_set_cycles(&g_bms,(int64_t)v);msg="Cycle count updated";}
 else return response(c,404,"{\"error\":\"not_found\"}");
 if(result!=BMS_OK){unsigned s=result==BMS_ERR_DISABLED?503:(result==BMS_ERR_INVALID?400:409);const char*e=result==BMS_ERR_DISABLED?"control_not_commissioned":result==BMS_ERR_UNSAFE?"safety_interlock":result==BMS_ERR_TRANSITION?"invalid_state_transition":"invalid_request";cJSON*j=obj();cJSON_AddStringToObject(j,"error",e);add_state(j);enum MHD_Result x=json_response(c,s,j);cJSON_Delete(j);return x;}
 cJSON*j=obj();add_state(j);cJSON_AddStringToObject(j,"message",msg);enum MHD_Result x=json_response(c,200,j);cJSON_Delete(j);return x;
}
static enum MHD_Result access_cb(void*cls,struct MHD_Connection*c,const char*url,const char*method,const char*ver,const char*data,size_t*size,void**ctx){
 (void)cls;(void)ver;Request*r=*ctx;if(!r){r=calloc(1,sizeof*r);if(!r)return MHD_NO;*ctx=r;return MHD_YES;}
 if(*size){if(r->used+*size>BODY_LIMIT)return response(c,413,"{\"error\":\"body_too_large\"}");memcpy(r->body+r->used,data,*size);r->used+=*size;r->body[r->used]='\0';*size=0;return MHD_YES;}
 return handle(c,url,method,r->body);
}
static void completed(void*cls,struct MHD_Connection*c,void**ctx,enum MHD_RequestTerminationCode code){(void)cls;(void)c;(void)code;free(*ctx);*ctx=NULL;}
static void*safety_loop(void*unused){(void)unused;struct timespec d={0,250000000L};while(running){bms_tick(&g_bms,mono_now());nanosleep(&d,NULL);}return NULL;}
int main(void){
 signal(SIGINT,stop_signal);signal(SIGTERM,stop_signal);api_token=getenv("BMS_API_TOKEN");device_token=getenv("BMS_DEVICE_TOKEN");cors_origin=getenv("BMS_CORS_ORIGIN");bms_init(&g_bms);
 unsigned port=8080;const char*p=getenv("PORT");if(p&&*p){char*e=NULL;unsigned long n=strtoul(p,&e,10);if(e&&!*e&&n>0&&n<=65535)port=(unsigned)n;}
 pthread_t worker;if(pthread_create(&worker,NULL,safety_loop,NULL)){bms_destroy(&g_bms);return 1;}
 struct MHD_Daemon*d=MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD,(uint16_t)port,NULL,NULL,&access_cb,NULL,MHD_OPTION_NOTIFY_COMPLETED,&completed,NULL,MHD_OPTION_CONNECTION_TIMEOUT,(unsigned int)5,MHD_OPTION_THREAD_POOL_SIZE,(unsigned int)4,MHD_OPTION_END);
 if(!d){fprintf(stderr,"HTTP server failed on port %u\n",port);running=0;pthread_join(worker,NULL);bms_destroy(&g_bms);return 1;}
 fprintf(stdout,"BMS C backend :%u; control %s\n",port,g_bms.control_enabled?"enabled":"DISABLED - not commissioned");
 while(running){struct timespec dly={0,200000000L};nanosleep(&dly,NULL);}
 MHD_stop_daemon(d);pthread_join(worker,NULL);bms_destroy(&g_bms);return 0;
}
