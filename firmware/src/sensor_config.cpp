#include "sensor_config.h"
#include "storage.h"
#include <vector>

namespace SensorConfig {

namespace {
    const char* SENSORS_PATH = "/sensors.json";
    std::vector<Sensor> sensors;
    std::vector<const char*> sensorIdPtrs;
    
    void saveConfig() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        
        for (const auto& sensor : sensors) {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = sensor.id;
            obj["name"] = sensor.name;
            obj["type"] = sensor.type;
            obj["unit"] = sensor.unit;
            obj["hardwareType"] = sensor.hardwareType;
        }
        
        Storage::writeJson(SENSORS_PATH, doc);
        Serial.printf("[SensorConfig] Saved %d sensors\n", sensors.size());
    }
    
    void loadConfig() {
        JsonDocument doc;
        if (!Storage::readJson(SENSORS_PATH, doc)) {
            Serial.println("[SensorConfig] No sensors file found");
            return;
        }
        
        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject obj : arr) {
            Sensor sensor;
            strlcpy(sensor.id, obj["id"] | "", sizeof(sensor.id));
            strlcpy(sensor.name, obj["name"] | "", sizeof(sensor.name));
            strlcpy(sensor.type, obj["type"] | "", sizeof(sensor.type));
            strlcpy(sensor.unit, obj["unit"] | "", sizeof(sensor.unit));
            strlcpy(sensor.hardwareType, obj["hardwareType"] | "", sizeof(sensor.hardwareType));
            
            sensors.push_back(sensor);
        }
        
        Serial.printf("[SensorConfig] Loaded %d sensors\n", sensors.size());
    }
    
    void updateIdPtrs() {
        sensorIdPtrs.clear();
        for (auto& sensor : sensors) {
            sensorIdPtrs.push_back(sensor.id);
        }
    }
}

void init() {
    loadConfig();
    updateIdPtrs();
    Serial.println("[SensorConfig] Initialized");
}

bool addSensor(JsonDocument& doc) {
    Sensor sensor;
    strlcpy(sensor.id, doc["id"] | "", sizeof(sensor.id));
    strlcpy(sensor.name, doc["name"] | "", sizeof(sensor.name));
    strlcpy(sensor.type, doc["type"] | "", sizeof(sensor.type));
    strlcpy(sensor.unit, doc["unit"] | "", sizeof(sensor.unit));
    strlcpy(sensor.hardwareType, doc["hardwareType"] | "", sizeof(sensor.hardwareType));
    
    sensors.push_back(sensor);
    updateIdPtrs();
    saveConfig();
    
    Serial.printf("[SensorConfig] Added sensor: %s\n", sensor.name);
    return true;
}

bool updateSensor(const char* sensorId, JsonDocument& doc) {
    for (auto& sensor : sensors) {
        if (strcmp(sensor.id, sensorId) == 0) {
            if (doc["name"].is<const char*>()) strlcpy(sensor.name, doc["name"], sizeof(sensor.name));
            if (doc["type"].is<const char*>()) strlcpy(sensor.type, doc["type"], sizeof(sensor.type));
            if (doc["unit"].is<const char*>()) strlcpy(sensor.unit, doc["unit"], sizeof(sensor.unit));
            if (doc["hardwareType"].is<const char*>()) strlcpy(sensor.hardwareType, doc["hardwareType"], sizeof(sensor.hardwareType));
            
            saveConfig();
            Serial.printf("[SensorConfig] Updated sensor: %s\n", sensor.name);
            return true;
        }
    }
    return false;
}

bool removeSensor(const char* sensorId) {
    for (auto it = sensors.begin(); it != sensors.end(); ++it) {
        if (strcmp(it->id, sensorId) == 0) {
            Serial.printf("[SensorConfig] Removed sensor: %s\n", it->name);
            sensors.erase(it);
            updateIdPtrs();
            saveConfig();
            return true;
        }
    }
    return false;
}

void getSensorsJson(String& out) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& sensor : sensors) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = sensor.id;
        obj["name"] = sensor.name;
        obj["type"] = sensor.type;
        obj["unit"] = sensor.unit;
        obj["hardwareType"] = sensor.hardwareType;
    }
    
    serializeJson(doc, out);
}

Sensor* getSensor(const char* sensorId) {
    for (auto& sensor : sensors) {
        if (strcmp(sensor.id, sensorId) == 0) {
            return &sensor;
        }
    }
    return nullptr;
}

size_t getSensorCount() {
    return sensors.size();
}

const char** getSensorIds(size_t& count) {
    count = sensorIdPtrs.size();
    return sensorIdPtrs.data();
}

}
