#include "device_controller.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

namespace DeviceController {

static constexpr int CONTROL_TIMEOUT_MS = 2000;
static constexpr int QUERY_TIMEOUT_MS = 500;

void init() {
    Serial.println("[DeviceCtrl] Initialized");
}

QueryResult setTasmota(const String& ip, bool on) {
    QueryResult result;
    HTTPClient http;
    String url = "http://" + ip + "/cm?cmnd=Power%20" + (on ? "On" : "Off");
    
    http.begin(url);
    http.setTimeout(CONTROL_TIMEOUT_MS);
    
    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();
        
        JsonDocument doc;
        if (!deserializeJson(doc, payload)) {
            result.reachable = true;
            // Tasmota returns {"POWER":"ON"} or {"POWER":"OFF"}
            const char* power = doc["POWER"];
            if (power) {
                result.isOn = (strcmp(power, "ON") == 0);
            }
        }
        Serial.printf("[DeviceCtrl] Tasmota %s -> %s\n", ip.c_str(), result.isOn ? "ON" : "OFF");
    } else {
        http.end();
        Serial.printf("[DeviceCtrl] Tasmota %s failed: %d\n", ip.c_str(), httpCode);
    }
    
    return result;
}

QueryResult setShellyGen1(const String& ip, bool on) {
    QueryResult result;
    HTTPClient http;
    String url = "http://" + ip + "/relay/0?turn=" + (on ? "on" : "off");
    
    http.begin(url);
    http.setTimeout(CONTROL_TIMEOUT_MS);
    
    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();
        
        JsonDocument doc;
        if (!deserializeJson(doc, payload)) {
            result.reachable = true;
            // Shelly Gen1 returns {"ison":true/false, ...}
            result.isOn = doc["ison"] | false;
        }
        Serial.printf("[DeviceCtrl] Shelly Gen1 %s -> %s\n", ip.c_str(), result.isOn ? "ON" : "OFF");
    } else {
        http.end();
        Serial.printf("[DeviceCtrl] Shelly Gen1 %s failed: %d\n", ip.c_str(), httpCode);
    }
    
    return result;
}

QueryResult setShellyGen2(const String& ip, bool on) {
    QueryResult result;
    HTTPClient http;
    String url = "http://" + ip + "/rpc/Switch.Set?id=0&on=" + (on ? "true" : "false");
    
    http.begin(url);
    http.setTimeout(CONTROL_TIMEOUT_MS);
    
    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();
        
        JsonDocument doc;
        if (!deserializeJson(doc, payload)) {
            result.reachable = true;
            // Shelly Gen2 returns {"was_on":true/false} for Set commands
            result.isOn = on;
        }
        Serial.printf("[DeviceCtrl] Shelly Gen2 %s -> %s\n", ip.c_str(), result.isOn ? "ON" : "OFF");
    } else {
        http.end();
        Serial.printf("[DeviceCtrl] Shelly Gen2 %s failed: %d\n", ip.c_str(), httpCode);
    }
    
    return result;
}

QueryResult control(const String& method, const String& target, bool on) {
    if (method == "tasmota") {
        return setTasmota(target, on);
    } else if (method == "shelly_gen1") {
        return setShellyGen1(target, on);
    } else if (method == "shelly_gen2") {
        return setShellyGen2(target, on);
    }
    
    Serial.printf("[DeviceCtrl] Unknown method: %s\n", method.c_str());
    return QueryResult{};
}

QueryResult queryTasmota(const String& ip) {
    QueryResult result;
    HTTPClient http;
    String url = "http://" + ip + "/cm?cmnd=Power";
    
    http.begin(url);
    http.setTimeout(QUERY_TIMEOUT_MS);
    
    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();
        
        JsonDocument doc;
        if (!deserializeJson(doc, payload)) {
            result.reachable = true;
            // Tasmota returns {"POWER":"ON"} or {"POWER":"OFF"}
            const char* power = doc["POWER"];
            if (power) {
                result.isOn = (strcmp(power, "ON") == 0);
            }
        }
    } else {
        http.end();
        Serial.printf("[DeviceCtrl] Tasmota query %s failed: %d\n", ip.c_str(), httpCode);
    }
    
    return result;
}

QueryResult queryShellyGen1(const String& ip) {
    QueryResult result;
    HTTPClient http;
    String url = "http://" + ip + "/relay/0";
    
    http.begin(url);
    http.setTimeout(QUERY_TIMEOUT_MS);
    
    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();
        
        JsonDocument doc;
        if (!deserializeJson(doc, payload)) {
            result.reachable = true;
            // Shelly Gen1 returns {"ison":true/false, ...}
            result.isOn = doc["ison"] | false;
        }
    } else {
        http.end();
        Serial.printf("[DeviceCtrl] Shelly Gen1 query %s failed: %d\n", ip.c_str(), httpCode);
    }
    
    return result;
}

QueryResult queryShellyGen2(const String& ip) {
    QueryResult result;
    HTTPClient http;
    String url = "http://" + ip + "/rpc/Switch.GetStatus?id=0";
    
    http.begin(url);
    http.setTimeout(QUERY_TIMEOUT_MS);
    
    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();
        
        JsonDocument doc;
        if (!deserializeJson(doc, payload)) {
            result.reachable = true;
            // Shelly Gen2 returns {"id":0, "source":"...", "output":true/false, ...}
            result.isOn = doc["output"] | false;
        }
    } else {
        http.end();
        Serial.printf("[DeviceCtrl] Shelly Gen2 query %s failed: %d\n", ip.c_str(), httpCode);
    }
    
    return result;
}

QueryResult queryState(const String& method, const String& target) {
    if (method == "tasmota") {
        return queryTasmota(target);
    } else if (method == "shelly_gen1") {
        return queryShellyGen1(target);
    } else if (method == "shelly_gen2") {
        return queryShellyGen2(target);
    }
    
    Serial.printf("[DeviceCtrl] Unknown query method: %s\n", method.c_str());
    return QueryResult{};
}

}
