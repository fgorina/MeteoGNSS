#include "AppConfig.h"
#include "GNSSScreen.h"
#include "MeteoScreen.h"
#include "PressureScreen.h"
#include "Screen.h"
#include "Sensors.h"
#include "State.h"
#include "net_signalk.h"
#include "net_webserver.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include <vector>

// ─── Global state ─────────────────────────────────────────────────────────────
AppConfig config;
State     state;
Sensors   sensors;
String    myIp = "Connecting...";

// ─── Hardware ─────────────────────────────────────────────────────────────────
SemaphoreHandle_t i2cMutex;
Preferences       prefs;

// ─── Network ──────────────────────────────────────────────────────────────────
NetSignalkWS *skWsServer = nullptr;
NetWebServer *webServer  = nullptr;

// ─── Screens ──────────────────────────────────────────────────────────────────
Screen              *currentScreen = nullptr;
std::vector<Screen*> screens;

// ─── Task handles ─────────────────────────────────────────────────────────────
TaskHandle_t taskSensor;
TaskHandle_t taskNetwork;
TaskHandle_t taskWss;

// ─── Forward declarations ─────────────────────────────────────────────────────
void writePreferences();
void switchTo(int i);

// ─── Preferences ──────────────────────────────────────────────────────────────
void writePreferences() {
    prefs.begin("MeteoGNSS", false);
    prefs.putString("SSID",    config.wifiSsid);
    prefs.putString("PASSWD",  config.wifiPassword);
    prefs.putString("SKHOST",  config.skServer);
    prefs.putInt   ("SKPORT",  config.skPort);
    prefs.putBool  ("USEN2K",  config.useN2k);
    prefs.putBool  ("USESK",   config.useSK);
    prefs.putBool  ("USE0183", config.use0183);
    prefs.putString("DEVNAME", config.deviceName);
    prefs.end();
}

void readPreferences() {
    prefs.begin("MeteoGNSS", true);
    config.wifiSsid     = prefs.getString("SSID",    config.wifiSsid);
    config.wifiPassword = prefs.getString("PASSWD",  config.wifiPassword);
    config.skServer     = prefs.getString("SKHOST",  config.skServer);
    config.skPort       = prefs.getInt   ("SKPORT",  config.skPort);
    config.useN2k       = prefs.getBool  ("USEN2K",  config.useN2k);
    config.useSK        = prefs.getBool  ("USESK",   config.useSK);
    config.use0183      = prefs.getBool  ("USE0183", config.use0183);
    config.deviceName   = prefs.getString("DEVNAME", "");
    prefs.end();

    if (config.deviceName.isEmpty()) {
        config.deviceName = "METEOGNSS_" + String(random(100, 1000));
        prefs.begin("MeteoGNSS", false);
        prefs.putString("DEVNAME", config.deviceName);
        prefs.end();
    }

    Serial.println("=== Preferences ===");
    Serial.println("SSID:     " + config.wifiSsid);
    Serial.println("SK host:  " + config.skServer + ":" + String(config.skPort));
    Serial.println("Use SK:   " + String(config.useSK));
    Serial.println("Device:   " + config.deviceName);
    Serial.println("===================");
}

// ─── WiFi ─────────────────────────────────────────────────────────────────────
bool checkConnection() {
    return WiFi.status() == WL_CONNECTED;
}

bool startWiFi() {
    static bool servicesStarted = false;
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.println("Connecting to " + config.wifiSsid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        tries++;
    }
    if (WiFi.status() != WL_CONNECTED) return false;

    myIp = WiFi.localIP().toString();
    Serial.println("Connected, IP: " + myIp);

    if (!servicesStarted) {
        configTime(0, 0, "europe.pool.ntp.org");
        MDNS.begin(config.deviceName.c_str());
        webServer->begin();
        servicesStarted = true;
    }
    return true;
}

bool startWiFiAP() {
    Serial.println("Starting AP: " + config.deviceName);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.deviceName.c_str(), "12345678");
    myIp = WiFi.softAPIP().toString();
    Serial.println("AP IP: " + myIp);
    MDNS.begin(config.deviceName.c_str());
    webServer->begin();
    return true;
}

void resetNetwork() {
    config.wifiSsid     = "";
    config.wifiPassword = "";
    config.skServer     = "";
    config.skPort       = 0;
    config.useSK        = false;
    config.useN2k       = false;
    writePreferences();
    ESP.restart();
}

// ─── Splash ───────────────────────────────────────────────────────────────────
void splash() {
    M5.Display.clear();
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.drawString("MeteoGNSS", M5.Display.width() / 2, 10);
    M5.Display.setTextDatum(CL_DATUM);
    M5.Display.drawString("SSID: " + config.wifiSsid, 10, 50);
    M5.Display.drawString("SK: " + config.skServer + ":" + String(config.skPort), 10, 85);
    M5.Display.drawString("Use SignalK: " + String(config.useSK ? "Si" : "No"), 10, 120);
    M5.Display.setTextDatum(CC_DATUM);
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.drawString("Manteniu per reset xarxa", M5.Display.width() / 2, 200);

    unsigned long start    = millis();
    long          pressAt  = -1;
    while (millis() - start < 10000) {
        M5.update();
        if (M5.Touch.getCount() > 0) {
            auto t = M5.Touch.getDetail(0);
            if (t.wasPressed())  pressAt = millis();
            if (t.wasReleased() && pressAt >= 0) {
                if (millis() - pressAt > 1000) resetNetwork();
                pressAt = -1;
            }
        }
        delay(10);
    }
}

// ─── Screen switching ─────────────────────────────────────────────────────────
void switchTo(int i) {
    if (i < 0 || i >= (int)screens.size() || screens[i] == currentScreen) return;
    if (currentScreen) currentScreen->exit(state);
    currentScreen = screens[i];
    currentScreen->enter(state);
}

// ─── Tasks ────────────────────────────────────────────────────────────────────
void sensorTask(void *) {
    for (;;) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sensors.update(state);
            xSemaphoreGive(i2cMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void networkTask(void *) {
    while (true) {
        if (!config.wifiSsid.isEmpty() && !checkConnection()) {
            startWiFi();
        }
        webServer->handleClient();
        vTaskDelay(10);
    }
}

void wssTask(void *) {
    while (true) {
        if (config.useSK && checkConnection()) {
            skWsServer->run();
        }
        vTaskDelay(10);
    }
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    M5.begin();
    Wire.begin(21, 22, 400000);
    i2cMutex = xSemaphoreCreateMutex();
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.setTextSize(1.0);

    readPreferences();

    skWsServer = new NetSignalkWS(config.skServer.c_str(), config.skPort, &state);
    webServer  = new NetWebServer(&config, 80, writePreferences);

    if (!config.wifiSsid.isEmpty()) {
        splash();
    } else {
        startWiFiAP();
    }

    sensors.init();

    screens.push_back(new MeteoScreen(M5.Display.width(), M5.Display.height()));
    screens.push_back(new PressureScreen(M5.Display.width(), M5.Display.height()));
    screens.push_back(new GNSSScreen(M5.Display.width(), M5.Display.height()));

    xTaskCreatePinnedToCore(sensorTask,  "sensors", 32768, nullptr, 1, &taskSensor,  1);
    xTaskCreate           (networkTask,  "network",  8192, nullptr, 1, &taskNetwork);
    if (config.useSK) {
        xTaskCreate(wssTask, "wss", 8192, nullptr, 0, &taskWss);
    }

    switchTo(0);
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
        M5.update();
        xSemaphoreGive(i2cMutex);
    }

    if (M5.BtnA.isPressed()) switchTo(0);
    if (M5.BtnB.isPressed()) switchTo(1);
    if (M5.BtnC.isPressed()) switchTo(2);

    auto &t = M5.Touch.getDetail(0);
    if (currentScreen) {
        int next = currentScreen->run(t, state);
        if (next >= 0 && next < (int)screens.size())
            switchTo(next);
    }
    vTaskDelay(1);
}
