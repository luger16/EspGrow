#pragma once

#include <Arduino.h>
#include <functional>

class AsyncWebServer;

namespace WebSocketServer {

using MessageCallback = std::function<void(uint32_t clientId, const String& message)>;

AsyncWebServer* getServer(uint16_t port = 80);

void init();
void loop();
void broadcast(const String& message);
void sendTo(uint32_t clientId, const String& message);
void onMessage(MessageCallback callback);
bool hasClients();

size_t getDeferredCount();

}
