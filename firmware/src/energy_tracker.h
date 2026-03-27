#pragma once

#include <Arduino.h>

namespace EnergyTracker {

void init();
void loop();

void getEnergiesJson(String& out);
void resetEnergy(const char* deviceId);
void resetAllEnergy();

}
