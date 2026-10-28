#ifndef GPS_STATE_H
#define GPS_STATE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t sequence, battery_mv, wake_reason, boot_count;
} sample_t;
typedef struct {
    uint32_t boot_count, next_sequence;
    bool pending;
    sample_t sample;
} persisted_t;
#endif
