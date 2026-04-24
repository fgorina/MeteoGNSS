#include "net_signalk.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

void NetSignalkWS::onWsEventsCallback(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("SK: connection opened");
        lastMillis = millis();
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("SK: connection closed");
        started = false;
    } else if (event == WebsocketsEvent::GotPing) {
        client->pong();
        lastMillis = millis();
    }
}

void NetSignalkWS::onWsMessageCallback(WebsocketsMessage message) {
    lastMillis = millis();
    if (!started) {
        subscribe();
        started = true;
    }

    if (!state->signalk_parse_ws(message.data().c_str())) {
        Serial.print("SK unhandled: ");
        Serial.println(message.data());
    }
}

NetSignalkWS::NetSignalkWS(const char *host, int port, State *state, String *token,
                           const char *clientId, bool *sendGNSS,
                           std::function<void()> saveCallback)
    : host(host), port(port), state(state), token(token),
      clientId(clientId), _sendGNSS(sendGNSS), _saveCallback(saveCallback) {}

void NetSignalkWS::sendPressure() {
    if (!client || !client->available()) return;
    if (isnan(state->pressure))          return;

    char msg[192];
    snprintf(msg, sizeof(msg),
        "{\"context\":\"vessels.self\",\"updates\":[{\"source\":{\"label\":\"%s\"},"
        "\"values\":[{\"path\":\"environment.outside.pressure\",\"value\":%.2f}]}]}",
        clientId, state->pressure * 100.0f);   // hPa -> Pa
    client->send(msg);
    lastMillis = millis();
}

void NetSignalkWS::sendTendencies() {
    if (!client || !client->available()) return;

    float t[3] = {
        state->tendencyHPaPerHour(3),
        state->tendencyHPaPerHour(12),
        state->tendencyHPaPerHour(24)
    };
    static const char *paths[3] = {
        "environment.outside.pressureTendency3h",
        "environment.outside.pressureTendency12h",
        "environment.outside.pressureTendency24h"
    };

    char vals[250];
    int  vlen = 0;
    for (int i = 0; i < 3; i++) {
        if (isnan(t[i])) continue;
        vlen += snprintf(vals + vlen, sizeof(vals) - vlen,
                         "%s{\"path\":\"%s\",\"value\":%.4f}",
                         vlen > 0 ? "," : "", paths[i], t[i] * 100.0f);  // hPa/h -> Pa/h
    }
    if (vlen == 0) return;

    char msg[380];
    snprintf(msg, sizeof(msg),
        "{\"context\":\"vessels.self\",\"updates\":[{\"source\":{\"label\":\"%s\"},"
        "\"values\":[%s]}]}",
        clientId, vals);
    client->send(msg);
    lastMillis = millis();
}

void NetSignalkWS::sendGNSS() {
    if (!client || !client->available()) return;
    if (!_sendGNSS || !*_sendGNSS)       return;
    if (!state->gpsValid)                 return;

    char msg[320];
    snprintf(msg, sizeof(msg),
        "{\"context\":\"vessels.self\",\"updates\":[{\"source\":{\"label\":\"%s\"},"
        "\"values\":["
        "{\"path\":\"navigation.position\",\"value\":{\"latitude\":%.8f,\"longitude\":%.8f}},"
        "{\"path\":\"navigation.gnss.horizontalDilution\",\"value\":%.2f},"
        "{\"path\":\"navigation.gnss.satellites\",\"value\":%d}"
        "]}]}",
        clientId, state->lat, state->lon, state->hdop, state->satellites);
    client->send(msg);
    lastMillis = millis();
}

// POST /signalk/v1/access/requests and return the polling href.
String NetSignalkWS::requestAuth() {
    HTTPClient http;
    String url = "http://" + String(host) + ":" + String(port) +
                 "/signalk/v1/access/requests";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    char body[200];
    snprintf(body, sizeof(body),
             "{\"clientId\":\"%s\",\"description\":\"MeteoGNSS\",\"permissions\":\"readwrite\"}",
             clientId);

    int code = http.POST(body);
    String href;
    if (code >= 200 && code < 300) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getStream())) {
            const char *h = doc["href"];
            if (h) {
                href = String(h);
                Serial.println("SK auth requested, href: " + href);
            }
        }
    } else {
        Serial.printf("SK auth request: HTTP %d\n", code);
    }
    http.end();
    return href;
}

// GET the polling href; if approved, store the JWT and invoke saveCallback.
bool NetSignalkWS::checkAuth() {
    if (authHref.isEmpty()) return false;

    HTTPClient http;
    String url = "http://" + String(host) + ":" + String(port) + authHref;
    http.begin(url);
    int code = http.GET();
    bool approved = false;
    if (code == 200) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getStream())) {
            const char *perm = doc["accessRequest"]["permission"];
            if (perm && strcmp(perm, "APPROVED") == 0) {
                const char *tok = doc["accessRequest"]["token"];
                if (tok) {
                    *token = String(tok);   // store raw JWT
                    authHref = "";
                    lastPut  = 0;           // trigger immediate PUT
                    if (_saveCallback) _saveCallback();
                    Serial.println("SK auth approved, token stored");
                    approved = true;
                }
            } else {
                Serial.printf("SK auth status: %s\n", perm ? perm : "null");
            }
        }
    } else {
        Serial.printf("SK auth check: HTTP %d\n", code);
    }
    http.end();
    return approved;
}

bool NetSignalkWS::connect() {
    if (strlen(host) == 0 || port == 0) return false;
    if (client != nullptr && client->available()) return true;

    delete client;
    client = new WebsocketsClient();
    client->onMessage([this](WebsocketsMessage msg) { onWsMessageCallback(msg); });
    client->onEvent([this](WebsocketsEvent ev, String d) { onWsEventsCallback(ev, d); });

    if (token && !token->isEmpty()) {
        client->addHeader("Authorization", "Bearer " + *token);
    }

    snprintf(buff, sizeof(buff), "ws://%s:%d/signalk/v1/stream?subscribe=none", host, port);
    Serial.print("SK connecting: ");
    Serial.println(buff);

    if (client->connect(String(buff))) {
        lastMillis = millis();
        return true;
    }
    Serial.println("SK: cannot connect");
    delete client;
    client = nullptr;
    return false;
}

void NetSignalkWS::subscribe() {
    const char *sub =
        "{\"context\":\"\",\"subscribe\":["
        "{\"path\":\"environment.wind.angleApparent\",\"policy\":\"instant\"},"
        "{\"path\":\"environment.wind.speedApparent\",\"policy\":\"instant\"},"
        "{\"path\":\"environment.wind.speedTrue\",\"policy\":\"instant\"},"
        "{\"path\":\"environment.wind.directionTrue\",\"policy\":\"instant\"},"
        "{\"path\":\"navigation.headingMagnetic\",\"policy\":\"instant\"},"
        "{\"path\":\"navigation.headingTrue\",\"policy\":\"instant\"},"
        "{\"path\":\"navigation.position\",\"policy\":\"instant\"},"
        "{\"path\":\"navigation.speedOverGround\",\"policy\":\"instant\"},"
        "{\"path\":\"navigation.courseOverGroundTrue\",\"policy\":\"instant\"}"
        "]}\n";
    client->send(sub);
    Serial.println("SK: subscribed");
}

void NetSignalkWS::begin() {
    connect();
}

void NetSignalkWS::run() {
    if (WiFi.status() != WL_CONNECTED) return;

    // Auth flow: acquire token if missing
    if (token && token->isEmpty()) {
        unsigned long now = millis();
        if (authHref.isEmpty()) {
            if (lastAuthReq == 0 || now - lastAuthReq >= AUTH_REQ_INTERVAL) {
                String href = requestAuth();
                if (!href.isEmpty()) authHref = href;
                lastAuthReq = now;
            }
        } else {
            if (now - lastAuthCheck >= AUTH_CHECK_INTERVAL) {
                checkAuth();
                lastAuthCheck = now;
            }
        }
    }

    // WebSocket management
    if (client == nullptr) {
        connect();
        return;
    }
    if (millis() - lastMillis > timeout) {
        Serial.println("SK: timeout, reconnecting");
        client->close();
        delete client;
        client  = nullptr;
        started = false;
        lastMillis = millis();
        connect();
        return;
    }
    client->poll();

    if (!client->available()) {
        delete client;
        client  = nullptr;
        started = false;
        return;
    }

    unsigned long now = millis();

    if (lastPut == 0 || now - lastPut >= PUT_INTERVAL) {
        sendPressure();
        sendTendencies();
        lastPut = now;
    }

    if (lastGNSS == 0 || now - lastGNSS >= GNSS_INTERVAL) {
        sendGNSS();
        lastGNSS = now;
    }
}
