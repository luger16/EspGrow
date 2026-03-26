#include "device_modes.h"
#include "storage.h"
#include "device_controller.h"
#include "devices.h"
#include "time_utils.h"
#include <vector>
#include <cmath>

namespace DeviceModes {

namespace {
    const char* MODES_PATH = "/device_modes.json";
    const char* DAYNIGHT_PATH = "/daynight.json";
    const unsigned long EVAL_INTERVAL = 2000;
    const unsigned long MIN_CYCLE_SEC = 5;

    std::vector<DeviceModeConfig> configs;
    DayNightConfig dayNightConfig;
    bool currentDaytime = true;
    DeviceStateCallback onDeviceStateChange = nullptr;

    unsigned long lastEvaluation = 0;

    struct CycleState {
        bool isOn = false;
        unsigned long lastToggleMs = 0;
    };
    std::map<String, CycleState> cycleStates;

    struct AutoState {
        bool triggered = false;
    };
    std::map<String, AutoState> autoStates;

    void applyDeviceState(const DeviceModeConfig& cfg, bool on) {
        Devices::Device* device = Devices::getDevice(cfg.deviceId);
        if (!device) return;

        if (device->isOn == on) return;

        bool success = DeviceController::control(
            device->controlMethod, device->ipAddress, on);

        if (success) {
            Devices::setDeviceState(cfg.deviceId, on);
            if (onDeviceStateChange) {
                onDeviceStateChange(cfg.deviceId, on);
            }
        }
    }

    float getSensorValue(const char* sensorType, const std::map<String, float>& readings) {
        for (const auto& pair : readings) {
            if (pair.first.indexOf(sensorType) >= 0) {
                return pair.second;
            }
        }
        return NAN;
    }

    void updateDayNight(const std::map<String, float>& readings) {
        if (dayNightConfig.useSchedule) {
            currentDaytime = TimeUtils::isTimeInRange(
                dayNightConfig.dayStartTime, dayNightConfig.nightStartTime);
            return;
        }

        float light = NAN;
        for (const auto& pair : readings) {
            if (pair.first.indexOf("light") >= 0) {
                light = pair.second;
                break;
            }
        }

        if (std::isnan(light)) {
            currentDaytime = TimeUtils::isTimeInRange(
                dayNightConfig.dayStartTime, dayNightConfig.nightStartTime);
            return;
        }

        if (currentDaytime) {
            if (light < dayNightConfig.lightThreshold - dayNightConfig.lightHysteresis) {
                currentDaytime = false;
                Serial.println("[DeviceModes] Day -> Night (light sensor)");
            }
        } else {
            if (light > dayNightConfig.lightThreshold + dayNightConfig.lightHysteresis) {
                currentDaytime = true;
                Serial.println("[DeviceModes] Night -> Day (light sensor)");
            }
        }
    }

    void evaluateAuto(DeviceModeConfig& cfg, const std::map<String, float>& readings) {
        if (cfg.triggerCount == 0) return;

        String key(cfg.deviceId);
        bool wasTriggered = autoStates[key].triggered;
        bool anyTriggerMet = false;

        for (uint8_t i = 0; i < cfg.triggerCount; i++) {
            const AutoTrigger& trigger = cfg.triggers[i];
            float value = getSensorValue(trigger.sensorType, readings);
            if (std::isnan(value)) continue;

            float threshold = currentDaytime ? trigger.dayThreshold : trigger.nightThreshold;

            if (trigger.triggerAbove) {
                if (wasTriggered) {
                    if (value > (threshold - trigger.hysteresis)) anyTriggerMet = true;
                } else {
                    if (value > threshold) anyTriggerMet = true;
                }
            } else {
                if (wasTriggered) {
                    if (value < (threshold + trigger.hysteresis)) anyTriggerMet = true;
                } else {
                    if (value < threshold) anyTriggerMet = true;
                }
            }
        }

        if (anyTriggerMet && !wasTriggered) {
            Serial.printf("[DeviceModes] AUTO triggered for %s\n", cfg.deviceId);
            applyDeviceState(cfg, true);
            autoStates[key].triggered = true;
        } else if (!anyTriggerMet && wasTriggered) {
            Serial.printf("[DeviceModes] AUTO cleared for %s\n", cfg.deviceId);
            applyDeviceState(cfg, false);
            autoStates[key].triggered = false;
        }
    }

    void evaluateCycle(DeviceModeConfig& cfg) {
        String key(cfg.deviceId);
        CycleState& state = cycleStates[key];

        if (cfg.cycle.dayOnly && !currentDaytime) {
            if (state.isOn) {
                applyDeviceState(cfg, false);
                state.isOn = false;
            }
            return;
        }

        if (state.lastToggleMs == 0) {
            state.lastToggleMs = millis();
            state.isOn = true;
            applyDeviceState(cfg, true);
            return;
        }

        unsigned long elapsed = (millis() - state.lastToggleMs) / 1000;
        unsigned long currentPhaseDuration = state.isOn
            ? cfg.cycle.onDurationSec : cfg.cycle.offDurationSec;

        if (elapsed >= currentPhaseDuration) {
            state.isOn = !state.isOn;
            state.lastToggleMs = millis();
            applyDeviceState(cfg, state.isOn);
            Serial.printf("[DeviceModes] CYCLE %s -> %s\n",
                cfg.deviceId, state.isOn ? "ON" : "OFF");
        }
    }

    void evaluateSchedule(DeviceModeConfig& cfg) {
        bool inRange = TimeUtils::isTimeInRange(cfg.schedule.startTime, cfg.schedule.endTime);
        applyDeviceState(cfg, inRange);
    }

    void saveModes() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (const auto& cfg : configs) {
            JsonObject obj = arr.add<JsonObject>();
            obj["deviceId"] = cfg.deviceId;
            obj["mode"] = modeToString(cfg.mode);

            if (cfg.mode == MODE_AUTO && cfg.triggerCount > 0) {
                JsonArray triggers = obj["triggers"].to<JsonArray>();
                for (uint8_t i = 0; i < cfg.triggerCount; i++) {
                    JsonObject t = triggers.add<JsonObject>();
                    t["sensorType"] = cfg.triggers[i].sensorType;
                    t["dayThreshold"] = cfg.triggers[i].dayThreshold;
                    t["nightThreshold"] = cfg.triggers[i].nightThreshold;
                    t["hysteresis"] = cfg.triggers[i].hysteresis;
                    t["triggerAbove"] = cfg.triggers[i].triggerAbove;
                }
            }

            if (cfg.mode == MODE_CYCLE) {
                JsonObject cycle = obj["cycle"].to<JsonObject>();
                cycle["onDurationSec"] = cfg.cycle.onDurationSec;
                cycle["offDurationSec"] = cfg.cycle.offDurationSec;
                cycle["dayOnly"] = cfg.cycle.dayOnly;
            }

            if (cfg.mode == MODE_SCHEDULE) {
                JsonObject sched = obj["schedule"].to<JsonObject>();
                sched["startTime"] = cfg.schedule.startTime;
                sched["endTime"] = cfg.schedule.endTime;
            }
        }

        Storage::writeJson(MODES_PATH, doc);
        Serial.printf("[DeviceModes] Saved %d mode configs\n", configs.size());
    }

    void loadModes() {
        configs.clear();
        JsonDocument doc;
        if (!Storage::readJson(MODES_PATH, doc)) {
            Serial.println("[DeviceModes] No modes file found");
            return;
        }

        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject obj : arr) {
            DeviceModeConfig cfg = {};
            strlcpy(cfg.deviceId, obj["deviceId"] | "", sizeof(cfg.deviceId));
            cfg.mode = stringToMode(obj["mode"] | "off");

            if (obj["triggers"].is<JsonArray>()) {
                JsonArray triggers = obj["triggers"].as<JsonArray>();
                cfg.triggerCount = 0;
                for (JsonObject t : triggers) {
                    if (cfg.triggerCount >= MAX_TRIGGERS) break;
                    AutoTrigger& trigger = cfg.triggers[cfg.triggerCount];
                    strlcpy(trigger.sensorType, t["sensorType"] | "", sizeof(trigger.sensorType));
                    trigger.dayThreshold = t["dayThreshold"] | 0.0f;
                    trigger.nightThreshold = t["nightThreshold"] | 0.0f;
                    trigger.hysteresis = t["hysteresis"] | 0.5f;
                    trigger.triggerAbove = t["triggerAbove"] | true;
                    cfg.triggerCount++;
                }
            }

            if (obj["cycle"].is<JsonObject>()) {
                JsonObject cycle = obj["cycle"].as<JsonObject>();
                cfg.cycle.onDurationSec = cycle["onDurationSec"] | 300UL;
                cfg.cycle.offDurationSec = cycle["offDurationSec"] | 300UL;
                cfg.cycle.dayOnly = cycle["dayOnly"] | false;
            }

            if (obj["schedule"].is<JsonObject>()) {
                JsonObject sched = obj["schedule"].as<JsonObject>();
                strlcpy(cfg.schedule.startTime, sched["startTime"] | "06:00", sizeof(cfg.schedule.startTime));
                strlcpy(cfg.schedule.endTime, sched["endTime"] | "22:00", sizeof(cfg.schedule.endTime));
            }

            configs.push_back(cfg);
        }

        Serial.printf("[DeviceModes] Loaded %d mode configs\n", configs.size());
    }

    void loadDayNightConfig() {
        dayNightConfig.useSchedule = false;
        strlcpy(dayNightConfig.dayStartTime, "06:00", sizeof(dayNightConfig.dayStartTime));
        strlcpy(dayNightConfig.nightStartTime, "22:00", sizeof(dayNightConfig.nightStartTime));
        dayNightConfig.lightThreshold = 10.0f;
        dayNightConfig.lightHysteresis = 5.0f;

        JsonDocument doc;
        if (!Storage::readJson(DAYNIGHT_PATH, doc)) return;

        if (doc["useSchedule"].is<bool>()) dayNightConfig.useSchedule = doc["useSchedule"];
        if (doc["dayStartTime"].is<const char*>()) strlcpy(dayNightConfig.dayStartTime, doc["dayStartTime"], sizeof(dayNightConfig.dayStartTime));
        if (doc["nightStartTime"].is<const char*>()) strlcpy(dayNightConfig.nightStartTime, doc["nightStartTime"], sizeof(dayNightConfig.nightStartTime));
        if (doc["lightThreshold"].is<float>()) dayNightConfig.lightThreshold = doc["lightThreshold"];
        if (doc["lightHysteresis"].is<float>()) dayNightConfig.lightHysteresis = doc["lightHysteresis"];

        Serial.printf("[DeviceModes] Day/night config: %s, threshold=%.1f\n",
            dayNightConfig.useSchedule ? "schedule" : "light", dayNightConfig.lightThreshold);
    }

    void saveDayNightConfig() {
        JsonDocument doc;
        doc["useSchedule"] = dayNightConfig.useSchedule;
        doc["dayStartTime"] = dayNightConfig.dayStartTime;
        doc["nightStartTime"] = dayNightConfig.nightStartTime;
        doc["lightThreshold"] = dayNightConfig.lightThreshold;
        doc["lightHysteresis"] = dayNightConfig.lightHysteresis;
        Storage::writeJson(DAYNIGHT_PATH, doc);
    }
}

void init() {
    loadDayNightConfig();
    loadModes();
    Serial.println("[DeviceModes] Initialized");
}

void loop(const std::map<String, float>& sensorReadings) {
    if (millis() - lastEvaluation < EVAL_INTERVAL) return;
    lastEvaluation = millis();

    updateDayNight(sensorReadings);

    for (auto& cfg : configs) {
        switch (cfg.mode) {
            case MODE_OFF:
                applyDeviceState(cfg, false);
                break;
            case MODE_ON:
                applyDeviceState(cfg, true);
                break;
            case MODE_AUTO:
                evaluateAuto(cfg, sensorReadings);
                break;
            case MODE_CYCLE:
                evaluateCycle(cfg);
                break;
            case MODE_SCHEDULE:
                evaluateSchedule(cfg);
                break;
        }
    }
}

void setDeviceStateCallback(DeviceStateCallback cb) {
    onDeviceStateChange = cb;
}

bool setMode(JsonDocument& doc) {
    const char* deviceId = doc["deviceId"];
    if (!deviceId || strlen(deviceId) == 0) return false;

    Devices::Device* device = Devices::getDevice(deviceId);
    if (!device) return false;

    DeviceModeConfig cfg = {};
    strlcpy(cfg.deviceId, deviceId, sizeof(cfg.deviceId));
    cfg.mode = stringToMode(doc["mode"] | "off");

    if (doc["triggers"].is<JsonArray>()) {
        JsonArray triggers = doc["triggers"].as<JsonArray>();
        cfg.triggerCount = 0;
        for (JsonObject t : triggers) {
            if (cfg.triggerCount >= MAX_TRIGGERS) break;
            AutoTrigger& trigger = cfg.triggers[cfg.triggerCount];
            strlcpy(trigger.sensorType, t["sensorType"] | "", sizeof(trigger.sensorType));
            trigger.dayThreshold = t["dayThreshold"] | 0.0f;
            trigger.nightThreshold = t["nightThreshold"] | 0.0f;
            trigger.hysteresis = t["hysteresis"] | 0.5f;
            trigger.triggerAbove = t["triggerAbove"] | true;
            cfg.triggerCount++;
        }
    }

    if (doc["cycle"].is<JsonObject>()) {
        JsonObject cycle = doc["cycle"].as<JsonObject>();
        cfg.cycle.onDurationSec = max((unsigned long)MIN_CYCLE_SEC, (unsigned long)(cycle["onDurationSec"] | 300));
        cfg.cycle.offDurationSec = max((unsigned long)MIN_CYCLE_SEC, (unsigned long)(cycle["offDurationSec"] | 300));
        cfg.cycle.dayOnly = cycle["dayOnly"] | false;
    }

    if (doc["schedule"].is<JsonObject>()) {
        JsonObject sched = doc["schedule"].as<JsonObject>();
        strlcpy(cfg.schedule.startTime, sched["startTime"] | "06:00", sizeof(cfg.schedule.startTime));
        strlcpy(cfg.schedule.endTime, sched["endTime"] | "22:00", sizeof(cfg.schedule.endTime));
    }

    for (auto& existing : configs) {
        if (strcmp(existing.deviceId, deviceId) == 0) {
            existing = cfg;
            autoStates.erase(String(deviceId));
            cycleStates.erase(String(deviceId));
            saveModes();
            Serial.printf("[DeviceModes] Updated mode for %s: %s\n", deviceId, modeToString(cfg.mode));
            return true;
        }
    }

    configs.push_back(cfg);
    saveModes();
    Serial.printf("[DeviceModes] Set mode for %s: %s\n", deviceId, modeToString(cfg.mode));
    return true;
}

bool removeMode(const char* deviceId) {
    for (auto it = configs.begin(); it != configs.end(); ++it) {
        if (strcmp(it->deviceId, deviceId) == 0) {
            Serial.printf("[DeviceModes] Removed mode for %s\n", deviceId);
            autoStates.erase(String(deviceId));
            cycleStates.erase(String(deviceId));
            configs.erase(it);
            saveModes();
            return true;
        }
    }
    return false;
}

void removeModeForDevice(const char* deviceId) {
    removeMode(deviceId);
}

void getModesJson(String& out) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (const auto& cfg : configs) {
        JsonObject obj = arr.add<JsonObject>();
        obj["deviceId"] = cfg.deviceId;
        obj["mode"] = modeToString(cfg.mode);

        if (cfg.mode == MODE_AUTO && cfg.triggerCount > 0) {
            JsonArray triggers = obj["triggers"].to<JsonArray>();
            for (uint8_t i = 0; i < cfg.triggerCount; i++) {
                JsonObject t = triggers.add<JsonObject>();
                t["sensorType"] = cfg.triggers[i].sensorType;
                t["dayThreshold"] = cfg.triggers[i].dayThreshold;
                t["nightThreshold"] = cfg.triggers[i].nightThreshold;
                t["hysteresis"] = cfg.triggers[i].hysteresis;
                t["triggerAbove"] = cfg.triggers[i].triggerAbove;
            }
        }

        if (cfg.mode == MODE_CYCLE) {
            JsonObject cycle = obj["cycle"].to<JsonObject>();
            cycle["onDurationSec"] = cfg.cycle.onDurationSec;
            cycle["offDurationSec"] = cfg.cycle.offDurationSec;
            cycle["dayOnly"] = cfg.cycle.dayOnly;
        }

        if (cfg.mode == MODE_SCHEDULE) {
            JsonObject sched = obj["schedule"].to<JsonObject>();
            sched["startTime"] = cfg.schedule.startTime;
            sched["endTime"] = cfg.schedule.endTime;
        }
    }

    serializeJson(doc, out);
}

bool isDaytime() {
    return currentDaytime;
}

void getDayNightConfigJson(String& out) {
    JsonDocument doc;
    doc["useSchedule"] = dayNightConfig.useSchedule;
    doc["dayStartTime"] = dayNightConfig.dayStartTime;
    doc["nightStartTime"] = dayNightConfig.nightStartTime;
    doc["lightThreshold"] = dayNightConfig.lightThreshold;
    doc["lightHysteresis"] = dayNightConfig.lightHysteresis;
    doc["isDaytime"] = currentDaytime;
    serializeJson(doc, out);
}

bool setDayNightConfig(JsonDocument& doc) {
    if (doc["useSchedule"].is<bool>()) dayNightConfig.useSchedule = doc["useSchedule"];
    if (doc["dayStartTime"].is<const char*>()) strlcpy(dayNightConfig.dayStartTime, doc["dayStartTime"], sizeof(dayNightConfig.dayStartTime));
    if (doc["nightStartTime"].is<const char*>()) strlcpy(dayNightConfig.nightStartTime, doc["nightStartTime"], sizeof(dayNightConfig.nightStartTime));
    if (doc["lightThreshold"].is<float>()) dayNightConfig.lightThreshold = doc["lightThreshold"];
    if (doc["lightHysteresis"].is<float>()) dayNightConfig.lightHysteresis = doc["lightHysteresis"];
    saveDayNightConfig();
    Serial.printf("[DeviceModes] Updated day/night config\n");
    return true;
}

const char* modeToString(Mode mode) {
    switch (mode) {
        case MODE_OFF: return "off";
        case MODE_ON: return "on";
        case MODE_AUTO: return "auto";
        case MODE_CYCLE: return "cycle";
        case MODE_SCHEDULE: return "schedule";
        default: return "off";
    }
}

Mode stringToMode(const char* str) {
    if (strcmp(str, "on") == 0) return MODE_ON;
    if (strcmp(str, "auto") == 0) return MODE_AUTO;
    if (strcmp(str, "cycle") == 0) return MODE_CYCLE;
    if (strcmp(str, "schedule") == 0) return MODE_SCHEDULE;
    return MODE_OFF;
}

}
