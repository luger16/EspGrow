#pragma once

#include <Arduino.h>

namespace Settings {
    void init();
    
    int getTimezoneOffsetMinutes();
    void setTimezoneOffsetMinutes(int minutes);
}
