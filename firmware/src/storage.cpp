#include "storage.h"
#include <LittleFS.h>

namespace Storage {
    bool init() {
        if (!LittleFS.begin(true)) {
            Serial.println("[Storage] Failed to mount LittleFS");
            return false;
        }
        Serial.println("[Storage] LittleFS mounted");
        return true;
    }

    bool readJson(const char* path, JsonDocument& doc) {
        if (!LittleFS.exists(path)) {
            Serial.printf("[Storage] File not found: %s\n", path);
            return false;
        }

        File file = LittleFS.open(path, "r");
        if (!file) {
            Serial.printf("[Storage] Failed to open: %s\n", path);
            return false;
        }

        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            Serial.printf("[Storage] JSON parse error in %s: %s\n", path, error.c_str());
            return false;
        }

        Serial.printf("[Storage] Loaded: %s\n", path);
        return true;
    }

    bool writeJson(const char* path, const JsonDocument& doc) {
        File file = LittleFS.open(path, "w");
        if (!file) {
            Serial.printf("[Storage] Failed to create: %s\n", path);
            return false;
        }

        size_t bytes = serializeJson(doc, file);
        file.close();

        if (bytes == 0) {
            Serial.printf("[Storage] Failed to write: %s\n", path);
            return false;
        }

        Serial.printf("[Storage] Saved: %s (%d bytes)\n", path, bytes);
        return true;
    }

    bool exists(const char* path) {
        return LittleFS.exists(path);
    }

    bool remove(const char* path) {
        if (LittleFS.remove(path)) {
            Serial.printf("[Storage] Removed: %s\n", path);
            return true;
        }
        return false;
    }
}
