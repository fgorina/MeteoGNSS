#include "State.h"
#include <ArduinoJson.h>
#include <math.h>

// -- Pressure history ----------------------------------------------------------

void State::addSample(float p, float tws_ms, float twd_rad) {
    // 1-min buffer (pressure + wind share the same slot)
    int idx1;
    if (pressure1mCount < PRESSURE_SIZE_1M) {
        idx1 = pressure1mCount++;
    } else {
        idx1 = pressure1mHead;
        pressure1mHead = (pressure1mHead + 1) % PRESSURE_SIZE_1M;
    }
    pressure1m[idx1] = p;
    wind1mTws[idx1]  = tws_ms;
    wind1mTwd[idx1]  = twd_rad;

    // 5-min accumulator
    _accum5m += p;
    if (!isnan(tws_ms) && !isnan(twd_rad)) {
        _accumTws5m    += tws_ms;
        _accumTwdSin5m += sinf(twd_rad);
        _accumTwdCos5m += cosf(twd_rad);
        _windCount5m++;
    }

    if (++_count5m >= 5) {
        float avgP   = _accum5m / (float)_count5m;
        float avgTws = (_windCount5m > 0) ? _accumTws5m / (float)_windCount5m : NAN;
        float avgTwd = (_windCount5m > 0) ? atan2f(_accumTwdSin5m, _accumTwdCos5m) : NAN;
        if (!isnan(avgTwd) && avgTwd < 0.0f) avgTwd += 2.0f * (float)M_PI;

        int idx5;
        if (pressure5mCount < PRESSURE_SIZE_5M) {
            idx5 = pressure5mCount++;
        } else {
            idx5 = pressure5mHead;
            pressure5mHead = (pressure5mHead + 1) % PRESSURE_SIZE_5M;
        }
        pressure5m[idx5] = avgP;
        wind5mTws[idx5]  = avgTws;
        wind5mTwd[idx5]  = avgTwd;

        _accum5m = _accumTws5m = _accumTwdSin5m = _accumTwdCos5m = 0.0f;
        _count5m = _windCount5m = 0;
    }
}

float State::computeTendency(const float *buf, int head, int count,
                              int maxSize, float minPerSample) const {
    if (count < 15) return NAN;
    float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (int i = 0; i < count; i++) {
        float xi = (float)i;
        float y  = buf[(head + i) % maxSize];
        sumX  += xi;
        sumY  += y;
        sumXY += xi * y;
        sumX2 += xi * xi;
    }
    float n     = (float)count;
    float denom = n * sumX2 - sumX * sumX;
    if (fabsf(denom) < 1e-6f) return 0.0f;
    float slopePerSample = (n * sumXY - sumX * sumY) / denom;
    return slopePerSample * (60.0f / minPerSample);  // -> hPa/h
}

float State::tendencyHPaPerHour(int hours) const {
    if (hours <= 3) {
        return computeTendency(pressure1m, pressure1mHead, pressure1mCount,
                               PRESSURE_SIZE_1M, PRESSURE_MIN_PER_SAMPLE_1M);
    }
    // 12h -> last 144 of 5m buffer; 24h -> all 288
    int samplesNeeded = (hours * 60) / (int)PRESSURE_MIN_PER_SAMPLE_5M;
    int useCount = (pressure5mCount < samplesNeeded) ? pressure5mCount : samplesNeeded;
    int skip     = pressure5mCount - useCount;
    int head     = (pressure5mHead + skip) % PRESSURE_SIZE_5M;
    return computeTendency(pressure5m, head, useCount,
                           PRESSURE_SIZE_5M, PRESSURE_MIN_PER_SAMPLE_5M);
}

const char *State::tendencyLabel(int hours) const {
    float t = tendencyHPaPerHour(hours);
    if (isnan(t))  return "Insuf. dades";
    if (t >  2.0f) return "Pujant rapid";
    if (t >  0.5f) return "Pujant";
    if (t < -2.0f) return "Baixant rapid";
    if (t < -0.5f) return "Baixant";
    return "Estable";
}

// -- SignalK parser ------------------------------------------------------------

bool State::signalk_parse_ws(const char *data) {
    JsonDocument doc;
    if (deserializeJson(doc, data) != DeserializationError::Ok) return false;
    if (!doc["updates"].is<JsonArray>()) return false;

    for (JsonObject update : doc["updates"].as<JsonArray>()) {
        if (!update["values"].is<JsonArray>()) continue;
        for (JsonObject v : update["values"].as<JsonArray>()) {
            const char *path = v["path"];
            if (!path) continue;
            JsonVariant val = v["value"];

            if      (!strcmp(path, "environment.wind.angleApparent"))    windAngleApparent = val.as<float>();
            else if (!strcmp(path, "environment.wind.speedApparent"))    windSpeedApparent = val.as<float>();
            else if (!strcmp(path, "environment.wind.speedTrue"))        windSpeedTrue     = val.as<float>();
            else if (!strcmp(path, "environment.wind.directionTrue"))    windDirectionTrue = val.as<float>();
            else if (!strcmp(path, "navigation.headingMagnetic"))        skHeadingMagnetic = val.as<float>();
            else if (!strcmp(path, "navigation.headingTrue"))            skHeadingTrue     = val.as<float>();
            else if (!strcmp(path, "navigation.speedOverGround"))        skSog             = val.as<float>();
            else if (!strcmp(path, "navigation.courseOverGroundTrue"))   skCog             = val.as<float>();
            else if (!strcmp(path, "navigation.position") && val.is<JsonObject>()) {
                skLat = val["latitude"].as<double>();
                skLon = val["longitude"].as<double>();
            }
        }
    }
    return true;
}
