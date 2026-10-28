#ifndef GPS_PROTOCOL_H
#define GPS_PROTOCOL_H
#include "config.h"
#include "state.h"
#include "commands.h"
#include <stddef.h>
typedef struct {
    char line[APP_FRAME_MAX];unsigned used;bool discard,acked;
    char device[25];uint32_t sequence;
    command_t commands[APP_COMMAND_MAX];unsigned command_count,invalid,dropped;
} protocol_t;
void protocol_init(protocol_t *p,const char device[25],uint32_t sequence);
void protocol_feed(protocol_t *p,uint8_t byte);
int protocol_encode(char *out,size_t size,const char device[25],const sample_t *sample);
#endif
