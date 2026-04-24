#pragma once

#include <math.h>
#include <stdint.h>

enum DisplaySaverState { DISPLAY_SLEEPING = 0, DISPLAY_WAKING = 1, DISPLAY_ACTIVE = 2 };

struct State {

    DisplaySaverState displaySaver = DISPLAY_ACTIVE;

    // Accelerometer (G)
    float accX = 0, accY = 0, accZ = 0;

    // Gyroscope (deg/s)
    float gyroX = 0, gyroY = 0, gyroZ = 0;

    // Magnetometer (T)
    int16_t magX = 0, magY = 0, magZ = 0;

    // Barometric
    float pressure           = 0;    // hPa
    float temperature        = 0;    // degC (onboard sensor)
    float outsideTemperature = NAN;  // degC (future external sensor)
    float baroAlt            = 0;    // m (derived from pressure)

    // -- Pressure history ------------------------------------------------------
    // 1-min buffer -> 3 h at full resolution
    static constexpr int   PRESSURE_SIZE_1M          = 180;
    static constexpr float PRESSURE_MIN_PER_SAMPLE_1M = 1.0f;
    float pressure1m[PRESSURE_SIZE_1M] = {};
    int   pressure1mHead  = 0;
    int   pressure1mCount = 0;

    // 5-min averaged buffer -> 24 h (12 h = last 144 of 288 entries)
    static constexpr int   PRESSURE_SIZE_5M          = 288;
    static constexpr float PRESSURE_MIN_PER_SAMPLE_5M = 5.0f;
    float pressure5m[PRESSURE_SIZE_5M] = {};
    int   pressure5mHead  = 0;
    int   pressure5mCount = 0;

    // Accumulator used to build each 5-min average from five 1-min samples
    float _accum5m = 0.0f;
    int   _count5m = 0;

    // Wind buffers - same indices as pressure buffers, sampled together
    float wind1mTws[PRESSURE_SIZE_1M] = {};  // m/s  (NAN = unavailable)
    float wind1mTwd[PRESSURE_SIZE_1M] = {};  // rad

    float wind5mTws[PRESSURE_SIZE_5M] = {};
    float wind5mTwd[PRESSURE_SIZE_5M] = {};

    // Wind accumulators for 5-min circular-mean averaging
    float _accumTws5m    = 0.0f;
    float _accumTwdSin5m = 0.0f;
    float _accumTwdCos5m = 0.0f;
    int   _windCount5m   = 0;

    // Feed pressure + wind together so every buffer slot stays aligned.
    // Pass NAN for tws/twd when wind data is unavailable.
    void addSample(float p, float tws_ms, float twd_rad);

    // Returns hPa/h via linear regression; NAN when insufficient data.
    // hours: 3 uses the 1-min buffer; 12 or 24 uses the 5-min buffer.
    float       tendencyHPaPerHour(int hours = 3) const;
    const char *tendencyLabel     (int hours = 3) const;

    // Wind statistics over the last N samples from the 1-min buffer.
    // Returns NAN when no valid wind data is available.
    float windAvgTws(int samples = 5) const;           // m/s, arithmetic mean
    float windAvgTwd(int samples = 5) const;           // rad, circular mean [0, 2π)
    float windMaxTws(int samples = 5) const;           // m/s, peak gust

    // GPS (from onboard GNSS module)
    bool   gpsValid   = false;
    double lat        = 0;
    double lon        = 0;
    float  gpsAlt     = 0;   // m (MSL)
    float  speed      = 0;   // km/h
    float  course     = 0;   // degrees true
    int    satellites = 0;
    float  hdop       = 0;

    // GPS date/time (UTC)
    uint16_t year      = 0;
    uint8_t  month     = 0;
    uint8_t  day       = 0;
    uint8_t  hour      = 0;
    uint8_t  minute    = 0;
    uint8_t  second    = 0;
    bool     timeValid = false;

    // SignalK data (from boat network via SK server)
    float  windAngleApparent = NAN;  // rad
    float  windSpeedApparent = NAN;  // m/s
    float  windSpeedTrue     = NAN;  // m/s
    float  windDirectionTrue = NAN;  // rad
    float  skHeadingMagnetic = NAN;  // rad (from SK, vs onboard magnetometer)
    float  skHeadingTrue     = NAN;  // rad (from SK)
    double skLat             = 0;
    double skLon             = 0;
    float  skSog             = NAN;  // m/s
    float  skCog             = NAN;  // rad (true)

    bool signalk_parse_ws(const char *data);

private:
    // Linear regression over a circular buffer window; returns hPa/h.
    float computeTendency(const float *buf, int head, int count,
                          int maxSize, float minPerSample) const;
};
