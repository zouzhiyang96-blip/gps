#include "modem.h"
#include "port.h"
#include <stdio.h>
#include <string.h>

enum { OFF,POWER_WAIT,KEY_WAIT,BOOT,ECHO,SIM,REG,REG_DELAY,APN,ACTIVATE,
       OPEN,SEND_PROMPT,SEND_WAIT,READ,RX_DELAY,DONE,FAILED,STOP,STOPPED };
#define RX_CAPACITY 1024u
static uint8_t rx[RX_CAPACITY];
static volatile unsigned head,tail,overflow;
#if defined(__GNUC__)
#define BARRIER() __asm__ volatile ("" ::: "memory")
#else
#define BARRIER() ((void)0)
#endif
static bool elapsed(uint32_t now,uint32_t at,uint32_t duration){return (uint32_t)(now-at)>=duration;}
static bool due(uint32_t now,uint32_t deadline){return (uint32_t)(now-deadline)<0x80000000u;}
void modem_rx_isr(uint8_t byte) {
    unsigned next=(head+1u)%RX_CAPACITY;
    if(next==tail){overflow=1;return;}
    rx[head]=byte;BARRIER();head=next;
}
void modem_rx_error_isr(void){overflow=1;}
void modem_init(modem_t *m) {
    memset(m,0,sizeof(*m));head=tail=overflow=0;port_modem_rx_handler(modem_rx_isr);
}
static bool text_safe(const char *s,bool host) {
    unsigned n=0;
    for(;*s;s++,n++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||
        (*s>='0'&&*s<='9')||*s=='.'||*s=='-'||(!host&&*s=='_')))return false;
    return n>0u && n<=(host?80u:63u);
}
static void command(modem_t *m,const char *s,unsigned stage,uint32_t timeout) {
    m->stage=stage;m->deadline=port_millis()+timeout;m->ok=false;m->error=false;
    if(!port_modem_write((const uint8_t *)s,strlen(s)))m->error=true;
}
static void line(modem_t *m) {
    unsigned id,n;int status,mode;
    if(strcmp(m->line,"OK")==0)m->ok=true;
    else if(strcmp(m->line,"ERROR")==0 || strncmp(m->line,"+CME ERROR",10)==0 ||
            strcmp(m->line,"SEND FAIL")==0)m->error=true;
    else if(strcmp(m->line,"SEND OK")==0)m->send_ok=true;
    else if(strcmp(m->line,"+CPIN: READY")==0)m->sim_ready=true;
    else if(strncmp(m->line,"+CEREG:",7)==0) {
        if(sscanf(m->line,"+CEREG: %d,%d",&mode,&status)==2)m->registered=status==1||status==5;
    } else if(sscanf(m->line,"+QIOPEN: %u,%u",&id,&n)==2 && id==0u) {
        m->opened=n==0u;m->open_failed=n!=0u;
    } else if(strncmp(m->line,"+QIRD:",6)==0 && m->stage==READ) {
        char extra;
        if(sscanf(m->line,"+QIRD: %u %c",&n,&extra)!=1 || n>512u)m->error=true;
        else m->raw_remaining=n;
    } else if(strcmp(m->line,"POWERED DOWN")==0||strcmp(m->line,"NORMAL POWER DOWN")==0)m->powered_down=true;
    else if(strncmp(m->line,"+QIURC: \"closed\",0",18)==0 && m->stage!=STOP)m->error=true;
}
static void pump(modem_t *m) {
    unsigned count=0;
    while(tail!=head && count++<RX_CAPACITY) {
        uint8_t b=rx[tail];BARRIER();tail=(tail+1u)%RX_CAPACITY;
        if(m->raw_remaining) {protocol_feed(m->protocol,b);m->raw_remaining--;continue;}
        if(m->stage==SEND_PROMPT && b=='>') {m->prompt=true;m->line_used=0;continue;}
        if(b=='\n') {
            while(m->line_used && m->line[m->line_used-1]=='\r')m->line_used--;
            m->line[m->line_used]=0;if(m->line_used)line(m);m->line_used=0;
        } else if(m->line_used<sizeof m->line-1u) m->line[m->line_used++]=(char)b;
        else m->error=true;
    }
    if(overflow)m->error=true;
}
bool modem_start(modem_t *m,const char *payload,size_t length,protocol_t *protocol) {
    modem_init(m);
    if(!text_safe(APP_APN,false)||!text_safe(APP_SERVER_HOST,true)||strcmp(APP_SERVER_HOST,"192.0.2.1")==0||
        APP_SERVER_PORT<1 || APP_SERVER_PORT>65535 || length==0u || length>=APP_FRAME_MAX)return false;
    m->payload=payload;m->payload_length=length;m->protocol=protocol;
    if(!port_modem_power(true)){(void)port_modem_power(false);return false;}
    port_modem_key(false);m->at=port_millis();m->stage=POWER_WAIT;return true;
}
modem_result_t modem_poll(modem_t *m) {
    char cmd[160];uint32_t now=port_millis();pump(m);
    if(m->protocol && m->protocol->acked && !m->ack_seen){m->ack_seen=true;m->ack_at=now;}
    if(m->stage==FAILED)return MODEM_FAILED;
    if(m->stage==DONE)return MODEM_DELIVERED;
    if(m->error && m->stage!=BOOT){m->stage=FAILED;return MODEM_FAILED;}
    switch(m->stage) {
    case POWER_WAIT:
        if(elapsed(now,m->at,2100)){port_modem_key(true);m->at=now;m->stage=KEY_WAIT;}break;
    case KEY_WAIT:
        if(elapsed(now,m->at,2100)){port_modem_key(false);m->start=now;command(m,"AT\r\n",BOOT,1000);}break;
    case BOOT:
        if(m->ok)command(m,"ATE0\r\n",ECHO,2000);
        else if(elapsed(now,m->start,20000))m->stage=FAILED;
        else if(m->error||due(now,m->deadline))command(m,"AT\r\n",BOOT,1000);
        break;
    case ECHO:if(m->ok)command(m,"AT+CPIN?\r\n",SIM,5000);break;
    case SIM:
        if(m->ok){if(!m->sim_ready)m->stage=FAILED;else {m->start=now;command(m,"AT+CEREG?\r\n",REG,5000);}}break;
    case REG:
        if(m->ok){
            if(m->registered){
                snprintf(cmd,sizeof cmd,"AT+QICSGP=1,1,\"%s\",\"\",\"\",1\r\n",APP_APN);command(m,cmd,APN,5000);
            } else {m->at=now;m->stage=REG_DELAY;}
        }
        if(elapsed(now,m->start,60000))m->stage=FAILED;
        break;
    case REG_DELAY:
        if(elapsed(now,m->start,60000))m->stage=FAILED;
        else if(elapsed(now,m->at,2000))command(m,"AT+CEREG?\r\n",REG,5000);
        break;
    case APN:if(m->ok)command(m,"AT+QIACT=1\r\n",ACTIVATE,45000);break;
    case ACTIVATE:
        if(m->ok){
            snprintf(cmd,sizeof cmd,"AT+QIOPEN=1,0,\"TCP\",\"%s\",%u,0,0\r\n",APP_SERVER_HOST,(unsigned)APP_SERVER_PORT);
            command(m,cmd,OPEN,45000);
        }break;
    case OPEN:
        if(m->open_failed)m->stage=FAILED;
        else if(m->ok&&m->opened){
            snprintf(cmd,sizeof cmd,"AT+QISEND=0,%u\r\n",(unsigned)m->payload_length);
            m->prompt=false;command(m,cmd,SEND_PROMPT,5000);
        }break;
    case SEND_PROMPT:
        if(m->prompt){m->stage=SEND_WAIT;m->deadline=now+10000u;m->send_ok=false;
            if(!port_modem_write((const uint8_t *)m->payload,m->payload_length))m->stage=FAILED;}
        break;
    case SEND_WAIT:
        if(m->send_ok){m->start=now;command(m,"AT+QIRD=0,512\r\n",READ,3000);}break;
    case READ:
        if(m->ok && !m->raw_remaining){m->at=now;m->stage=RX_DELAY;}
        if(elapsed(now,m->start,12000)&&!m->ack_seen)m->stage=FAILED;
        break;
    case RX_DELAY:
        if(m->ack_seen&&elapsed(now,m->ack_at,APP_RX_WINDOW_MS))m->stage=DONE;
        else if(elapsed(now,m->start,12000)&&!m->ack_seen)m->stage=FAILED;
        else if(elapsed(now,m->at,200))command(m,"AT+QIRD=0,512\r\n",READ,3000);
        break;
    default:break;
    }
    if(m->stage>=ECHO && m->stage<=READ && m->stage!=REG_DELAY && due(now,m->deadline))m->stage=FAILED;
    return m->stage==FAILED?MODEM_FAILED:m->stage==DONE?MODEM_DELIVERED:MODEM_WAIT;
}
void modem_stop_begin(modem_t *m) {
    m->at=port_millis();command(m,"AT+QPOWD=1\r\n",STOP,APP_SHUTDOWN_MS);
}
bool modem_stop_poll(modem_t *m) {
    pump(m);
    if(m->powered_down || elapsed(port_millis(),m->at,APP_SHUTDOWN_MS)) {
        (void)port_modem_power(false);m->stage=STOPPED;return true;
    }
    return false;
}
