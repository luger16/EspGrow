#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <functional>

namespace DeviceModes {

enum Mode : uint8_t {
    MODE_OFF = 0,
    MODE_ON,
    MODE_AUTO,
    MODE_CYCLE,
    MODE_SCHEDULE
};

struct AutoTrigger {
    char sensorType[16];
    float dayThreshold;
    float nightThreshold;
    float hysteresis;
    bool triggerAbove;      // true = turn on when value EXCEEDS threshold
};

static const uint8_t MAX_TRIGGERS = 3;

struct CycleConfig {
    unsigned long onDurationSec;
    unsigned long offDurationSec;
    bool dayOnly;
};

struct ScheduleConfig {
    char startTime[6];      // HH:MM
    char endTime[6];        // HH:MM
};

struct DeviceModeConfig {
    char deviceId[24];
    Mode mode;
    AutoTrigger triggers[MAX_TRIGGERS];
    uint8_t triggerCount;
    CycleConfig cycle;
    ScheduleConfig schedule;
};

struct DayNightConfig {
    char dayStartTime[6];   // HH:MM
    char nightStartTime[6]; // HH:MM
    float lightThreshold;
    float lightHysteresis;
    bool useSchedule;
};

using DeviceStateCallback = std::function<void(const char* deviceId, bool on)>;

void init();
void loop(const std::map<String, float>& sensorReadings);
void setDeviceStateCallback(DeviceStateCallback cb);

bool setMode(JsonDocument& doc);
bool removeMode(const char* deviceId);
void removeModeForDevice(const char* deviceId);

void getModesJson(String& out);

bool isDaytime();
void getDayNightConfigJson(String& out);
bool setDayNightConfig(JsonDocument& doc);

const char* modeToString(Mode mode);
Mode stringToMode(const char* str);

}
