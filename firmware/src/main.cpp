#include <Arduino.h>
#include "storage.h"
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
            JsonObject histData = doc["data"].to<JsonObject>();
            histData["sensorId"] = sensorId;
            histData["range"] = range;
            histData["pointSize"] = sizeof(History::HistoryPoint);
            histData["count"] = size / sizeof(History::HistoryPoint);
            
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
            
            histData["payload"] = base64Data;
            
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

        // Extract payload from "data" key (new protocol), fallback to top-level (legacy)
        JsonObject payload = doc["data"].is<JsonObject>() ? doc["data"].as<JsonObject>() : doc.as<JsonObject>();

        if (strcmp(type, "ping") == 0) {
            JsonDocument response;
            response["type"] = "pong";
            response["data"]["timestamp"] = millis();
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "device_control") == 0) {
            const char* method = payload["method"];
            const char* target = payload["target"];
            bool on = payload["on"];
            
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
            JsonObject respData = response["data"].to<JsonObject>();
            respData["target"] = target;
            respData["on"] = on;
            respData["success"] = success;
            if (device) {
                respData["deviceId"] = device->id;
            }
            if (overrideActive) {
                respData["overrideActive"] = true;
                respData["overrideRemainingMs"] = overrideRemaining;
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
            ruleDoc["id"] = payload["id"];
            ruleDoc["name"] = payload["name"];
            ruleDoc["enabled"] = payload["enabled"];
            ruleDoc["ruleType"] = payload["ruleType"];
            ruleDoc["sensorId"] = payload["sensorId"];
            ruleDoc["operator"] = payload["operator"];
            ruleDoc["threshold"] = payload["threshold"];
            ruleDoc["thresholdOff"] = payload["thresholdOff"];
            ruleDoc["useHysteresis"] = payload["useHysteresis"];
            ruleDoc["minRunTimeMs"] = payload["minRunTimeMs"];
            ruleDoc["onTime"] = payload["onTime"];
            ruleDoc["offTime"] = payload["offTime"];
            ruleDoc["deviceId"] = payload["deviceId"];
            ruleDoc["deviceMethod"] = payload["deviceMethod"];
            ruleDoc["deviceTarget"] = payload["deviceTarget"];
            ruleDoc["action"] = payload["action"];
            
            Automation::addRule(ruleDoc);
            broadcastRules();
            broadcastDevices();
        }
        else if (strcmp(type, "update_rule") == 0) {
            const char* ruleId = payload["id"];
            JsonDocument updates;
            if (payload["name"].is<const char*>()) updates["name"] = payload["name"];
            if (payload["enabled"].is<bool>()) updates["enabled"] = payload["enabled"];
            if (payload["ruleType"].is<const char*>()) updates["ruleType"] = payload["ruleType"];
            if (payload["sensorId"].is<const char*>()) updates["sensorId"] = payload["sensorId"];
            if (payload["operator"].is<const char*>()) updates["operator"] = payload["operator"];
            if (payload["threshold"].is<float>()) updates["threshold"] = payload["threshold"];
            if (payload["thresholdOff"].is<float>()) updates["thresholdOff"] = payload["thresholdOff"];
            if (payload["useHysteresis"].is<bool>()) updates["useHysteresis"] = payload["useHysteresis"];
            if (payload["minRunTimeMs"].is<unsigned long>()) updates["minRunTimeMs"] = payload["minRunTimeMs"];
            if (payload["onTime"].is<const char*>()) updates["onTime"] = payload["onTime"];
            if (payload["offTime"].is<const char*>()) updates["offTime"] = payload["offTime"];
            if (payload["deviceId"].is<const char*>()) updates["deviceId"] = payload["deviceId"];
            if (payload["deviceMethod"].is<const char*>()) updates["deviceMethod"] = payload["deviceMethod"];
            if (payload["deviceTarget"].is<const char*>()) updates["deviceTarget"] = payload["deviceTarget"];
            if (payload["action"].is<const char*>()) updates["action"] = payload["action"];
            
            Automation::updateRule(ruleId, updates);
            broadcastRules();
            broadcastDevices();
        }
        else if (strcmp(type, "remove_rule") == 0) {
            const char* ruleId = payload["id"];
            Automation::removeRule(ruleId);
            broadcastRules();
            broadcastDevices();
        }
        else if (strcmp(type, "toggle_rule") == 0) {
            const char* ruleId = payload["id"];
            Automation::toggleRule(ruleId);
            broadcastRules();
            broadcastDevices();
        }
        else if (strcmp(type, "get_devices") == 0) {
            broadcastDevices();
        }
        else if (strcmp(type, "add_device") == 0) {
            JsonDocument deviceDoc;
            deviceDoc["id"] = payload["id"];
            deviceDoc["name"] = payload["name"];
            deviceDoc["type"] = payload["deviceType"];
            deviceDoc["controlMethod"] = payload["controlMethod"];
            deviceDoc["ipAddress"] = payload["ipAddress"];
            deviceDoc["controlMode"] = payload["controlMode"];
            
            Devices::addDevice(deviceDoc);
            broadcastDevices();
        }
        else if (strcmp(type, "update_device") == 0) {
            const char* deviceId = payload["id"];
            JsonDocument updates;
            if (payload["name"].is<const char*>()) updates["name"] = payload["name"];
            if (payload["deviceType"].is<const char*>()) updates["type"] = payload["deviceType"];
            if (payload["controlMethod"].is<const char*>()) updates["controlMethod"] = payload["controlMethod"];
            if (payload["ipAddress"].is<const char*>()) updates["ipAddress"] = payload["ipAddress"];
            
            Devices::updateDevice(deviceId, updates);
            broadcastDevices();
        }
        else if (strcmp(type, "remove_device") == 0) {
            const char* deviceId = payload["id"];
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
            sensorDoc["id"] = payload["id"];
            sensorDoc["name"] = payload["name"];
            sensorDoc["type"] = payload["sensorType"];
            sensorDoc["unit"] = payload["unit"];
            sensorDoc["hardwareType"] = payload["hardwareType"];
            sensorDoc["address"] = payload["address"];
            sensorDoc["tempSourceId"] = payload["tempSourceId"];
            sensorDoc["humSourceId"] = payload["humSourceId"];
            
            SensorConfig::addSensor(sensorDoc);
            broadcastSensors();
        }
        else if (strcmp(type, "update_sensor") == 0) {
            const char* sensorId = payload["id"];
            JsonDocument updates;
            if (payload["name"].is<const char*>()) updates["name"] = payload["name"];
            if (payload["sensorType"].is<const char*>()) updates["type"] = payload["sensorType"];
            if (payload["unit"].is<const char*>()) updates["unit"] = payload["unit"];
            if (payload["hardwareType"].is<const char*>()) updates["hardwareType"] = payload["hardwareType"];
            if (payload["address"].is<const char*>()) updates["address"] = payload["address"];
            if (payload["tempSourceId"].is<const char*>()) updates["tempSourceId"] = payload["tempSourceId"];
            if (payload["humSourceId"].is<const char*>()) updates["humSourceId"] = payload["humSourceId"];
            
            SensorConfig::updateSensor(sensorId, updates);
            broadcastSensors();
        }
        else if (strcmp(type, "remove_sensor") == 0) {
            const char* sensorId = payload["id"];
            SensorConfig::removeSensor(sensorId);
            broadcastSensors();
        }
        else if (strcmp(type, "get_history") == 0) {
            const char* sensorId = payload["sensorId"];
            const char* range = payload["range"];
            if (sensorId && range) {
                sendHistory(sensorId, range);
            }
        }
        else if (strcmp(type, "get_ppfd_calibration") == 0) {
            JsonDocument response;
            response["type"] = "ppfd_calibration";
            response["data"]["factor"] = Sensors::getPpfdCalibrationFactor();
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "calibrate_ppfd") == 0) {
            float knownPpfd = payload["knownPpfd"] | 0.0f;
            float rawPpfd = Sensors::getRawPpfd();
            
            if (knownPpfd > 0 && !isnan(rawPpfd) && rawPpfd > 0) {
                float factor = knownPpfd / rawPpfd;
                Sensors::setPpfdCalibrationFactor(factor);
                
                JsonDocument response;
                response["type"] = "ppfd_calibration";
                JsonObject respData = response["data"].to<JsonObject>();
                respData["factor"] = factor;
                respData["success"] = true;
                String out;
                serializeJson(response, out);
                WebSocketServer::broadcast(out);
            } else {
                JsonDocument response;
                response["type"] = "ppfd_calibration";
                JsonObject respData = response["data"].to<JsonObject>();
                respData["success"] = false;
                respData["error"] = isnan(rawPpfd) || rawPpfd <= 0 
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
            JsonObject respData = response["data"].to<JsonObject>();
            respData["factor"] = 1.0f;
            respData["success"] = true;
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "clear_override") == 0) {
            const char* deviceId = payload["deviceId"];
            if (deviceId) {
                Automation::clearOverride(deviceId);
                Automation::forceEvaluation(cachedSensorReadings);
                
                JsonDocument response;
                response["type"] = "override_cleared";
                response["data"]["deviceId"] = deviceId;
                String out;
                serializeJson(response, out);
                WebSocketServer::broadcast(out);
            }
        }
        else if (strcmp(type, "get_system_info") == 0) {
            JsonDocument response;
            response["type"] = "system_info";
            JsonObject respData = response["data"].to<JsonObject>();
            respData["uptime"] = millis() / 1000;
            respData["freeHeap"] = ESP.getFreeHeap();
            respData["chipModel"] = ESP.getChipModel();
            respData["wifiRssi"] = WiFi.RSSI();
            respData["ipAddress"] = WiFiManager::getIP();
            respData["firmwareVersion"] = FIRMWARE_VERSION;
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
        JsonObject respData = response["data"].to<JsonObject>();
        respData["deviceId"] = deviceId;
        respData["target"] = target;
        respData["on"] = on;
        respData["success"] = true;
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
