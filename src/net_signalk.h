#pragma once
#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <functional>
#include "State.h"
#include "AppConfig.h"
using namespace websockets;

class NetSignalkWS {

protected:
    WebsocketsClient *client = nullptr;
    const char       *host;
    int               port;
    State            *state;
    String           *token;           // raw JWT (no "Bearer " prefix)
    const char       *clientId;
    bool             *_sendGNSS;
    std::function<void()> _saveCallback;

    unsigned long     lastMillis    = 0;
    unsigned long     timeout       = 30000;
    unsigned long     lastPut       = 0;
    unsigned long     lastGNSS      = 0;
    unsigned long     lastAuthReq   = 0;
    unsigned long     lastAuthCheck = 0;

#ifdef TEST_MODE
    static constexpr unsigned long PUT_INTERVAL        =   1000UL;  // 1 s
    static constexpr unsigned long GNSS_INTERVAL       =   1000UL;  // 1 s
#else
    static constexpr unsigned long PUT_INTERVAL        =  60000UL;  // 1 min
    static constexpr unsigned long GNSS_INTERVAL       =   1000UL;  // 1 s
#endif
    static constexpr unsigned long AUTH_REQ_INTERVAL   = 120000UL;
    static constexpr unsigned long AUTH_CHECK_INTERVAL =  10000UL;

    bool   started  = false;
    char   buff[300];
    String authHref;

    void   onWsEventsCallback(WebsocketsEvent event, String data);
    void   onWsMessageCallback(WebsocketsMessage message);
    void   sendPressure();
    void   sendTendencies();
    void   sendGNSS();
    String requestAuth();
    bool   checkAuth();

public:
    NetSignalkWS(const char *host, int port, State *state, String *token,
                 const char *clientId, bool *sendGNSS,
                 std::function<void()> saveCallback);
    bool connect();
    void subscribe();
    void begin();
    void run();
};
