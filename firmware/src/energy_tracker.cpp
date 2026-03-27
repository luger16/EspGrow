#include "energy_tracker.h"
#include "devices.h"
#include "storage.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>

namespace EnergyTracker {

namespace {
    const char* ENERGY_PATH = "/energy.json";
    const unsigned long POLL_INTERVAL = 10000;       // 10 seconds
    const unsigned long PERSIST_INTERVAL = 300000;   // 5 minutes

    struct DeviceEnergy {
        char deviceId[24];
        float watts = 0.0f;
        double kWh = 0.0;
        unsigned long resetTimestamp = 0;  // millis() at last reset
        unsigned long lastPollTime = 0;    // millis() at last successful poll
    };

    std::vector<DeviceEnergy> energies;
    unsigned long lastPollTime = 0;
    unsigned long lastPersistTime = 0;

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
        entry.resetTimestamp = millis();
        energies.push_back(entry);
        return energies.back();
    }

    float pollWattsTasmota(const String& ip) {
        HTTPClient http;
        String url = "http://" + ip + "/cm?cmnd=Status%208";

        http.begin(url);
        http.setTimeout(3000);

        int code = http.GET();
        if (code != 200) {
            http.end();
            return NAN;
        }

        String body = http.getString();
        http.end();

        JsonDocument doc;
        if (deserializeJson(doc, body)) return NAN;

        return doc["StatusSNS"]["ENERGY"]["Power"] | NAN;
    }

    float pollWattsShellyGen1(const String& ip) {
        HTTPClient http;
        String url = "http://" + ip + "/meter/0";

        http.begin(url);
        http.setTimeout(3000);

        int code = http.GET();
        if (code != 200) {
            http.end();
            return NAN;
        }

        String body = http.getString();
        http.end();

        JsonDocument doc;
        if (deserializeJson(doc, body)) return NAN;

        return doc["power"] | NAN;
    }

    float pollWattsShellyGen2(const String& ip) {
        HTTPClient http;
        String url = "http://" + ip + "/rpc/Switch.GetStatus?id=0";

        http.begin(url);
        http.setTimeout(3000);

        int code = http.GET();
        if (code != 200) {
            http.end();
            return NAN;
        }

        String body = http.getString();
        http.end();

        JsonDocument doc;
        if (deserializeJson(doc, body)) return NAN;

        return doc["apower"] | NAN;
    }

    float pollWatts(const Devices::Device& device) {
        String ip(device.ipAddress);
        if (strcmp(device.controlMethod, "tasmota") == 0) {
            return pollWattsTasmota(ip);
        } else if (strcmp(device.controlMethod, "shelly_gen1") == 0) {
            return pollWattsShellyGen1(ip);
        } else if (strcmp(device.controlMethod, "shelly_gen2") == 0) {
            return pollWattsShellyGen2(ip);
        }
        return NAN;
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
            entry.resetTimestamp = obj["resetTimestamp"] | (unsigned long)millis();
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

    if (now - lastPollTime >= POLL_INTERVAL) {
        lastPollTime = now;

        String devicesJson;
        Devices::getDevicesJson(devicesJson);
        
        JsonDocument devDoc;
        if (deserializeJson(devDoc, devicesJson)) return;

        JsonArray devArr = devDoc.as<JsonArray>();
        for (JsonObject devObj : devArr) {
            bool hasEnergy = devObj["hasEnergyMonitoring"] | false;
            if (!hasEnergy) continue;

            const char* deviceId = devObj["id"];
            if (!deviceId) continue;

            Devices::Device* device = Devices::getDevice(deviceId);
            if (!device) continue;

            float watts = pollWatts(*device);
            if (isnan(watts)) {
                DeviceEnergy& entry = getOrCreateEnergy(deviceId);
                entry.watts = 0.0f;
                continue;
            }

            DeviceEnergy& entry = getOrCreateEnergy(deviceId);
            
            if (entry.lastPollTime > 0) {
                unsigned long elapsed = now - entry.lastPollTime;
                // kWh = watts * (ms / 3600000)
                entry.kWh += (double)entry.watts * (double)elapsed / 3600000.0;
            }

            entry.watts = watts;
            entry.lastPollTime = now;
        }
    }

    if (now - lastPersistTime >= PERSIST_INTERVAL) {
        lastPersistTime = now;
        if (!energies.empty()) {
            saveEnergies();
        }
    }
}

void getEnergiesJson(String& out) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    String devicesJson;
    Devices::getDevicesJson(devicesJson);
    
    JsonDocument devDoc;
    if (deserializeJson(devDoc, devicesJson)) {
        serializeJson(doc, out);
        return;
    }

    JsonArray devArr = devDoc.as<JsonArray>();
    for (JsonObject devObj : devArr) {
        bool hasEnergy = devObj["hasEnergyMonitoring"] | false;
        if (!hasEnergy) continue;

        const char* deviceId = devObj["id"];
        if (!deviceId) continue;

        DeviceEnergy* entry = findEnergy(deviceId);
        
        JsonObject obj = arr.add<JsonObject>();
        obj["deviceId"] = deviceId;
        obj["deviceName"] = devObj["name"] | "";
        obj["watts"] = entry ? entry->watts : 0.0f;
        obj["kWh"] = entry ? (float)entry->kWh : 0.0f;
        obj["resetTimestamp"] = entry ? entry->resetTimestamp : (unsigned long)millis();
    }

    serializeJson(doc, out);
}

void resetEnergy(const char* deviceId) {
    DeviceEnergy* entry = findEnergy(deviceId);
    if (entry) {
        entry->kWh = 0.0;
        entry->resetTimestamp = millis();
        Serial.printf("[Energy] Reset energy for device: %s\n", deviceId);
        saveEnergies();
    }
}

void resetAllEnergy() {
    for (auto& e : energies) {
        e.kWh = 0.0;
        e.resetTimestamp = millis();
    }
    Serial.println("[Energy] Reset all energy counters");
    saveEnergies();
}

}
