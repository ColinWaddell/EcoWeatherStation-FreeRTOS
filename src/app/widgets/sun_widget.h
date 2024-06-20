#include "app/managers/data_manager.h"

#ifndef SUN_H
#define SUN_H

void sun_widget_init();
void sun_update(sun_packet packet);
void sun_rise_fall_update(struct tm timeinfo);
void sun_cycle_update(struct tm timeinfo);

#endif