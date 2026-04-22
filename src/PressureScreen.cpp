#include "PressureScreen.h"

PressureScreen::PressureScreen(int width, int height)
    : Screen(width, height, "PRESSURE") {}

void PressureScreen::draw(State &state) {
    const int MAX  = State::PRESSURE_HISTORY_SIZE;
    const int cnt  = state.pressureCount;
    const int head = state.pressureHead;

    canvas->fillScreen(TFT_BLACK);

    // Title bar
    canvas->fillRoundRect(0, 0, width, 24, 12, TFT_NAVY);
    canvas->setFont(&fonts::FreeSans12pt7b);
    canvas->setTextDatum(TC_DATUM);
    canvas->setTextColor(TFT_WHITE, TFT_NAVY);
    canvas->drawString("PRESSURE (1h)", width / 2, 4);

    // Current value
    canvas->setFont(&fonts::FreeSans12pt7b);
    canvas->setTextDatum(TL_DATUM);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f hPa", state.pressure);
    canvas->drawString(buf, 10, 34);

    // Graph area
    const int PX = 35, PY = 70, PW = 270, PH = 140;
    canvas->drawRect(PX, PY, PW, PH, TFT_DARKGREY);

    if (cnt < 2) {
        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextDatum(MC_DATUM);
        canvas->setTextColor(TFT_DARKGREY, TFT_BLACK);
        canvas->drawString("Collecting data...", PX + PW / 2, PY + PH / 2);
        canvas->pushSprite(0, 0);
        return;
    }

    // Find min/max across buffer
    float minP = state.pressureHistory[head], maxP = state.pressureHistory[head];
    for (int i = 1; i < cnt; i++) {
        float v = state.pressureHistory[(head + i) % MAX];
        if (v < minP) minP = v;
        if (v > maxP) maxP = v;
    }
    float range = maxP - minP;
    if (range < 2.0f) {
        float mid = (maxP + minP) / 2.0f;
        minP = mid - 1.0f;
        maxP = mid + 1.0f;
        range = 2.0f;
    }

    // Y axis labels
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas->setTextDatum(MR_DATUM);
    snprintf(buf, sizeof(buf), "%.0f", maxP);
    canvas->drawString(buf, PX - 2, PY + 5);
    snprintf(buf, sizeof(buf), "%.0f", minP);
    canvas->drawString(buf, PX - 2, PY + PH - 5);

    // X axis labels
    canvas->setTextDatum(TL_DATUM);
    snprintf(buf, sizeof(buf), "%ds ago", cnt - 1);
    canvas->drawString(buf, PX, PY + PH + 4);
    canvas->setTextDatum(TR_DATUM);
    canvas->drawString("now", PX + PW, PY + PH + 4);

    // Data line
    for (int i = 1; i < cnt; i++) {
        float v0 = state.pressureHistory[(head + i - 1) % MAX];
        float v1 = state.pressureHistory[(head + i)     % MAX];
        int x0 = PX + (i - 1) * (PW - 1) / (cnt - 1);
        int x1 = PX +  i      * (PW - 1) / (cnt - 1);
        int y0 = PY + PH - (int)((v0 - minP) / range * PH);
        int y1 = PY + PH - (int)((v1 - minP) / range * PH);
        canvas->drawLine(x0, y0, x1, y1, TFT_CYAN);
    }

    canvas->pushSprite(0, 0);
}

int PressureScreen::run(const m5::touch_detail_t &t, State &state) {
    if (t.wasFlicked()) {
        if (t.distanceX() > 5) return 0;  // swipe right → Meteo
        if (t.distanceX() < -5) return 2;  // swipe left → GNSS
    }
    unsigned long now = millis();
    if ((now - lastDraw) >= 1000UL) {
        draw(state);
        lastDraw = now;
    }
    return -1;
}
