#include "net_signalk.h"
#include <WiFi.h>

void NetSignalkWS::onWsEventsCallback(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("SK: connection opened");
        lastMillis = millis();
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("SK: connection closed");
        started = false;
        // client cleanup deferred to run() — deleting here is unsafe (inside poll())
    } else if (event == WebsocketsEvent::GotPing) {
        client->pong();
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

NetSignalkWS::NetSignalkWS(const char *host, int port, State *state)
    : host(host), port(port), state(state) {}

bool NetSignalkWS::connect() {
    if (strlen(host) == 0 || port == 0) return false;
    if (client != nullptr && client->available()) return true;

    delete client;
    client = new WebsocketsClient();
    client->onMessage([this](WebsocketsMessage msg) { onWsMessageCallback(msg); });
    client->onEvent([this](WebsocketsEvent ev, String d) { onWsEventsCallback(ev, d); });

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

    // If poll() caused the connection to close, clean up now so the next
    // run() iteration reconnects immediately instead of waiting for timeout.
    if (!client->available()) {
        delete client;
        client  = nullptr;
        started = false;
    }
}
