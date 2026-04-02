#include "event_log.h"
#include "climate_config.h"
#include "device_modes.h"
#include "sensor_config.h"
#include "storage.h"
#include "websocket_server.h"
#include "wifi_manager.h"
#include <ArduinoJson.h>
#include <cmath>
#include <time.h>

namespace EventLog {

namespace {
    const char* EVENTS_PATH = "/events.json";
    const unsigned long PERSIST_INTERVAL = 300000;
    const unsigned long EVAL_INTERVAL = 5000;
    const unsigned long ALERT_COOLDOWN = 300000;

    Event events[MAX_EVENTS];
    size_t eventHead = 0;
    size_t eventCount = 0;

    unsigned long lastPersist = 0;
    unsigned long lastEval = 0;
    unsigned long eventIdCounter = 0;
    bool dirty = false;

    struct AlertCooldown {
        char sensorType[16];
        unsigned long lastAlert;
    };
    static constexpr size_t MAX_COOLDOWNS = 8;
    AlertCooldown cooldowns[MAX_COOLDOWNS];
    size_t cooldownCount = 0;

    struct AlertMargin {
        const char* sensorType;
        float margin;
    };

    const AlertMargin MARGINS[] = {
        {"temperature", 2.0f},
        {"humidity", 10.0f},
        {"vpd", 0.2f},
        {"co2", 200.0f},
    };
    static constexpr size_t MARGIN_COUNT = 4;

    float getMargin(const char* sensorType) {
        for (size_t i = 0; i < MARGIN_COUNT; i++) {
            if (strcmp(MARGINS[i].sensorType, sensorType) == 0) return MARGINS[i].margin;
        }
        return 0.0f;
    }

    float getTarget(const char* sensorType, bool daytime, const ClimateConfig::PhaseTargets& t) {
        if (strcmp(sensorType, "temperature") == 0) return daytime ? t.tempDay : t.tempNight;
        if (strcmp(sensorType, "humidity") == 0) return daytime ? t.humidityDay : t.humidityNight;
        if (strcmp(sensorType, "vpd") == 0) return daytime ? t.vpdDay : t.vpdNight;
        if (strcmp(sensorType, "co2") == 0) return daytime ? t.co2Day : t.co2Night;
        return NAN;
    }

    bool isOnCooldown(const char* sensorType) {
        unsigned long now = millis();
        for (size_t i = 0; i < cooldownCount; i++) {
            if (strcmp(cooldowns[i].sensorType, sensorType) == 0) {
                return (now - cooldowns[i].lastAlert) < ALERT_COOLDOWN;
            }
        }
        return false;
    }

    void setCooldown(const char* sensorType) {
        for (size_t i = 0; i < cooldownCount; i++) {
            if (strcmp(cooldowns[i].sensorType, sensorType) == 0) {
                cooldowns[i].lastAlert = millis();
                return;
            }
        }
        if (cooldownCount < MAX_COOLDOWNS) {
            strlcpy(cooldowns[cooldownCount].sensorType, sensorType, sizeof(cooldowns[0].sensorType));
            cooldowns[cooldownCount].lastAlert = millis();
            cooldownCount++;
        }
    }

    void addEvent(const Event& event) {
        events[eventHead] = event;
        eventHead = (eventHead + 1) % MAX_EVENTS;
        if (eventCount < MAX_EVENTS) eventCount++;
        dirty = true;
    }

    void broadcastEvent(const Event& event) {
        JsonDocument doc;
        doc["type"] = "event";
        JsonObject data = doc["data"].to<JsonObject>();
        data["id"] = event.id;
        data["eventType"] = event.type;
        data["title"] = event.title;
        data["description"] = event.description;
        data["severity"] = event.severity;
        data["timestamp"] = event.timestamp;

        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }

    void saveEvents() {
        if (eventCount == 0) return;

        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        size_t start = (eventCount >= MAX_EVENTS) ? eventHead : 0;
        for (size_t i = 0; i < eventCount; i++) {
            size_t idx = (start + i) % MAX_EVENTS;
            const Event& e = events[idx];
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = e.id;
            obj["type"] = e.type;
            obj["title"] = e.title;
            obj["description"] = e.description;
            obj["severity"] = e.severity;
            obj["timestamp"] = e.timestamp;
        }

        Storage::writeJson(EVENTS_PATH, doc);
    }

    void loadEvents() {
        JsonDocument doc;
        if (!Storage::readJson(EVENTS_PATH, doc)) {
            Serial.println("[EventLog] No events file found");
            return;
        }

        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject obj : arr) {
            Event e = {};
            strlcpy(e.id, obj["id"] | "", sizeof(e.id));
            strlcpy(e.type, obj["type"] | "", sizeof(e.type));
            strlcpy(e.title, obj["title"] | "", sizeof(e.title));
            strlcpy(e.description, obj["description"] | "", sizeof(e.description));
            strlcpy(e.severity, obj["severity"] | "info", sizeof(e.severity));
            e.timestamp = obj["timestamp"] | 0;
            addEvent(e);
        }

        dirty = false;
        Serial.printf("[EventLog] Loaded %d events\n", eventCount);
    }

    void evaluateAlerts(const std::map<String, float>& sensorReadings) {
        const ClimateConfig::PhaseTargets& targets = ClimateConfig::getTargets();
        bool daytime = DeviceModes::isDaytime();

        for (const auto& pair : sensorReadings) {
            SensorConfig::Sensor* cfg = SensorConfig::getSensor(pair.first.c_str());
            if (!cfg) continue;

            float margin = getMargin(cfg->type);
            if (margin == 0.0f) continue;

            float target = getTarget(cfg->type, daytime, targets);
            if (std::isnan(target)) continue;

            float value = pair.second;
            if (std::isnan(value)) continue;

            float min = target - margin * 2;
            float max = target + margin * 2;

            if (value >= min && value <= max) continue;
            if (isOnCooldown(cfg->type)) continue;

            const char* severity = (value < target - margin * 3 || value > target + margin * 3)
                ? "critical" : "warning";

            const char* direction = (value > max) ? "high" : "low";

            char title[48];
            snprintf(title, sizeof(title), "%s too %s", cfg->type, direction);
            title[0] = toupper(title[0]);

            char desc[128];
            if (strcmp(cfg->type, "vpd") == 0) {
                snprintf(desc, sizeof(desc), "%.2f (target %.2f)", value, target);
            } else if (strcmp(cfg->type, "temperature") == 0) {
                snprintf(desc, sizeof(desc), "%.1f\xC2\xB0""C (target %.1f\xC2\xB0""C)", value, target);
            } else if (strcmp(cfg->type, "humidity") == 0) {
                snprintf(desc, sizeof(desc), "%.0f%% (target %.0f%%)", value, target);
            } else if (strcmp(cfg->type, "co2") == 0) {
                snprintf(desc, sizeof(desc), "%.0f ppm (target %.0f ppm)", value, target);
            } else {
                snprintf(desc, sizeof(desc), "%.1f (target %.1f)", value, target);
            }

            pushEvent("alert", title, desc, severity);
            setCooldown(cfg->type);
        }
    }
}

void init() {
    loadEvents();
    Serial.println("[EventLog] Initialized");
}

void loop(const std::map<String, float>& sensorReadings) {
    unsigned long now = millis();

    if (now - lastEval >= EVAL_INTERVAL) {
        lastEval = now;
        evaluateAlerts(sensorReadings);
    }

    if (now - lastPersist >= PERSIST_INTERVAL) {
        lastPersist = now;
        if (dirty) {
            saveEvents();
            dirty = false;
        }
    }
}

void pushEvent(const char* type, const char* title, const char* description,
               const char* severity) {
    uint32_t ts = (uint32_t)time(nullptr);

    Event e = {};
    snprintf(e.id, sizeof(e.id), "ev_%lu_%lu", ts, ++eventIdCounter);
    strlcpy(e.type, type, sizeof(e.type));
    strlcpy(e.title, title, sizeof(e.title));
    strlcpy(e.description, description, sizeof(e.description));
    strlcpy(e.severity, severity, sizeof(e.severity));
    e.timestamp = ts;

    addEvent(e);
    broadcastEvent(e);

    Serial.printf("[EventLog] %s: %s — %s\n", type, title, description);
}

void getEventsJson(String& out) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    size_t start = (eventCount >= MAX_EVENTS) ? eventHead : 0;
    for (size_t i = 0; i < eventCount; i++) {
        size_t idx = (start + i) % MAX_EVENTS;
        const Event& e = events[idx];
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = e.id;
        obj["type"] = e.type;
        obj["title"] = e.title;
        obj["description"] = e.description;
        obj["severity"] = e.severity;
        obj["timestamp"] = e.timestamp;
    }

    serializeJson(doc, out);
}

}
