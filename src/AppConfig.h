#pragma once
#include <Arduino.h>

struct AppConfig {
    String wifiSsid     = "elrond";
    String wifiPassword = "ailataN1991";
    String skServer     = "192.168.1.150";
    int    skPort       = 3000;
    bool   useN2k       = false;
    bool   useSK        = true;
    bool   use0183      = false;
    String deviceName;
};
