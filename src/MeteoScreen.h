#pragma once
#include "Screen.h"

class MeteoScreen : public Screen {
public:
    MeteoScreen(int width, int height);
    void draw(State &state) override;
    int  run(const m5::touch_detail_t &t, State &state) override;

private:
    unsigned long lastDraw = 0;
};
