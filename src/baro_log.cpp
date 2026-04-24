#include "baro_log.h"
#include <math.h>
#include <time.h>

BaroLog::BaroLog(State *state) : _state(state) {
    _curDate[0] = '\0';
}

void BaroLog::init() {
    if (!SD.begin(SD_CS)) {
        Serial.println("BaroLog: SD init failed or no card");
        return;
    }
    if (!SD.exists("/baro_logs")) {
        SD.mkdir("/baro_logs");
    }
    _sdReady = true;
    Serial.println("BaroLog: SD ready");
}

void BaroLog::getDateTime(char date[11], char timeStr[9]) const {
    if (_state->timeValid && _state->year > 2000) {
        snprintf(date,    11, "%04d_%02d_%02d",
                 _state->year, _state->month, _state->day);
        snprintf(timeStr,  9, "%02d:%02d:%02d",
                 _state->hour, _state->minute, _state->second);
        return;
    }
    // Fall back to NTP-synced system clock (already set to UTC via configTime)
    time_t now; time(&now);
    struct tm *t = gmtime(&now);
    if (t->tm_year > 100) {
        snprintf(date,    11, "%04d_%02d_%02d",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
        snprintf(timeStr,  9, "%02d:%02d:%02d",
                 t->tm_hour, t->tm_min, t->tm_sec);
    } else {
        strncpy(date,    "0000_00_00", 11);
        strncpy(timeStr, "00:00:00",    9);
    }
}

void BaroLog::writeHeader() {
    _file.println("Time\tLat\tLon\tP_hPa\t"
                  "Tend3h_hPa_h\tTend12h_hPa_h\tTend24h_hPa_h\t"
                  "TWD_deg\tTWSmax_ms\tTWSavg_ms");
}

bool BaroLog::ensureFile() {
    char date[11], timeStr[9];
    getDateTime(date, timeStr);

    if (strcmp(date, _curDate) == 0 && _file) return true;

    if (_file) {
        _file.flush();
        _file.close();
    }

    char path[32];
    snprintf(path, sizeof(path), "/baro_logs/%s.tsv", date);
    bool isNew = !SD.exists(path);
    _file = SD.open(path, FILE_APPEND);
    if (!_file) {
        Serial.printf("BaroLog: cannot open %s\n", path);
        return false;
    }
    strncpy(_curDate, date, sizeof(_curDate));
    if (isNew) writeHeader();
    Serial.printf("BaroLog: logging to %s\n", path);
    return true;
}

void BaroLog::appendRow() {
    char date[11], timeStr[9];
    getDateTime(date, timeStr);

    char buf[256];
    int  len = snprintf(buf, sizeof(buf), "%s", timeStr);

    // Position (GPS only)
    if (_state->gpsValid) {
        len += snprintf(buf + len, sizeof(buf) - len,
                        "\t%.6f\t%.6f", _state->lat, _state->lon);
    } else {
        len += snprintf(buf + len, sizeof(buf) - len, "\t\t");
    }

    // Pressure
    if (!isnan(_state->pressure))
        len += snprintf(buf + len, sizeof(buf) - len, "\t%.2f", _state->pressure);
    else
        len += snprintf(buf + len, sizeof(buf) - len, "\t");

    // Tendencies (hPa/h)
    float tend[3] = {
        _state->tendencyHPaPerHour(3),
        _state->tendencyHPaPerHour(12),
        _state->tendencyHPaPerHour(24)
    };
    for (int i = 0; i < 3; i++) {
        if (!isnan(tend[i]))
            len += snprintf(buf + len, sizeof(buf) - len, "\t%.4f", tend[i]);
        else
            len += snprintf(buf + len, sizeof(buf) - len, "\t");
    }

    // Wind (TWD deg, TWS max m/s, TWS avg m/s)
    float twd    = _state->windAvgTwd();
    float twsMax = _state->windMaxTws();
    float twsAvg = _state->windAvgTws();

    if (!isnan(twd))
        len += snprintf(buf + len, sizeof(buf) - len, "\t%.1f",
                        twd * (180.0f / (float)M_PI));
    else
        len += snprintf(buf + len, sizeof(buf) - len, "\t");

    if (!isnan(twsMax))
        len += snprintf(buf + len, sizeof(buf) - len, "\t%.2f", twsMax);
    else
        len += snprintf(buf + len, sizeof(buf) - len, "\t");

    if (!isnan(twsAvg))
        len += snprintf(buf + len, sizeof(buf) - len, "\t%.2f", twsAvg);
    else
        len += snprintf(buf + len, sizeof(buf) - len, "\t");

    len += snprintf(buf + len, sizeof(buf) - len, "\n");
    _file.write((const uint8_t *)buf, len);
    _file.flush();
}

void BaroLog::run() {
    if (!_sdReady) return;
    if (isnan(_state->pressure)) return;   // sensor not ready yet
    unsigned long now = millis();
    if (_lastLog != 0 && now - _lastLog < LOG_INTERVAL) return;
    _lastLog = now;
    if (ensureFile()) appendRow();
}
