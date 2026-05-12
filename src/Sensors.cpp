#include "Sensors.h"
#include <Arduino.h>
#include <sys/time.h>

Sensors::Sensors() : bmp(&Wire), oneWire(DS18B20_PIN), ds18b20(&oneWire) {}

bool Sensors::init() {
    if (!bmp.begin(BMP280_ADDR)) {
        Serial.println("BMP280 not found");
        return false;
    }

    bmi270.init(I2C_NUM_0, BMI270_ADDR);

    Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    ds18b20.begin();
    ds18b20.setWaitForConversion(false);
    ds18b20.requestTemperatures();
    lastTempRequest = millis();

    return true;
}

void Sensors::update(State &state) {
    feedGPS();
    readIMU(state);
    readBaro(state);
    readGPS(state);
    readExternalTemp(state);
}

void Sensors::feedGPS() {
    while (Serial2.available())
        gps.encode(Serial2.read());
}

void Sensors::readIMU(State &state) {
    bmi270.readAcceleration(state.accX, state.accY, state.accZ);
    bmi270.readGyroscope(state.gyroX, state.gyroY, state.gyroZ);
    if (bmi270.bmm150isEnabled())
        bmi270.readMagneticField(state.magX, state.magY, state.magZ);
}

void Sensors::readBaro(State &state) {
    state.temperature = bmp.readTemperature();
    state.pressure    = bmp.readPressure() / 100.0f;  // Pa -> hPa
    state.baroAlt     = bmp.readAltitude(SEA_LEVEL_HPA);

    unsigned long now = millis();
    if (state.pressure1mCount == 0 || (now - lastPressureSample) >= PRESSURE_SAMPLE_INTERVAL) {
        state.addSample(state.pressure, state.windSpeedTrue, state.windDirectionTrue);
        lastPressureSample = now;
    }
}

void Sensors::readGPS(State &state) {
    state.gpsValid   = gps.location.isValid();
    state.satellites = gps.satellites.isValid() ? (int)gps.satellites.value() : 0;
    state.hdop       = gps.hdop.isValid()        ? gps.hdop.hdop()             : 0.0f;

    if (gps.location.isValid()) {
        state.lat    = gps.location.lat();
        state.lon    = gps.location.lng();
    }

    if (gps.altitude.isValid())
        state.gpsAlt = gps.altitude.meters();

    if (gps.speed.isValid())
        state.speed  = gps.speed.kmph();

    if (gps.course.isValid())
        state.course = gps.course.deg();

    state.timeValid = gps.date.isValid() && gps.time.isValid();
    if (state.timeValid) {
        state.year   = gps.date.year();
        state.month  = gps.date.month();
        state.day    = gps.date.day();
        state.hour   = gps.time.hour();
        state.minute = gps.time.minute();
        state.second = gps.time.second();

        if (!_gpsSynced) {
            struct tm t = {};
            t.tm_year = state.year  - 1900;
            t.tm_mon  = state.month - 1;
            t.tm_mday = state.day;
            t.tm_hour = state.hour;
            t.tm_min  = state.minute;
            t.tm_sec  = state.second;
            struct timeval tv = { mktime(&t), 0 };
            settimeofday(&tv, nullptr);
            _gpsSynced = true;
            Serial.printf("System clock synced from GPS: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                          state.year, state.month, state.day,
                          state.hour, state.minute, state.second);
        }
    }
}

void Sensors::readExternalTemp(State &state) {
    unsigned long now = millis();

    if ((now - lastTempRequest) >= TEMP_CONVERSION_MS) {
        float temp = ds18b20.getTempCByIndex(0);
        if (temp != DEVICE_DISCONNECTED_C) {
            state.outsideTemperature = temp;
        }
        if ((now - lastTempRequest) >= TEMP_READ_INTERVAL) {
            ds18b20.requestTemperatures();
            lastTempRequest = now;
        }
    }
}
