#pragma once

#include "State.h"
#include "BMI270.h"
#include <Adafruit_BMP280.h>
#include <TinyGPSPlus.h>
#include <Wire.h>

class Sensors {
public:
    Sensors();

    bool init();
    void update(State &state);

private:
    BMI270::BMI270   bmi270;
    Adafruit_BMP280  bmp;
    TinyGPSPlus      gps;

    void feedGPS();
    void readIMU(State &state);
    void readBaro(State &state);
    void readGPS(State &state);

    static constexpr uint8_t  BMI270_ADDR    = 0x68;
    static constexpr uint8_t  BMP280_ADDR    = 0x76;
    static constexpr float    SEA_LEVEL_HPA  = 1013.25f;
    static constexpr uint32_t GPS_BAUD       = 38400;
    static constexpr int      GPS_RX_PIN     = 13;
    static constexpr int      GPS_TX_PIN     = 14;

    // 1000 ms for testing; change to 60000 for 1 sample/min in production
    static constexpr unsigned long PRESSURE_SAMPLE_INTERVAL = 1000UL;
    unsigned long lastPressureSample = 0;
};
