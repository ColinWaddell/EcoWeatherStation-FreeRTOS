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
#include "app/tasks.h"
#include "app/tft.h"
#include "app/widgets/moon_phases.h"
#include "app/widgets/toolbar_widget.h"
#include "config.h"

#define TOOLBAR_RENDER_PERIOD_SLOW 1000
#define TOOLBAR_RENDER_PERIOD_FAST 20
#define TOOLBAR_MSG_FONT_SIZE 1
#define TOOLBAR_CLOCK_FONT_SIZE 1
#define TIME_STRING_MAX_LEN 10
#define MSG_STRING_MAX_LEN 40

/* Widgets */
TFT_eSprite time_sprite = TFT_eSprite(&tft);
TFT_eSprite wifi_sprite = TFT_eSprite(&tft);
TFT_eSprite msg_sprite = TFT_eSprite(&tft);
TFT_eSprite data_sprite = TFT_eSprite(&tft);
TFT_eSprite moon_sprite = TFT_eSprite(&tft);
extern SemaphoreHandle_t TFTLock;

/* Moon phase */
#include <sunset.h>
SunSet moon;

/* RTOS */
#define QUEUE_TIMEOUT_MS 50
QueueHandle_t timeQueue;
QueueHandle_t timeStatusQueue;
QueueHandle_t wifiQueue;
QueueHandle_t msgQueue;
QueueHandle_t dataQueue;

void draw_moon_phase(TFT_eSprite *sprite, int phase) {
#if 0
    static int test = 0;
    phase = test;
    test++;
    if (test > 29) {
        test = 0;
    }
    log_d(">>>> PHASE: %d", phase);
#endif
    int x;

    if (phase == 0 || phase == 29) {
        x = -1 * MOON_SPRITE_WIDTH * 0;
    } else if (phase < 8) {
        x = -1 * MOON_SPRITE_WIDTH * 1;
    } else if (phase == 8) {
        x = -1 * MOON_SPRITE_WIDTH * 2;
    } else if (phase < 14) {
        x = -1 * MOON_SPRITE_WIDTH * 3;
    } else if (phase == 14) {
        x = -1 * MOON_SPRITE_WIDTH * 4;
    } else if (phase < 21) {
        x = -1 * MOON_SPRITE_WIDTH * 5;
    } else if (phase == 21) {
        x = -1 * MOON_SPRITE_WIDTH * 6;
    } else {
        x = -1 * MOON_SPRITE_WIDTH * 7;
    }

    sprite->fillSprite(COLOURS_MOON_BACKGROUND);
    sprite->drawXBitmap(
        x, 0, moon_phases, MOON_PHASES_WIDTH, MOON_PHASES_HEIGHT, TFT_BACKGROUND, TFT_WHITE
    );
}

void toolbar_moon_phase_set(struct tm timeinfo) {
    int phase;
    moon.setCurrentDate(timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_mday);

    /* The return value is 0 to 29, with 0 and 29 being hidden and 14 being full */
    phase = moon.moonPhase();
    draw_moon_phase(&moon_sprite, phase);

    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        moon_sprite.pushSprite(MOON_SPRITE_X, MOON_SPRITE_Y);
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void toolbar_draw_data_status(data_status status) {
    bool error = false;
    uint32_t color;

    data_sprite.fillSprite(FOUR_BIT_BLACK);

    switch (status) {
        case DATA_STATUS_EMPTY:
            color = FOUR_BIT_BLACK;
            break;

        case DATA_STATUS_SEARCHING:
            color = FOUR_BIT_BLACK;
            break;

        case DATA_STATUS_DOWNLOADED:
            color = FOUR_BIT_BLACK;
            break;

        case DATA_STATUS_DECODER_FAILURE:
            color = FOUR_BIT_VIOLET;
            error = true;
            break;

        case DATA_STATUS_DOWNLOAD_FAILURE:
        default:
            color = FOUR_BIT_RED;
            error = true;
    }

    if (error) {
        /* Draw Cross */
        data_sprite.drawLine(0, 0, 16, 16, color);
        data_sprite.drawLine(0, 16, 16, 0, color);
    } else {
        /* Draw Down Arrow */
        data_sprite.fillRect(6, 0, 3, 10, color);
        data_sprite.fillTriangle(1, 9, 13, 9, 7, 15, color);
    }

    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        data_sprite.pushSprite(DATA_STATUS_SPRITE_X, DATA_STATUS_SPRITE_Y);
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void toolbar_draw_time_status(time_status status) {
    switch (status) {
        case TIME_STATUS_UPDATING:
        case TIME_STATUS_TIME_LOCKED:
            /* Do nothing */
            break;

        case TIME_STATUS_NO_TIME:
        default:
            time_sprite.fillSprite(BGR(TFT_BACKGROUND));
            time_sprite.setTextDatum(ML_DATUM);
            time_sprite.setTextColor(COLOURS_TEXT_FOREGROUND);
            time_sprite.drawString("...", 0, TIME_SPRITE_HEIGHT / 2);

            if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
                time_sprite.pushSprite(TIME_SPRITE_X, TIME_SPRITE_Y);
                xSemaphoreGive(TFTLock);
            } else {
                log_e("Can't unlock TFT Semaphore");
            }
    }
}

void toolbar_draw_time(struct tm timeinfo) {
    String new_time;
    static String old_time = "";

    new_time = String(timeinfo.tm_hour < 10 ? "0" : "") + String(timeinfo.tm_hour) + ":"
               + String(timeinfo.tm_min < 10 ? "0" : "") + String(timeinfo.tm_min);

    time_sprite.setTextDatum(ML_DATUM);

    time_sprite.setTextColor(COLOURS_TEXT_BACKGROUND);
    time_sprite.drawString(old_time.c_str(), 0, TIME_SPRITE_HEIGHT / 2);

    time_sprite.setTextColor(COLOURS_TEXT_FOREGROUND);
    time_sprite.drawString(new_time, 0, TIME_SPRITE_HEIGHT / 2);

    old_time = new_time;

    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        time_sprite.pushSprite(TIME_SPRITE_X, TIME_SPRITE_Y);
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void toolbar_draw_wifi_status(wifi_status status) {
    static uint8_t grey = 0;
    static uint8_t green = 0;
    uint32_t colour;

    switch (status) {
        case WIFI_STATUS_UNCONNECTED:
            colour = BGR(TFT_PURPLE);
            break;

        case WIFI_STATUS_CONNECTING:
            grey += 1;
            grey &= 0x7F;
            colour = BGR(tft.color565(grey, grey, grey));
            break;

        case WIFI_STATUS_CONNECTED:
            colour = BGR(TFT_WHITE);
            break;

        case WIFI_STATUS_FAILED:
        default:
            colour = BGR(TFT_RED);
            break;
    }

    const uint32_t x = WIFI_STATUS_SPRITE_WIDTH / 2;
    const uint32_t y = WIFI_STATUS_SPRITE_HEIGHT;
    const uint32_t r = WIFI_STATUS_SPRITE_HEIGHT / 2;

    wifi_sprite.drawArc(x, y, r - 4, r - 8, 135, 225, colour, TFT_BACKGROUND, true);
    wifi_sprite.drawArc(x, y, r, r - 2, 135, 225, colour, TFT_BACKGROUND, true);
    wifi_sprite.drawArc(x, y, r + 4, r + 2, 135, 225, colour, TFT_BACKGROUND, true);
    wifi_sprite.drawArc(x, y, r + 8, r + 6, 135, 225, colour, TFT_BACKGROUND, true);

    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        wifi_sprite.pushSprite(WIFI_STATUS_SPRITE_X, WIFI_STATUS_SPRITE_Y);
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void toolbar_draw_message(String msg) {
    if (!ENABLE_STATUS_MSG) {
        return;
    }

    static String old_msg = "";
    msg_sprite.setTextDatum(ML_DATUM);

    msg_sprite.setTextColor(COLOURS_TEXT_BACKGROUND);
    msg_sprite.drawString(old_msg, 0, WIFI_MSG_SPRITE_HEIGHT / 2);

    msg_sprite.setTextColor(COLOURS_TEXT_FOREGROUND);
    msg_sprite.drawString(msg, 0, WIFI_MSG_SPRITE_HEIGHT / 2);

    log_d("Toolbar message: '%d", msg);
    old_msg = msg;

    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        msg_sprite.pushSprite(WIFI_MSG_SPRITE_X, WIFI_MSG_SPRITE_Y);
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void toolbar_render(void *pvParameters) {
    String new_time;
    String new_msg;
    struct tm time_buffer;
    time_status time_status_buffer;
    char msg_buffer[MSG_STRING_MAX_LEN];
    wifi_status wifi_buffer;
    wifi_status wifi_buffer_prev;
    data_status data_buffer;
    bool high_framerate = false;

    while (1) {
        high_framerate = false;

        if (xQueueReceive(timeQueue, &time_buffer, QUEUE_TIMEOUT_MS) == pdPASS) {
            toolbar_draw_time(time_buffer);
            toolbar_moon_phase_set(time_buffer);
        }

        if (xQueueReceive(timeStatusQueue, &time_status_buffer, QUEUE_TIMEOUT_MS) == pdPASS) {
            toolbar_draw_time_status(time_status_buffer);
        }

        if (xQueueReceive(wifiQueue, &wifi_buffer, QUEUE_TIMEOUT_MS) == pdPASS) {
            toolbar_draw_wifi_status(wifi_buffer);
            wifi_buffer_prev = wifi_buffer;
        } else if (wifi_buffer_prev == WIFI_STATUS_CONNECTING) {
            /* Force animation */
            high_framerate = true;
            toolbar_draw_wifi_status(WIFI_STATUS_CONNECTING);
        }

        if (xQueueReceive(msgQueue, &msg_buffer, QUEUE_TIMEOUT_MS) == pdPASS) {
            new_msg = String(msg_buffer);
            toolbar_draw_message(new_msg);
        }

        if (xQueueReceive(dataQueue, &data_buffer, QUEUE_TIMEOUT_MS) == pdPASS) {
            toolbar_draw_data_status(data_buffer);
        }

        if (high_framerate) {
            vTaskDelay(TOOLBAR_RENDER_PERIOD_FAST / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(TOOLBAR_RENDER_PERIOD_SLOW / portTICK_PERIOD_MS);
        }
    }
}

void toolbar_wifi_status_set(wifi_status status) {
    xQueueSend(wifiQueue, &status, portMAX_DELAY);
}

void toolbar_clock_status_set(time_status status) {
    xQueueSend(timeStatusQueue, &status, portMAX_DELAY);
}

void toolbar_clock_time_set(struct tm timeinfo) {
    xQueueSend(timeQueue, &timeinfo, portMAX_DELAY);
}

void toolbar_data_status_set(data_status status) {
    xQueueSend(dataQueue, &status, portMAX_DELAY);
}

void toolbar_message_set(String msg) {
    char buffer[MSG_STRING_MAX_LEN];
    msg.toCharArray(buffer, MSG_STRING_MAX_LEN);
    xQueueSend(msgQueue, &buffer, portMAX_DELAY);
}

void toolbar_draw_permanent_elements() {
    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        tft.drawLine(0, BORDERS_TOP_Y, 320, BORDERS_TOP_Y, BGR(TFT_DARKGREY));
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void toolbar_init() {
    /* On a permo */
    toolbar_draw_permanent_elements();

    /* Setup Sprites */
    time_sprite.createSprite(TIME_SPRITE_WIDTH, TIME_SPRITE_HEIGHT);
    time_sprite.fillSprite(TFT_BACKGROUND);
    time_sprite.setTextSize(TOOLBAR_CLOCK_FONT_SIZE);
    time_sprite.setFreeFont(NORMAL_FONT_SMALL);
    time_sprite.setColorDepth(1);
    time_sprite.createPalette(text_palette);

    wifi_sprite.createSprite(WIFI_STATUS_SPRITE_WIDTH, WIFI_STATUS_SPRITE_HEIGHT);
    wifi_sprite.fillSprite(TFT_BACKGROUND);

    msg_sprite.createSprite(WIFI_MSG_SPRITE_WIDTH, WIFI_MSG_SPRITE_HEIGHT);
    msg_sprite.fillSprite(TFT_BACKGROUND);
    msg_sprite.setTextSize(TOOLBAR_CLOCK_FONT_SIZE);
    msg_sprite.setFreeFont(NORMAL_FONT_SMALL);
    msg_sprite.setColorDepth(1);
    msg_sprite.createPalette(text_palette);

    moon_sprite.createSprite(MOON_SPRITE_WIDTH, MOON_SPRITE_HEIGHT);
    moon_sprite.createPalette(text_palette);
    moon_sprite.setColorDepth(1);
    moon_sprite.fillSprite(TFT_BACKGROUND);

    data_sprite.createSprite(DATA_STATUS_SPRITE_WIDTH, DATA_STATUS_SPRITE_HEIGHT);
    data_sprite.fillSprite(TFT_BACKGROUND);
    data_sprite.setColorDepth(4);

    /* RTOS Stuff */
    timeQueue = xQueueCreate(3, sizeof(struct tm));
    if (timeQueue == NULL) {
        log_e("No timeQueue created");
    }

    timeStatusQueue = xQueueCreate(3, sizeof(time_status));
    if (timeQueue == NULL) {
        log_e("No timeQueue created");
    }

    wifiQueue = xQueueCreate(3, sizeof(wifi_status));
    if (wifiQueue == NULL) {
        log_e("No wifiQueue created");
    }

    msgQueue = xQueueCreate(3, MSG_STRING_MAX_LEN);
    if (msgQueue == NULL) {
        log_e("No msgQueue created");
    }

    dataQueue = xQueueCreate(3, sizeof(data_status));
    if (dataQueue == NULL) {
        log_e("No dataQueue created");
    }

    xTaskCreatePinnedToCore(
        toolbar_render,
        "Toolbar Render",
        TASK_TOOLBAR_RENDER_STACK,
        NULL,
        TASK_TOOLBAR_RENDER_PRIORITY,
        NULL,
        APP_CORE
    );
}
