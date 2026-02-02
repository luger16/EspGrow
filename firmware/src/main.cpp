#include <Arduino.h>
#include "storage.h"
#include "wifi_manager.h"
#include "websocket_server.h"
#include "device_controller.h"
#include "sensors.h"
#include "automation.h"
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
            if (doc.containsKey("name")) updates["name"] = doc["name"];
            if (doc.containsKey("enabled")) updates["enabled"] = doc["enabled"];
            if (doc.containsKey("sensorId")) updates["sensorId"] = doc["sensorId"];
            if (doc.containsKey("operator")) updates["operator"] = doc["operator"];
            if (doc.containsKey("threshold")) updates["threshold"] = doc["threshold"];
            if (doc.containsKey("deviceId")) updates["deviceId"] = doc["deviceId"];
            if (doc.containsKey("deviceMethod")) updates["deviceMethod"] = doc["deviceMethod"];
            if (doc.containsKey("deviceTarget")) updates["deviceTarget"] = doc["deviceTarget"];
            if (doc.containsKey("action")) updates["action"] = doc["action"];
            
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
    }

    void broadcastSensorData() {
        if (!WebSocketServer::hasClients()) return;

        Sensors::SensorData sensor = Sensors::read();
        if (!sensor.valid) return;
        
        lastSensorData = sensor;

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
        
        if (millis() - lastBroadcast >= BROADCAST_INTERVAL) {
            lastBroadcast = millis();
            broadcastSensorData();
        }
        
        if (lastSensorData.valid) {
            Automation::loop(lastSensorData.temperature, lastSensorData.humidity, lastSensorData.co2);
        }
    }
}
