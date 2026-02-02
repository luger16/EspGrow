#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace Devices {

struct Device {
    char id[24];
    char name[32];
    char type[16];
    char controlMethod[16];
    uint8_t gpioPin;
    char ipAddress[40];
    char controlMode[12];
};

void init();

bool addDevice(JsonDocument& doc);
bool updateDevice(const char* deviceId, JsonDocument& doc);
bool removeDevice(const char* deviceId);

void getDevicesJson(String& out);
Device* getDevice(const char* deviceId);
size_t getDeviceCount();

}
