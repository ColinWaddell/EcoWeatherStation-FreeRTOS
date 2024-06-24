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
#include "app/widgets/temperature_widget.h"
#include "config.h"

/* Prototypes */
static uint16_t temperature_to_color(float temperature);

/* Widgets */
TFT_eSprite temperature_text_sprite = TFT_eSprite(&tft);
TFT_eSprite temperature_graph_sprite = TFT_eSprite(&tft);
FIFOGraph temperature_graph = FIFOGraph(
    &temperature_graph_sprite,
    TEMPERATURE_GRAPH_WIDTH,
    TEMPERATURE_GRAPH_HEIGHT,
    TEMPERATURE_GRAPH_SPRITE_X,
    TEMPERATURE_GRAPH_SPIRTE_Y,
    0,
    30,
    5,
    FOUR_BIT_RED
);

/* RTOS */
#define TEMPERATURE_WIDGET_UPDATE_PERIOD 50
QueueHandle_t temperatureQueue;
extern SemaphoreHandle_t TFTLock;

#include <TFT_eSPI.h>

// Convert float temperature to a 565 color value
static uint16_t temperature_to_color(float temperature) {
    // Define temperature range
    float minTemp = -10.0;
    float maxTemp = 40.0;

    // Clamp the temperature to the min and max range
    if (temperature < minTemp)
        temperature = minTemp;
    if (temperature > maxTemp)
        temperature = maxTemp;

    // Normalize the temperature to a value between 0 and 1
    float normalized = (temperature - minTemp) / (maxTemp - minTemp);

    // Interpolate color from blue (cold) to red (hot)
    uint8_t r = (uint8_t)(normalized * 255);
    uint8_t g = (uint8_t)((1.0 - abs(normalized - 0.5) * 2) * 255);
    uint8_t b = (uint8_t)((1.0 - normalized) * 255);

    // Convert 8-bit RGB to 565 color
    uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return color;
}

void temperature_update_sprite(temperature_packet packet) {
    static float _temperature_val = -999.9;
    float temperature_val = packet.currently;
    String temperature_now = "";
    String temperature_prev = "";
    char buffer[10] = { 0 };

    if (temperature_val != _temperature_val) {
        /* Generate Message */
        snprintf(
            buffer, sizeof(buffer), _temperature_val <= -10 ? "%.1f" : "%.1fC", _temperature_val
        );
        temperature_prev = String(buffer);
        snprintf(
            buffer, sizeof(buffer), temperature_val <= -10 ? "%.1f" : "%.1fC", temperature_val
        );
        temperature_now = String(buffer);

        /* Update Sprite */
        temperature_text_sprite.setTextDatum(TR_DATUM);
        temperature_text_sprite.setTextColor(COLOURS_TEXT_BACKGROUND);
        temperature_text_sprite.drawRightString(
            temperature_prev, TEMPERATURE_TEXT_SPRITE_WIDTH, 0, 1
        );
        temperature_text_sprite.setTextColor(COLOURS_TEXT_FOREGROUND);
        temperature_text_sprite.drawRightString(
            temperature_now, TEMPERATURE_TEXT_SPRITE_WIDTH, 0, 1
        );

        /* Render */
        if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
            temperature_text_sprite.pushSprite(
                TEMPERATURE_TEXT_SPRITE_X, TEMPERATURE_TEXT_SPRITE_Y
            );
            _temperature_val = temperature_val;
            xSemaphoreGive(TFTLock);
        } else {
            log_e("Can't unlock TFT Semaphore");
        }
    }
}

void temperature_widget_task(void *pvParameters) {
    temperature_packet packet;
    uint8_t graph_interval = 0;

    log_i("Temperature widget task created");

    while (1) {
        if (xQueueReceive(temperatureQueue, &packet, portMAX_DELAY) == pdPASS) {
            log_d("Temperature update received");
            if (!graph_interval) {
                temperature_graph.addDataPoint(packet.currently);
            }
            graph_interval = (graph_interval + 1) % GRAPH_UPDATE_INTERVAL;
            temperature_update_sprite(packet);
        }

        vTaskDelay(TEMPERATURE_WIDGET_UPDATE_PERIOD / portTICK_PERIOD_MS);
    }
}

void temperature_update(temperature_packet packet) {
    xQueueSend(temperatureQueue, &packet, portMAX_DELAY);
}

void temperature_draw_permanent_elements() {
}

void temperature_widget_init() {
    temperature_draw_permanent_elements();

    temperature_text_sprite.createSprite(
        TEMPERATURE_TEXT_SPRITE_WIDTH, TEMPERATURE_TEXT_SPRITE_HEIGHT
    );
    temperature_text_sprite.setFreeFont(DIGITAL_FONT_LARGE);
    temperature_text_sprite.setColorDepth(1);
    temperature_text_sprite.fillSprite(COLOURS_TEXT_BACKGROUND);
    temperature_text_sprite.createPalette(text_palette);

    temperatureQueue = xQueueCreate(3, sizeof(temperature_packet));
    if (temperatureQueue == NULL) {
        log_e("No temperatureQueue created");
    }

    xTaskCreatePinnedToCore(
        temperature_widget_task,
        "Temperature Widget",
        TEMPERATURE_WIDGET_STACK,
        NULL,
        TEMPERATURE_WIDGET_PRIORITY,
        NULL,
        APP_CORE
    );
}