#include <Arduino.h>

#include <SPI.h>
#include <TFT_eSPI.h>

#include "app/tft.h"
#include "config.h"

#include "app/widgets/atmosphere_widget.h"
#include "app/widgets/rain_widget.h"
#include "app/widgets/sun_widget.h"
#include "app/widgets/temperature_widget.h"
#include "app/widgets/toolbar_widget.h"
#include "app/widgets/wind_widget.h"

#include "app/managers/clock_manager.h"
#include "app/managers/data_manager.h"
#include "app/managers/wifi_manager.h"

#include "app.h"

extern TFT_eSPI tft;

void app_setup() {
    /* Setup Panel */
    tft_init();

    /* Register Widgets */
    toolbar_init();
    temperature_widget_init();
    atmosphere_widget_init();
    rain_widget_init();
    sun_widget_init();
    wind_widget_init();

    /* Register Managers */
    wifi_init();
    clock_init();
    data_init();
}