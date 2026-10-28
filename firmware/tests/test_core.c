#include "app.h"
#include "port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char device_id[]="0123456789ABCDEF01234567";
static uint8_t flash_data[2][APP_FLASH_SLOT_BYTES];
static unsigned writes,fail_write,erases,tx_data,power_on_count,power_off_count;
static uint32_t now,battery=3600,scheduled_at,registration_at;
static bool battery_ok=true,powered,expect_payload,have_payload,ack_sent;
static void (*receive_byte)(uint8_t);
static char pending_reply[160],data_frame[160];
enum { HAPPY,NO_ACK,WRONG_ACK,NO_MODEM,BAD_QIRD,DEADLINE };
static int mock_mode;
static unsigned passed;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static void inject(const char *s){for(;*s;s++)receive_byte((uint8_t)*s);}
static void reset(void) {
    memset(flash_data,0xFF,sizeof flash_data);writes=fail_write=erases=tx_data=0;
    power_on_count=power_off_count=0;now=1000;battery=3600;battery_ok=true;
    powered=expect_payload=have_payload=ack_sent=false;mock_mode=HAPPY;
    pending_reply[0]=0;data_frame[0]=0;registration_at=0;
}
bool port_init(void){return true;}
uint32_t port_millis(void){return now;}
void port_idle(void){}
bool port_battery_mv(uint32_t *mv){*mv=battery;return battery_ok;}
void port_device_id(char out[25]){memcpy(out,device_id,25);}
void port_log(const char *s){(void)s;}
void port_modem_rx_handler(void (*fn)(uint8_t)){receive_byte=fn;}
bool port_modem_power(bool on) {
    powered=on;if(on){power_on_count++;expect_payload=have_payload=ack_sent=false;}
    else power_off_count++;
    return true;
}
void port_modem_key(bool asserted){(void)asserted;}
bool port_modem_write(const uint8_t *data,size_t length) {
    char cmd[256],payload[240],header[80],id[25];unsigned long sequence,mv,wake,boots;
    CHECK(length<sizeof cmd);memcpy(cmd,data,length);cmd[length]=0;
    if(mock_mode==NO_MODEM)return true;
    if(expect_payload){
        expect_payload=false;have_payload=true;tx_data++;
        CHECK(length<sizeof data_frame);memcpy(data_frame,cmd,length+1);
        inject("\r\nSEND OK\r\n");return true;
    }
    if(strcmp(cmd,"AT\r\n")==0||strcmp(cmd,"ATE0\r\n")==0)inject("\r\nOK\r\n");
    else if(strcmp(cmd,"AT+CPIN?\r\n")==0)inject("\r\n+CPIN: READY\r\nOK\r\n");
    else if(strcmp(cmd,"AT+CEREG?\r\n")==0){
        if(!registration_at)registration_at=now;
        inject(mock_mode==DEADLINE && (uint32_t)(now-registration_at)<57000u?
            "\r\n+CEREG: 0,2\r\nOK\r\n":"\r\n+CEREG: 0,1\r\nOK\r\n");
    } else if(strncmp(cmd,"AT+QICSGP",9)==0)inject("\r\nOK\r\n");
    else if(strncmp(cmd,"AT+QIACT",8)==0){
        if(mock_mode==DEADLINE){strcpy(pending_reply,"\r\nOK\r\n");scheduled_at=now+44000u;}
        else inject("\r\nOK\r\n");
    } else if(strncmp(cmd,"AT+QIOPEN",9)==0){
        if(mock_mode==DEADLINE){strcpy(pending_reply,"\r\nOK\r\n+QIOPEN: 0,0\r\n");scheduled_at=now+44000u;}
        else inject("\r\n+QIOPEN: 0,0\r\nOK\r\n"); /* URC before OK is legal. */
    } else if(strncmp(cmd,"AT+QISEND",9)==0){expect_payload=true;inject("\r\n> ");}
    else if(strncmp(cmd,"AT+QIRD",7)==0){
        if(mock_mode==BAD_QIRD){inject("\r\n+QIRD: 99999\r\nOK\r\n");return true;}
        if(have_payload&&!ack_sent && mock_mode!=NO_ACK){
            CHECK(sscanf(data_frame,"DATA,%24[^,],%lu,%lu,%lu,%lu",id,&sequence,&mv,&wake,&boots)==5);
            if(mock_mode==WRONG_ACK)sequence++;
            /* AT-looking words inside QIRD payload must never become AT results. */
            snprintf(payload,sizeof payload,"ACK,%s,%lu\nERROR\nOK\nCMD,42,1,\n",id,sequence);
            snprintf(header,sizeof header,"\r\n+QIRD: %u\r\n",(unsigned)strlen(payload));
            inject(header);inject(payload);inject("\r\nOK\r\n");ack_sent=true;
        } else inject("\r\n+QIRD: 0\r\nOK\r\n");
    } else if(strcmp(cmd,"AT+QPOWD=1\r\n")==0)inject("\r\nPOWERED DOWN\r\n");
    else CHECK(0);
    return true;
}
bool port_storage_read(unsigned slot,uint8_t *data,size_t length){CHECK(slot<2);memcpy(data,flash_data[slot],length);return true;}
bool port_storage_erase(unsigned slot){CHECK(slot<2);memset(flash_data[slot],0xFF,APP_FLASH_SLOT_BYTES);erases++;return true;}
bool port_storage_write64(unsigned slot,unsigned offset,const uint8_t data[8]) {
    unsigned i;CHECK(slot<2&&offset%8u==0u&&offset<=APP_FLASH_SLOT_BYTES-8);
    writes++;if(fail_write && writes==fail_write)return false;
    for(i=0;i<8;i++){CHECK((flash_data[slot][offset+i]&data[i])==data[i]);flash_data[slot][offset+i]=data[i];}
    return true;
}
bool port_schedule_wakeup(uint32_t seconds){return seconds>0&&seconds<=65535;}
uint32_t port_deep_sleep(void){return WAKE_RTC;}
static void tick(uint32_t step) {
    now+=step;
    if(pending_reply[0] && (uint32_t)(now-scheduled_at)<0x80000000u){inject(pending_reply);pending_reply[0]=0;}
}
static void run_task(app_t *a) {
    unsigned ticks=0;
    while(!app_done(a)&&ticks++<20000){app_step(a);tick(10);}
    CHECK(app_done(a));CHECK(!powered);CHECK((uint32_t)(now-a->started)<=APP_TASK_MS+100u);
}
static void test_protocol(void) {
    protocol_t p;char frame[200];unsigned i;
    protocol_init(&p,device_id,7);
    snprintf(frame,sizeof frame,"ACK,%s,8\nACK,FFFFFFFFFFFFFFFFFFFFFFFF,7\n",device_id);
    for(i=0;frame[i];i++)protocol_feed(&p,(uint8_t)frame[i]);CHECK(!p.acked);
    for(i=0;i<300;i++)protocol_feed(&p,'X');
    snprintf(frame,sizeof frame,"ACK,%s,7\n",device_id);
    for(i=0;frame[i];i++)protocol_feed(&p,(uint8_t)frame[i]);CHECK(!p.acked);
    for(i=0;frame[i];i++)protocol_feed(&p,(uint8_t)frame[i]);CHECK(p.acked);
    strcpy(frame,"CMD,4294967296,1,\nCMD,1,65536,\nCMD,1,1,\n");
    for(i=0;frame[i];i++)protocol_feed(&p,(uint8_t)frame[i]);CHECK(p.command_count==1);
    CHECK(command_dispatch(&p.commands[0])==COMMAND_READ_ONLY_OK);
    p.commands[0].opcode=65535;CHECK(command_dispatch(&p.commands[0])==COMMAND_UNSUPPORTED);
    passed++;puts("PASS bounded fragmented protocol and command registry");
}
static void test_journal(void) {
    journal_t j,reloaded;persisted_t s,t;uint8_t saved[sizeof flash_data];unsigned cut;
    reset();CHECK(journal_load(&j,&s)==JOURNAL_EMPTY);s.boot_count=1;
    CHECK(journal_save(&j,&s));memcpy(saved,flash_data,sizeof saved);
    for(cut=1;cut<=8;cut++){
        memcpy(flash_data,saved,sizeof saved);writes=0;fail_write=cut;
        CHECK(journal_load(&j,&s)==JOURNAL_OK);s.pending=true;s.sample.sequence=1;s.next_sequence=2;
        CHECK(!journal_save(&j,&s));CHECK(journal_load(&reloaded,&t)==JOURNAL_OK);
        CHECK(!t.pending&&t.next_sequence==1);
    }
    fail_write=0;CHECK(journal_load(&j,&s)==JOURNAL_OK);s.pending=true;s.sample.sequence=1;s.next_sequence=2;
    CHECK(journal_save(&j,&s));CHECK(journal_load(&reloaded,&t)==JOURNAL_OK);CHECK(t.pending&&t.sample.sequence==1);
    flash_data[reloaded.active][28]^=1;CHECK(journal_load(&reloaded,&t)==JOURNAL_OK);CHECK(!t.pending);
    memset(flash_data,0,sizeof flash_data);CHECK(journal_load(&reloaded,&t)==JOURNAL_CORRUPT);
    passed++;puts("PASS journal torn writes at all 8 doublewords and CRC recovery");
}
static void test_success(void) {
    app_t a;journal_t j;persisted_t s;
    reset();CHECK(app_init(&a));app_begin(&a,WAKE_BOOT);run_task(&a);
    CHECK(a.delivered&&!a.state.pending&&a.error==APP_ERROR_NONE);CHECK(tx_data==1);
    CHECK(a.commands_processed==1);CHECK(journal_load(&j,&s)==JOURNAL_OK&&!s.pending&&s.next_sequence==2);
    passed++;puts("PASS one complete DATA / durable ACK / command / shutdown task");
}
static void test_retry(void) {
    app_t a,b;char original[160];
    reset();CHECK(app_init(&a));mock_mode=NO_ACK;app_begin(&a,WAKE_BOOT);run_task(&a);
    CHECK(a.state.pending&&!a.delivered);strcpy(original,data_frame);
    mock_mode=HAPPY;CHECK(app_init(&b));battery=3500;app_begin(&b,WAKE_RTC);run_task(&b);
    CHECK(b.delivered&&!b.state.pending);CHECK(strcmp(original,data_frame)==0);
    passed++;puts("PASS reboot resends the identical pending record before sampling another");
}
static void test_bad_ack(void) {
    app_t a;reset();mock_mode=WRONG_ACK;CHECK(app_init(&a));app_begin(&a,WAKE_BOOT);run_task(&a);
    CHECK(!a.delivered&&a.state.pending);
    passed++;puts("PASS SEND OK and mismatched ACK never erase pending data");
}
static void test_low_battery(void) {
    app_t a;reset();CHECK(app_init(&a));battery=2000;app_begin(&a,WAKE_RTC);
    CHECK(app_done(&a)&&a.error==APP_ERROR_LOW_BATTERY&&power_on_count==0);
    battery_ok=false;app_begin(&a,WAKE_RTC);CHECK(a.error==APP_ERROR_BATTERY&&power_on_count==0);
    passed++;puts("PASS low voltage and ADC failure keep the radio off");
}
static void test_deadlines(void) {
    app_t a;reset();mock_mode=NO_MODEM;CHECK(app_init(&a));app_begin(&a,WAKE_BOOT);run_task(&a);CHECK(a.state.pending);
    reset();mock_mode=DEADLINE;CHECK(app_init(&a));app_begin(&a,WAKE_BOOT);run_task(&a);
    CHECK(a.error==APP_ERROR_DEADLINE&&a.state.pending);
    reset();now=UINT32_MAX-500u;CHECK(app_init(&a));app_begin(&a,WAKE_BOOT);run_task(&a);CHECK(a.delivered);
    passed++;puts("PASS absent modem, shared task deadline, and millisecond counter wrap");
}
static void test_overflow(void) {
    app_t a;unsigned i;reset();CHECK(app_init(&a));app_begin(&a,WAKE_BOOT);
    for(i=0;i<2000;i++)modem_rx_isr('X');run_task(&a);CHECK(a.state.pending&&!a.delivered);
    reset();CHECK(app_init(&a));app_begin(&a,WAKE_BOOT);modem_rx_error_isr();run_task(&a);CHECK(a.state.pending&&!a.delivered);
    reset();mock_mode=BAD_QIRD;CHECK(app_init(&a));app_begin(&a,WAKE_BOOT);run_task(&a);CHECK(a.state.pending);
    passed++;puts("PASS UART overflow and oversized QIRD payload fail within budget");
}
static void test_ack_commit_failure(void) {
    app_t a;journal_t j;persisted_t s;unsigned ticks=0;
    reset();CHECK(app_init(&a));app_begin(&a,WAKE_BOOT);
    while(a.stage!=APP_COMMIT&&ticks++<10000){app_step(&a);tick(10);}
    CHECK(a.stage==APP_COMMIT&&a.delivered);fail_write=writes+8;
    run_task(&a);CHECK(a.state.pending&&a.error==APP_ERROR_STORAGE);
    CHECK(journal_load(&j,&s)==JOURNAL_OK&&s.pending);
    passed++;puts("PASS ACK clear interrupted at commit retains a safely repeatable record");
}
int main(void) {
    test_protocol();test_journal();test_success();test_retry();test_bad_ack();
    test_low_battery();test_deadlines();test_overflow();test_ack_commit_failure();
    printf("%u test groups passed\n",passed);return 0;
}
