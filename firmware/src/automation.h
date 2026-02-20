#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <functional>

namespace Automation {

struct Rule {
    char id[24];
    char name[32];
    bool enabled;
    char ruleType[12];
    char sensorId[24];
    char op[3];
    float threshold;
    float thresholdOff;
    bool useHysteresis;
    char onTime[6];
    char offTime[6];
    unsigned long minRunTimeMs;
    char deviceId[24];
    char deviceMethod[16];
    char deviceTarget[32];
    bool actionOn;
    unsigned long lastStateChangeMs;
};

using DeviceStateCallback = std::function<void(const char* deviceId, const char* method, const char* target, bool on)>;

void init();
void loop(const std::map<String, float>& sensorReadings);
void setDeviceStateCallback(DeviceStateCallback cb);

bool addRule(JsonDocument& doc);
bool updateRule(const char* ruleId, JsonDocument& doc);
bool removeRule(const char* ruleId);
bool toggleRule(const char* ruleId);

void getRulesJson(String& out);

bool isDeviceUsedByEnabledRule(const char* deviceId);
void removeRulesForDevice(const char* deviceId);

void setManualOverride(const char* deviceId, unsigned long durationMs = 300000);
bool isDeviceOverridden(const char* deviceId);
unsigned long getOverrideRemaining(const char* deviceId);
void clearExpiredOverrides();
void clearOverride(const char* deviceId);
void forceEvaluation(const std::map<String, float>& sensorReadings);

}
