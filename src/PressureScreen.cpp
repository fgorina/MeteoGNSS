#include "PressureScreen.h"

PressureScreen::PressureScreen(int width, int height)
    : Screen(width, height, "PRESSURE") {}

// Human-readable elapsed-time label for the X-axis left edge.
static String ageLabel(int minutes) {
    if (minutes < 60) return String(minutes) + " min";
    int h = minutes / 60, m = minutes % 60;
    if (m == 0) return String(h) + "h";
    return String(h) + "h " + String(m) + "m";
}

void PressureScreen::draw(State &state) {
    canvas->fillScreen(TFT_BLACK);

    // -- Select buffer based on current view -----------------------------------
    const float *buf;
    int          head, count, maxSize;
    float        minPerSample;

    if (viewHours == 3) {
        buf          = state.pressure1m;
        head         = state.pressure1mHead;
        count        = state.pressure1mCount;
        maxSize      = State::PRESSURE_SIZE_1M;
        minPerSample = State::PRESSURE_MIN_PER_SAMPLE_1M;
    } else {
        int samplesMax = (viewHours == 12) ? 144 : State::PRESSURE_SIZE_5M;
        count          = (state.pressure5mCount < samplesMax) ? state.pressure5mCount : samplesMax;
        int skip       = state.pressure5mCount - count;
        head           = (state.pressure5mHead + skip) % State::PRESSURE_SIZE_5M;
        buf            = state.pressure5m;
        maxSize        = State::PRESSURE_SIZE_5M;
        minPerSample   = State::PRESSURE_MIN_PER_SAMPLE_5M;
    }

    // -- Title bar -------------------------------------------------------------
    char titleBuf[24];
    snprintf(titleBuf, sizeof(titleBuf), "PRESSURE %dh", viewHours);
    canvas->fillRoundRect(0, 0, width, 24, 12, TFT_NAVY);
    canvas->setFont(&fonts::FreeSans12pt7b);
    canvas->setTextDatum(TC_DATUM);
    canvas->setTextColor(TFT_WHITE, TFT_NAVY);
    canvas->drawString(titleBuf, width / 2, 4);

    // -- Current pressure ------------------------------------------------------
    char buf32[48];
    snprintf(buf32, sizeof(buf32), "%.1f hPa", state.pressure);
    canvas->setFont(&fonts::FreeSans12pt7b);
    canvas->setTextDatum(TL_DATUM);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->drawString(buf32, 10, 30);

    // -- Tendency (hidden when data is insufficient) ---------------------------
    float tendency = state.tendencyHPaPerHour(viewHours);
    if (!isnan(tendency)) {
        canvas->setFont(&fonts::FreeSans9pt7b);
        uint16_t col = (tendency >= 1.0f)  ? TFT_YELLOW :
                       (tendency >  0.0f)  ? TFT_GREEN  :
                       (tendency <= -1.0f) ? TFT_RED    : TFT_YELLOW;
        canvas->setTextColor(col, TFT_BLACK);
        snprintf(buf32, sizeof(buf32), "%+.2f hPa/h  %s",
                 tendency, state.tendencyLabel(viewHours));
        canvas->drawString(buf32, 10, 54);
    }

    // -- Graph -----------------------------------------------------------------
    const int PX = 38, PY = 76, PW = 272, PH = 136;
    canvas->drawRect(PX, PY, PW, PH, TFT_DARKGREY);

    if (count < 2) {
        // Not enough data yet - show the empty graph border silently.
        canvas->pushSprite(0, 0);
        return;
    }

    // Find min/max across visible window
    float minP = buf[head % maxSize], maxP = minP;
    for (int i = 1; i < count; i++) {
        float v = buf[(head + i) % maxSize];
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
    snprintf(buf32, sizeof(buf32), "%.0f", maxP);
    canvas->drawString(buf32, PX - 2, PY + 6);
    snprintf(buf32, sizeof(buf32), "%.0f", minP);
    canvas->drawString(buf32, PX - 2, PY + PH - 6);

    // X axis labels
    int spanMin = (int)((count - 1) * minPerSample);
    canvas->setTextDatum(TL_DATUM);
    canvas->drawString(ageLabel(spanMin), PX, PY + PH + 4);
    canvas->setTextDatum(TR_DATUM);
    canvas->drawString("ara", PX + PW, PY + PH + 4);

    // Data line
    for (int i = 1; i < count; i++) {
        float v0 = buf[(head + i - 1) % maxSize];
        float v1 = buf[(head + i)     % maxSize];
        int x0 = PX + (i - 1) * (PW - 1) / (count - 1);
        int x1 = PX +  i      * (PW - 1) / (count - 1);
        int y0 = PY + PH - 1 - (int)((v0 - minP) / range * (PH - 1));
        int y1 = PY + PH - 1 - (int)((v1 - minP) / range * (PH - 1));
        canvas->drawLine(x0, y0, x1, y1, TFT_CYAN);
    }

    canvas->pushSprite(0, 0);
}

int PressureScreen::run(const m5::touch_detail_t &t, State &state) {
    if (t.wasFlicked()) {
        if (t.distanceX() > 5)  return 0;   // swipe right -> Meteo
        if (t.distanceX() < -5) return 2;   // swipe left  -> GNSS
    } else if (t.wasClicked()) {
        // Cycle view: 3h -> 12h -> 24h -> 3h
        if      (viewHours == 3)  viewHours = 12;
        else if (viewHours == 12) viewHours = 24;
        else                      viewHours = 3;
        draw(state);
        lastDraw = millis();
        return -1;
    }
    unsigned long now = millis();
    if ((now - lastDraw) >= 1000UL) {
        draw(state);
        lastDraw = now;
    }
    return -1;
}
