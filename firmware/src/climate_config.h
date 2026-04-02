#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace ClimateConfig {

struct PhaseTargets {
    float tempDay;
    float tempNight;
    float humidityDay;
    float humidityNight;
    float vpdDay;
    float vpdNight;
    float co2Day;
    float co2Night;
    float dli;
};

void init();

const char* getActivePhase();
const PhaseTargets& getTargets();
const PhaseTargets& getTargetsForPhase(const char* phase);

void getConfigJson(String& out);

bool setPhase(const char* phase, const char* phaseStartDate = nullptr);
bool setTargets(const char* phase, JsonObject& targets);
bool resetTargets(const char* phase);

}
