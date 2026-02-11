#pragma once

#include <Arduino.h>
#include <functional>

class AsyncWebServer;

namespace WebSocketServer {

using MessageCallback = std::function<void(const String& message)>;

// Get or create the shared server instance
AsyncWebServer* getServer(uint16_t port = 80);

void init();  // Sets up WebSocket and static file serving on existing server
void loop();  // Drains message queue + cleans up clients
void broadcast(const String& message);
void onMessage(MessageCallback callback);
bool hasClients();

}
