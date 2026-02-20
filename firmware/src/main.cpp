#include <Arduino.h>
#include "storage.h"
#include "settings.h"
#include "wifi_manager.h"
#include "websocket_server.h"
#include "device_controller.h"
#include "sensors.h"
#include "automation.h"
#include "devices.h"
#include "sensor_config.h"
#include "history.h"
#include "ota_manager.h"
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <map>

namespace {
    unsigned long lastBroadcast = 0;
    const unsigned long BROADCAST_INTERVAL = 5000;
    std::map<String, float> cachedSensorReadings;
    bool sensorReadingsDirty = false;

    void broadcastRules() {
        JsonDocument doc;
        doc["type"] = "rules";
        
        String rulesJson;
        Automation::getRulesJson(rulesJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, rulesJson);
        doc["data"] = dataDoc.as<JsonArray>();
        
        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }

    void broadcastDevices() {
        Devices::computeControlModes();
        
        JsonDocument doc;
        doc["type"] = "devices";
        
        String devicesJson;
        Devices::getDevicesJson(devicesJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, devicesJson);
        doc["data"] = dataDoc.as<JsonArray>();
        
        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }

    void broadcastSensors() {
        JsonDocument doc;
        doc["type"] = "sensor_config";
        
        String sensorsJson;
        SensorConfig::getSensorsJson(sensorsJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, sensorsJson);
        doc["data"] = dataDoc.as<JsonArray>();
        
        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }

    void sendHistory(const char* sensorId, const char* range) {
        History::Range r;
        if (strcmp(range, "12h") == 0) r = History::RANGE_12H;
        else if (strcmp(range, "24h") == 0) r = History::RANGE_24H;
        else if (strcmp(range, "7d") == 0) r = History::RANGE_7D;
        else return;
        
        uint8_t buffer[History::POINTS_7D * sizeof(History::HistoryPoint)];
        size_t size = History::getHistory(sensorId, r, buffer, sizeof(buffer));
        
        if (size > 0) {
            JsonDocument doc;
            doc["type"] = "history";
            doc["sensorId"] = sensorId;
            doc["range"] = range;
            doc["pointSize"] = sizeof(History::HistoryPoint);
            doc["count"] = size / sizeof(History::HistoryPoint);
            
            String base64Data;
            size_t encodedLen = (size + 2) / 3 * 4;
            base64Data.reserve(encodedLen + 1);
            
            const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            for (size_t i = 0; i < size; i += 3) {
                uint32_t n = ((uint32_t)buffer[i]) << 16;
                if (i + 1 < size) n |= ((uint32_t)buffer[i + 1]) << 8;
                if (i + 2 < size) n |= buffer[i + 2];
                
                base64Data += b64chars[(n >> 18) & 0x3F];
                base64Data += b64chars[(n >> 12) & 0x3F];
                base64Data += (i + 1 < size) ? b64chars[(n >> 6) & 0x3F] : '=';
                base64Data += (i + 2 < size) ? b64chars[n & 0x3F] : '=';
            }
            
            doc["data"] = base64Data;
            
            String out;
            serializeJson(doc, out);
            WebSocketServer::broadcast(out);
        }
    }

    void handleMessage(const String& message) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, message);
        if (err) {
            Serial.printf("[Main] JSON parse error: %s\n", err.c_str());
            return;
        }

        const char* type = doc["type"];
        if (!type) return;

        if (strcmp(type, "ping") == 0) {
            JsonDocument response;
            response["type"] = "pong";
            response["timestamp"] = millis();
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "device_control") == 0) {
            const char* method = doc["method"];
            const char* target = doc["target"];
            bool on = doc["on"];
            
            bool success = DeviceController::control(
                method ? method : "",
                target ? target : "",
                on
            );
            
            Devices::Device* device = Devices::findDeviceByTarget(
                method ? method : "", target ? target : "");
            
            bool overrideActive = false;
            unsigned long overrideRemaining = 0;
            if (success && device) {
                Devices::setDeviceState(device->id, on);
                if (Automation::isDeviceUsedByEnabledRule(device->id)) {
                    Automation::setManualOverride(device->id);
                    overrideActive = true;
                    overrideRemaining = Automation::getOverrideRemaining(device->id);
                }
            }
            
            JsonDocument response;
            response["type"] = "device_status";
            response["target"] = target;
            response["on"] = on;
            response["success"] = success;
            if (device) {
                response["deviceId"] = device->id;
            }
            if (overrideActive) {
                response["overrideActive"] = true;
                response["overrideRemainingMs"] = overrideRemaining;
            }
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "get_rules") == 0) {
            broadcastRules();
        }
        else if (strcmp(type, "add_rule") == 0) {
            JsonDocument ruleDoc;
            ruleDoc["id"] = doc["id"];
            ruleDoc["name"] = doc["name"];
            ruleDoc["enabled"] = doc["enabled"];
            ruleDoc["ruleType"] = doc["ruleType"];
            ruleDoc["sensorId"] = doc["sensorId"];
            ruleDoc["operator"] = doc["operator"];
            ruleDoc["threshold"] = doc["threshold"];
            ruleDoc["thresholdOff"] = doc["thresholdOff"];
            ruleDoc["useHysteresis"] = doc["useHysteresis"];
            ruleDoc["minRunTimeMs"] = doc["minRunTimeMs"];
            ruleDoc["onTime"] = doc["onTime"];
            ruleDoc["offTime"] = doc["offTime"];
            ruleDoc["deviceId"] = doc["deviceId"];
            ruleDoc["deviceMethod"] = doc["deviceMethod"];
            ruleDoc["deviceTarget"] = doc["deviceTarget"];
            ruleDoc["action"] = doc["action"];
            
            Automation::addRule(ruleDoc);
            broadcastRules();
            broadcastDevices();
        }
        else if (strcmp(type, "update_rule") == 0) {
            const char* ruleId = doc["id"];
            JsonDocument updates;
            if (doc["name"].is<const char*>()) updates["name"] = doc["name"];
            if (doc["enabled"].is<bool>()) updates["enabled"] = doc["enabled"];
            if (doc["ruleType"].is<const char*>()) updates["ruleType"] = doc["ruleType"];
            if (doc["sensorId"].is<const char*>()) updates["sensorId"] = doc["sensorId"];
            if (doc["operator"].is<const char*>()) updates["operator"] = doc["operator"];
            if (doc["threshold"].is<float>()) updates["threshold"] = doc["threshold"];
            if (doc["thresholdOff"].is<float>()) updates["thresholdOff"] = doc["thresholdOff"];
            if (doc["useHysteresis"].is<bool>()) updates["useHysteresis"] = doc["useHysteresis"];
            if (doc["minRunTimeMs"].is<unsigned long>()) updates["minRunTimeMs"] = doc["minRunTimeMs"];
            if (doc["onTime"].is<const char*>()) updates["onTime"] = doc["onTime"];
            if (doc["offTime"].is<const char*>()) updates["offTime"] = doc["offTime"];
            if (doc["deviceId"].is<const char*>()) updates["deviceId"] = doc["deviceId"];
            if (doc["deviceMethod"].is<const char*>()) updates["deviceMethod"] = doc["deviceMethod"];
            if (doc["deviceTarget"].is<const char*>()) updates["deviceTarget"] = doc["deviceTarget"];
            if (doc["action"].is<const char*>()) updates["action"] = doc["action"];
            
            Automation::updateRule(ruleId, updates);
            broadcastRules();
            broadcastDevices();
        }
        else if (strcmp(type, "remove_rule") == 0) {
            const char* ruleId = doc["id"];
            Automation::removeRule(ruleId);
            broadcastRules();
            broadcastDevices();
        }
        else if (strcmp(type, "toggle_rule") == 0) {
            const char* ruleId = doc["id"];
            Automation::toggleRule(ruleId);
            broadcastRules();
            broadcastDevices();
        }
        else if (strcmp(type, "get_devices") == 0) {
            broadcastDevices();
        }
        else if (strcmp(type, "add_device") == 0) {
            JsonDocument deviceDoc;
            deviceDoc["id"] = doc["id"];
            deviceDoc["name"] = doc["name"];
            deviceDoc["type"] = doc["deviceType"];
            deviceDoc["controlMethod"] = doc["controlMethod"];
            deviceDoc["ipAddress"] = doc["ipAddress"];
            deviceDoc["controlMode"] = doc["controlMode"];
            
            Devices::addDevice(deviceDoc);
            broadcastDevices();
        }
        else if (strcmp(type, "update_device") == 0) {
            const char* deviceId = doc["id"];
            JsonDocument updates;
            if (doc["name"].is<const char*>()) updates["name"] = doc["name"];
            if (doc["deviceType"].is<const char*>()) updates["type"] = doc["deviceType"];
            if (doc["controlMethod"].is<const char*>()) updates["controlMethod"] = doc["controlMethod"];
            if (doc["ipAddress"].is<const char*>()) updates["ipAddress"] = doc["ipAddress"];
            
            Devices::updateDevice(deviceId, updates);
            broadcastDevices();
        }
        else if (strcmp(type, "remove_device") == 0) {
            const char* deviceId = doc["id"];
            Automation::removeRulesForDevice(deviceId);
            Devices::removeDevice(deviceId);
            broadcastDevices();
            broadcastRules();
        }
        else if (strcmp(type, "get_sensors") == 0) {
            broadcastSensors();
        }
        else if (strcmp(type, "add_sensor") == 0) {
            JsonDocument sensorDoc;
            sensorDoc["id"] = doc["id"];
            sensorDoc["name"] = doc["name"];
            sensorDoc["type"] = doc["sensorType"];
            sensorDoc["unit"] = doc["unit"];
            sensorDoc["hardwareType"] = doc["hardwareType"];
            sensorDoc["address"] = doc["address"];
            sensorDoc["tempSourceId"] = doc["tempSourceId"];
            sensorDoc["humSourceId"] = doc["humSourceId"];
            
            SensorConfig::addSensor(sensorDoc);
            broadcastSensors();
        }
        else if (strcmp(type, "update_sensor") == 0) {
            const char* sensorId = doc["id"];
            JsonDocument updates;
            if (doc["name"].is<const char*>()) updates["name"] = doc["name"];
            if (doc["sensorType"].is<const char*>()) updates["type"] = doc["sensorType"];
            if (doc["unit"].is<const char*>()) updates["unit"] = doc["unit"];
            if (doc["hardwareType"].is<const char*>()) updates["hardwareType"] = doc["hardwareType"];
            if (doc["address"].is<const char*>()) updates["address"] = doc["address"];
            if (doc["tempSourceId"].is<const char*>()) updates["tempSourceId"] = doc["tempSourceId"];
            if (doc["humSourceId"].is<const char*>()) updates["humSourceId"] = doc["humSourceId"];
            
            SensorConfig::updateSensor(sensorId, updates);
            broadcastSensors();
        }
        else if (strcmp(type, "remove_sensor") == 0) {
            const char* sensorId = doc["id"];
            SensorConfig::removeSensor(sensorId);
            broadcastSensors();
        }
        else if (strcmp(type, "get_history") == 0) {
            const char* sensorId = doc["sensorId"];
            const char* range = doc["range"];
            if (sensorId && range) {
                sendHistory(sensorId, range);
            }
        }
        else if (strcmp(type, "get_ppfd_calibration") == 0) {
            JsonDocument response;
            response["type"] = "ppfd_calibration";
            response["factor"] = Sensors::getPpfdCalibrationFactor();
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "calibrate_ppfd") == 0) {
            float knownPpfd = doc["knownPpfd"] | 0.0f;
            float rawPpfd = Sensors::getRawPpfd();
            
            if (knownPpfd > 0 && !isnan(rawPpfd) && rawPpfd > 0) {
                float factor = knownPpfd / rawPpfd;
                Sensors::setPpfdCalibrationFactor(factor);
                
                JsonDocument response;
                response["type"] = "ppfd_calibration";
                response["factor"] = factor;
                response["success"] = true;
                String out;
                serializeJson(response, out);
                WebSocketServer::broadcast(out);
            } else {
                JsonDocument response;
                response["type"] = "ppfd_calibration";
                response["success"] = false;
                response["error"] = isnan(rawPpfd) || rawPpfd <= 0 
                    ? "no_reading" : "invalid_value";
                String out;
                serializeJson(response, out);
                WebSocketServer::broadcast(out);
            }
        }
        else if (strcmp(type, "reset_ppfd_calibration") == 0) {
            Sensors::setPpfdCalibrationFactor(1.0f);
            
            JsonDocument response;
            response["type"] = "ppfd_calibration";
            response["factor"] = 1.0f;
            response["success"] = true;
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "clear_override") == 0) {
            const char* deviceId = doc["deviceId"];
            if (deviceId) {
                Automation::clearOverride(deviceId);
                Automation::forceEvaluation(cachedSensorReadings);
                
                JsonDocument response;
                response["type"] = "override_cleared";
                response["deviceId"] = deviceId;
                String out;
                serializeJson(response, out);
                WebSocketServer::broadcast(out);
            }
        }
        else if (strcmp(type, "get_system_info") == 0) {
            JsonDocument response;
            response["type"] = "system_info";
            response["uptime"] = millis() / 1000;
            response["freeHeap"] = ESP.getFreeHeap();
            response["chipModel"] = ESP.getChipModel();
            response["wifiRssi"] = WiFi.RSSI();
            response["ipAddress"] = WiFiManager::getIP();
            response["firmwareVersion"] = FIRMWARE_VERSION;
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "get_settings") == 0) {
            JsonDocument response;
            response["type"] = "settings";
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
    }

    void broadcastSensorData() {
        if (!WebSocketServer::hasClients()) return;

        Sensors::read();

        size_t sensorCount;
        const char** sensorIds = SensorConfig::getSensorIds(sensorCount);
        
        bool anyValid = false;
        
        JsonDocument doc;
        doc["type"] = "sensors";
        JsonArray data = doc["data"].to<JsonArray>();
        
        for (size_t i = 0; i < sensorCount; i++) {
            float value = Sensors::getSensorValue(sensorIds[i]);
            
            if (!isnan(value)) {
                anyValid = true;
                
                JsonObject entry = data.add<JsonObject>();
                entry["id"] = sensorIds[i];
                SensorConfig::Sensor* cfg = SensorConfig::getSensor(sensorIds[i]);
                if (cfg) entry["type"] = cfg->type;
                entry["value"] = value;
                
                History::record(sensorIds[i], value);
                cachedSensorReadings[String(sensorIds[i])] = value;
            }
        }
        
        if (!anyValid) return;
        
        sensorReadingsDirty = true;
        doc["timestamp"] = millis();

        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }
}

void setup() {
    Serial.begin(115200);
    
    while (!Serial && millis() < 5000) {
        delay(10);
    }
    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("       EspGrow Starting...       ");
    Serial.println("=================================");
    Serial.printf("Chip: %s\n", ESP.getChipModel());
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    
    if (!Storage::init()) {
        Serial.println("[ERROR] Storage init failed!");
    }
    
    Settings::init();
    WiFiManager::init();
    DeviceController::init();
    Sensors::init();
    Devices::init();
    SensorConfig::init();
    History::init();
    Automation::init();
    Automation::setDeviceStateCallback([](const char* deviceId, const char* method, const char* target, bool on) {
        JsonDocument response;
        response["type"] = "device_status";
        response["deviceId"] = deviceId;
        response["target"] = target;
        response["on"] = on;
        response["success"] = true;
        String out;
        serializeJson(response, out);
        WebSocketServer::broadcast(out);
    });
    
    WebSocketServer::onMessage(handleMessage);
    
    OtaManager::validateRollback();
}

void loop() {
    static bool wsInitialized = false;
    
    WiFiManager::loop();
    
    if (WiFiManager::isConnected()) {
        if (!wsInitialized) {
            if (MDNS.begin("espgrow")) {
                Serial.println("[mDNS] Started: espgrow.local");
                MDNS.addService("http", "tcp", 80);
            }
            
            WebSocketServer::init();
            wsInitialized = true;
        }
        
        WebSocketServer::loop();
        History::loop();
        
        if (millis() - lastBroadcast >= BROADCAST_INTERVAL) {
            lastBroadcast = millis();
            broadcastSensorData();
        }
        
        if (sensorReadingsDirty) {
            sensorReadingsDirty = false;
            Automation::clearExpiredOverrides();
            Automation::loop(cachedSensorReadings);
        }
    }
}
