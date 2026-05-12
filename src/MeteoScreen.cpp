#include "MeteoScreen.h"
#include <math.h>

MeteoScreen::MeteoScreen(int width, int height)
    : Screen(width, height, "METEO") {}

// -- Helpers -------------------------------------------------------------------

static const char *toCardinal(float deg) {
    if (deg >= 337.5f || deg < 22.5f)  return "N";
    if (deg <  67.5f)                   return "NE";
    if (deg < 112.5f)                   return "E";
    if (deg < 157.5f)                   return "SE";
    if (deg < 202.5f)                   return "S";
    if (deg < 247.5f)                   return "SW";
    if (deg < 292.5f)                   return "W";
    return "NW";
}

// Draw a filled trend arrow of `size` pixels; no-op when data is unavailable.
// Colour rules (tendency in hPa/h; 1 hPa/h == 3 hPa/3h):
//   Rising  < 1 hPa/h  -> green   (normal rise)
//   Rising   1 hPa/h  -> yellow  (fast rise)
//   Falling > 1 hPa/h -> yellow  (moderate fall)
//   Falling  1 hPa/h -> red     (rapid fall)
static void drawTrendArrow(M5Canvas *canvas, int x, int y, int size, float tendency) {
    if (isnan(tendency)) return;

    if (tendency > 0.0f) {
        uint16_t col = (tendency >= 1.0f) ? TFT_YELLOW : TFT_GREEN;
        canvas->fillTriangle(x + size / 2, y,
                             x,            y + size,
                             x + size,     y + size,
                             col);
    } else if (tendency < 0.0f) {
        uint16_t col = (tendency <= -1.0f) ? TFT_RED : TFT_YELLOW;
        canvas->fillTriangle(x + size / 2, y + size,
                             x,            y,
                             x + size,     y,
                             col);
    }
}

// Wind arrow uses simpler colours: building -> yellow, easing -> green.
static void drawWindArrow(M5Canvas *canvas, int x, int y, int size, float knPerMin) {
    if (isnan(knPerMin) || fabsf(knPerMin) < 0.05f) return;
    if (knPerMin > 0.0f) {
        canvas->fillTriangle(x + size / 2, y,
                             x,            y + size,
                             x + size,     y + size,
                             TFT_YELLOW);  // building
    } else {
        canvas->fillTriangle(x + size / 2, y + size,
                             x,            y,
                             x + size,     y,
                             TFT_GREEN);   // easing
    }
}

// -- TWS local history ---------------------------------------------------------

void MeteoScreen::addTwsSample(float tws_ms) {
    float kn = tws_ms * 1.94384f;
    if (twsCount < TWS_HISTORY) {
        twsHistory[twsCount++] = kn;
    } else {
        twsHistory[twsHead] = kn;
        twsHead = (twsHead + 1) % TWS_HISTORY;
    }
}

float MeteoScreen::twsTendency() const {
    if (twsCount < 10) return NAN;
    float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int n = twsCount;
    for (int i = 0; i < n; i++) {
        float xi = (float)i;
        float y  = twsHistory[(twsHead + i) % TWS_HISTORY];
        sumX  += xi;  sumY  += y;
        sumXY += xi * y;  sumX2 += xi * xi;
    }
    float denom = (float)n * sumX2 - sumX * sumX;
    if (fabsf(denom) < 1e-6f) return 0.0f;
    return ((float)n * sumXY - sumX * sumY) / denom;  // kn per sample (per second)
}

// -- draw ----------------------------------------------------------------------

void MeteoScreen::draw(State &state) {
    canvas->fillScreen(TFT_BLACK);

    // -- Title bar -------------------------------------------------------------
    canvas->fillRoundRect(0, 0, width, 24, 12, TFT_NAVY);
    canvas->setFont(&fonts::FreeSans12pt7b);
    canvas->setTextDatum(TC_DATUM);
    canvas->setTextColor(TFT_WHITE, TFT_NAVY);
    canvas->drawString("METEO", width / 2, 4);

    char buf[32];
    const int cx        = width / 2;
    const int arrowSize = 24;

    // -- Pressure - upper half (y 24-132) -------------------------------------
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(TC_DATUM);
    canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas->drawString("Pressure", cx, 34);

    snprintf(buf, sizeof(buf), "%.1f hPa", state.pressure);
    canvas->setFont(&fonts::FreeSans18pt7b);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);

    int textW  = canvas->textWidth(buf);
    int totalW = textW + 11 + arrowSize;
    int startX = (width - totalW) / 2;
    int valueY = 62;

    canvas->setTextDatum(TL_DATUM);
    canvas->drawString(buf, startX, valueY);

    arrowX = startX + textW + 11;
    arrowY = valueY + 2;
    drawTrendArrow(canvas, arrowX, arrowY, arrowSize,
                   state.tendencyHPaPerHour(tendencyHours));

    // Period label under the arrow
    char periodBuf[5];
    snprintf(periodBuf, sizeof(periodBuf), "%dh", tendencyHours);
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(TC_DATUM);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->drawString(periodBuf, arrowX + arrowSize / 2, arrowY + arrowSize + 3);

    // External temperature (if available)
    if (!isnan(state.outsideTemperature)) {
        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextDatum(CL_DATUM);
        canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        snprintf(buf, sizeof(buf), "Ext: %.1f C", state.outsideTemperature);
        canvas->drawString(buf, 20, 110);
    }

    // -- Divider ---------------------------------------------------------------
    canvas->drawLine(20, 132, width - 20, 132, TFT_DARKGREY);

    // -- True Wind - lower half (y 132-240) -----------------------------------
    // Layout: TWD left-aligned, TWS right-aligned, both vertically centred.
    // Value area: y 154-240 = 86 px.  Centre at y = 197.
    // FreeSans18pt7b cap height  26 px -> top at 197 - 13 = 184.

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(TC_DATUM);
    canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas->drawString("True Wind", cx, 140);

    bool haveWind = !isnan(state.windSpeedTrue) && !isnan(state.windDirectionTrue);

    const int LM   = 12;   // left margin
    const int valY = 179;  // top of large-font values

    // Pre-compute direction so both sides can use it
    float      deg  = NAN;
    const char *card = "";
    if (haveWind) {
        deg = state.windDirectionTrue * (180.0f / (float)M_PI);
        if (deg < 0) deg += 360.0f;
        card = toCardinal(deg);
    }

    // -- Left: TWD degrees + cardinal -----------------------------------------
    canvas->setFont(&fonts::FreeSans18pt7b);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setTextDatum(TL_DATUM);
    if (haveWind) {
        snprintf(buf, sizeof(buf), "%.0f", deg);
    } else {
        snprintf(buf, sizeof(buf), "---");
    }
    canvas->drawString(buf, LM, valY);

    if (haveWind) {
        int degW = canvas->textWidth(buf);  // still FreeSans18pt7b
        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        canvas->drawString(card, LM + degW + 6, valY + 6);
    }

    // -- Right: TWS  ---------------------------------------------------------
    // Order from right edge: [arrow][11px][speed 18pt]
    canvas->setFont(&fonts::FreeSans18pt7b);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    if (haveWind) {
        float kn = state.windSpeedTrue * 1.94384f;
        snprintf(buf, sizeof(buf), "%.1f kn", kn);
    } else {
        snprintf(buf, sizeof(buf), "---");
    }
    int speedW = canvas->textWidth(buf);

    // Centre the [speed + 11px gap + arrow] block in the right half
    int blockW    = speedW + 11 + arrowSize;
    int speedX    = (width + cx - blockW) / 2;
    int twsArrowX = speedX + speedW + 11;

    canvas->setTextDatum(TL_DATUM);
    canvas->drawString(buf, speedX, valY);

    if (haveWind) {
        drawWindArrow(canvas, twsArrowX, valY + 2, arrowSize, twsTendency());
    }

    canvas->pushSprite(0, 0);
}

// -- run -----------------------------------------------------------------------

int MeteoScreen::run(const m5::touch_detail_t &t, State &state) {
    if (t.wasFlicked()) {
        if (t.distanceX() > 5)  return 3;  // swipe right -> Summary
        if (t.distanceX() < -5) return 1;  // swipe left  -> Pressure
    } else if (t.wasClicked()) {
        // Tap on the pressure trend arrow + label area -> cycle tendency period
        bool inArrow = (t.x >= arrowX - 6) && (t.x <= arrowX + arrowSize + 6) &&
                       (t.y >= arrowY - 6) && (t.y <= arrowY + arrowSize + 20);
        if (inArrow) {
            if      (tendencyHours == 3)  tendencyHours = 12;
            else if (tendencyHours == 12) tendencyHours = 24;
            else                          tendencyHours = 3;
            draw(state);
            lastDraw = millis();
            return -1;
        }
    }
    unsigned long now = millis();
    if ((now - lastDraw) >= 1000UL) {
        if (!isnan(state.windSpeedTrue))
            addTwsSample(state.windSpeedTrue);
        draw(state);
        lastDraw = now;
    }
    return -1;
}
