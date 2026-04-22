#pragma once

#include <math.h>
#include <stdint.h>

struct State {

    // Accelerometer (G)
    float accX = 0, accY = 0, accZ = 0;

    // Gyroscope (deg/s)
    float gyroX = 0, gyroY = 0, gyroZ = 0;

    // Magnetometer (µT)
    int16_t magX = 0, magY = 0, magZ = 0;

    // Barometric
    float pressure          = 0;    // hPa
    float temperature       = 0;    // °C (onboard sensor)
    float outsideTemperature = NAN; // °C (future external sensor)
    float baroAlt           = 0;    // m (derived from pressure)

    // Pressure history (circular buffer, filled by Sensors)
    static constexpr int PRESSURE_HISTORY_SIZE = 60;
    float pressureHistory[PRESSURE_HISTORY_SIZE] = {};
    int   pressureHead  = 0;
    int   pressureCount = 0;

    void addPressureSample(float p) {
        if (pressureCount < PRESSURE_HISTORY_SIZE) {
            pressureHistory[pressureCount++] = p;
        } else {
            pressureHistory[pressureHead] = p;
            pressureHead = (pressureHead + 1) % PRESSURE_HISTORY_SIZE;
        }
    }

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
    uint16_t year     = 0;
    uint8_t  month    = 0;
    uint8_t  day      = 0;
    uint8_t  hour     = 0;
    uint8_t  minute   = 0;
    uint8_t  second   = 0;
    bool     timeValid = false;

    // SignalK data (from boat network via SK server)
    float  windAngleApparent  = NAN;  // rad
    float  windSpeedApparent  = NAN;  // m/s
    float  windSpeedTrue      = NAN;  // m/s
    float  windDirectionTrue  = NAN;  // rad
    float  skHeadingMagnetic  = NAN;  // rad (from SK, vs onboard magnetometer)
    float  skHeadingTrue      = NAN;  // rad (from SK)
    double skLat              = 0;
    double skLon              = 0;
    float  skSog              = NAN;  // m/s
    float  skCog              = NAN;  // rad (true)

    bool signalk_parse_ws(const char *data);
};
