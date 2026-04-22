#include "net_webserver.h"

NetWebServer::NetWebServer(AppConfig *cfg, int port, void (*onSave)())
    : server(port), config(cfg), onSave(onSave) {}

String NetWebServer::getFullUri(const String &path) const {
    return "http://" + config->deviceName + ".local/" + path;
}

void NetWebServer::handleMenu() {
    String out = "<html><head>"
                 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                 "<title>" + config->deviceName + "</title></head><body>"
                 "<h1>MeteoGNSS</h1><ul>"
                 "<li><a href=\"/prefs\">Prefer&egrave;ncies</a></li>"
                 "<li><a href=\"/restart\">Restart</a></li>"
                 "</ul></body></html>";
    server.send(200, "text/html", out);
}

void NetWebServer::handlePreferences() {
    String chk = " checked";
    String out = "<html><head>"
                 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                 "<title>Prefer&egrave;ncies</title></head><body>"
                 "<h1><a href=\"/\">MeteoGNSS</a> / Prefer&egrave;ncies</h1>"
                 "<form action=\"/updatePrefs\" method=\"post\"><table>"
                 "<tr><td>SSID:</td><td><input name=\"ssid\" value=\"" + config->wifiSsid + "\"></td></tr>"
                 "<tr><td>Password:</td><td><input type=\"password\" name=\"password\" value=\"" + config->wifiPassword + "\"></td></tr>"
                 "<tr><td>SK Server:</td><td><input name=\"skserver\" value=\"" + config->skServer + "\"></td></tr>"
                 "<tr><td>SK Port:</td><td><input type=\"number\" name=\"skport\" value=\"" + String(config->skPort) + "\"></td></tr>"
                 "<tr><td>Use N2k:</td><td><input type=\"checkbox\" name=\"usen2k\" value=\"on\"" + (config->useN2k ? chk : "") + "></td></tr>"
                 "<tr><td>Use SignalK:</td><td><input type=\"checkbox\" name=\"usesk\" value=\"on\"" + (config->useSK ? chk : "") + "></td></tr>"
                 "<tr><td>Use NMEA0183:</td><td><input type=\"checkbox\" name=\"use0183\" value=\"on\"" + (config->use0183 ? chk : "") + "></td></tr>"
                 "<tr><td colspan=2 align=center><input type=\"submit\" value=\"Desa\"></td></tr>"
                 "</table></form></body></html>";
    server.send(200, "text/html", out);
}

void NetWebServer::handleUpdatePreferences() {
    if (server.hasArg("ssid"))     config->wifiSsid     = server.arg("ssid");
    if (server.hasArg("password")) config->wifiPassword = server.arg("password");
    if (server.hasArg("skserver")) config->skServer     = server.arg("skserver");
    if (server.hasArg("skport"))   config->skPort       = server.arg("skport").toInt();
    config->useN2k  = server.hasArg("usen2k");
    config->useSK   = server.hasArg("usesk");
    config->use0183 = server.hasArg("use0183");
    if (onSave) onSave();
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

void NetWebServer::handleRestart() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
    delay(100);
    ESP.restart();
}

void NetWebServer::begin() {
    if (started) return;
    server.on("/",            HTTP_GET,  [this]() { handleMenu(); });
    server.on("/index.html",  HTTP_GET,  [this]() { handleMenu(); });
    server.on("/prefs",       HTTP_GET,  [this]() { handlePreferences(); });
    server.on("/updatePrefs", HTTP_POST, [this]() { handleUpdatePreferences(); });
    server.on("/restart",     HTTP_GET,  [this]() { handleRestart(); });
    server.onNotFound([this]() { server.send(404, "text/plain", "Not Found"); });
    server.begin();
    started = true;
    Serial.println("HTTP server started");
}

void NetWebServer::handleClient() {
    if (started) server.handleClient();
}
