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
#include "app/widgets/sun_widget.h"
#include "config.h"

/* Widgets */
TFT_eSprite uv_sprite = TFT_eSprite(&tft);
TFT_eSprite rise_set_sprite = TFT_eSprite(&tft);
TFT_eSprite solar_cycle_sprite = TFT_eSprite(&tft);

/* Sun Rise/Fall */
#include <sunset.h>
SunSet sun;

/* RTOS */
#define SUN_WIDGET_UPDATE_PERIOD 50
QueueHandle_t sunQueue;
extern SemaphoreHandle_t TFTLock;

void draw_uv_level(int16_t level) {
    uint16_t colour;

    uv_sprite.setTextDatum(MC_DATUM);

    switch (level) {
        case 0:
            colour = FOUR_BIT_DARK_GREY;
            break;
        case 1:
            uv_sprite.setTextDatum(MR_DATUM);
        case 2:
            colour = FOUR_BIT_GREEN;
            break;
        case 3:
        case 4:
        case 5:
            colour = FOUR_BIT_YELLOW;
            break;
        default:
            colour = FOUR_BIT_RED;
    }

    String uv_level_pretty = level < 10 ? (String)level : "X";
    uv_sprite.fillTriangle(UV_SPRITE_TRIANGLE_POINTS, colour);
    uv_sprite.setTextColor(BGR(TFT_BACKGROUND));
    uv_sprite.drawString((String)uv_level_pretty, UV_SPRITE_TEXT_XY);

    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        uv_sprite.pushSprite(UV_SPRITE_X, UV_SPRITE_Y);
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void sun_update_sprite(sun_packet packet) {
    draw_uv_level(packet.uv_level);
}

void sun_widget_task(void *pvParameters) {
    sun_packet packet;

    log_i("Sun widget task created");

    while (1) {
        if (xQueueReceive(sunQueue, &packet, portMAX_DELAY) == pdPASS) {
            log_d("Sun update received");
            sun_update_sprite(packet);
        }

        vTaskDelay(SUN_WIDGET_UPDATE_PERIOD / portTICK_PERIOD_MS);
    }
}

void sun_update(sun_packet packet) {
    xQueueSend(sunQueue, &packet, portMAX_DELAY);
}

void sun_cycle_update(struct tm timeinfo) {
    float p = 3;
    float h = (solar_cycle_sprite.height() - (p * 2));
    float w = (solar_cycle_sprite.width() - (p * 2));
    float y_0 = solar_cycle_sprite.height() / 2;

    float sunrise_civ;
    float sunset_start;
    float sunrise_start;
    float sunset_end;
    sun.setCurrentDate(timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_mday);
    sunrise_civ = static_cast<float>(sun.calcSunrise());
    sunset_start = static_cast<float>(sun.calcSunset());
    sunrise_start = static_cast<float>(sun.calcAstronomicalSunrise());
    sunset_end = static_cast<float>(sun.calcAstronomicalSunset());

    float sunrise_civ_ratio = (float)sunrise_civ / (24.0f * 60.0f);
    float sunset_start_ratio = (float)sunset_start / (24.0f * 60.0f);
    float sunrise_start_ratio = (float)sunrise_start / (24.0f * 60.0f);
    float sunset_end_ratio = (float)sunset_end / (24.0f * 60.0f);
    float now_ratio = (float)((timeinfo.tm_hour * 60) + timeinfo.tm_min) / (24.0f * 60.0f);
    float sunrise_end_x = (sunrise_civ_ratio * w) + p;
    float sunset_start_x = (sunset_start_ratio * w) + p;
    float sunrise_start_x = (sunrise_start_ratio * w) + p;
    float sunset_end_x = (sunset_end_ratio * w) + p;
    float now_x = (now_ratio * w) + p;
    float now_y = (0.5 * h * cos(2 * PI * now_ratio)) + y_0;
    float y_pixel;

    solar_cycle_sprite.fillSprite(TFT_BLACK);

    for (int16_t x = p + 1; x < sunrise_start_x; x++) {
        y_pixel = (0.5 * h * cos(2 * PI * (((float)x - p) / w))) + y_0;
        solar_cycle_sprite.drawPixel(x, y_pixel, COLOURS_SOLAR_CYCLE_NIGHT_NAUT);
    }

    for (int16_t x = sunrise_start_x; x < sunrise_end_x; x++) {
        y_pixel = (0.5 * h * cos(2 * PI * (((float)x - p) / w))) + y_0;
        solar_cycle_sprite.drawPixel(x, y_pixel, COLOURS_SOLAR_CYCLE_NIGHT_CIV);
    }

    for (int16_t x = sunrise_end_x; x < sunset_start_x; x++) {
        y_pixel = (0.5 * h * cos(2 * PI * (((float)x - p) / w))) + y_0;
        solar_cycle_sprite.drawPixel(x, y_pixel, COLOURS_SOLAR_CYCLE_DAY);
    }

    for (int16_t x = sunset_start_x; x < sunset_end_x; x++) {
        y_pixel = (0.5 * h * cos(2 * PI * (((float)x - p) / w))) + y_0;
        solar_cycle_sprite.drawPixel(x, y_pixel, COLOURS_SOLAR_CYCLE_NIGHT_CIV);
    }

    for (int16_t x = sunset_end_x; x < w + p; x++) {
        y_pixel = (0.5 * h * cos(2 * PI * (((float)x - p) / w))) + y_0;
        solar_cycle_sprite.drawPixel(x, y_pixel, COLOURS_SOLAR_CYCLE_NIGHT_NAUT);
    }

    if (now_ratio < sunset_start_ratio && now_ratio > sunrise_civ_ratio) {
        solar_cycle_sprite.fillCircle(now_x, now_y, p, COLOURS_SOLAR_CYCLE_NOW_DAY);
    } else {
        solar_cycle_sprite.fillCircle(now_x, now_y, p, COLOURS_SOLAR_CYCLE_NOW_NIGHT);
    }

    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        solar_cycle_sprite.pushSprite(SOLAR_CYCLE_SPRITE_X, SOLAR_CYCLE_SPRITE_Y);
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void sun_rise_fall_update(struct tm timeinfo) {
    int sunrise;
    int sunset;
    int16_t x;
    char sunrise_pretty[10] = { 0 };
    char sunset_pretty[10] = { 0 };

    sun.setCurrentDate(timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_mday);
    sunrise = static_cast<int>(sun.calcCivilSunrise());
    sunset = static_cast<int>(sun.calcCivilSunset());

    snprintf(sunrise_pretty, sizeof(sunrise_pretty), "%d:%02dam", (sunrise / 60), (sunrise % 60));
    snprintf(sunset_pretty, sizeof(sunset_pretty), "%d:%02dpm", (sunset / 60), (sunset % 60));

    /* Set */
    rise_set_sprite.fillSprite(BGR(TFT_BACKGROUND));
    rise_set_sprite.setTextColor(BGR(SUN_SET_COLOUR));
    x = RISE_SET_SPRITE_WIDTH
        - rise_set_sprite.drawRightString(sunset_pretty, RISE_SET_SPRITE_WIDTH, 0, 1) - 4;
    rise_set_sprite.fillTriangle(RISE_SET_TRIANGLE_DOWN(x), BGR(SUN_SET_COLOUR));

    /* Rise */
    x -= 15;
    rise_set_sprite.setTextColor(BGR(SUN_RISE_COLOUR));
    x = x - rise_set_sprite.drawRightString(sunrise_pretty, x, 0, 1) - 4;
    rise_set_sprite.fillTriangle(RISE_SET_TRIANGLE_UP(x), BGR(SUN_RISE_COLOUR));

    /* Render */
    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        rise_set_sprite.pushSprite(RISE_SET_SPRITE_X, RISE_SET_SPRITE_Y);

        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void sun_widget_init() {
    /* Widgets */
    uv_sprite.createSprite(UV_SPRITE_WIDTH, UV_SPRITE_HEIGHT);
    uv_sprite.fillSprite(BGR(TFT_BACKGROUND));
    uv_sprite.setFreeFont(DIGITAL_FONT_SMALL);
    uv_sprite.setColorDepth(4);

    rise_set_sprite.createSprite(RISE_SET_SPRITE_WIDTH, RISE_SET_SPRITE_HEIGHT);
    rise_set_sprite.fillSprite(BGR(TFT_BACKGROUND));
    rise_set_sprite.setFreeFont(NORMAL_FONT_XSMALL);

    solar_cycle_sprite.createSprite(SOLAR_CYCLE_SPRITE_WIDTH, SOLAR_CYCLE_SPRITE_HEIGHT);
    solar_cycle_sprite.setColorDepth(4);
    solar_cycle_sprite.fillSprite(BGR(TFT_BACKGROUND));

    /* Sun rise/fall */
    sun.setPosition(LATITUDE, LONGITUDE, DST_OFFSET);

    sunQueue = xQueueCreate(3, sizeof(sun_packet));
    if (sunQueue == NULL) {
        log_e("No sunQueue created");
    }

    xTaskCreatePinnedToCore(
        sun_widget_task, "Sun Widget", SUN_WIDGET_STACK, NULL, SUN_WIDGET_PRIORITY, NULL, APP_CORE
    );
}
