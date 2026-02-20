#pragma once

#include <Arduino.h>

namespace TimeUtils {
    void setTimezoneOffset(int minutes);
    bool isTimeInRange(const char* startHHMM, const char* endHHMM);
    String getCurrentTimeHHMM();
}
