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
#include <ArduinoJson.h>
#include <ESPmDNS.h>

namespace {
    unsigned long lastBroadcast = 0;
    const unsigned long BROADCAST_INTERVAL = 5000;
    Sensors::SensorData lastSensorData;

    void broadcastRules() {
        JsonDocument doc;
        doc["type"] = "rules";
        
        String rulesJson;
        Automation::getRulesJson(rulesJson);
        doc["data"] = serialized(rulesJson);
        
        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }

    void broadcastDevices() {
        JsonDocument doc;
        doc["type"] = "devices";
        
        String devicesJson;
        Devices::getDevicesJson(devicesJson);
        doc["data"] = serialized(devicesJson);
        
        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }

    void broadcastSensors() {
        JsonDocument doc;
        doc["type"] = "sensor_config";
        
        String sensorsJson;
        SensorConfig::getSensorsJson(sensorsJson);
        doc["data"] = serialized(sensorsJson);
        
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
            
            JsonDocument response;
            response["type"] = "device_status";
            response["target"] = target;
            response["on"] = on;
            response["success"] = success;
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
            ruleDoc["sensorId"] = doc["sensorId"];
            ruleDoc["operator"] = doc["operator"];
            ruleDoc["threshold"] = doc["threshold"];
            ruleDoc["deviceId"] = doc["deviceId"];
            ruleDoc["deviceMethod"] = doc["deviceMethod"];
            ruleDoc["deviceTarget"] = doc["deviceTarget"];
            ruleDoc["action"] = doc["action"];
            
            Automation::addRule(ruleDoc);
            broadcastRules();
        }
        else if (strcmp(type, "update_rule") == 0) {
            const char* ruleId = doc["id"];
            JsonDocument updates;
            if (doc["name"].is<const char*>()) updates["name"] = doc["name"];
            if (doc["enabled"].is<bool>()) updates["enabled"] = doc["enabled"];
            if (doc["sensorId"].is<const char*>()) updates["sensorId"] = doc["sensorId"];
            if (doc["operator"].is<const char*>()) updates["operator"] = doc["operator"];
            if (doc["threshold"].is<float>()) updates["threshold"] = doc["threshold"];
            if (doc["deviceId"].is<const char*>()) updates["deviceId"] = doc["deviceId"];
            if (doc["deviceMethod"].is<const char*>()) updates["deviceMethod"] = doc["deviceMethod"];
            if (doc["deviceTarget"].is<const char*>()) updates["deviceTarget"] = doc["deviceTarget"];
            if (doc["action"].is<const char*>()) updates["action"] = doc["action"];
            
            Automation::updateRule(ruleId, updates);
            broadcastRules();
        }
        else if (strcmp(type, "remove_rule") == 0) {
            const char* ruleId = doc["id"];
            Automation::removeRule(ruleId);
            broadcastRules();
        }
        else if (strcmp(type, "toggle_rule") == 0) {
            const char* ruleId = doc["id"];
            Automation::toggleRule(ruleId);
            broadcastRules();
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
            deviceDoc["gpioPin"] = doc["gpioPin"];
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
            if (doc["gpioPin"].is<int>()) updates["gpioPin"] = doc["gpioPin"];
            if (doc["ipAddress"].is<const char*>()) updates["ipAddress"] = doc["ipAddress"];
            if (doc["controlMode"].is<const char*>()) updates["controlMode"] = doc["controlMode"];
            
            Devices::updateDevice(deviceId, updates);
            broadcastDevices();
        }
        else if (strcmp(type, "remove_device") == 0) {
            const char* deviceId = doc["id"];
            Devices::removeDevice(deviceId);
            broadcastDevices();
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
    }

    void broadcastSensorData() {
        if (!WebSocketServer::hasClients()) return;

        Sensors::SensorData sensor = Sensors::read();
        if (!sensor.valid) return;
        
        lastSensorData = sensor;

        History::record("temperature", sensor.temperature);
        History::record("humidity", sensor.humidity);
        History::record("co2", sensor.co2);
        History::record("vpd", sensor.vpd);

        JsonDocument doc;
        doc["type"] = "sensors";
        
        JsonObject data = doc["data"].to<JsonObject>();
        data["temperature"] = sensor.temperature;
        data["humidity"] = sensor.humidity;
        data["co2"] = sensor.co2;
        data["vpd"] = sensor.vpd;
        
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
    
    WiFiManager::init();
    DeviceController::init();
    Sensors::init();
    Devices::init();
    SensorConfig::init();
    History::init();
    Automation::init();
    
    WebSocketServer::onMessage(handleMessage);
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
        
        if (lastSensorData.valid) {
            Automation::loop(lastSensorData.temperature, lastSensorData.humidity, lastSensorData.co2);
        }
    }
}
