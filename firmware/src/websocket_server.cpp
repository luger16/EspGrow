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

    void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
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

void init(uint16_t port) {
    server = new AsyncWebServer(port);
    ws = new AsyncWebSocket("/ws");
    
    ws->onEvent(onWsEvent);
    server->addHandler(ws);
    
    server->serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("max-age=86400");
    
    server->begin();
    Serial.printf("[WS] Server started on port %d\n", port);
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
