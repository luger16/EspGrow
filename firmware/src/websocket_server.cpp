#include "websocket_server.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

namespace WebSocketServer {

namespace {
    AsyncWebServer* server = nullptr;
    AsyncWebSocket* ws = nullptr;
    MessageCallback messageCallback;
    bool initialized = false;

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
                    String message;
                    message.reserve(len + 1);
                    for (size_t i = 0; i < len; i++) {
                        message += (char)data[i];
                    }
                    Serial.printf("[WS] Received: %s\n", message.c_str());
                    if (messageCallback) {
                        messageCallback(message);
                    }
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
        server->begin();
        Serial.printf("[Server] Started on port %d\n", port);
    }
    return server;
}

void init() {
    if (initialized) return;
    
    if (!server) {
        server = new AsyncWebServer(80);
        server->begin();
        Serial.println("[Server] Started on port 80");
    }
    
    ws = new AsyncWebSocket("/ws");
    ws->onEvent(onWsEvent);
    server->addHandler(ws);
    
    server->serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("max-age=86400");
    
    server->onNotFound([](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    
    initialized = true;
    Serial.println("[WS] WebSocket and static routes configured");
}

void loop() {
    if (ws) {
        ws->cleanupClients();
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
