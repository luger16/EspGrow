#pragma once

#include <Arduino.h>
#include <map>

namespace EventLog {

struct Event {
    char id[24];
    char type[16];        // "alert", "automation", "device", "system"
    char title[48];
    char description[128];
    char severity[12];    // "info", "warning", "critical"
    uint32_t timestamp;
};

static constexpr size_t MAX_EVENTS = 50;

void init();
void loop(const std::map<String, float>& sensorReadings);

void pushEvent(const char* type, const char* title, const char* description,
               const char* severity = "info");

void getEventsJson(String& out);

// Persist events to LittleFS immediately (e.g. before reboot).
void flush();

}
