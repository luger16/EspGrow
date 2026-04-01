#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace Devices {

struct Device {
    char id[24];
    char name[32];
    char type[16];
    char controlMethod[16];
    char ipAddress[40];
    char controlMode[12];
    bool isOn = false;
    bool isOnline = false;
    bool hasEnergyMonitoring = false;
};

void init();

bool addDevice(JsonDocument& doc);
bool updateDevice(const char* deviceId, JsonDocument& doc);
bool removeDevice(const char* deviceId);

void getDevicesJson(String& out);
Device* getDevice(const char* deviceId);
Device* getDeviceByIndex(size_t index);
size_t getDeviceCount();

bool setDeviceState(const char* deviceId, bool on);
bool setDeviceOnline(const char* deviceId, bool online);
Device* findDeviceByTarget(const char* method, const char* target);
void computeControlModes();

}
