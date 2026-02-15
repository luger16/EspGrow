#include "websocket_server.h"
#include "ota_manager.h"
#include "storage.h"
#include "devices.h"
#include "automation.h"
#include "sensor_config.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

namespace WebSocketServer {

namespace {
    AsyncWebServer* server = nullptr;
    AsyncWebSocket* ws = nullptr;
    MessageCallback messageCallback;
    bool initialized = false;

    static constexpr size_t MSG_QUEUE_SIZE = 8;
    static constexpr size_t MSG_MAX_LEN = 512;
    
    struct MessageSlot {
        char data[MSG_MAX_LEN];
        size_t len = 0;
        bool used = false;
    };
    
    volatile size_t queueHead = 0;
    volatile size_t queueTail = 0;
    MessageSlot messageQueue[MSG_QUEUE_SIZE];

    void onWsEvent(AsyncWebSocket* wsServer, AsyncWebSocketClient* client, 
                   AwsEventType type, void* arg, uint8_t* data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                Serial.printf("[WS] Client #%u connected from %s\n", 
                    client->id(), client->remoteIP().toString().c_str());
                break;
            case WS_EVT_DISCONNECT:
                Serial.printf("[WS] Client #%u disconnected\n", client->id());
                break;
            case WS_EVT_DATA: {
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                    size_t next = (queueHead + 1) % MSG_QUEUE_SIZE;
                    if (next == queueTail) {
                        Serial.println("[WS] Message queue full, dropping");
                        break;
                    }
                    size_t copyLen = (len < MSG_MAX_LEN - 1) ? len : MSG_MAX_LEN - 1;
                    memcpy(messageQueue[queueHead].data, data, copyLen);
                    messageQueue[queueHead].data[copyLen] = '\0';
                    messageQueue[queueHead].len = copyLen;
                    queueHead = next;
                }
                break;
            }
            case WS_EVT_ERROR:
                Serial.printf("[WS] Error #%u: %s\n", client->id(), (char*)arg);
                break;
            case WS_EVT_PONG:
                break;
        }
    }
}

AsyncWebServer* getServer(uint16_t port) {
    if (!server) {
        server = new AsyncWebServer(port);
    }
    return server;
}

void init() {
    if (initialized) return;
    
    if (!server) {
        server = new AsyncWebServer(80);
    }
    
    ws = new AsyncWebSocket("/ws");
    ws->onEvent(onWsEvent);
    server->addHandler(ws);
    
    // API: backup all config files as a single JSON bundle
    server->on("/api/config/backup", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        JsonDocument sub;

        const char* files[] = {"/devices.json", "/rules.json", "/sensors.json"};
        const char* keys[] = {"devices", "rules", "sensors"};
        
        for (int i = 0; i < 3; i++) {
            sub.clear();
            if (Storage::readJson(files[i], sub)) {
                doc[keys[i]] = sub.as<JsonArray>();
            } else {
                doc[keys[i]] = JsonArray();
            }
        }

        String output;
        serializeJsonPretty(doc, output);
        
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", output);
        response->addHeader("Content-Disposition", "attachment; filename=\"espgrow-backup.json\"");
        request->send(response);
    });

    // API: restore config from backup (replaces existing)
    AsyncCallbackJsonWebHandler* restoreHandler = new AsyncCallbackJsonWebHandler("/api/config/restore");
    restoreHandler->setMethod(HTTP_POST);
    restoreHandler->setMaxContentLength(32768);
    
    restoreHandler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject obj = json.as<JsonObject>();
        
        if (!obj["devices"].is<JsonArray>() || !obj["rules"].is<JsonArray>() || !obj["sensors"].is<JsonArray>()) {
            Serial.println("[API] Restore: missing or invalid required keys");
            request->send(400, "application/json", "{\"error\":\"Missing or invalid required keys\"}");
            return;
        }

        bool success = true;
        JsonDocument sub;

        sub.clear();
        sub.set(obj["devices"]);
        success &= Storage::writeJson("/devices.json", sub);

        sub.clear();
        sub.set(obj["rules"]);
        success &= Storage::writeJson("/rules.json", sub);

        sub.clear();
        sub.set(obj["sensors"]);
        success &= Storage::writeJson("/sensors.json", sub);

        if (success) {
            Serial.println("[API] Restore: success, reloading modules");
            
            Devices::init();
            Automation::init();
            SensorConfig::init();
            Devices::computeControlModes();
            
            JsonDocument broadcastDoc;
            JsonDocument dataDoc;
            String jsonStr;
            String output;
            
            broadcastDoc["type"] = "devices";
            Devices::getDevicesJson(jsonStr);
            deserializeJson(dataDoc, jsonStr);
            broadcastDoc["data"] = dataDoc.as<JsonArray>();
            serializeJson(broadcastDoc, output);
            broadcast(output);
            
            broadcastDoc.clear();
            dataDoc.clear();
            jsonStr.clear();
            output.clear();
            
            broadcastDoc["type"] = "rules";
            Automation::getRulesJson(jsonStr);
            deserializeJson(dataDoc, jsonStr);
            broadcastDoc["data"] = dataDoc.as<JsonArray>();
            serializeJson(broadcastDoc, output);
            broadcast(output);
            
            broadcastDoc.clear();
            dataDoc.clear();
            jsonStr.clear();
            output.clear();
            
            broadcastDoc["type"] = "sensor_config";
            SensorConfig::getSensorsJson(jsonStr);
            deserializeJson(dataDoc, jsonStr);
            broadcastDoc["data"] = dataDoc.as<JsonArray>();
            serializeJson(broadcastDoc, output);
            broadcast(output);
            
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            Serial.println("[API] Restore: write failed");
            request->send(500, "application/json", "{\"error\":\"Write failed\"}");
        }
    });
    
    server->addHandler(restoreHandler);
    
    OtaManager::begin(server, [](const OtaManager::StatusEvent& event) {
        JsonDocument doc;
        doc["type"] = "ota_status";
        
        switch (event.status) {
            case OtaManager::Status::Idle:       doc["status"] = "idle"; break;
            case OtaManager::Status::Uploading:   doc["status"] = "uploading"; break;
            case OtaManager::Status::Downloading: doc["status"] = "downloading"; break;
            case OtaManager::Status::Installing:  doc["status"] = "installing"; break;
            case OtaManager::Status::Success:     doc["status"] = "success"; break;
            case OtaManager::Status::Error:       doc["status"] = "error"; break;
            case OtaManager::Status::Rebooting:   doc["status"] = "rebooting"; break;
        }
        
        if (event.progress >= 0) doc["progress"] = event.progress;
        if (event.error.length() > 0) doc["error"] = event.error;
        
        String out;
        serializeJson(doc, out);
        broadcast(out);
    });
    
    server->serveStatic("/_app/immutable/", LittleFS, "/_app/immutable/")
        .setCacheControl("max-age=31536000, immutable");

    server->serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("no-cache");
    
    server->onNotFound([](AsyncWebServerRequest *request) {
        if (request->url().startsWith("/api/")) {
            request->send(404, "application/json", "{\"error\":\"Not found\"}");
            return;
        }
        request->send(LittleFS, "/index.html", "text/html");
    });
    
    server->begin();
    Serial.println("[Server] Started on port 80");
    
    initialized = true;
    Serial.println("[WS] WebSocket, API routes, and static files configured");
}

void loop() {
    if (ws) {
        ws->cleanupClients();
    }
    
    while (queueTail != queueHead) {
        String message(messageQueue[queueTail].data);
        queueTail = (queueTail + 1) % MSG_QUEUE_SIZE;
        
        if (messageCallback) {
            messageCallback(message);
        }
    }
}

void broadcast(const String& message) {
    if (ws && ws->count() > 0) {
        ws->textAll(message);
    }
}

void onMessage(MessageCallback callback) {
    messageCallback = callback;
}

bool hasClients() {
    return ws && ws->count() > 0;
}

}
