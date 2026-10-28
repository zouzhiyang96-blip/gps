#include "journal.h"
#include "config.h"
#include "port.h"
#include <string.h>

#define MAGIC 0x47505331u
#define VERSION 1u
#define COMMIT 0xC01117EDu
static uint32_t rd(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void wr(uint8_t *p,uint32_t n) {
    unsigned i; for(i=0;i<4;i++)p[i]=(uint8_t)(n>>(8u*i));
}
uint32_t crc32_bytes(const uint8_t *data,unsigned length) {
    uint32_t crc=~0u; unsigned i,b;
    for(i=0;i<length;i++) { crc^=data[i]; for(b=0;b<8;b++)crc=(crc>>1)^((0u-(crc&1u))&0xEDB88320u); }
    return ~crc;
}
static bool blank(const uint8_t *b) {
    unsigned i; for(i=0;i<APP_SNAPSHOT_BYTES;i++)if(b[i]!=0xFFu)return false;
    return true;
}
static bool valid(const uint8_t *b) {
    return rd(b)==MAGIC && rd(b+4)==VERSION && rd(b+20)<=1u &&
        rd(b+40)==crc32_bytes(b,40) && rd(b+56)==COMMIT && rd(b+60)==~COMMIT &&
        rd(b+16)!=0u && (!rd(b+20) || (rd(b+24)>0u && rd(b+24)<rd(b+16)));
}
static bool newer(uint32_t a,uint32_t b) {return a!=b && (uint32_t)(a-b)<0x80000000u;}
journal_result_t journal_load(journal_t *j,persisted_t *s) {
    uint8_t a[APP_SNAPSHOT_BYTES],b[APP_SNAPSHOT_BYTES];const uint8_t *p;
    bool av,bv;
    memset(s,0,sizeof(*s));s->next_sequence=1;j->active=-1;j->generation=0;
    if(!port_storage_read(0,a,sizeof a)||!port_storage_read(1,b,sizeof b))return JOURNAL_IO;
    av=valid(a);bv=valid(b);
    if(!av&&!bv)return blank(a)&&blank(b)?JOURNAL_EMPTY:JOURNAL_CORRUPT;
    j->active=av&&(!bv||newer(rd(a+8),rd(b+8)))?0:1;
    p=j->active==0?a:b;j->generation=rd(p+8);
    s->boot_count=rd(p+12);s->next_sequence=rd(p+16);s->pending=rd(p+20)!=0;
    s->sample.sequence=rd(p+24);s->sample.battery_mv=rd(p+28);
    s->sample.wake_reason=rd(p+32);s->sample.boot_count=rd(p+36);
    return JOURNAL_OK;
}
bool journal_save(journal_t *j,const persisted_t *s) {
    uint8_t b[APP_SNAPSHOT_BYTES],verify[APP_SNAPSHOT_BYTES];unsigned offset;
    unsigned slot=j->active==0?1u:0u;uint32_t generation=j->generation+1u;
    if(s->next_sequence==0u || (s->pending && (s->sample.sequence==0u || s->sample.sequence>=s->next_sequence)))return false;
    memset(b,0xFF,sizeof b);wr(b,MAGIC);wr(b+4,VERSION);wr(b+8,generation);
    wr(b+12,s->boot_count);wr(b+16,s->next_sequence);wr(b+20,s->pending?1u:0u);
    wr(b+24,s->sample.sequence);wr(b+28,s->sample.battery_mv);
    wr(b+32,s->sample.wake_reason);wr(b+36,s->sample.boot_count);
    wr(b+40,crc32_bytes(b,40));wr(b+56,COMMIT);wr(b+60,~COMMIT);
    if(!port_storage_erase(slot))return false;
    /* Program the commit doubleword LAST. The previous slot remains intact. */
    for(offset=0;offset<sizeof b;offset+=8)
        if(!port_storage_write64(slot,offset,b+offset))return false;
    if(!port_storage_read(slot,verify,sizeof verify)||memcmp(b,verify,sizeof b)!=0||!valid(verify))return false;
    j->active=(int)slot;j->generation=generation;return true;
}
