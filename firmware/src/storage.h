#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace Storage {
    bool init();
    
    bool readJson(const char* path, JsonDocument& doc);
    bool writeJson(const char* path, const JsonDocument& doc);
    bool exists(const char* path);
    bool remove(const char* path);
}
