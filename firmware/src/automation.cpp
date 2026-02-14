#include "automation.h"
#include "storage.h"
#include "device_controller.h"
#include "devices.h"
#include <vector>
#include <map>

namespace Automation {

namespace {
    const char* RULES_PATH = "/rules.json";
    std::vector<Rule> rules;
    std::map<String, bool> lastTriggerState;
    std::map<String, unsigned long> manualOverrides;
    unsigned long lastEvaluation = 0;
    const unsigned long EVAL_INTERVAL = 2000;
    DeviceStateCallback onDeviceStateChange = nullptr;

    void syncTriggerStateToDevice(const char* deviceId) {
        Devices::Device* device = Devices::getDevice(deviceId);
        if (!device) return;
        for (auto& rule : rules) {
            if (strcmp(rule.deviceId, deviceId) == 0) {
                lastTriggerState[String(rule.id)] = (device->isOn == rule.actionOn);
                rule.lastStateChangeMs = 0;
            }
        }
    }
    
    bool evaluateCondition(float value, const char* op, float threshold, bool deviceCurrentlyOn, float thresholdOff, bool useHysteresis) {
        if (useHysteresis) {
            if (deviceCurrentlyOn) {
                if (strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) {
                    return value > thresholdOff;
                } else if (strcmp(op, "<") == 0 || strcmp(op, "<=") == 0) {
                    return value < thresholdOff;
                }
            } else {
                if (strcmp(op, ">") == 0) return value > threshold;
                if (strcmp(op, ">=") == 0) return value >= threshold;
                if (strcmp(op, "<") == 0) return value < threshold;
                if (strcmp(op, "<=") == 0) return value <= threshold;
            }
        } else {
            if (strcmp(op, ">") == 0) return value > threshold;
            if (strcmp(op, ">=") == 0) return value >= threshold;
            if (strcmp(op, "<") == 0) return value < threshold;
            if (strcmp(op, "<=") == 0) return value <= threshold;
        }
        if (strcmp(op, "=") == 0) return abs(value - threshold) < 0.1f;
        return false;
    }
    
    float getSensorValue(const char* sensorId, const std::map<String, float>& sensorReadings) {
        auto it = sensorReadings.find(String(sensorId));
        if (it != sensorReadings.end()) {
            return it->second;
        }
        return 0;
    }
    
    void saveRules() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        
        for (const auto& rule : rules) {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = rule.id;
            obj["name"] = rule.name;
            obj["enabled"] = rule.enabled;
            obj["sensorId"] = rule.sensorId;
            obj["operator"] = rule.op;
            obj["threshold"] = rule.threshold;
            obj["thresholdOff"] = rule.thresholdOff;
            obj["useHysteresis"] = rule.useHysteresis;
            obj["minRunTimeMs"] = rule.minRunTimeMs;
            obj["deviceId"] = rule.deviceId;
            obj["deviceMethod"] = rule.deviceMethod;
            obj["deviceTarget"] = rule.deviceTarget;
            obj["action"] = rule.actionOn ? "turn_on" : "turn_off";
        }
        
        Storage::writeJson(RULES_PATH, doc);
        Serial.printf("[Automation] Saved %d rules\n", rules.size());
    }
    
    void loadRules() {
        JsonDocument doc;
        if (!Storage::readJson(RULES_PATH, doc)) {
            Serial.println("[Automation] No rules file found");
            return;
        }
        
        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject obj : arr) {
            Rule rule;
            strlcpy(rule.id, obj["id"] | "", sizeof(rule.id));
            strlcpy(rule.name, obj["name"] | "", sizeof(rule.name));
            rule.enabled = obj["enabled"] | false;
            strlcpy(rule.sensorId, obj["sensorId"] | "", sizeof(rule.sensorId));
            strlcpy(rule.op, obj["operator"] | ">", sizeof(rule.op));
            rule.threshold = obj["threshold"] | 0.0f;
            rule.thresholdOff = obj["thresholdOff"] | rule.threshold;
            rule.useHysteresis = obj["useHysteresis"] | false;
            rule.minRunTimeMs = obj["minRunTimeMs"] | 0UL;
            strlcpy(rule.deviceId, obj["deviceId"] | "", sizeof(rule.deviceId));
            strlcpy(rule.deviceMethod, obj["deviceMethod"] | "", sizeof(rule.deviceMethod));
            strlcpy(rule.deviceTarget, obj["deviceTarget"] | "", sizeof(rule.deviceTarget));
            
            const char* action = obj["action"] | "turn_on";
            rule.actionOn = strcmp(action, "turn_on") == 0;
            
            rule.lastStateChangeMs = 0;
            
            rules.push_back(rule);
        }
        
        Serial.printf("[Automation] Loaded %d rules\n", rules.size());
    }
}

void init() {
    loadRules();
    Serial.println("[Automation] Initialized");
}

void setDeviceStateCallback(DeviceStateCallback cb) {
    onDeviceStateChange = cb;
}

void loop(const std::map<String, float>& sensorReadings) {
    if (millis() - lastEvaluation < EVAL_INTERVAL) return;
    lastEvaluation = millis();
    
    for (auto& rule : rules) {
        if (!rule.enabled) continue;
        
        if (manualOverrides.count(String(rule.deviceId)) && 
            millis() < manualOverrides[String(rule.deviceId)]) {
            continue;
        }
        
        float value = getSensorValue(rule.sensorId, sensorReadings);
        
        String ruleKey(rule.id);
        bool wasMet = lastTriggerState.count(ruleKey) ? lastTriggerState[ruleKey] : false;
        bool deviceCurrentlyOn = (wasMet == rule.actionOn);
        
        bool conditionMet = evaluateCondition(value, rule.op, rule.threshold, deviceCurrentlyOn, rule.thresholdOff, rule.useHysteresis);
        
        bool wantsToChange = (conditionMet && !wasMet) || (!conditionMet && wasMet);
        
        if (wantsToChange && rule.minRunTimeMs > 0 && rule.lastStateChangeMs > 0) {
            unsigned long timeSinceChange = millis() - rule.lastStateChangeMs;
            if (timeSinceChange < rule.minRunTimeMs) {
                continue;
            }
        }
        
        lastTriggerState[ruleKey] = conditionMet;
        
        if (conditionMet && !wasMet) {
            Serial.printf("[Automation] Rule '%s' triggered: %s %.1f %s %.1f\n", 
                rule.name, rule.sensorId, value, rule.op, rule.threshold);
            bool success = DeviceController::control(rule.deviceMethod, rule.deviceTarget, rule.actionOn);
            if (success) {
                Devices::setDeviceState(rule.deviceId, rule.actionOn);
                rule.lastStateChangeMs = millis();
                if (onDeviceStateChange) {
                    onDeviceStateChange(rule.deviceId, rule.deviceMethod, rule.deviceTarget, rule.actionOn);
                }
            }
        } else if (!conditionMet && wasMet) {
            Serial.printf("[Automation] Rule '%s' condition cleared, reverting device\n", rule.name);
            bool revertState = !rule.actionOn;
            bool success = DeviceController::control(rule.deviceMethod, rule.deviceTarget, revertState);
            if (success) {
                Devices::setDeviceState(rule.deviceId, revertState);
                rule.lastStateChangeMs = millis();
                if (onDeviceStateChange) {
                    onDeviceStateChange(rule.deviceId, rule.deviceMethod, rule.deviceTarget, revertState);
                }
            }
        }
    }
}

bool addRule(JsonDocument& doc) {
    Rule rule;
    strlcpy(rule.id, doc["id"] | "", sizeof(rule.id));
    strlcpy(rule.name, doc["name"] | "", sizeof(rule.name));
    rule.enabled = doc["enabled"] | true;
    strlcpy(rule.sensorId, doc["sensorId"] | "", sizeof(rule.sensorId));
    strlcpy(rule.op, doc["operator"] | ">", sizeof(rule.op));
    rule.threshold = doc["threshold"] | 0.0f;
    rule.thresholdOff = doc["thresholdOff"] | rule.threshold;
    rule.useHysteresis = doc["useHysteresis"] | false;
    rule.minRunTimeMs = doc["minRunTimeMs"] | 0UL;
    strlcpy(rule.deviceId, doc["deviceId"] | "", sizeof(rule.deviceId));
    strlcpy(rule.deviceMethod, doc["deviceMethod"] | "", sizeof(rule.deviceMethod));
    strlcpy(rule.deviceTarget, doc["deviceTarget"] | "", sizeof(rule.deviceTarget));
    
    const char* action = doc["action"] | "turn_on";
    rule.actionOn = strcmp(action, "turn_on") == 0;
    
    rule.lastStateChangeMs = 0;
    
    rules.push_back(rule);
    saveRules();
    
    Serial.printf("[Automation] Added rule: %s\n", rule.name);
    return true;
}

bool updateRule(const char* ruleId, JsonDocument& doc) {
    for (auto& rule : rules) {
        if (strcmp(rule.id, ruleId) == 0) {
            if (doc["name"].is<const char*>()) strlcpy(rule.name, doc["name"], sizeof(rule.name));
            if (doc["enabled"].is<bool>()) rule.enabled = doc["enabled"];
            if (doc["sensorId"].is<const char*>()) strlcpy(rule.sensorId, doc["sensorId"], sizeof(rule.sensorId));
            if (doc["operator"].is<const char*>()) strlcpy(rule.op, doc["operator"], sizeof(rule.op));
            if (doc["threshold"].is<float>()) rule.threshold = doc["threshold"];
            if (doc["thresholdOff"].is<float>()) rule.thresholdOff = doc["thresholdOff"];
            if (doc["useHysteresis"].is<bool>()) rule.useHysteresis = doc["useHysteresis"];
            if (doc["minRunTimeMs"].is<unsigned long>()) rule.minRunTimeMs = doc["minRunTimeMs"];
            if (doc["deviceId"].is<const char*>()) strlcpy(rule.deviceId, doc["deviceId"], sizeof(rule.deviceId));
            if (doc["deviceMethod"].is<const char*>()) strlcpy(rule.deviceMethod, doc["deviceMethod"], sizeof(rule.deviceMethod));
            if (doc["deviceTarget"].is<const char*>()) strlcpy(rule.deviceTarget, doc["deviceTarget"], sizeof(rule.deviceTarget));
            if (doc["action"].is<const char*>()) {
                const char* action = doc["action"];
                rule.actionOn = strcmp(action, "turn_on") == 0;
            }
            
            saveRules();
            Serial.printf("[Automation] Updated rule: %s\n", rule.name);
            return true;
        }
    }
    return false;
}

bool removeRule(const char* ruleId) {
    for (auto it = rules.begin(); it != rules.end(); ++it) {
        if (strcmp(it->id, ruleId) == 0) {
            Serial.printf("[Automation] Removed rule: %s\n", it->name);
            lastTriggerState.erase(String(it->id));
            rules.erase(it);
            saveRules();
            return true;
        }
    }
    return false;
}

bool toggleRule(const char* ruleId) {
    for (auto& rule : rules) {
        if (strcmp(rule.id, ruleId) == 0) {
            rule.enabled = !rule.enabled;
            if (!rule.enabled) {
                lastTriggerState.erase(String(rule.id));
            }
            saveRules();
            Serial.printf("[Automation] Toggled rule '%s' -> %s\n", rule.name, rule.enabled ? "enabled" : "disabled");
            return true;
        }
    }
    return false;
}

void getRulesJson(String& out) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& rule : rules) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = rule.id;
        obj["name"] = rule.name;
        obj["enabled"] = rule.enabled;
        obj["sensorId"] = rule.sensorId;
        obj["operator"] = rule.op;
        obj["threshold"] = rule.threshold;
        obj["thresholdOff"] = rule.thresholdOff;
        obj["useHysteresis"] = rule.useHysteresis;
        obj["minRunTimeMs"] = rule.minRunTimeMs;
        obj["deviceId"] = rule.deviceId;
        obj["deviceMethod"] = rule.deviceMethod;
        obj["deviceTarget"] = rule.deviceTarget;
        obj["action"] = rule.actionOn ? "turn_on" : "turn_off";
    }
    
    serializeJson(doc, out);
}

bool isDeviceUsedByEnabledRule(const char* deviceId) {
    for (const auto& rule : rules) {
        if (rule.enabled && strcmp(rule.deviceId, deviceId) == 0) {
            return true;
        }
    }
    return false;
}

void removeRulesForDevice(const char* deviceId) {
    bool changed = false;
    for (auto it = rules.begin(); it != rules.end(); ) {
        if (strcmp(it->deviceId, deviceId) == 0) {
            Serial.printf("[Automation] Removing rule '%s' for deleted device %s\n", it->name, deviceId);
            lastTriggerState.erase(String(it->id));
            it = rules.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) saveRules();
}

void setManualOverride(const char* deviceId, unsigned long durationMs) {
    manualOverrides[String(deviceId)] = millis() + durationMs;
    Serial.printf("[Automation] Manual override set for %s (%lums)\n", deviceId, durationMs);
}

bool isDeviceOverridden(const char* deviceId) {
    auto it = manualOverrides.find(String(deviceId));
    if (it == manualOverrides.end()) return false;
    if (millis() >= it->second) {
        manualOverrides.erase(it);
        return false;
    }
    return true;
}

unsigned long getOverrideRemaining(const char* deviceId) {
    auto it = manualOverrides.find(String(deviceId));
    if (it == manualOverrides.end()) return 0;
    unsigned long now = millis();
    if (now >= it->second) {
        manualOverrides.erase(it);
        return 0;
    }
    return it->second - now;
}

void clearExpiredOverrides() {
    for (auto it = manualOverrides.begin(); it != manualOverrides.end(); ) {
        if (millis() >= it->second) {
            Serial.printf("[Automation] Override expired for %s\n", it->first.c_str());
            syncTriggerStateToDevice(it->first.c_str());
            it = manualOverrides.erase(it);
        } else {
            ++it;
        }
    }
}

void clearOverride(const char* deviceId) {
    auto it = manualOverrides.find(String(deviceId));
    if (it != manualOverrides.end()) {
        manualOverrides.erase(it);
        syncTriggerStateToDevice(deviceId);
        Serial.printf("[Automation] Manual override cleared for %s\n", deviceId);
    }
}

void forceEvaluation(const std::map<String, float>& sensorReadings) {
    lastEvaluation = 0;
    loop(sensorReadings);
}

}
