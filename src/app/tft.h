#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#ifndef TFT_H
#define TFT_H

extern TFT_eSPI tft;
extern SemaphoreHandle_t TFTLock;

void tft_init();

#endif