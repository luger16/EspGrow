#include "device_controller.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

namespace DeviceController {

static constexpr int CONTROL_TIMEOUT_MS = 2000;
static constexpr int QUERY_TIMEOUT_MS = 1000;
static constexpr int CONNECT_TIMEOUT_MS = 1000;

namespace {
    struct HttpResult {
        bool ok = false;
        JsonDocument doc;
    };

    HttpResult httpGet(const String& url, int timeoutMs) {
        HttpResult result;
        HTTPClient http;
        http.begin(url);
        http.setConnectTimeout(CONNECT_TIMEOUT_MS);
        http.setTimeout(timeoutMs);
        
        int httpCode = http.GET();
        if (httpCode == 200) {
            String payload = http.getString();
            http.end();
            if (!deserializeJson(result.doc, payload)) {
                result.ok = true;
            }
        } else {
            http.end();
        }
        return result;
    }
}

void init() {
    Serial.println("[DeviceCtrl] Initialized");
}

QueryResult setTasmota(const String& ip, bool on) {
    QueryResult result;
    String url = "http://" + ip + "/cm?cmnd=Power%20" + (on ? "On" : "Off");
    auto resp = httpGet(url, CONTROL_TIMEOUT_MS);
    
    if (resp.ok) {
        result.reachable = true;
        const char* power = resp.doc["POWER"];
        if (power) {
            result.isOn = (strcmp(power, "ON") == 0);
        }
        Serial.printf("[DeviceCtrl] Tasmota %s -> %s\n", ip.c_str(), result.isOn ? "ON" : "OFF");
    } else {
        Serial.printf("[DeviceCtrl] Tasmota %s failed\n", ip.c_str());
    }
    
    return result;
}

QueryResult setShellyGen1(const String& ip, bool on) {
    QueryResult result;
    String url = "http://" + ip + "/relay/0?turn=" + (on ? "on" : "off");
    auto resp = httpGet(url, CONTROL_TIMEOUT_MS);
    
    if (resp.ok) {
        result.reachable = true;
        result.isOn = resp.doc["ison"] | false;
        Serial.printf("[DeviceCtrl] Shelly Gen1 %s -> %s\n", ip.c_str(), result.isOn ? "ON" : "OFF");
    } else {
        Serial.printf("[DeviceCtrl] Shelly Gen1 %s failed\n", ip.c_str());
    }
    
    return result;
}

QueryResult setShellyGen2(const String& ip, bool on) {
    QueryResult result;
    String url = "http://" + ip + "/rpc/Switch.Set?id=0&on=" + (on ? "true" : "false");
    auto resp = httpGet(url, CONTROL_TIMEOUT_MS);
    
    if (resp.ok) {
        result.reachable = true;
        bool wasOn = resp.doc["was_on"] | !on;
        result.isOn = on;
        Serial.printf("[DeviceCtrl] Shelly Gen2 %s -> %s (was %s)\n",
            ip.c_str(), result.isOn ? "ON" : "OFF", wasOn ? "ON" : "OFF");
    } else {
        Serial.printf("[DeviceCtrl] Shelly Gen2 %s failed\n", ip.c_str());
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
    auto resp = httpGet("http://" + ip + "/cm?cmnd=Power", QUERY_TIMEOUT_MS);
    
    if (resp.ok) {
        result.reachable = true;
        const char* power = resp.doc["POWER"];
        if (power) {
            result.isOn = (strcmp(power, "ON") == 0);
        }
    } else {
        Serial.printf("[DeviceCtrl] Tasmota query %s failed\n", ip.c_str());
    }
    
    return result;
}

QueryResult queryShellyGen1(const String& ip) {
    QueryResult result;
    auto resp = httpGet("http://" + ip + "/relay/0", QUERY_TIMEOUT_MS);
    
    if (resp.ok) {
        result.reachable = true;
        result.isOn = resp.doc["ison"] | false;
    } else {
        Serial.printf("[DeviceCtrl] Shelly Gen1 query %s failed\n", ip.c_str());
    }
    
    return result;
}

QueryResult queryShellyGen2(const String& ip) {
    QueryResult result;
    auto resp = httpGet("http://" + ip + "/rpc/Switch.GetStatus?id=0", QUERY_TIMEOUT_MS);
    
    if (resp.ok) {
        result.reachable = true;
        result.isOn = resp.doc["output"] | false;
    } else {
        Serial.printf("[DeviceCtrl] Shelly Gen2 query %s failed\n", ip.c_str());
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
