#include "Sensors.h"
#include <Arduino.h>

Sensors::Sensors() : bmp(&Wire) {}

bool Sensors::init() {
    if (!bmp.begin(BMP280_ADDR)) {
        Serial.println("BMP280 not found");
        return false;
    }

    bmi270.init(I2C_NUM_0, BMI270_ADDR);

    Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    return true;
}

void Sensors::update(State &state) {
    feedGPS();
    readIMU(state);
    readBaro(state);
    readGPS(state);
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
    }
}
