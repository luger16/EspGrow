#pragma once

#include <Arduino.h>

namespace DeviceController {
    void init();
    
    bool setTasmota(const String& ip, bool on);
    bool setShellyGen1(const String& ip, bool on);
    bool setShellyGen2(const String& ip, bool on);
    
    bool control(const String& method, const String& target, bool on);
}
