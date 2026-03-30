#pragma once

#include <Arduino.h>

namespace DeviceController {

    struct QueryResult {
        bool reachable = false;
        bool isOn = false;
    };

    void init();
    
    QueryResult setTasmota(const String& ip, bool on);
    QueryResult setShellyGen1(const String& ip, bool on);
    QueryResult setShellyGen2(const String& ip, bool on);
    
    QueryResult control(const String& method, const String& target, bool on);

    QueryResult queryTasmota(const String& ip);
    QueryResult queryShellyGen1(const String& ip);
    QueryResult queryShellyGen2(const String& ip);
    QueryResult queryState(const String& method, const String& target);
}
