#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static bool number(const char *s,uint32_t *n) {
    uint32_t value=0;unsigned digit;
    if(!*s)return false;
    for(;*s;s++) { if(*s<'0'||*s>'9')return false;digit=(unsigned)(*s-'0');
        if(value>(UINT32_MAX-digit)/10u)return false;
        value=value*10u+digit; }
    *n=value;return true;
}
void protocol_init(protocol_t *p,const char device[25],uint32_t sequence) {
    memset(p,0,sizeof(*p));memcpy(p->device,device,25);p->sequence=sequence;
}
static void parse(protocol_t *p) {
    char *first,*second,*third;uint32_t n,opcode;command_t *c;
    if(strncmp(p->line,"ACK,",4)==0) {
        first=strchr(p->line+4,',');
        if(!first){p->invalid++;return;}*first++=0;
        if(strcmp(p->line+4,p->device)==0 && number(first,&n) && n==p->sequence)p->acked=true;
        else p->invalid++;
        return;
    }
    if(strncmp(p->line,"CMD,",4)!=0){p->invalid++;return;}
    first=p->line+4;second=strchr(first,',');if(!second){p->invalid++;return;}*second++=0;
    third=strchr(second,',');if(!third){p->invalid++;return;}*third++=0;
    if(!number(first,&n)||!number(second,&opcode)||opcode>65535u||strlen(third)>48u){p->invalid++;return;}
    if(p->command_count==APP_COMMAND_MAX){p->dropped++;return;}
    c=&p->commands[p->command_count++];c->id=n;c->opcode=(uint16_t)opcode;
    memcpy(c->argument,third,strlen(third)+1);
}
void protocol_feed(protocol_t *p,uint8_t byte) {
    if(byte=='\n') {
        if(!p->discard && p->used) {if(p->line[p->used-1]=='\r')p->used--;p->line[p->used]=0;parse(p);}
        p->used=0;p->discard=false;return;
    }
    if(p->discard)return;
    if(byte==0u||(byte<0x20u && byte!='\r')||byte>0x7Eu||p->used>=sizeof p->line-1u) {
        p->discard=true;p->invalid++;return;
    }
    p->line[p->used++]=(char)byte;
}
int protocol_encode(char *out,size_t size,const char device[25],const sample_t *s) {
    int n=snprintf(out,size,"DATA,%s,%lu,%lu,%lu,%lu\n",device,(unsigned long)s->sequence,
        (unsigned long)s->battery_mv,(unsigned long)s->wake_reason,(unsigned long)s->boot_count);
    return n>0 && (size_t)n<size?n:-1;
}
