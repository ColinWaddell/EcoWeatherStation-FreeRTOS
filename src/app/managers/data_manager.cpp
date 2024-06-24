#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app/tasks.h"
#include "config.h"

#include "app/widgets/atmosphere_widget.h"
#include "app/widgets/rain_widget.h"
#include "app/widgets/sun_widget.h"
#include "app/widgets/temperature_widget.h"
#include "app/widgets/toolbar_widget.h"
#include "app/widgets/wind_widget.h"

#include "app/managers/data_manager.h"

/**********************************************************
* RTOS
**********************************************************/
static data_status status = DATA_STATUS_EMPTY;

const char *serverUrl = "http://10.0.0.24/get_livedata_info";
#define DATA_FONT_FAMILY 1
#define CATEGORY_FONT_SIZE 3
#define TITLE_FONT_SIZE 2

data_packet data_parse(JsonDocument doc) {
    data_packet packet = { 0 };

    /* Parse data */
    try {
        packet.temperature.currently = (float)std::stof((const char *)doc["common_list"][0]["val"]);
        packet.temperature.feelslike = (float)std::stof((const char *)doc["common_list"][2]["val"]);
        packet.sun.solar = (float)std::stof((const char *)doc["common_list"][7]["val"]);
        packet.sun.uv_level = (int16_t)std::stoi((const char *)doc["common_list"][8]["val"]);
        packet.wind.direction = (int16_t)std::stoi((const char *)doc["common_list"][9]["val"]);
        packet.wind.speed = (float)std::stof((const char *)doc["common_list"][4]["val"]);
        packet.wind.gust = (float)std::stof((const char *)doc["common_list"][5]["val"]);
        packet.rain.rate = (float)std::stof((const char *)doc["rain"][1]["val"]);
        packet.rain.daily = (float)std::stof((const char *)doc["rain"][0]["val"]);
        packet.atmosphere.humidity = (float)std::stof((const char *)doc["common_list"][1]["val"]);
        packet.atmosphere.dew_point = (float)std::stof((const char *)doc["common_list"][3]["val"]);
    } catch (const std::invalid_argument &e) {
        /* ignore */
    } catch (const std::out_of_range &e) {
        /* ignore */
    }

    return packet;
}

void data_update() {
    data_packet packet;
    status = DATA_STATUS_EMPTY;
    toolbar_data_status_set(DATA_STATUS_EMPTY);

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        status = DATA_STATUS_SEARCHING;
        toolbar_data_status_set(DATA_STATUS_SEARCHING);
        http.setConnectTimeout(10000);
        http.setConnectTimeout(10000);
        http.begin(serverUrl);

        int httpCode = http.GET();

        if (httpCode > 0) {
            // File found at server
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();

                // Allocate a temporary JsonDocument
                // Use arduinojson.org/v6/assistant to compute the capacity.
                JsonDocument doc;

                // Deserialize the JSON document
                DeserializationError error = deserializeJson(doc, payload);

                // Test if parsing succeeds
                if (error) {
                    log_e("deserializeJson() failed:\n\r%s", error.f_str());
                    status = DATA_STATUS_DECODER_FAILURE;
                    toolbar_data_status_set(DATA_STATUS_DECODER_FAILURE);
                    return;
                }

                /* PUSH DATA */
                log_d("Ready to update display");

                packet = data_parse(doc);
                temperature_update(packet.temperature);
                rain_update(packet.rain);
                sun_update(packet.sun);
                wind_update(packet.wind);
                atmosphere_update(packet.atmosphere);
            }
        } else {
            log_e("HTTP GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
            status = DATA_STATUS_DOWNLOAD_FAILURE;
            toolbar_data_status_set(DATA_STATUS_DOWNLOAD_FAILURE);
        }

        http.end();
    }
}

void data_update_task(void *pvParameters) {
    log_i("Data manager task created");
    while (1) {
        data_update();
        vTaskDelay(DATA_UPDATE_PERIOD / portTICK_PERIOD_MS);
    }
}

void data_init() {
    xTaskCreatePinnedToCore(
        data_update_task,
        "Data Manager",
        DATA_MANAGER_STACK,
        NULL,
        DATA_MANAGER_PRIORITY,
        NULL,
        APP_CORE
    );
}