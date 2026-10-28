#ifndef GPS_CONFIG_H
#define GPS_CONFIG_H
/* Override using -include config/local_config.h. Do not commit credentials. */
#ifndef APP_APN
#define APP_APN ""
#endif
#ifndef APP_SERVER_HOST
#define APP_SERVER_HOST "192.0.2.1" /* RFC 5737 placeholder: change before flashing. */
#endif
#ifndef APP_SERVER_PORT
#define APP_SERVER_PORT 9000
#endif
#ifndef APP_DEBUG_UART
#define APP_DEBUG_UART 0
#endif
#ifndef APP_INTERVAL_SECONDS
#define APP_INTERVAL_SECONDS 3600u
#endif
#ifndef APP_LOW_BATTERY_MV
#define APP_LOW_BATTERY_MV 2800u /* Example only: calibrate for the actual supply. */
#endif
#define APP_TASK_MS 180000u
#define APP_SHUTDOWN_MS 30000u
#define APP_WORK_MS (APP_TASK_MS - APP_SHUTDOWN_MS)
#define APP_RX_WINDOW_MS 2000u
#define APP_FRAME_MAX 160u
#define APP_COMMAND_MAX 4u
#define APP_FLASH_SLOT_BYTES 2048u
#define APP_SNAPSHOT_BYTES 64u
#define APP_STATE_A_ADDRESS 0x080FF000u
#define APP_STATE_B_ADDRESS 0x080FF800u
#define APP_FLASH_COMMAND_BASE 0x080C0000u
#if APP_INTERVAL_SECONDS < 1 || APP_INTERVAL_SECONDS > 65535
#error "RTC interval must be between 1 and 65535 seconds"
#endif
#endif
