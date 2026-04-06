#include "energy_tracker.h"
#include "devices.h"
#include "storage.h"
#include <ArduinoJson.h>
#include <vector>
#include <time.h>

namespace EnergyTracker {

namespace {
    const char* ENERGY_PATH = "/energy.json";
    const unsigned long PERSIST_INTERVAL = 300000;   // 5 minutes

    struct DeviceEnergy {
        char deviceId[24];
        float watts = 0.0f;
        double kWh = 0.0;
        uint32_t resetTimestamp = 0;      // Unix timestamp at last reset
        unsigned long lastUpdateTime = 0; // millis() at last watts update
    };

    std::vector<DeviceEnergy> energies;
    unsigned long lastPersistTime = 0;
    String lastBroadcastJson;

    DeviceEnergy* findEnergy(const char* deviceId) {
        for (auto& e : energies) {
            if (strcmp(e.deviceId, deviceId) == 0) return &e;
        }
        return nullptr;
    }

    DeviceEnergy& getOrCreateEnergy(const char* deviceId) {
        DeviceEnergy* existing = findEnergy(deviceId);
        if (existing) return *existing;

        DeviceEnergy entry;
        strlcpy(entry.deviceId, deviceId, sizeof(entry.deviceId));
        entry.resetTimestamp = (uint32_t)time(nullptr);
        energies.push_back(entry);
        return energies.back();
    }

    void saveEnergies() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (const auto& e : energies) {
            JsonObject obj = arr.add<JsonObject>();
            obj["deviceId"] = e.deviceId;
            obj["kWh"] = e.kWh;
            obj["resetTimestamp"] = e.resetTimestamp;
        }

        Storage::writeJson(ENERGY_PATH, doc);
        Serial.printf("[Energy] Saved %d entries\n", energies.size());
    }

    void loadEnergies() {
        JsonDocument doc;
        if (!Storage::readJson(ENERGY_PATH, doc)) {
            Serial.println("[Energy] No energy file found");
            return;
        }

        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject obj : arr) {
            const char* deviceId = obj["deviceId"];
            if (!deviceId) continue;

            DeviceEnergy& entry = getOrCreateEnergy(deviceId);
            entry.kWh = obj["kWh"] | 0.0;
            entry.resetTimestamp = obj["resetTimestamp"] | (uint32_t)time(nullptr);
        }

        Serial.printf("[Energy] Loaded %d entries\n", energies.size());
    }
}

void init() {
    loadEnergies();
    Serial.println("[Energy] Initialized");
}

void loop() {
    unsigned long now = millis();

    if (now - lastPersistTime >= PERSIST_INTERVAL) {
        lastPersistTime = now;
        if (!energies.empty()) {
            saveEnergies();
        }
    }
}

void updateWatts(const char* deviceId, float watts) {
    DeviceEnergy& entry = getOrCreateEnergy(deviceId);
    unsigned long now = millis();

    if (entry.lastUpdateTime > 0 && !isnan(entry.watts) && entry.watts > 0) {
        unsigned long elapsed = now - entry.lastUpdateTime;
        entry.kWh += (double)entry.watts * (double)elapsed / 3600000000.0;
    }

    entry.watts = isnan(watts) ? 0.0f : watts;
    entry.lastUpdateTime = now;
}

void getEnergiesJson(String& out) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    size_t count = Devices::getDeviceCount();
    for (size_t i = 0; i < count; i++) {
        Devices::Device* device = Devices::getDeviceByIndex(i);
        if (!device || !device->hasEnergyMonitoring) continue;

        DeviceEnergy* entry = findEnergy(device->id);
        
        JsonObject obj = arr.add<JsonObject>();
        obj["deviceId"] = device->id;
        obj["deviceName"] = device->name;
        obj["watts"] = entry ? entry->watts : 0.0f;
        obj["kWh"] = entry ? (float)entry->kWh : 0.0f;
        obj["resetTimestamp"] = entry ? entry->resetTimestamp : (uint32_t)time(nullptr);
    }

    serializeJson(doc, out);
}

void resetEnergy(const char* deviceId) {
    DeviceEnergy* entry = findEnergy(deviceId);
    if (entry) {
        entry->kWh = 0.0;
        entry->resetTimestamp = (uint32_t)time(nullptr);
        Serial.printf("[Energy] Reset energy for device: %s\n", deviceId);
        saveEnergies();
    }
}

void resetAllEnergy() {
    for (auto& e : energies) {
        e.kWh = 0.0;
        e.resetTimestamp = (uint32_t)time(nullptr);
    }
    Serial.println("[Energy] Reset all energy counters");
    saveEnergies();
}

bool hasChanged() {
    String current;
    getEnergiesJson(current);
    if (current == lastBroadcastJson) return false;
    lastBroadcastJson = current;
    return true;
}

}
