#include <Arduino.h>
#include "storage.h"
#include "wifi_manager.h"
#include "websocket_server.h"
#include "device_controller.h"
#include <ArduinoJson.h>

namespace {
    unsigned long lastBroadcast = 0;
    const unsigned long BROADCAST_INTERVAL = 5000;

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
    }

    void broadcastSensorData() {
        if (!WebSocketServer::hasClients()) return;

        JsonDocument doc;
        doc["type"] = "sensors";
        
        JsonObject data = doc["data"].to<JsonObject>();
        data["temperature"] = 24.5 + (random(-10, 10) / 10.0);
        data["humidity"] = 55.0 + (random(-50, 50) / 10.0);
        data["co2"] = 800 + random(-50, 50);
        data["vpd"] = 1.2 + (random(-2, 2) / 10.0);
        
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
    
    WebSocketServer::onMessage(handleMessage);
}

void loop() {
    WiFiManager::loop();
    
    if (WiFiManager::isConnected()) {
        static bool wsStarted = false;
        static unsigned long connectedAt = 0;
        
        if (!wsStarted && connectedAt == 0) {
            connectedAt = millis();
        }
        
        if (!wsStarted && millis() - connectedAt >= 500) {
            WebSocketServer::init(80);
            wsStarted = true;
        }
        
        WebSocketServer::loop();
        
        if (millis() - lastBroadcast >= BROADCAST_INTERVAL) {
            lastBroadcast = millis();
            broadcastSensorData();
        }
    }
}
