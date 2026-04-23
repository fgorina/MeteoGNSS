#include "GNSSScreen.h"

GNSSScreen::GNSSScreen(int width, int height)
    : Screen(width, height, "GNSS") {}

void GNSSScreen::draw(State &state) {
    canvas->fillScreen(TFT_BLACK);

    // Title bar
    canvas->fillRoundRect(0, 0, width, 24, 12, TFT_NAVY);
    canvas->setFont(&fonts::FreeSans12pt7b);
    canvas->setTextDatum(TC_DATUM);
    canvas->setTextColor(TFT_WHITE, TFT_NAVY);
    canvas->drawString("GNSS", width / 2, 4);

    char buf[48];
    const int cx = width / 2;

    // --- Top section: Lat / Lon ---
    if (!state.gpsValid) {
        canvas->setFont(&fonts::FreeSans12pt7b);
        canvas->setTextDatum(MC_DATUM);
        canvas->setTextColor(TFT_DARKGREY, TFT_BLACK);
        canvas->drawString("No GPS fix", cx, 75);
    } else {
        canvas->setFont(&fonts::FreeSans18pt7b);
        canvas->setTextDatum(TC_DATUM);
        canvas->setTextColor(TFT_WHITE, TFT_BLACK);

        double lat = state.lat;
        char ns = lat >= 0 ? 'N' : 'S';
        if (lat < 0) lat = -lat;
        int latDeg = (int)lat;
        double latMin = (lat - latDeg) * 60.0;
        snprintf(buf, sizeof(buf), "%d %06.3f %c", latDeg, latMin, ns);
        canvas->drawString(buf, cx, 35);

        double lon = state.lon;
        char ew = lon >= 0 ? 'E' : 'W';
        if (lon < 0) lon = -lon;
        int lonDeg = (int)lon;
        double lonMin = (lon - lonDeg) * 60.0;
        snprintf(buf, sizeof(buf), "%03d %06.3f %c", lonDeg, lonMin, ew);
        canvas->drawString(buf, cx, 75);
    }

    // Divider
    canvas->drawLine(20, 121, width - 20, 121, TFT_DARKGREY);

    // --- Bottom section: COG/SOG and Time ---
    canvas->setTextDatum(TC_DATUM);

    if (state.gpsValid) {
        float sog = state.speed / 1.852f;
        snprintf(buf, sizeof(buf), "COG %.1f    SOG %.1f kt", state.course, sog);
        canvas->setFont(&fonts::FreeSans12pt7b);
        canvas->setTextColor(TFT_WHITE, TFT_BLACK);
        canvas->drawString(buf, cx, 129);
    }

    if (state.timeValid) {
        canvas->setFont(&fonts::FreeSans12pt7b);
        canvas->setTextColor(TFT_WHITE, TFT_BLACK);
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d UTC", state.hour, state.minute, state.second);
        canvas->drawString(buf, cx, 158);
        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        snprintf(buf, sizeof(buf), "%04d/%02d/%02d", state.year, state.month, state.day);
        canvas->drawString(buf, cx, 183);
    } else {
        canvas->setFont(&fonts::FreeSans12pt7b);
        canvas->setTextColor(TFT_DARKGREY, TFT_BLACK);
        canvas->drawString("No time fix", cx, 158);
    }

    // --- Info bar: Sats left, HDOP right ---
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas->setTextDatum(TL_DATUM);
    snprintf(buf, sizeof(buf), "Sats: %d", state.satellites);
    canvas->drawString(buf, 10, 208);
    canvas->setTextDatum(TR_DATUM);
    snprintf(buf, sizeof(buf), "HDOP: %.1f", state.hdop);
    canvas->drawString(buf, width - 10, 208);

    canvas->pushSprite(0, 0);
}

int GNSSScreen::run(const m5::touch_detail_t &t, State &state) {
    if (t.wasFlicked()) {
        if (t.distanceX() > 5) return 1;  // swipe right -> Pressure
        if (t.distanceX() < -5) return 3;  // swipe left -> Summary
    }
    unsigned long now = millis();
    if ((now - lastDraw) >= 1000UL) {
        draw(state);
        lastDraw = now;
    }
    return -1;
}
