#include "app.h"
#include "port.h"
static app_t app;
int main(void) {
    uint32_t reason=WAKE_BOOT;
    if(!port_init())for(;;)port_idle();
    if(!app_init(&app)) {
        port_log("persistent state unavailable: uplink disabled");
        for(;;){if(!port_schedule_wakeup(APP_INTERVAL_SECONDS))for(;;)port_idle();(void)port_deep_sleep();}
    }
    for(;;) {
        app_begin(&app,reason);
        while(!app_done(&app)){app_step(&app);port_idle();}
        port_log(app.error==APP_ERROR_NONE?"task finished":"task failed or deferred; pending preserved");
        if(!port_schedule_wakeup(APP_INTERVAL_SECONDS))for(;;)port_idle();
        reason=port_deep_sleep();
    }
}
