#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "app/colours.h"
#include "app/tft.h"
#include "config.h"

TFT_eSPI tft = TFT_eSPI();
SemaphoreHandle_t TFTLock;

void tft_init() {
    TFTLock = xSemaphoreCreateBinary();

    if (TFTLock != NULL) {
        xSemaphoreGive(TFTLock);
        log_i("TFT Semaphore available");
    } else {
        log_e("Could not create TFT Semaphore");
    }

    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        tft.init();
        tft.setRotation(6);
        tft.fillScreen(TFT_BACKGROUND);
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Could not initialise TFT screen");
    }
}