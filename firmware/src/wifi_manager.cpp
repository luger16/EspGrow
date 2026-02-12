#include "wifi_manager.h"
#include "storage.h"
#include "captive_portal.h"
#include <WiFi.h>
#include <ArduinoJson.h>

namespace WiFiManager {

namespace {
    const char* WIFI_CONFIG_PATH = "/wifi.json";
    String savedSSID;
    String savedPassword;
    bool provisioningActive = false;
    bool wasConnected = false;
    unsigned long lastReconnectAttempt = 0;
    const unsigned long RECONNECT_INTERVAL = 30000;
    int reconnectFailures = 0;
    const int MAX_RECONNECT_FAILURES = 5;

    void loadCredentials() {
        Serial.println("[WiFi] Loading credentials...");
        JsonDocument doc;
        if (Storage::readJson(WIFI_CONFIG_PATH, doc)) {
            savedSSID = doc["ssid"].as<String>();
            savedPassword = doc["password"].as<String>();
            Serial.printf("[WiFi] Loaded credentials for: %s\n", savedSSID.c_str());
        } else {
            Serial.println("[WiFi] No saved credentials found");
        }
    }

    void saveCredentials(const String& ssid, const String& password) {
        JsonDocument doc;
        doc["ssid"] = ssid;
        doc["password"] = password;
        Storage::writeJson(WIFI_CONFIG_PATH, doc);
        savedSSID = ssid;
        savedPassword = password;
        Serial.printf("[WiFi] Saved credentials for: %s\n", ssid.c_str());
    }

    bool connectWithSaved() {
        if (!hasCredentials()) {
            Serial.println("[WiFi] No credentials to connect with");
            return false;
        }
        
        Serial.printf("[WiFi] Connecting to: %s\n", savedSSID.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            configTime(0, 0, "time.cloudflare.com", "pool.ntp.org");
            Serial.println("[WiFi] NTP time sync started");
            return true;
        }
        
        Serial.println("[WiFi] Connection failed");
        return false;
    }
}

bool hasCredentials() {
    return savedSSID.length() > 0;
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String getIP() {
    return WiFi.localIP().toString();
}

void clearCredentials() {
    Storage::remove(WIFI_CONFIG_PATH);
    savedSSID = "";
    savedPassword = "";
    Serial.println("[WiFi] Credentials cleared");
}

void startProvisioning() {
    Serial.println("[WiFi] Starting provisioning portal...");
    
    CaptivePortal::Config config;
    config.apName = "EspGrow-Setup";
    config.connectionTimeout = 15000;
    
    CaptivePortal::start(config, [](const String& ssid, const String& password) {
        Serial.printf("[WiFi] Provisioned: %s\n", ssid.c_str());
        saveCredentials(ssid, password);
    });
    
    provisioningActive = true;
}

void init() {
    Serial.println("[WiFi] Initializing...");
    loadCredentials();
    
    if (hasCredentials()) {
        Serial.println("[WiFi] Has credentials, attempting connection...");
        if (connectWithSaved()) {
            wasConnected = true;
            return;
        }
        Serial.println("[WiFi] Connection failed, starting provisioning...");
    } else {
        Serial.println("[WiFi] No credentials, starting provisioning...");
    }
    
    startProvisioning();
}

void loop() {
    if (provisioningActive) {
        CaptivePortal::loop();
        
        if (CaptivePortal::isConnected()) {
            Serial.println("[WiFi] Provisioning complete");
            CaptivePortal::stop();
            provisioningActive = false;
            wasConnected = true;
            reconnectFailures = 0;
        }
        return;
    }

    bool connected = isConnected();

    if (connected) {
        wasConnected = true;
        reconnectFailures = 0;
        return;
    }

    if (!wasConnected || !hasCredentials()) return;

    if (millis() - lastReconnectAttempt < RECONNECT_INTERVAL) return;
    lastReconnectAttempt = millis();
    reconnectFailures++;

    Serial.printf("[WiFi] Connection lost — reconnect attempt %d/%d\n", reconnectFailures, MAX_RECONNECT_FAILURES);

    if (reconnectFailures > MAX_RECONNECT_FAILURES) {
        Serial.println("[WiFi] Max reconnect attempts reached, starting provisioning");
        wasConnected = false;
        reconnectFailures = 0;
        startProvisioning();
        return;
    }

    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
}

}
