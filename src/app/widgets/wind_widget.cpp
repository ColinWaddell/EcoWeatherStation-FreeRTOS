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
#include "app/widgets/wind_widget.h"
#include "config.h"

/* Widgets */
TFT_eSprite wind_text_sprite = TFT_eSprite(&tft);
TFT_eSprite arrow_sprite = TFT_eSprite(&tft);
TFT_eSprite wind_graph_sprite = TFT_eSprite(&tft);
FIFOGraph wind_graph = FIFOGraph(
    &wind_graph_sprite,
    WIND_GRAPH_WIDTH,
    WIND_GRAPH_HEIGHT,
    WIND_GRAPH_SPRITE_X,
    WIND_GRAPH_SPIRTE_Y,
    0,
    10,
    5,
    FOUR_BIT_YELLOW
);

/* RTOS */
#define WIND_WIDGET_UPDATE_PERIOD 50
QueueHandle_t windQueue;
extern SemaphoreHandle_t TFTLock;

void direction_arrow_update_widget(int16_t wind_direction) {
    static int16_t wind_direction_prev = -1;
    int16_t min_x, min_y, max_x, max_y;

    wind_direction += WIND_DIRECTION_NORTH_OFFSET;

    if (wind_direction != wind_direction_prev) {
        if (arrow_sprite.getRotatedBounds(wind_direction_prev, &min_x, &min_y, &max_x, &max_y)) {
            if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
                tft.fillRect(min_x, min_y, max_x - min_x, max_y - min_y, TFT_BACKGROUND);
                xSemaphoreGive(TFTLock);
            } else {
                log_e("Can't unlock TFT Semaphore");
            }
        }

        /* Render */
        if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
            tft.setPivot(ARROW_SPRITE_X, ARROW_SPRITE_Y);
            arrow_sprite.pushRotated(wind_direction);
            wind_direction_prev = wind_direction;
            xSemaphoreGive(TFTLock);
        } else {
            log_e("Can't unlock TFT Semaphore");
        }
    }
}

void wind_text_update_widget(float speed, float gust) {
    static float _speed = -999.9;
    static float _gust = -999.9;
    String wind_now = "";
    String wind_prev = "";
    char buffer[10] = { 0 };

    if (speed != _speed || gust != _gust) {
        /* Generate Message */
        snprintf(buffer, sizeof(buffer), "%.1f/%.1f", _speed, _gust);
        wind_prev = String(buffer);
        snprintf(buffer, sizeof(buffer), "%.1f/%.1f", speed, gust);
        wind_now = String(buffer);

        /* Update Sprite */
        wind_text_sprite.setTextDatum(TR_DATUM);
        wind_text_sprite.setTextColor(COLOURS_TEXT_BACKGROUND);
        wind_text_sprite.drawRightString(wind_prev, WIND_TEXT_SPRITE_WIDTH, 0, 1);
        wind_text_sprite.setTextColor(COLOURS_TEXT_FOREGROUND);
        wind_text_sprite.drawRightString(wind_now, WIND_TEXT_SPRITE_WIDTH, 0, 1);

        /* Render */
        if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
            wind_text_sprite.pushSprite(WIND_TEXT_SPRITE_X, WIND_TEXT_SPRITE_Y);
            _speed = speed;
            _gust = gust;
            xSemaphoreGive(TFTLock);
        } else {
            log_e("Can't unlock TFT Semaphore");
        }
    }
}

void wind_widget_task(void *pvParameters) {
    wind_packet packet;
    uint8_t graph_interval = 0;

    log_e("Wind widget task created");

    while (1) {
        if (xQueueReceive(windQueue, &packet, portMAX_DELAY) == pdPASS) {
            log_d("Wind update received");
            wind_text_update_widget(packet.speed, packet.gust);
            direction_arrow_update_widget(packet.direction);

            if (!graph_interval) {
                wind_graph.addDataPoint(packet.gust);
            }
            graph_interval = (graph_interval + 1) % GRAPH_UPDATE_INTERVAL;
        }

        vTaskDelay(WIND_WIDGET_UPDATE_PERIOD / portTICK_PERIOD_MS);
    }
}

void wind_update(wind_packet packet) {
    xQueueSend(windQueue, &packet, portMAX_DELAY);
}

void wind_draw_permanent_elements() {
    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        /* Labels */
        tft.setFreeFont(NORMAL_FONT_SMALL);
        tft.setTextColor(BGR(WIND_COLOUR));
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
        tft.drawString("Wind", 0, WIND_LABEL_Y, 1);
        tft.drawRightString("Speed/Gust (mph)", 320, WIND_LABEL_Y, 1);
        tft.drawLine(0, WIND_LABEL_Y - 8, 320, WIND_LABEL_Y - 8, BGR(TFT_YELLOW));

        /* Wind arrow outline */
        tft.drawCircle(ARROW_SPRITE_X, ARROW_SPRITE_Y, COMPASS_RADIUS + 8, BGR(TFT_DARKGREY));
        tft.drawArc(
            ARROW_SPRITE_X,
            ARROW_SPRITE_Y,
            (ARROW_SPRITE_HEIGHT / 2) + 12,
            (ARROW_SPRITE_HEIGHT / 2) + 6,
            178 + WIND_DIRECTION_NORTH_OFFSET,
            182 + WIND_DIRECTION_NORTH_OFFSET,
            BGR(TFT_WHITE),
            BGR(TFT_BACKGROUND),
            false
        );
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void wind_widget_init() {
    wind_draw_permanent_elements();

    wind_text_sprite.createSprite(WIND_TEXT_SPRITE_WIDTH, WIND_TEXT_SPRITE_HEIGHT);
    wind_text_sprite.fillSprite(BGR(TFT_BACKGROUND));
    wind_text_sprite.setFreeFont(DIGITAL_FONT_MEDIUM);
    wind_text_sprite.setColorDepth(1);
    wind_text_sprite.createPalette(text_palette);

    arrow_sprite.createSprite(ARROW_SPRITE_WIDTH, ARROW_SPRITE_HEIGHT);
    arrow_sprite.fillSprite(BGR(TFT_BACKGROUND));
    arrow_sprite.setPivot(ARROW_SPRITE_WIDTH / 2, ARROW_SPRITE_HEIGHT / 2);
    arrow_sprite.setColorDepth(4);
    arrow_sprite.fillTriangle(
        0,
        2,
        (ARROW_SPRITE_WIDTH - 1) / 2,
        ARROW_SPRITE_HEIGHT - 1,
        ARROW_SPRITE_WIDTH - 1,
        2,
        FOUR_BIT_YELLOW
    );

    windQueue = xQueueCreate(3, sizeof(wind_packet));
    if (windQueue == NULL) {
        log_e("No windQueue created");
    }

    xTaskCreatePinnedToCore(
        wind_widget_task,
        "Wind Widget",
        WIND_WIDGET_STACK,
        NULL,
        WIND_WIDGET_PRIORITY,
        NULL,
        APP_CORE
    );
}
