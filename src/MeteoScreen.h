#pragma once
#include "Screen.h"

class MeteoScreen : public Screen {
public:
    MeteoScreen(int width, int height);
    void draw(State &state) override;
    int  run(const m5::touch_detail_t &t, State &state) override;

private:
    unsigned long lastDraw = 0;

    // Pressure tendency period - cycles 3 -> 12 -> 24 -> 3 on arrow tap
    int tendencyHours = 3;

    // Stored position of the pressure trend arrow for tap hit-testing
    int arrowX    = 0;
    int arrowY    = 0;
    int arrowSize = 24;

    // Local TWS history for tendency (1 sample/s in run())
    static constexpr int TWS_HISTORY = 60;
    float twsHistory[TWS_HISTORY] = {};
    int   twsHead  = 0;
    int   twsCount = 0;

    void  addTwsSample(float tws_ms);
    float twsTendency() const;   // kn per sample; NAN when < 10 samples
};
