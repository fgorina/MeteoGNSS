#pragma once
#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include "State.h"
using namespace websockets;

class NetSignalkWS {

protected:
    WebsocketsClient *client = nullptr;
    const char       *host;
    int               port;
    State            *state;
    unsigned long     lastMillis = 0;
    unsigned long     timeout    = 30000;  // ms before reconnect
    bool              started    = false;
    char              buff[300];

    void onWsEventsCallback(WebsocketsEvent event, String data);
    void onWsMessageCallback(WebsocketsMessage message);

public:
    NetSignalkWS(const char *host, int port, State *state);
    bool connect();
    void subscribe();
    void begin();
    void run();
};
