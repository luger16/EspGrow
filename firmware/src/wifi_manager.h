#pragma once

#include <Arduino.h>

namespace WiFiManager {
    void init();
    void loop();
    
    bool hasCredentials();
    bool isConnected();
    String getIP();
    
    void startProvisioning();
    void clearCredentials();
}
