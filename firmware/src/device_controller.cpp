#include "device_controller.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <set>

namespace DeviceController {

namespace {
    std::set<uint8_t> initializedPins;
}

void init() {
    Serial.println("[DeviceCtrl] Initialized");
}

bool setTasmota(const String& ip, bool on) {
    HTTPClient http;
    String url = "http://" + ip + "/cm?cmnd=Power%20" + (on ? "On" : "Off");
    
    http.begin(url);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    http.end();
    
    if (httpCode == 200) {
        Serial.printf("[DeviceCtrl] Tasmota %s -> %s\n", ip.c_str(), on ? "ON" : "OFF");
        return true;
    }
    
    Serial.printf("[DeviceCtrl] Tasmota %s failed: %d\n", ip.c_str(), httpCode);
    return false;
}

bool setShelly(const String& ip, bool on) {
    HTTPClient http;
    String url = "http://" + ip + "/relay/0?turn=" + (on ? "on" : "off");
    
    http.begin(url);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    http.end();
    
    if (httpCode == 200) {
        Serial.printf("[DeviceCtrl] Shelly %s -> %s\n", ip.c_str(), on ? "ON" : "OFF");
        return true;
    }
    
    Serial.printf("[DeviceCtrl] Shelly %s failed: %d\n", ip.c_str(), httpCode);
    return false;
}

bool setRelay(uint8_t pin, bool on) {
    if (initializedPins.find(pin) == initializedPins.end()) {
        pinMode(pin, OUTPUT);
        initializedPins.insert(pin);
    }
    digitalWrite(pin, on ? HIGH : LOW);
    Serial.printf("[DeviceCtrl] Relay GPIO%d -> %s\n", pin, on ? "ON" : "OFF");
    return true;
}

bool control(const String& method, const String& target, bool on) {
    if (method == "tasmota") {
        return setTasmota(target, on);
    } else if (method == "shelly") {
        return setShelly(target, on);
    } else if (method == "relay") {
        return setRelay(target.toInt(), on);
    }
    
    Serial.printf("[DeviceCtrl] Unknown method: %s\n", method.c_str());
    return false;
}

}
