#ifndef GPS_APP_H
#define GPS_APP_H
#include "journal.h"
#include "modem.h"
typedef enum { APP_IDLE,APP_NETWORK,APP_COMMANDS,APP_COMMIT,APP_SHUTDOWN,APP_DONE } app_stage_t;
typedef enum { APP_ERROR_NONE,APP_ERROR_STORAGE,APP_ERROR_BATTERY,APP_ERROR_LOW_BATTERY,
    APP_ERROR_CONFIG_OR_POWER,APP_ERROR_NETWORK,APP_ERROR_DEADLINE,APP_ERROR_SEQUENCE } app_error_t;
typedef struct {
    journal_t journal;persisted_t state;modem_t modem;protocol_t protocol;
    app_stage_t stage;app_error_t error;bool powered,delivered;
    uint32_t started;char device[25],payload[APP_FRAME_MAX];
    unsigned commands_processed;
} app_t;
bool app_init(app_t *app);
void app_begin(app_t *app,uint32_t wake_reason);
void app_step(app_t *app);
bool app_done(const app_t *app);
#endif
