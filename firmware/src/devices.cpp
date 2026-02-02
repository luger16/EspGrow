#include "devices.h"
#include "storage.h"
#include <vector>

namespace Devices {

namespace {
    const char* DEVICES_PATH = "/devices.json";
    std::vector<Device> devices;
    
    void saveDevices() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        
        for (const auto& device : devices) {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = device.id;
            obj["name"] = device.name;
            obj["type"] = device.type;
            obj["controlMethod"] = device.controlMethod;
            obj["gpioPin"] = device.gpioPin;
            obj["ipAddress"] = device.ipAddress;
            obj["controlMode"] = device.controlMode;
        }
        
        Storage::writeJson(DEVICES_PATH, doc);
        Serial.printf("[Devices] Saved %d devices\n", devices.size());
    }
    
    void loadDevices() {
        JsonDocument doc;
        if (!Storage::readJson(DEVICES_PATH, doc)) {
            Serial.println("[Devices] No devices file found");
            return;
        }
        
        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject obj : arr) {
            Device device;
            strlcpy(device.id, obj["id"] | "", sizeof(device.id));
            strlcpy(device.name, obj["name"] | "", sizeof(device.name));
            strlcpy(device.type, obj["type"] | "", sizeof(device.type));
            strlcpy(device.controlMethod, obj["controlMethod"] | "", sizeof(device.controlMethod));
            device.gpioPin = obj["gpioPin"] | 0;
            strlcpy(device.ipAddress, obj["ipAddress"] | "", sizeof(device.ipAddress));
            strlcpy(device.controlMode, obj["controlMode"] | "manual", sizeof(device.controlMode));
            
            devices.push_back(device);
        }
        
        Serial.printf("[Devices] Loaded %d devices\n", devices.size());
    }
}

void init() {
    loadDevices();
    Serial.println("[Devices] Initialized");
}

bool addDevice(JsonDocument& doc) {
    Device device;
    strlcpy(device.id, doc["id"] | "", sizeof(device.id));
    strlcpy(device.name, doc["name"] | "", sizeof(device.name));
    strlcpy(device.type, doc["type"] | "", sizeof(device.type));
    strlcpy(device.controlMethod, doc["controlMethod"] | "", sizeof(device.controlMethod));
    device.gpioPin = doc["gpioPin"] | 0;
    strlcpy(device.ipAddress, doc["ipAddress"] | "", sizeof(device.ipAddress));
    strlcpy(device.controlMode, doc["controlMode"] | "manual", sizeof(device.controlMode));
    
    devices.push_back(device);
    saveDevices();
    
    Serial.printf("[Devices] Added device: %s\n", device.name);
    return true;
}

bool updateDevice(const char* deviceId, JsonDocument& doc) {
    for (auto& device : devices) {
        if (strcmp(device.id, deviceId) == 0) {
            if (doc["name"].is<const char*>()) strlcpy(device.name, doc["name"], sizeof(device.name));
            if (doc["type"].is<const char*>()) strlcpy(device.type, doc["type"], sizeof(device.type));
            if (doc["controlMethod"].is<const char*>()) strlcpy(device.controlMethod, doc["controlMethod"], sizeof(device.controlMethod));
            if (doc["gpioPin"].is<int>()) device.gpioPin = doc["gpioPin"];
            if (doc["ipAddress"].is<const char*>()) strlcpy(device.ipAddress, doc["ipAddress"], sizeof(device.ipAddress));
            if (doc["controlMode"].is<const char*>()) strlcpy(device.controlMode, doc["controlMode"], sizeof(device.controlMode));
            
            saveDevices();
            Serial.printf("[Devices] Updated device: %s\n", device.name);
            return true;
        }
    }
    return false;
}

bool removeDevice(const char* deviceId) {
    for (auto it = devices.begin(); it != devices.end(); ++it) {
        if (strcmp(it->id, deviceId) == 0) {
            Serial.printf("[Devices] Removed device: %s\n", it->name);
            devices.erase(it);
            saveDevices();
            return true;
        }
    }
    return false;
}

void getDevicesJson(String& out) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& device : devices) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = device.id;
        obj["name"] = device.name;
        obj["type"] = device.type;
        obj["controlMethod"] = device.controlMethod;
        obj["gpioPin"] = device.gpioPin;
        obj["ipAddress"] = device.ipAddress;
        obj["controlMode"] = device.controlMode;
    }
    
    serializeJson(doc, out);
}

Device* getDevice(const char* deviceId) {
    for (auto& device : devices) {
        if (strcmp(device.id, deviceId) == 0) {
            return &device;
        }
    }
    return nullptr;
}

size_t getDeviceCount() {
    return devices.size();
}

}
