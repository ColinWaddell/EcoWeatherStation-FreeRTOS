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
#include "app/widgets/fifo_graph.h"
#include "app/widgets/rain_widget.h"
#include "config.h"

/* Widgets */
TFT_eSprite rate_text_sprite = TFT_eSprite(&tft);
TFT_eSprite current_text_sprite = TFT_eSprite(&tft);
TFT_eSprite rain_graph_sprite = TFT_eSprite(&tft);
FIFOGraph rain_graph = FIFOGraph(
    &rain_graph_sprite,
    RAIN_GRAPH_WIDTH,
    RAIN_GRAPH_HEIGHT,
    RAIN_GRAPH_SPRITE_X,
    RAIN_GRAPH_SPIRTE_Y,
    0,
    0.6 * 5,
    0.6,
    FOUR_BIT_LIGHT_BLUE
);

/* RTOS */
#define RAIN_WIDGET_UPDATE_PERIOD 50
QueueHandle_t rainQueue;
extern SemaphoreHandle_t TFTLock;

void current_text_sprite_update(float current) {
    static float _current = -999.9;
    String current_now = "";
    String current_prev = "";
    char buffer[10] = { 0 };

    if (current != _current) {
        /* Generate Message */
        snprintf(buffer, sizeof(buffer), "%.1f", _current);
        current_prev = String(buffer);
        snprintf(buffer, sizeof(buffer), "%.1f", current);
        current_now = String(buffer);

        /* Update Sprite */
        current_text_sprite.setTextDatum(TR_DATUM);
        current_text_sprite.setTextColor(COLOURS_TEXT_BACKGROUND);
        current_text_sprite.drawRightString(current_prev, CURRENT_TEXT_SPRITE_WIDTH, 0, 1);
        current_text_sprite.setTextColor(COLOURS_TEXT_FOREGROUND);
        current_text_sprite.drawRightString(current_now, CURRENT_TEXT_SPRITE_WIDTH, 0, 1);

        /* Render */
        if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
            current_text_sprite.pushSprite(CURRENT_TEXT_SPRITE_X, CURRENT_TEXT_SPRITE_Y);
            _current = current;
            xSemaphoreGive(TFTLock);
        } else {
            log_e("Can't unlock TFT Semaphore");
        }
    }
}

void rate_text_sprite_update(float rate) {
    static float _rate = -999.9;
    String rate_now = "";
    String rate_prev = "";
    char buffer[10] = { 0 };

    if (rate != _rate) {
        /* Generate Message */
        snprintf(buffer, sizeof(buffer), "%.1f", _rate);
        rate_prev = String(buffer);
        snprintf(buffer, sizeof(buffer), "%.1f", rate);
        rate_now = String(buffer);

        /* Update Sprite */
        rate_text_sprite.setTextDatum(TR_DATUM);
        rate_text_sprite.setTextColor(COLOURS_TEXT_BACKGROUND);
        rate_text_sprite.drawRightString(rate_prev, RATE_TEXT_SPRITE_WIDTH, 0, 1);
        rate_text_sprite.setTextColor(COLOURS_TEXT_FOREGROUND);
        rate_text_sprite.drawRightString(rate_now, RATE_TEXT_SPRITE_WIDTH, 0, 1);

        /* Render */
        if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
            rate_text_sprite.pushSprite(RATE_TEXT_SPRITE_X, RATE_TEXT_SPRITE_Y);
            _rate = rate;
            xSemaphoreGive(TFTLock);
        } else {
            log_e("Can't unlock TFT Semaphore");
        }
    }
}

void rain_widget_task(void *pvParameters) {
    rain_packet packet;
    uint8_t graph_interval = 0;

    log_i("Rain widget task created");

    while (1) {
        if (xQueueReceive(rainQueue, &packet, portMAX_DELAY) == pdPASS) {
            log_d("Rain update received");
            rate_text_sprite_update(packet.rate);
            current_text_sprite_update(packet.daily);

            if (!graph_interval) {
                rain_graph.addDataPoint(packet.rate);
            }
            graph_interval = (graph_interval + 1) % GRAPH_UPDATE_INTERVAL;
        }

        vTaskDelay(RAIN_WIDGET_UPDATE_PERIOD / portTICK_PERIOD_MS);
    }
}

void rain_update(rain_packet packet) {
    xQueueSend(rainQueue, &packet, portMAX_DELAY);
}

void rain_draw_permanent_elements() {
    /* Title */
    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        tft.setFreeFont(NORMAL_FONT_SMALL);
        tft.setTextColor(BGR(RAIN_COLOUR));
        tft.setTextSize(1);
        tft.drawString("Rain", 0, RAIN_LABEL_Y, 1);
        tft.drawRightString("Rate (mm/h)", 320, RAIN_LABEL_Y, 1);
        tft.drawRightString("Today (mm)", 320, RAIN_LABEL_Y + 62, 1);
        tft.drawLine(0, RAIN_LABEL_Y - 8, 320, RAIN_LABEL_Y - 8, BGR(RAIN_COLOUR));

        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void rain_widget_init() {
    rain_draw_permanent_elements();

    rate_text_sprite.createSprite(RATE_TEXT_SPRITE_WIDTH, RATE_TEXT_SPRITE_HEIGHT);
    rate_text_sprite.fillSprite(BGR(TFT_BACKGROUND));
    rate_text_sprite.setFreeFont(DIGITAL_FONT_MEDIUM);
    rate_text_sprite.setColorDepth(1);
    rate_text_sprite.createPalette(text_palette);

    current_text_sprite.createSprite(CURRENT_TEXT_SPRITE_WIDTH, CURRENT_TEXT_SPRITE_HEIGHT);
    current_text_sprite.fillSprite(BGR(TFT_BACKGROUND));
    current_text_sprite.setFreeFont(DIGITAL_FONT_MEDIUM);
    current_text_sprite.setColorDepth(1);
    current_text_sprite.createPalette(text_palette);

    rainQueue = xQueueCreate(3, sizeof(rain_packet));
    if (rainQueue == NULL) {
        log_e("No rainQueue created");
    }

    xTaskCreatePinnedToCore(
        rain_widget_task,
        "Rain Widget",
        RAIN_WIDGET_STACK,
        NULL,
        RAIN_WIDGET_PRIORITY,
        NULL,
        APP_CORE
    );
}
