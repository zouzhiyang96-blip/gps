#ifndef GPS_PORT_H
#define GPS_PORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { WAKE_BOOT=1u, WAKE_RTC=2u, WAKE_SENSOR=4u };
bool port_init(void);
uint32_t port_millis(void);
void port_idle(void);
bool port_battery_mv(uint32_t *mv);
void port_device_id(char out[25]);
void port_log(const char *message);
void port_modem_rx_handler(void (*receive)(uint8_t));
bool port_modem_power(bool on);
void port_modem_key(bool asserted);
bool port_modem_write(const uint8_t *data,size_t length);
bool port_storage_read(unsigned slot,uint8_t *data,size_t length);
bool port_storage_erase(unsigned slot);
bool port_storage_write64(unsigned slot,unsigned offset,const uint8_t data[8]);
bool port_schedule_wakeup(uint32_t seconds);
uint32_t port_deep_sleep(void);
#endif
