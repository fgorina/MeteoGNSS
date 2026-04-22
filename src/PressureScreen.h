#pragma once
#include "Screen.h"

class PressureScreen : public Screen {
public:
    PressureScreen(int width, int height);
    void draw(State &state) override;
    int  run(const m5::touch_detail_t &t, State &state) override;

private:
    unsigned long lastDraw = 0;
};
