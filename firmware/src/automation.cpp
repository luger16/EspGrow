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
    unsigned long lastEvaluation = 0;
    const unsigned long EVAL_INTERVAL = 2000;
    DeviceStateCallback onDeviceStateChange = nullptr;
    
    bool evaluateCondition(float value, const char* op, float threshold) {
        if (strcmp(op, ">") == 0) return value > threshold;
        if (strcmp(op, ">=") == 0) return value >= threshold;
        if (strcmp(op, "<") == 0) return value < threshold;
        if (strcmp(op, "<=") == 0) return value <= threshold;
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
            strlcpy(rule.deviceId, obj["deviceId"] | "", sizeof(rule.deviceId));
            strlcpy(rule.deviceMethod, obj["deviceMethod"] | "", sizeof(rule.deviceMethod));
            strlcpy(rule.deviceTarget, obj["deviceTarget"] | "", sizeof(rule.deviceTarget));
            
            const char* action = obj["action"] | "turn_on";
            rule.actionOn = strcmp(action, "turn_on") == 0;
            
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
    
    for (const auto& rule : rules) {
        if (!rule.enabled) continue;
        
        float value = getSensorValue(rule.sensorId, sensorReadings);
        bool conditionMet = evaluateCondition(value, rule.op, rule.threshold);
        
        String ruleKey(rule.id);
        bool wasMet = lastTriggerState.count(ruleKey) ? lastTriggerState[ruleKey] : false;
        lastTriggerState[ruleKey] = conditionMet;
        
        if (conditionMet && !wasMet) {
            Serial.printf("[Automation] Rule '%s' triggered: %s %.1f %s %.1f\n", 
                rule.name, rule.sensorId, value, rule.op, rule.threshold);
            bool success = DeviceController::control(rule.deviceMethod, rule.deviceTarget, rule.actionOn);
            if (success) {
                Devices::setDeviceState(rule.deviceId, rule.actionOn);
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
    strlcpy(rule.deviceId, doc["deviceId"] | "", sizeof(rule.deviceId));
    strlcpy(rule.deviceMethod, doc["deviceMethod"] | "", sizeof(rule.deviceMethod));
    strlcpy(rule.deviceTarget, doc["deviceTarget"] | "", sizeof(rule.deviceTarget));
    
    const char* action = doc["action"] | "turn_on";
    rule.actionOn = strcmp(action, "turn_on") == 0;
    
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

}
