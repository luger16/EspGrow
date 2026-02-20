#include "settings.h"
#include "storage.h"
#include "time_utils.h"
#include <ArduinoJson.h>

namespace Settings {

namespace {
    const char* SETTINGS_PATH = "/settings.json";
    int timezoneOffsetMinutes = 0; // Default: UTC+0
    
    void saveSettings() {
        JsonDocument doc;
        doc["timezoneOffsetMinutes"] = timezoneOffsetMinutes;
        
        Storage::writeJson(SETTINGS_PATH, doc);
        Serial.printf("[Settings] Saved (timezone: %+d minutes)\n", timezoneOffsetMinutes);
    }
    
    void loadSettings() {
        JsonDocument doc;
        if (!Storage::readJson(SETTINGS_PATH, doc)) {
            Serial.println("[Settings] No settings file found, using defaults");
            return;
        }
        
        timezoneOffsetMinutes = doc["timezoneOffsetMinutes"] | 0;
        TimeUtils::setTimezoneOffset(timezoneOffsetMinutes);
        
        Serial.printf("[Settings] Loaded (timezone: %+d minutes)\n", timezoneOffsetMinutes);
    }
}

void init() {
    loadSettings();
    Serial.println("[Settings] Initialized");
}

int getTimezoneOffsetMinutes() {
    return timezoneOffsetMinutes;
}

void setTimezoneOffsetMinutes(int minutes) {
    timezoneOffsetMinutes = minutes;
    TimeUtils::setTimezoneOffset(minutes);
    saveSettings();
    Serial.printf("[Settings] Timezone updated to %+d minutes\n", minutes);
}

}
