#pragma once

#include "AppConfig.h"
#include "State.h"
#include "BMI270.h"
#include <Adafruit_BMP280.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class Sensors {
public:
    Sensors();

    bool init();
    void update(State &state);

private:
    BMI270::BMI270   bmi270;
    Adafruit_BMP280  bmp;
    TinyGPSPlus      gps;
    OneWire          oneWire;
    DallasTemperature ds18b20;

    void feedGPS();
    void readIMU(State &state);
    void readBaro(State &state);
    void readGPS(State &state);
    void readExternalTemp(State &state);

    static constexpr uint8_t  BMI270_ADDR    = 0x68;
    static constexpr uint8_t  BMP280_ADDR    = 0x76;
    static constexpr float    SEA_LEVEL_HPA  = 1013.25f;
    static constexpr uint32_t GPS_BAUD       = 38400;
    static constexpr int      GPS_RX_PIN     = 13;
    static constexpr int      GPS_TX_PIN     = 14;
    static constexpr int      DS18B20_PIN    = 32;

#ifdef TEST_MODE
    static constexpr unsigned long PRESSURE_SAMPLE_INTERVAL =  1000UL;  // 1 s
#else
    static constexpr unsigned long PRESSURE_SAMPLE_INTERVAL = 60000UL;  // 1 min
#endif
    unsigned long lastPressureSample = 0;
    bool          _gpsSynced         = false;
    unsigned long lastTempRequest    = 0;
    static constexpr unsigned long TEMP_CONVERSION_MS = 750;
    static constexpr unsigned long TEMP_READ_INTERVAL = 10000;  // 10 s
};
