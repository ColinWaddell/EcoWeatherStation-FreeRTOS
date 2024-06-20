#include "app/managers/wifi_manager.h"
#include "app/credentials.h"
#include "app/tasks.h"
#include "app/widgets/toolbar_widget.h"
#include "config.h"
#include <WiFi.h>

const char *ssid = WIFI_SSID;
const char *password = WIFI_PWD;

static wifi_status status;
#define WIFI_STATUS_DELAY 100
#define WIFI_WAIT_SEC 15 * (1000 / WIFI_STATUS_DELAY)
#define WIFI_UPDATE_PERIOD 15000

#define WIFI_FONT_FAMILY 1
#define WIFI_FONT_SIZE 2

void wifi_connect() {
    bool state = true;
    int i = WIFI_WAIT_SEC;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    log_i("Connecting");
    toolbar_message_set("Connecting...");

    /* Wait for connection */
    status = WIFI_STATUS_CONNECTING;
    toolbar_wifi_status_set(WIFI_STATUS_CONNECTING);
    while (WiFi.status() != WL_CONNECTED) {
        delay(WIFI_STATUS_DELAY);
        if (!i--) {
            state = false;
            break;
        }
    }

    if (state) {
        toolbar_message_set("");
        log_i("Connected");
        status = WIFI_STATUS_CONNECTED;
        toolbar_wifi_status_set(WIFI_STATUS_CONNECTED);
    } else {
        toolbar_message_set("Connection Failed");
        status = WIFI_STATUS_FAILED;
        toolbar_wifi_status_set(WIFI_STATUS_FAILED);
    }
}

void wifi_update_task(void *pvParameters) {
    log_i("Wifi Task created");
    while (1) {
        switch (status) {
            case WIFI_STATUS_UNCONNECTED:
                wifi_connect();
                break;

            default:
                if (WiFi.status() != WL_CONNECTED) {
                    status = WIFI_STATUS_UNCONNECTED;
                }
                toolbar_wifi_status_set(status);
        }
        vTaskDelay(WIFI_UPDATE_PERIOD / portTICK_PERIOD_MS);
    }
}

void wifi_init() {
    xTaskCreatePinnedToCore(
        wifi_update_task,
        "Wifi Manager",
        WIFI_MANAGER_STACK,
        NULL,
        WIFI_MANAGER_PRIORITY,
        NULL,
        APP_CORE
    );
}