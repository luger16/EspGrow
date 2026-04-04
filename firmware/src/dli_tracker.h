#pragma once

#include <Arduino.h>

namespace DliTracker {

void init();
void loop();

void getDliJson(String& out);
void resetDli();
bool hasChanged();

}
