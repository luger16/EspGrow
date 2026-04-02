#include "climate_config.h"
#include "storage.h"
#include <ArduinoJson.h>

namespace ClimateConfig {

namespace {
    const char* CONFIG_PATH = "/climate.json";

    // Phase names — order matters for iteration
    const char* PHASE_NAMES[] = {"seedling", "veg", "flower", "dry"};
    static constexpr size_t PHASE_COUNT = 4;

    char activePhase[16] = "veg";
    char phaseStartDate[32] = "";

    // Default targets per phase (same as web presets)
    const PhaseTargets DEFAULTS[] = {
        // seedling
        { 25, 21, 70, 75, 0.6f, 0.5f, 500, 500, 15 },
        // veg
        { 25, 20, 60, 65, 1.0f, 0.8f, 1000, 1000, 30 },
        // flower
        { 23, 18, 48, 52, 1.2f, 1.0f, 1200, 1200, 40 },
        // dry
        { 20, 18, 50, 55, 1.2f, 1.0f, 500, 500, 0 },
    };

    PhaseTargets phases[PHASE_COUNT];

    int phaseIndex(const char* phase) {
        for (size_t i = 0; i < PHASE_COUNT; i++) {
            if (strcmp(PHASE_NAMES[i], phase) == 0) return (int)i;
        }
        return -1;
    }

    void loadDefaults() {
        for (size_t i = 0; i < PHASE_COUNT; i++) {
            phases[i] = DEFAULTS[i];
        }
    }

    void loadConfig() {
        loadDefaults();

        JsonDocument doc;
        if (!Storage::readJson(CONFIG_PATH, doc)) {
            Serial.println("[Climate] No config file, using defaults");
            return;
        }

        if (doc["activePhase"].is<const char*>()) {
            strlcpy(activePhase, doc["activePhase"], sizeof(activePhase));
        }
        if (doc["phaseStartDate"].is<const char*>()) {
            strlcpy(phaseStartDate, doc["phaseStartDate"], sizeof(phaseStartDate));
        }

        if (doc["phases"].is<JsonObject>()) {
            JsonObject phasesObj = doc["phases"].as<JsonObject>();
            for (size_t i = 0; i < PHASE_COUNT; i++) {
                if (!phasesObj[PHASE_NAMES[i]].is<JsonObject>()) continue;
                JsonObject p = phasesObj[PHASE_NAMES[i]].as<JsonObject>();
                PhaseTargets& t = phases[i];

                if (p["temp"].is<JsonObject>()) {
                    t.tempDay = p["temp"]["day"] | DEFAULTS[i].tempDay;
                    t.tempNight = p["temp"]["night"] | DEFAULTS[i].tempNight;
                }
                if (p["humidity"].is<JsonObject>()) {
                    t.humidityDay = p["humidity"]["day"] | DEFAULTS[i].humidityDay;
                    t.humidityNight = p["humidity"]["night"] | DEFAULTS[i].humidityNight;
                }
                if (p["vpd"].is<JsonObject>()) {
                    t.vpdDay = p["vpd"]["day"] | DEFAULTS[i].vpdDay;
                    t.vpdNight = p["vpd"]["night"] | DEFAULTS[i].vpdNight;
                }
                if (p["co2"].is<JsonObject>()) {
                    t.co2Day = p["co2"]["day"] | DEFAULTS[i].co2Day;
                    t.co2Night = p["co2"]["night"] | DEFAULTS[i].co2Night;
                }
                t.dli = p["dli"] | DEFAULTS[i].dli;
            }
        }

        Serial.printf("[Climate] Loaded config: phase=%s\n", activePhase);
    }

    void saveConfig() {
        JsonDocument doc;
        doc["activePhase"] = activePhase;
        if (strlen(phaseStartDate) > 0) {
            doc["phaseStartDate"] = phaseStartDate;
        }

        JsonObject phasesObj = doc["phases"].to<JsonObject>();
        for (size_t i = 0; i < PHASE_COUNT; i++) {
            JsonObject p = phasesObj[PHASE_NAMES[i]].to<JsonObject>();
            const PhaseTargets& t = phases[i];

            JsonObject temp = p["temp"].to<JsonObject>();
            temp["day"] = t.tempDay;
            temp["night"] = t.tempNight;

            JsonObject hum = p["humidity"].to<JsonObject>();
            hum["day"] = t.humidityDay;
            hum["night"] = t.humidityNight;

            JsonObject vpd = p["vpd"].to<JsonObject>();
            vpd["day"] = t.vpdDay;
            vpd["night"] = t.vpdNight;

            JsonObject co2 = p["co2"].to<JsonObject>();
            co2["day"] = t.co2Day;
            co2["night"] = t.co2Night;

            p["dli"] = t.dli;
        }

        Storage::writeJson(CONFIG_PATH, doc);
        Serial.printf("[Climate] Saved config: phase=%s\n", activePhase);
    }
}

void init() {
    loadConfig();
    Serial.println("[Climate] Initialized");
}

const char* getActivePhase() {
    return activePhase;
}

const PhaseTargets& getTargets() {
    int idx = phaseIndex(activePhase);
    return phases[idx >= 0 ? idx : 1]; // default to veg
}

const PhaseTargets& getTargetsForPhase(const char* phase) {
    int idx = phaseIndex(phase);
    return phases[idx >= 0 ? idx : 1];
}

void getConfigJson(String& out) {
    JsonDocument doc;
    doc["activePhase"] = activePhase;
    if (strlen(phaseStartDate) > 0) {
        doc["phaseStartDate"] = phaseStartDate;
    }

    JsonObject phasesObj = doc["phases"].to<JsonObject>();
    for (size_t i = 0; i < PHASE_COUNT; i++) {
        JsonObject p = phasesObj[PHASE_NAMES[i]].to<JsonObject>();
        const PhaseTargets& t = phases[i];

        JsonObject temp = p["temp"].to<JsonObject>();
        temp["day"] = t.tempDay;
        temp["night"] = t.tempNight;

        JsonObject hum = p["humidity"].to<JsonObject>();
        hum["day"] = t.humidityDay;
        hum["night"] = t.humidityNight;

        JsonObject vpd = p["vpd"].to<JsonObject>();
        vpd["day"] = t.vpdDay;
        vpd["night"] = t.vpdNight;

        JsonObject co2 = p["co2"].to<JsonObject>();
        co2["day"] = t.co2Day;
        co2["night"] = t.co2Night;

        p["dli"] = t.dli;
    }

    // Include dayNightMode info — web needs this
    doc["dayNightMode"] = "auto";
    doc["lightThreshold"] = 50;

    serializeJson(doc, out);
}

bool setPhase(const char* phase, const char* startDate) {
    if (phaseIndex(phase) < 0) return false;
    strlcpy(activePhase, phase, sizeof(activePhase));
    if (startDate && strlen(startDate) > 0) {
        strlcpy(phaseStartDate, startDate, sizeof(phaseStartDate));
    } else {
        phaseStartDate[0] = '\0';
    }
    saveConfig();
    Serial.printf("[Climate] Phase set to %s\n", phase);
    return true;
}

bool setTargets(const char* phase, JsonObject& targets) {
    int idx = phaseIndex(phase);
    if (idx < 0) return false;

    PhaseTargets& t = phases[idx];

    if (targets["temp"].is<JsonObject>()) {
        JsonObject temp = targets["temp"].as<JsonObject>();
        if (temp["day"].is<float>()) t.tempDay = temp["day"];
        if (temp["night"].is<float>()) t.tempNight = temp["night"];
    }
    if (targets["humidity"].is<JsonObject>()) {
        JsonObject hum = targets["humidity"].as<JsonObject>();
        if (hum["day"].is<float>()) t.humidityDay = hum["day"];
        if (hum["night"].is<float>()) t.humidityNight = hum["night"];
    }
    if (targets["vpd"].is<JsonObject>()) {
        JsonObject vpd = targets["vpd"].as<JsonObject>();
        if (vpd["day"].is<float>()) t.vpdDay = vpd["day"];
        if (vpd["night"].is<float>()) t.vpdNight = vpd["night"];
    }
    if (targets["co2"].is<JsonObject>()) {
        JsonObject co2 = targets["co2"].as<JsonObject>();
        if (co2["day"].is<float>()) t.co2Day = co2["day"];
        if (co2["night"].is<float>()) t.co2Night = co2["night"];
    }
    if (targets["dli"].is<float>()) t.dli = targets["dli"];

    saveConfig();
    Serial.printf("[Climate] Updated targets for phase %s\n", phase);
    return true;
}

bool resetTargets(const char* phase) {
    int idx = phaseIndex(phase);
    if (idx < 0) return false;
    phases[idx] = DEFAULTS[idx];
    saveConfig();
    Serial.printf("[Climate] Reset targets for phase %s\n", phase);
    return true;
}

}
