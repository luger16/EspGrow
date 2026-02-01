#pragma once

#include <Arduino.h>
#include <functional>

namespace WebSocketServer {

using MessageCallback = std::function<void(const String& message)>;

void init(uint16_t port = 80);
void loop();
void broadcast(const String& message);
void onMessage(MessageCallback callback);
bool hasClients();

}
