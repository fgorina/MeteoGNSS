#pragma once
#include <Arduino.h>

// Uncomment to use second-scale intervals instead of minute-scale
//#define TEST_MODE

struct AppConfig {
    String wifiSsid     = "elrond";
    String wifiPassword = "ailataN1991";
    String skServer     = "192.168.1.150";
    int    skPort       = 3000;
    bool   useN2k       = false;
    bool   useSK        = true;
    bool   use0183      = false;
    String deviceName;
    String skToken;        // JWT bearer token for SignalK authenticated writes
    bool   sendGNSS = false;
};
