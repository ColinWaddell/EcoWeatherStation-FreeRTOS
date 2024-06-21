/* The following code is a refactor of that provided by
 * https://github.com/Bodmer/TFT_eSPI
 */
#include <Arduino.h>

#include <SPI.h>
#include <TFT_eSPI.h>

#include "esp_log.h"

#include "app/tasks.h"
#include "app/tft.h"
#include "config.h"

#define PIXEL_TIMEOUT 100    // 100ms Time-out between pixel requests
#define START_TIMEOUT 10000  // 10s Maximum time to wait at start transfer
#define SCREEN_SERVER_UPDATE_PERIOD 10000

#define BITS_PER_PIXEL 16  // 24 for RGB colour format, 16 for 565 colour format

// File names must be alpha-numeric characters (0-9, a-z, A-Z) or "/" underscore "_"
// other ascii characters are stripped out by client, including / generates
// sub-directories
#define DEFAULT_FILENAME "tft_screenshots/screenshot"  // In case none is specified
#define FILE_TYPE "png"                                // jpg, bmp, png, tif are valid

// Filename extension
// '#' = add incrementing number, '@' = add timestamp, '%' add millis() timestamp,
// '*' = add nothing
// '@' and '%' will generate new unique filenames, so beware of cluttering up your
// hard drive with lots of images! The PC client sketch is set to limit the number of
// saved images to 1000 and will then prompt for a restart.
#define FILE_EXT '@'

// Number of pixels to send in a burst (minimum of 1), no benefit above 8
// NPIXELS values and render times:
// NPIXELS 1 = use readPixel() = >5s and 16-bit pixels only
// NPIXELS >1 using rectRead() 2 = 1.75s, 4 = 1.68s, 8 = 1.67s
#define NPIXELS 8  // Must be integer division of both TFT width and TFT height

bool screenServer(void);
bool screenServer(String filename);
bool serialScreenServer(String filename);
void sendParameters(String filename);
void rainbow_fill();

//====================================================================================
//                           Screen server call with no filename
//====================================================================================
// Start a screen dump server (serial or network) - no filename specified
bool screenServer(void) {
    // With no filename the screenshot will be saved with a default name e.g. tft_screen_#.xxx
    // where # is a number 0-9 and xxx is a file type specified below
    return screenServer(DEFAULT_FILENAME);
}

//====================================================================================
//                           Screen server call with filename
//====================================================================================
// Start a screen dump server (serial or network) - filename specified
bool screenServer(String filename) {
    delay(0);  // Equivalent to yield() for ESP8266;

    bool result = serialScreenServer(filename);  // Screenshot serial port server

    delay(0);  // Equivalent to yield()

    return result;
}

//====================================================================================
//                Serial server function that sends the data to the client
//====================================================================================
bool serialScreenServer(String filename) {
    // Precautionary receive buffer garbage flush for 50ms
    uint32_t clearTime = millis() + 50;
    while (millis() < clearTime && Serial.read() >= 0)
        delay(0);  // Equivalent to yield() for ESP8266;

    bool wait = true;
    uint32_t lastCmdTime = millis();  // Initialise start of command time-out

    // Wait for the starting flag with a start time-out
    while (wait) {
        delay(0);  // Equivalent to yield() for ESP8266;
        // Check serial buffer
        if (Serial.available() > 0) {
            // Read the command byte
            uint8_t cmd = Serial.read();
            // If it is 'S' (start command) then clear the serial buffer for 100ms and stop waiting
            if (cmd == 'S') {
                // Precautionary receive buffer garbage flush for 50ms
                clearTime = millis() + 50;
                while (millis() < clearTime && Serial.read() >= 0)
                    delay(0);  // Equivalent to yield() for ESP8266;

                wait = false;            // No need to wait anymore
                lastCmdTime = millis();  // Set last received command time

                // Send screen size etc.using a simple header with delimiters for client checks
                sendParameters(filename);
            }
        } else {
            // Check for time-out
            if (millis() > lastCmdTime + START_TIMEOUT)
                return false;
        }
    }

    uint8_t color[3 * NPIXELS];  // RGB and 565 format color buffer for N pixels

    // Send all the pixels on the whole screen
    for (uint32_t y = 0; y < tft.height(); y++) {
        // Increment x by NPIXELS as we send NPIXELS for every byte received
        for (uint32_t x = 0; x < tft.width(); x += NPIXELS) {
            delay(0);  // Equivalent to yield() for ESP8266;

            // Wait here for serial data to arrive or a time-out elapses
            while (Serial.available() == 0) {
                if (millis() > lastCmdTime + PIXEL_TIMEOUT)
                    return false;
                delay(0);  // Equivalent to yield() for ESP8266;
            }

            // Serial data must be available to get here, read 1 byte and
            // respond with N pixels, i.e. N x 3 RGB bytes or N x 2 565 format bytes
            if (Serial.read() == 'X') {
                // X command byte means abort, so clear the buffer and return
                clearTime = millis() + 50;
                while (millis() < clearTime && Serial.read() >= 0)
                    delay(0);  // Equivalent to yield() for ESP8266;
                return false;
            }
            // Save arrival time of the read command (for later time-out check)
            lastCmdTime = millis();

#if defined BITS_PER_PIXEL && BITS_PER_PIXEL >= 24 && NPIXELS > 1
            // Fetch N RGB pixels from x,y and put in buffer
            tft.readRectRGB(x, y, NPIXELS, 1, color);
            // Send buffer to client
            Serial.write(color, 3 * NPIXELS);  // Write all pixels in the buffer
#else
            // Fetch N 565 format pixels from x,y and put in buffer
            if (NPIXELS > 1)
                tft.readRect(x, y, NPIXELS, 1, (uint16_t *)color);
            else {
                uint16_t c = tft.readPixel(x, y);
                color[0] = c >> 8;
                color[1] = c & 0xFF;  // Swap bytes
            }
            // Send buffer to client
            Serial.write(color, 2 * NPIXELS);  // Write all pixels in the buffer
#endif
        }
    }

    Serial.flush();  // Make sure all pixel bytes have been despatched

    return true;
}

//====================================================================================
//    Send screen size etc.using a simple header with delimiters for client checks
//====================================================================================
void sendParameters(String filename) {
    Serial.write('W');  // Width
    Serial.write(tft.width() >> 8);
    Serial.write(tft.width() & 0xFF);

    Serial.write('H');  // Height
    Serial.write(tft.height() >> 8);
    Serial.write(tft.height() & 0xFF);

    Serial.write('Y');  // Bits per pixel (16 or 24)
    if (NPIXELS > 1)
        Serial.write(BITS_PER_PIXEL);
    else
        Serial.write(16);  // readPixel() only provides 16-bit values

    Serial.write('?');  // Filename next
    Serial.print(filename);

    Serial.write('.');  // End of filename marker

    Serial.write(FILE_EXT);  // Filename extension identifier

    Serial.write(*FILE_TYPE);  // First character defines file type j,b,p,t
}

void screen_server_task(void *pvParameters) {
    while (1) {
        /* Delay first so there's something to show */
        vTaskDelay(SCREEN_SERVER_UPDATE_PERIOD / portTICK_PERIOD_MS);

        /* Only perform this if we have TFT control
         * Taking out this semaphore will also block
         * other tasks from writing to the screen
         */
        if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
            screenServer();
            xSemaphoreGive(TFTLock);
        }
    }
}

void screen_server_init() {
    /* Disable logging to avoid polluting serial Tx */
    esp_log_level_set("*", ESP_LOG_NONE);

    xTaskCreatePinnedToCore(
        screen_server_task,
        "Screen Server",
        SCREEN_SERVER_STACK,
        NULL,
        SCREEN_SERVER_PRIORITY,
        NULL,
        APP_CORE
    );
}