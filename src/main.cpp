#include <Arduino.h>

#include "app/app.h"

void setup() {
    Serial.begin(115200);

    /* Add tasks to the RTOS */
    app_setup();
}

void loop() {
    /* keep this empty as it messes with FreeRTOS */
}