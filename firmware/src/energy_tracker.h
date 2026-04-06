#pragma once

#include <Arduino.h>

namespace EnergyTracker {

void init();
void loop();

void updateWatts(const char* deviceId, float watts);
void getEnergiesJson(String& out);
void resetEnergy(const char* deviceId);
void resetAllEnergy();
bool hasChanged();

}
