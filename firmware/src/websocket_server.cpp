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
    
    server->serveStatic("/_app/immutable/", LittleFS, "/_app/immutable/")
        .setCacheControl("max-age=31536000, immutable");

    server->serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("no-cache");
    
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
