#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace Automation {

struct Rule {
    char id[24];
    char name[32];
    bool enabled;
    char sensorId[24];
    char op[3];
    float threshold;
    char deviceId[24];
    char deviceMethod[16];
    char deviceTarget[32];
    bool actionOn;
};

void init();
void loop(float temperature, float humidity, float co2);

bool addRule(JsonDocument& doc);
bool updateRule(const char* ruleId, JsonDocument& doc);
bool removeRule(const char* ruleId);
bool toggleRule(const char* ruleId);

void getRulesJson(String& out);

}
