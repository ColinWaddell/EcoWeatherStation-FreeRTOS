#include <Arduino.h>

#include "app/managers/clock_manager.h"
#include "app/managers/data_manager.h"
#include "app/managers/wifi_manager.h"

#ifndef TOOLBAR_H
#define TOOLBAR_H

void toolbar_init();
void toolbar_wifi_status_set(wifi_status status);
void toolbar_clock_status_set(time_status status);
void toolbar_clock_time_set(struct tm timeinfo);
void toolbar_moon_phase_set(struct tm timeinfo);
void toolbar_data_status_set(data_status status);
void toolbar_message_set(String msg);
void toolbar_render(void *pvParameters);

#endif