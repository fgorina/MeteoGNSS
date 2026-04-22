#include "MeteoScreen.h"

MeteoScreen::MeteoScreen(int width, int height)
    : Screen(width, height, "METEO") {}

void MeteoScreen::draw(State &state) {
    canvas->fillScreen(TFT_BLACK);

    // Title bar
    canvas->fillRoundRect(0, 0, width, 24, 12,TFT_NAVY);
    canvas->setFont(&fonts::FreeSans12pt7b);
    canvas->setTextDatum(TC_DATUM);
    canvas->setTextColor(TFT_WHITE, TFT_NAVY);
    canvas->drawString("METEO", width / 2, 4);

    char buf[32];
    const int cx = width / 2;

    // Temperature — upper half (y 24–132)
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(TC_DATUM);
    canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas->drawString("Temperature", cx, 44);
    canvas->setFont(&fonts::FreeSans18pt7b);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%.1f C", state.temperature);
    canvas->drawString(buf, cx, 68);

    // Divider
    canvas->drawLine(20, 132, width - 20, 132, TFT_DARKGREY);

    // Pressure — lower half (y 132–240)
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(TC_DATUM);
    canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas->drawString("Pressure", cx, 152);
    canvas->setFont(&fonts::FreeSans18pt7b);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%.1f hPa", state.pressure);
    canvas->drawString(buf, cx, 176);

    canvas->pushSprite(0, 0);
}

int MeteoScreen::run(const m5::touch_detail_t &t, State &state) {
    if (t.wasFlicked()) {
        if (t.distanceX() > 5) return 2;  // swipe right → GNSS
        if (t.distanceX() < -5) return 1;  // swipe left → Pressure
    }
    unsigned long now = millis();
    if ((now - lastDraw) >= 1000UL) {
        draw(state);
        lastDraw = now;
    }
    return -1;
}
