#include "dli_tracker.h"
#include "sensors.h"
#include "sensor_config.h"
#include "device_modes.h"
#include "storage.h"
#include <ArduinoJson.h>
#include <time.h>

namespace DliTracker {

namespace {
    const char* DLI_PATH = "/dli.json";
    const unsigned long ACCUMULATE_INTERVAL = 10000;
    const unsigned long PERSIST_INTERVAL = 300000;

    double dliAccumulated = 0.0;
    unsigned long lastAccumulateTime = 0;
    unsigned long lastPersistTime = 0;
    bool wasDaytime = false;
    bool dirty = false;
    uint8_t lastDay = 0;

    uint8_t getCurrentDay() {
        time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        return t.tm_mday;
    }

    const char* findLightSensorId() {
        size_t count;
        const char** ids = SensorConfig::getSensorIds(count);
        for (size_t i = 0; i < count; i++) {
            SensorConfig::Sensor* s = SensorConfig::getSensor(ids[i]);
            if (s && strcmp(s->type, "light") == 0) return ids[i];
        }
        return nullptr;
    }

    void saveDli() {
        JsonDocument doc;
        doc["dli"] = dliAccumulated;
        doc["day"] = lastDay;
        Storage::writeJson(DLI_PATH, doc);
    }

    void loadDli() {
        JsonDocument doc;
        if (!Storage::readJson(DLI_PATH, doc)) {
            Serial.println("[DLI] No DLI file found");
            return;
        }

        uint8_t savedDay = doc["day"] | 0;
        uint8_t today = getCurrentDay();

        if (savedDay == today) {
            dliAccumulated = doc["dli"] | 0.0;
            Serial.printf("[DLI] Resumed: %.2f mol/m²/d\n", dliAccumulated);
        } else {
            dliAccumulated = 0.0;
            Serial.println("[DLI] New day — reset to 0");
        }

        lastDay = today;
    }
}

void init() {
    loadDli();
    wasDaytime = DeviceModes::isDaytime();
    Serial.println("[DLI] Initialized");
}

void loop() {
    unsigned long now = millis();

    if (now - lastAccumulateTime >= ACCUMULATE_INTERVAL) {
        unsigned long elapsed = lastAccumulateTime > 0 ? (now - lastAccumulateTime) : 0;
        lastAccumulateTime = now;

        bool isDay = DeviceModes::isDaytime();

        uint8_t today = getCurrentDay();
        if ((isDay && !wasDaytime) || today != lastDay) {
            dliAccumulated = 0.0;
            lastDay = today;
            dirty = true;
        }
        wasDaytime = isDay;

        if (!isDay || elapsed == 0) return;

        const char* lightId = findLightSensorId();
        if (!lightId) return;

        float ppfd = Sensors::getSensorValue(lightId);
        if (isnan(ppfd) || ppfd < 0) return;

        double intervalSec = (double)elapsed / 1000.0;
        dliAccumulated += (ppfd * intervalSec) / 1000000.0;
        dirty = true;
    }

    if (now - lastPersistTime >= PERSIST_INTERVAL) {
        lastPersistTime = now;
        if (dirty) {
            saveDli();
            dirty = false;
        }
    }
}

void getDliJson(String& out) {
    JsonDocument doc;
    doc["dli"] = (float)(round(dliAccumulated * 10.0) / 10.0);
    doc["isDay"] = DeviceModes::isDaytime();

    serializeJson(doc, out);
}

void resetDli() {
    dliAccumulated = 0.0;
    dirty = true;
    saveDli();
    dirty = false;
    Serial.println("[DLI] Reset");
}

}
