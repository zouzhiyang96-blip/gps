#ifndef GPS_MODEM_H
#define GPS_MODEM_H
#include "protocol.h"
typedef enum { MODEM_WAIT, MODEM_DELIVERED, MODEM_FAILED } modem_result_t;
typedef struct {
    unsigned stage;uint32_t at,deadline,poll_at,start,ack_at;
    bool ok,error,prompt,registered,sim_ready,opened,open_failed,send_ok,powered_down,ack_seen;
    unsigned line_used,raw_remaining;char line[192];
    const char *payload;size_t payload_length;protocol_t *protocol;
} modem_t;
void modem_init(modem_t *m);
bool modem_start(modem_t *m,const char *payload,size_t length,protocol_t *protocol);
modem_result_t modem_poll(modem_t *m);
void modem_stop_begin(modem_t *m);
bool modem_stop_poll(modem_t *m);
void modem_rx_isr(uint8_t byte);
void modem_rx_error_isr(void);
#endif
