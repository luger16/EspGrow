#pragma once

#include <Arduino.h>

namespace WiFiManager {
    void init();
    void loop();
    
    bool isConnected();
    bool isTimeSynced();
    String getIP();
    
    void startProvisioning();
}
