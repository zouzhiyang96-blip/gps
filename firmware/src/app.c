#include "app.h"
#include "port.h"
#include <string.h>
bool app_init(app_t *a) {
    journal_result_t result;memset(a,0,sizeof(*a));
    modem_init(&a->modem);port_device_id(a->device);
    result=journal_load(&a->journal,&a->state);
    if(result!=JOURNAL_OK&&result!=JOURNAL_EMPTY){a->error=APP_ERROR_STORAGE;return false;}
    if(a->state.boot_count!=UINT32_MAX)a->state.boot_count++;
    if(!journal_save(&a->journal,&a->state)){a->error=APP_ERROR_STORAGE;return false;}
    a->stage=APP_IDLE;return true;
}
void app_begin(app_t *a,uint32_t wake_reason) {
    uint32_t battery;persisted_t previous;int length;
    a->started=port_millis();a->powered=false;a->delivered=false;
    a->commands_processed=0;a->error=APP_ERROR_NONE;a->stage=APP_DONE;
    if(!port_battery_mv(&battery)){a->error=APP_ERROR_BATTERY;return;}
    if(battery<APP_LOW_BATTERY_MV){a->error=APP_ERROR_LOW_BATTERY;return;}
    if(!a->state.pending){
        if(a->state.next_sequence==UINT32_MAX){a->error=APP_ERROR_SEQUENCE;return;}
        previous=a->state;
        a->state.sample.sequence=a->state.next_sequence++;
        a->state.sample.battery_mv=battery;a->state.sample.wake_reason=wake_reason;
        a->state.sample.boot_count=a->state.boot_count;a->state.pending=true;
        if(!journal_save(&a->journal,&a->state)){a->state=previous;a->error=APP_ERROR_STORAGE;return;}
    }
    length=protocol_encode(a->payload,sizeof a->payload,a->device,&a->state.sample);
    protocol_init(&a->protocol,a->device,a->state.sample.sequence);
    if(length<0||!modem_start(&a->modem,a->payload,(size_t)length,&a->protocol)) {
        a->error=APP_ERROR_CONFIG_OR_POWER;return;
    }
    a->powered=true;a->stage=APP_NETWORK;
}
void app_step(app_t *a) {
    modem_result_t result;unsigned i;
    switch(a->stage) {
    case APP_NETWORK:
        result=modem_poll(&a->modem);
        if(result==MODEM_DELIVERED || result==MODEM_FAILED ||
            (uint32_t)(port_millis()-a->started)>=APP_WORK_MS) {
            a->delivered=a->protocol.acked;
            if(!a->delivered)a->error=result==MODEM_FAILED?APP_ERROR_NETWORK:APP_ERROR_DEADLINE;
            a->stage=APP_COMMANDS;
        }break;
    case APP_COMMANDS:
        /* Hooks are read-only in this version. No remote result uplink yet. */
        if(a->delivered)for(i=0;i<a->protocol.command_count;i++) {
            command_result_t r=command_dispatch(&a->protocol.commands[i]);
            port_log(r==COMMAND_READ_ONLY_OK?"command read-only OK":"command unsupported");
            a->commands_processed++;
        }
        a->stage=APP_COMMIT;break;
    case APP_COMMIT:
        if(a->delivered){
            a->state.pending=false;
            if(!journal_save(&a->journal,&a->state)) {a->state.pending=true;a->error=APP_ERROR_STORAGE;}
        }
        if(a->powered){modem_stop_begin(&a->modem);a->stage=APP_SHUTDOWN;}
        else a->stage=APP_DONE;
        break;
    case APP_SHUTDOWN:
        if(modem_stop_poll(&a->modem))a->stage=APP_DONE;
        else if((uint32_t)(port_millis()-a->started)>=APP_TASK_MS) {
            (void)port_modem_power(false);a->stage=APP_DONE;
        }
        break;
    default:break;
    }
}
bool app_done(const app_t *a){return a->stage==APP_DONE;}
