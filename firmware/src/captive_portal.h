#pragma once

#include <Arduino.h>
#include <functional>

namespace CaptivePortal {

using SuccessCallback = std::function<void(const String& ssid, const String& password)>;

struct Config {
    const char* apName = "EspGrow-Setup";
    const char* apPassword = nullptr;
    uint32_t connectionTimeout = 15000;
};

void start(const Config& config, SuccessCallback onSuccess);
void stop();
void loop();
bool isActive();
bool isConnected();

}
