#include "SummaryScreen.h"
#include <math.h>

SummaryScreen::SummaryScreen(int width, int height)
    : Screen(width, height, "SUMMARY") {}

// Uncomment to inject synthetic data for visual testing (remove for production):
//#define SIMULATE_DATA

// -- Constants -----------------------------------------------------------------
static constexpr float STD_HPA    = 1013.25f;
static constexpr float CENTER_KN  = 15.0f;   // TWS at plot centre
static constexpr float HALF_KN    = 15.0f;   // plot radius in knots (0-30 kn)
static constexpr float MIN_HRANGE = 10.0f;   // minimum pressure half-range (hPa)

// Circle shifted left so the right panel fits without a title bar.
static constexpr int CX     = 110;
static constexpr int CY     = 120;
static constexpr int R_OUT  = 105;  // outer edge of colour ring
static constexpr int R_DATA = 95;   // inner data-plot radius (ring width = 10)

static constexpr int PNL_X  = 267;  // centre x of right info panel (x 218-315)

// -- Colour helpers ------------------------------------------------------------

static uint16_t twdColor(float twd_rad) {
    static const uint8_t R[8] = {  0,   0,   0, 255, 255, 255, 255, 128 };
    static const uint8_t G[8] = {  0, 255, 255, 255, 128,   0,   0,   0 };
    static const uint8_t B[8] = {160, 255,   0,   0,   0,   0, 255, 128 };

    float deg = fmodf(twd_rad * (180.0f / (float)M_PI) + 360.0f, 360.0f);
    float t   = deg / 45.0f;
    int   seg = ((int)t + 4) % 8;   // +4 rotates wheel 180 deg: N -> warm colours
    float f   = t - (float)(int)t;
    int   nxt = (seg + 1) % 8;

    uint8_t r  = (uint8_t)((int)R[seg] + (int)(((int)R[nxt] - (int)R[seg]) * f));
    uint8_t g  = (uint8_t)((int)G[seg] + (int)(((int)G[nxt] - (int)G[seg]) * f));
    uint8_t b2 = (uint8_t)((int)B[seg] + (int)(((int)B[nxt] - (int)B[seg]) * f));
    return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b2 >> 3);
}

static uint16_t fadeColor(uint16_t color, float fade) {
    if (fade <= 0.0f) return 0x0000;
    if (fade >= 1.0f) return color;
    uint8_t r = (uint8_t)(((color >> 8) & 0xF8) * fade);
    uint8_t g = (uint8_t)(((color >> 3) & 0xFC) * fade);
    uint8_t b = (uint8_t)(((color << 3) & 0xF8) * fade);
    return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
}

static const char *twdCardinal(float twd_rad) {
    float deg = fmodf(twd_rad * (180.0f / (float)M_PI) + 360.0f, 360.0f);
    if (deg >= 337.5f || deg < 22.5f)  return "N";
    if (deg <  67.5f)                  return "NE";
    if (deg < 112.5f)                  return "E";
    if (deg < 157.5f)                  return "SE";
    if (deg < 202.5f)                  return "S";
    if (deg < 247.5f)                  return "SW";
    if (deg < 292.5f)                  return "W";
    return "NW";
}

// -- Coordinate mapping --------------------------------------------------------

static void dataToScreen(float tws_kn, float p, float halfRange, int &px, int &py) {
    px = CX + (int)((tws_kn - CENTER_KN) / HALF_KN * R_DATA + 0.5f);
    py = CY - (int)((p - STD_HPA)        / halfRange * R_DATA + 0.5f);

    float dx = (float)(px - CX);
    float dy = (float)(py - CY);
    float d2 = dx * dx + dy * dy;
    float limit = (float)(R_DATA - 1);
    if (d2 > limit * limit) {
        float scale = limit / sqrtf(d2);
        px = CX + (int)(dx * scale);
        py = CY + (int)(dy * scale);
    }
}

// -- draw ----------------------------------------------------------------------

void SummaryScreen::draw(State &state) {
    canvas->fillScreen(TFT_BLACK);

    // -- TWD compass colour ring ----------------------------------------------
    // N (0 rad) maps to the top: x = sin(a), y = -cos(a)
    for (int i = 0; i < 720; i++) {
        float ang = (float)i / 720.0f * 2.0f * (float)M_PI;
        uint16_t col = twdColor(ang);
        int x0 = CX + (int)(R_DATA * sinf(ang) + 0.5f);
        int y0 = CY - (int)(R_DATA * cosf(ang) + 0.5f);
        int x1 = CX + (int)(R_OUT  * sinf(ang) + 0.5f);
        int y1 = CY - (int)(R_OUT  * cosf(ang) + 0.5f);
        canvas->drawLine(x0, y0, x1, y1, col);
    }
    // Inner boundary of ring
    canvas->drawCircle(CX, CY, R_DATA, TFT_DARKGREY);

    // -- Crosshair (visible) --------------------------------------------------
    canvas->drawLine(CX - R_DATA + 4, CY, CX + R_DATA - 4, CY, TFT_DARKGREY);
    canvas->drawLine(CX, CY - R_DATA + 4, CX, CY + R_DATA - 4, TFT_DARKGREY);
    canvas->fillCircle(CX, CY, 2, TFT_DARKGREY);

    // -- Quadrant context labels (drawn under the trajectory) -----------------
    // Top-left:  high P + low wind  -> Fair (dim green)
    // Top-right: high P + high wind -> Gusty (dim yellow)
    // Bot-left:  low P  + low wind  -> Watch (dim orange)
    // Bot-right: low P  + high wind -> Storm (dim red)
    {
        uint16_t colFair  = fadeColor(TFT_GREEN,  0.30f);
        uint16_t colGusty = fadeColor(TFT_YELLOW, 0.28f);
        uint16_t colWatch = fadeColor(0xFD20,     0.32f);  // orange
        uint16_t colStorm = fadeColor(TFT_RED,    0.35f);
        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextDatum(TC_DATUM);
        canvas->setTextColor(colFair,  TFT_BLACK);
        canvas->drawString("Fair",  CX - 38, CY - 52);
        canvas->setTextColor(colGusty, TFT_BLACK);
        canvas->drawString("Gusty", CX + 42, CY - 52);
        canvas->setTextColor(colWatch, TFT_BLACK);
        canvas->drawString("Watch", CX - 38, CY + 40);
        canvas->setTextColor(colStorm, TFT_BLACK);
        canvas->drawString("Storm", CX + 42, CY + 40);
    }

    // -- 5-min buffer (up to 24 h) -------------------------------------------
    const float *pressBuf = state.pressure5m;
    const float *twsBuf   = state.wind5mTws;
    const float *twdBuf   = state.wind5mTwd;
    int          head     = state.pressure5mHead;
    int          count    = state.pressure5mCount;
    const int    maxSize  = State::PRESSURE_SIZE_5M;

    // Auto pressure half-range: minimum MIN_HRANGE, expand to fit + 15% headroom
    float halfRange = MIN_HRANGE;
    for (int i = 0; i < count; i++) {
        float p = pressBuf[(head + i) % maxSize];
        if (!isnan(p)) {
            float d = fabsf(p - STD_HPA);
            if (d > halfRange) halfRange = d;
        }
    }
    if (!isnan(state.pressure)) {
        float d = fabsf(state.pressure - STD_HPA);
        if (d > halfRange) halfRange = d;
    }
    halfRange *= 1.15f;

    // -- Trajectory: last 3 h full brightness, older fades to black -----------
    static constexpr int BRIGHT_SAMPLES = 36;  // 3 h at 5 min/sample

    int lastX = -1, lastY = -1;
    for (int i = 0; i < count; i++) {
        float p   = pressBuf[(head + i) % maxSize];
        float tws = twsBuf  [(head + i) % maxSize];
        float twd = twdBuf  [(head + i) % maxSize];

        if (isnan(p) || isnan(tws)) { lastX = lastY = -1; continue; }

        int x, y;
        dataToScreen(tws * 1.94384f, p, halfRange, x, y);

        int   dist = count - 1 - i;
        float fade = (dist <= BRIGHT_SAMPLES || count <= BRIGHT_SAMPLES + 1)
                     ? 1.0f
                     : 1.0f - (float)(dist - BRIGHT_SAMPLES)
                               / (float)(count - 1 - BRIGHT_SAMPLES);
        if (fade < 0.25f) fade = 0.25f;  // keep oldest segments faintly visible

        if (lastX >= 0) {
            uint16_t col = isnan(twd) ? TFT_DARKGREY : twdColor(twd);
            canvas->drawLine(lastX, lastY, x, y, fadeColor(col, fade));
        }
        lastX = x; lastY = y;
    }

    // -- Live current state: dot in TWD colour, white ring --------------------
    if (!isnan(state.pressure) && !isnan(state.windSpeedTrue)) {
        int lx, ly;
        dataToScreen(state.windSpeedTrue * 1.94384f, state.pressure, halfRange, lx, ly);
        uint16_t dotCol = isnan(state.windDirectionTrue)
                          ? TFT_LIGHTGREY
                          : twdColor(state.windDirectionTrue);
        canvas->fillCircle(lx, ly, 5, dotCol);
        canvas->drawCircle(lx, ly, 6, TFT_WHITE);
        canvas->drawCircle(lx, ly, 7, TFT_WHITE);
    }

    // -- Right info panel: P / TWS / TWD --------------------------------------
    char buf[16];
    canvas->setTextDatum(TC_DATUM);

    // Dividers
    canvas->drawLine(218, 80,  315, 80,  TFT_DARKGREY);
    canvas->drawLine(218, 160, 315, 160, TFT_DARKGREY);

    // --- Pressure (top third: y 0-80, centred at y 40) ---
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(TFT_DARKGREY, TFT_BLACK);
    canvas->drawString("hPa", PNL_X, 18);
    canvas->setFont(&fonts::FreeSans12pt7b);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    if (!isnan(state.pressure)) {
        snprintf(buf, sizeof(buf), "%.1f", state.pressure);
    } else {
        snprintf(buf, sizeof(buf), "---");
    }
    canvas->drawString(buf, PNL_X, 38);

    // --- TWS (middle third: y 80-160, centred at y 120) ---
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(TFT_DARKGREY, TFT_BLACK);
    canvas->drawString("kn", PNL_X, 96);
    canvas->setFont(&fonts::FreeSans12pt7b);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    if (!isnan(state.windSpeedTrue)) {
        snprintf(buf, sizeof(buf), "%.1f", state.windSpeedTrue * 1.94384f);
    } else {
        snprintf(buf, sizeof(buf), "---");
    }
    canvas->drawString(buf, PNL_X, 116);

    // --- TWD (bottom third: y 160-240, centred at y 200) ---
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(TFT_DARKGREY, TFT_BLACK);
    canvas->drawString("TWD", PNL_X, 174);
    if (!isnan(state.windDirectionTrue)) {
        float deg = state.windDirectionTrue * (180.0f / (float)M_PI);
        if (deg < 0.0f) deg += 360.0f;
        uint16_t twdCol = twdColor(state.windDirectionTrue);
        canvas->setFont(&fonts::FreeSans12pt7b);
        canvas->setTextColor(twdCol, TFT_BLACK);
        snprintf(buf, sizeof(buf), "%.0f %s", deg, twdCardinal(state.windDirectionTrue));
        canvas->drawString(buf, PNL_X, 194);
    } else {
        canvas->setFont(&fonts::FreeSans12pt7b);
        canvas->setTextColor(TFT_WHITE, TFT_BLACK);
        canvas->drawString("---", PNL_X, 194);
    }

    canvas->pushSprite(0, 0);
}

// -- run -----------------------------------------------------------------------

int SummaryScreen::run(const m5::touch_detail_t &t, State &state) {
    if (t.wasFlicked()) {
        if (t.distanceX() > 5)  return 2;  // swipe right -> GNSS
        if (t.distanceX() < -5) return 0;  // swipe left  -> Meteo
    }

#ifdef SIMULATE_DATA
    {
        static unsigned long lastSim = 0;
        static float phase = 0.0f;
        unsigned long now2 = millis();
        if (now2 - lastSim >= 200) {
            phase += 0.05f;
            // Lissajous-like path through pressure x TWS space, rotating TWD
            float p   = STD_HPA + 8.0f * sinf(phase);
            float tws = (8.0f + 7.0f * cosf(phase * 1.7f)) / 1.94384f;  // m/s
            float twd = fmodf(phase * 0.4f, 2.0f * (float)M_PI);
            state.addSample(p, tws, twd);
            lastSim = now2;
        }
    }
#endif

    unsigned long now = millis();
    if ((now - lastDraw) >= 1000UL) {
        draw(state);
        lastDraw = now;
    }
    return -1;
}
