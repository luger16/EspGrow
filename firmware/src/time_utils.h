#pragma once

#include <Arduino.h>

namespace TimeUtils {
    bool isTimeInRange(const char* startHHMM, const char* endHHMM);
    String getCurrentTimeHHMM();
}
