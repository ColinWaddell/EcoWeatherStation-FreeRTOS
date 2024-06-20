#ifndef FIFO_GRAPH_H
#define FIFO_GRAPH_H

#include <TFT_eSPI.h>
#include <queue>
typedef std::function<uint16_t(float)> ColorFunction;

class FIFOGraph {
private:
    TFT_eSprite *sprite;
    int x, y;
    int width, height;
    int length;
    float yMin, yMax;
    float stepSize;
    float originalYMin, originalYMax;
    std::queue<float> data;
    uint32_t colour;

    void drawGraph();
    void adjustBounds();

public:
    FIFOGraph(
        TFT_eSprite *spr,
        int w,
        int h,
        int x,
        int y,
        float minY,
        float maxY,
        float step,
        uint32_t colour
    );

    ~FIFOGraph();

    void addDataPoint(float newPoint);
    void resetGraph();
};

#endif
