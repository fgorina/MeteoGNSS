#pragma once
#include <Arduino.h>
#include <SD.h>
#include "AppConfig.h"
#include "State.h"

class BaroLog {
public:
    explicit BaroLog(State *state);
    void init();
    void run();

private:
#ifdef TEST_MODE
    static constexpr unsigned long LOG_INTERVAL =   5000UL;  // 5 s
#else
    static constexpr unsigned long LOG_INTERVAL = 300000UL;  // 5 min
#endif
    static constexpr int SD_CS = 4;

    State        *_state;
    bool          _sdReady  = false;
    char          _curDate[11];   // "YYYY_MM_DD\0"
    File          _file;
    unsigned long _lastLog  = 0;

    void getDateTime(char date[11], char timeStr[9]) const;
    bool ensureFile();
    void writeHeader();
    void appendRow();
};
