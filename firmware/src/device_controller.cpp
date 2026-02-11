#include "device_controller.h"
#include <HTTPClient.h>
#include <WiFi.h>

namespace DeviceController {

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

bool setShellyGen1(const String& ip, bool on) {
    HTTPClient http;
    String url = "http://" + ip + "/relay/0?turn=" + (on ? "on" : "off");
    
    http.begin(url);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    http.end();
    
    if (httpCode == 200) {
        Serial.printf("[DeviceCtrl] Shelly Gen1 %s -> %s\n", ip.c_str(), on ? "ON" : "OFF");
        return true;
    }
    
    Serial.printf("[DeviceCtrl] Shelly Gen1 %s failed: %d\n", ip.c_str(), httpCode);
    return false;
}

bool setShellyGen2(const String& ip, bool on) {
    HTTPClient http;
    String url = "http://" + ip + "/rpc/Switch.Set?id=0&on=" + (on ? "true" : "false");
    
    http.begin(url);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    http.end();
    
    if (httpCode == 200) {
        Serial.printf("[DeviceCtrl] Shelly Gen2 %s -> %s\n", ip.c_str(), on ? "ON" : "OFF");
        return true;
    }
    
    Serial.printf("[DeviceCtrl] Shelly Gen2 %s failed: %d\n", ip.c_str(), httpCode);
    return false;
}

bool control(const String& method, const String& target, bool on) {
    if (method == "tasmota") {
        return setTasmota(target, on);
    } else if (method == "shelly_gen1") {
        return setShellyGen1(target, on);
    } else if (method == "shelly_gen2") {
        return setShellyGen2(target, on);
    }
    
    Serial.printf("[DeviceCtrl] Unknown method: %s\n", method.c_str());
    return false;
}

}
