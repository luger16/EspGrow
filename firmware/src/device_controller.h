#pragma once

#include <Arduino.h>

namespace DeviceController {
    void init();
    
    bool setTasmota(const String& ip, bool on);
    bool setShelly(const String& ip, bool on);
    bool setRelay(uint8_t pin, bool on);
    
    bool control(const String& method, const String& target, bool on);
}
