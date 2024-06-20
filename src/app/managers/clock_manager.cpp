#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <stdint.h>
#include <time.h>

#include "app/managers/clock_manager.h"
#include "app/tasks.h"
#include "app/widgets/sun_widget.h"
#include "app/widgets/toolbar_widget.h"
#include "config.h"

/**********************************************************
* RTOS
**********************************************************/
#define CLOCK_UPDATE_PERIOD 1000
static time_status status = TIME_STATUS_NO_TIME;

/**********************************************************
* NTP Time
**********************************************************/
#define NTP_SERVER_NAME "pool.ntp.org"
#define NTP_UPDATE_INTERVAL 60000

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER_NAME, 0, NTP_UPDATE_INTERVAL);

bool clock_sync() {
    bool success = true;
    toolbar_clock_status_set(TIME_STATUS_UPDATING);

    timeClient.begin();
    success = timeClient.update();

    if (success) {
        /* UK timezone */
        setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0/2", 1);
        tzset();

        /* Get the current time from the NTP server */
        timeClient.update();
        unsigned long epochTime = timeClient.getEpochTime();

        /* Convert to tm struct */
        struct tm *ptm = gmtime((time_t *)&epochTime);

        /* Set the time on the RTC */
        struct timeval tv;
        tv.tv_sec = epochTime;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
    }

    return success;
}

void clock_update() {
    struct tm timeinfo;

    if (getLocalTime(&timeinfo)) {
        toolbar_clock_time_set(timeinfo);
        toolbar_clock_status_set(TIME_STATUS_UPDATING);
        sun_rise_fall_update(timeinfo);
        sun_cycle_update(timeinfo);
    }
}

void clock_update_task(void *pvParameters) {
    log_i("Clock manager task created");
    while (1) {
        if (WiFi.status() == WL_CONNECTED) {
            switch (status) {
                case TIME_STATUS_TIME_LOCKED:
                    clock_update();
                    break;

                case TIME_STATUS_UPDATING:
                case TIME_STATUS_NO_TIME:
                default:
                    if (clock_sync()) {
                        status = TIME_STATUS_TIME_LOCKED;
                        toolbar_clock_status_set(TIME_STATUS_TIME_LOCKED);
                    } else {
                        status = TIME_STATUS_NO_TIME;
                        toolbar_clock_status_set(TIME_STATUS_NO_TIME);
                    }
            }
        }

        vTaskDelay(CLOCK_UPDATE_PERIOD / portTICK_PERIOD_MS);
    }
}

void clock_init() {
    xTaskCreatePinnedToCore(
        clock_update_task,
        "Clock Manager",
        CLOCK_MANAGER_STACK,
        NULL,
        CLOCK_MANAGER_PRIORITY,
        NULL,
        APP_CORE
    );
}