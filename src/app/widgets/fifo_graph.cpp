#include "fifo_graph.h"
#include "app/colours.h"
#include "app/tft.h"

#define GRAPH_PIXEL_SPACING 3
#define FIFO_INIT_VAL -999.0

// Constructor with colors
FIFOGraph::FIFOGraph(
    TFT_eSprite *spr,
    int w,
    int h,
    int x,
    int y,
    float minY,
    float maxY,
    float step,
    uint32_t colour
)
    : sprite(spr)
    , width(w)
    , height(h)
    , x(x)
    , y(y)
    , yMin(minY)
    , yMax(maxY)
    , stepSize(step)
    , originalYMin(minY)
    , originalYMax(maxY)
    , length(width / GRAPH_PIXEL_SPACING)
    , colour(colour) {
    sprite->createSprite(width, height);
    sprite->setColorDepth(4);
    sprite->fillSprite(COLOURS_GRAPH_BACKGROUND);

    for (int i = 0; i < length; ++i) {
        data.push(FIFO_INIT_VAL);
    }
}

FIFOGraph::~FIFOGraph() {
    sprite->deleteSprite();
}

void FIFOGraph::drawGraph() {
    sprite->fillSprite(COLOURS_GRAPH_BACKGROUND);  // Clear the sprite
    float yRange = yMax - yMin;

    int x_tmp = 0;
    int y_tmp;
    int height_tmp = height - 1;
    int y_0 = height_tmp - (height_tmp * (0.0f - yMin) / yRange);
    int y_pixel;

    std::queue<float> temp = data;  // Create a temporary copy to iterate through the data

    while (!temp.empty()) {
        float point = temp.front();
        temp.pop();

        if (point == FIFO_INIT_VAL) {
            y_tmp = height_tmp - (height_tmp * (0.0f - yMin) / yRange);
        } else {
            y_tmp = height_tmp - (height_tmp * (point - yMin) / yRange);
        }

        for (y_pixel = y_0; y_pixel >= y_tmp; y_pixel -= GRAPH_PIXEL_SPACING) {
            sprite->drawPixel(x_tmp, y_pixel, COLOURS_GRAPH_FADED);
        }
        if (point != FIFO_INIT_VAL) {
            sprite->drawPixel(x_tmp, y_pixel + GRAPH_PIXEL_SPACING, colour);
        }

        x_tmp += GRAPH_PIXEL_SPACING;
    }

    char y_min_str[10] = { 0 };
    char y_mid_str[10] = { 0 };
    char y_max_str[10] = { 0 };
    snprintf(y_min_str, sizeof(y_min_str), "%0.1f", yMin);
    snprintf(y_mid_str, sizeof(y_mid_str), "%0.1f", (yMax - yMin) / 2.0f);
    snprintf(y_max_str, sizeof(y_max_str), "%0.1f", yMax);

    sprite->setTextColor(COLOURS_GRAPH_FOREGROUND, COLOURS_GRAPH_BACKGROUND);
    sprite->setTextFont(0);
    sprite->setTextSize(1);
    sprite->setTextDatum(TL_DATUM);
    sprite->drawString(y_max_str, 0, 0);
    sprite->setTextDatum(ML_DATUM);
    sprite->drawString(y_mid_str, 0, sprite->height() / 2);
    sprite->setTextDatum(BL_DATUM);
    sprite->drawString(y_min_str, 0, sprite->height() - 1);

    if (xSemaphoreTake(TFTLock, portMAX_DELAY) == pdTRUE) {
        sprite->pushSprite(x, y);
        xSemaphoreGive(TFTLock);
    } else {
        log_e("Can't unlock TFT Semaphore");
    }
}

void FIFOGraph::adjustBounds() {
    float newYMin = data.front();
    float newYMax = data.front();
    std::queue<float> temp = data;  // Create a temporary copy to iterate through the data

    while (!temp.empty()) {
        float point = temp.front();
        temp.pop();
        if (point < newYMin)
            newYMin = point;
        if (point > newYMax)
            newYMax = point;
    }

    // Round down the minimum bound and round up the maximum bound to the nearest step size
    yMin = std::floor(newYMin / stepSize) * stepSize;
    yMax = std::ceil(newYMax / stepSize) * stepSize;
}

void FIFOGraph::addDataPoint(float newPoint) {
    if (data.size() >= length) {
        data.pop();
    }
    data.push(newPoint);

    if (newPoint < yMin || newPoint > yMax) {
        adjustBounds();
    }

    drawGraph();
}

void FIFOGraph::resetGraph() {
    std::queue<float> empty;
    std::swap(data, empty);  // Clear the queue
    yMin = originalYMin;
    yMax = originalYMax;
    drawGraph();
}