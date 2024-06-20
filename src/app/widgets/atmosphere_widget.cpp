#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "app/colours.h"
#include "app/fonts/custom_fonts.h"
#include "app/helpers.h"
#include "app/layout.h"
#include "app/managers/data_manager.h"
#include "app/tasks.h"
#include "app/tft.h"
#include "app/widgets/atmosphere_widget.h"
#include "config.h"

/* Widgets */
TFT_eSprite humidity_text_sprite = TFT_eSprite(&tft);
TFT_eSprite humidity_pct_sprite = TFT_eSprite(&tft);

/* RTOS */
#define ATMOSPHERE_WIDGET_UPDATE_PERIOD 50
QueueHandle_t atmosphereQueue;
extern SemaphoreHandle_t TFTLock;

void humidity_pct_update_widget(float humidity) {
    static float _humidity = 0;
    int16_t old_pos;
    int16_t new_pos;

    if (humidity != _humidity) {
        old_pos = (_humidity * HUMIDITY_PCT_SPRITE_WIDTH) / 100;
        new_pos = (humidity * HUMIDITY_PCT_SPRITE_WIDTH) / 100;

        if (new_pos > old_pos) {
            humidity_pct_sprite.fillRect(
                old_pos,
                0,
                (new_pos - old_pos),
                (HUMIDITY_PCT_SPRITE_HEIGHT - 1),
                HUMIDITY_PCT_COLOUR_ACTIVE
            );
        } else {
            humidity_pct_sprite.fillRect(
                new_pos,
                0,
                (old_pos - new_pos),
                (HUMIDITY_PCT_SPRITE_HEIGHT - 1),
                HUMIDITY_PCT_COLOUR_INACTIVE
            );
        }

        /* Render */
        if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
            humidity_pct_sprite.pushSprite(HUMIDITY_PCT_SPRITE_X, HUMIDITY_PCT_SPRITE_Y);
            _humidity = humidity;
            xSemaphoreGive(TFTLock);
        } else {
            log_e("Can't unlock TFT Semaphore");
        }
    }
}

void humidity_text_update_widget(float humidity) {
    static float _humidity = -999.9;
    String humidity_now = "";
    String humidity_prev = "";
    char buffer[10] = { 0 };

    if (humidity != _humidity) {
        /* Humidity Message */
        snprintf(buffer, sizeof(buffer), "%.1f%%", _humidity);
        humidity_prev = String(buffer);
        snprintf(buffer, sizeof(buffer), "%.1f%%", humidity);
        humidity_now = String(buffer);

        /* Update Sprite */
        humidity_text_sprite.setTextDatum(TR_DATUM);
        humidity_text_sprite.setTextColor(COLOURS_TEXT_BACKGROUND);
        humidity_text_sprite.drawRightString(humidity_prev, HUMIDITY_TEXT_SPRITE_WIDTH, 0, 1);
        humidity_text_sprite.setTextColor(COLOURS_TEXT_FOREGROUND);
        humidity_text_sprite.drawRightString(humidity_now, HUMIDITY_TEXT_SPRITE_WIDTH, 0, 1);

        /* Render */
        if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
            humidity_text_sprite.pushSprite(HUMIDITY_TEXT_SPRITE_X, HUMIDITY_TEXT_SPRITE_Y);
            _humidity = humidity;
            xSemaphoreGive(TFTLock);
        } else {
            log_e("Can't unlock TFT Semaphore");
        }
    }
}

void atmosphere_widget_task(void *pvParameters) {
    atmosphere_packet packet;

    log_i("Atmosphere widget task created");

    while (1) {
        if (xQueueReceive(atmosphereQueue, &packet, portMAX_DELAY) == pdPASS) {
            log_d("Atmosphere update received");
            humidity_text_update_widget(packet.humidity);
            humidity_pct_update_widget(packet.humidity);
        }

        vTaskDelay(ATMOSPHERE_WIDGET_UPDATE_PERIOD / portTICK_PERIOD_MS);
    }
}

void atmosphere_update(atmosphere_packet packet) {
    xQueueSend(atmosphereQueue, &packet, portMAX_DELAY);
}

void atmosphere_draw_permanent_elements() {
    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        tft.setFreeFont(NORMAL_FONT_SMALL);
        tft.setTextColor(BGR(HUMIDITY_COLOUR));
        tft.setTextSize(1);
        tft.drawString("Humidity", 0, HUMIDITY_LABEL_Y, 1);
        tft.drawLine(0, HUMIDITY_LABEL_Y - 8, 320, HUMIDITY_LABEL_Y - 8, BGR(HUMIDITY_COLOUR));
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Could not initialise TFT screen");
    }
}

void atmosphere_widget_init() {
    atmosphere_draw_permanent_elements();

    humidity_text_sprite.createSprite(HUMIDITY_TEXT_SPRITE_WIDTH, HUMIDITY_TEXT_SPRITE_HEIGHT);
    humidity_text_sprite.fillSprite(BGR(TFT_YELLOW));
    humidity_text_sprite.setFreeFont(DIGITAL_FONT_SMALL);
    humidity_text_sprite.setColorDepth(1);
    humidity_text_sprite.createPalette(text_palette);

    humidity_pct_sprite.createSprite(HUMIDITY_PCT_SPRITE_WIDTH, HUMIDITY_PCT_SPRITE_HEIGHT);
    humidity_pct_sprite.fillSprite(BGR(HUMIDITY_PCT_COLOUR_INACTIVE));

    atmosphereQueue = xQueueCreate(3, sizeof(atmosphere_packet));
    if (atmosphereQueue == NULL) {
        log_e("No atmosphereQueue created");
    }

    xTaskCreatePinnedToCore(
        atmosphere_widget_task,
        "Atmosphere Widget",
        ATMOSPHERE_WIDGET_STACK,
        NULL,
        ATMOSPHERE_WIDGET_PRIORITY,
        NULL,
        APP_CORE
    );
}
