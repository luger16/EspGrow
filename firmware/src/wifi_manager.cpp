#include "wifi_manager.h"
#include "captive_portal.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sntp.h>

namespace WiFiManager {

namespace {
    bool provisioningActive = false;
    bool wasConnected = false;
    volatile bool timeSynced = false;
    unsigned long lastReconnectAttempt = 0;
    const unsigned long RECONNECT_INTERVAL = 30000;
    int reconnectFailures = 0;
    const int MAX_RECONNECT_FAILURES = 5;

    void onTimeSync(struct timeval* tv) {
        timeSynced = true;
        Serial.println("[WiFi] NTP time synced");
    }

    void startNTP() {
        sntp_set_time_sync_notification_cb(onTimeSync);
        configTime(0, 0, "time.cloudflare.com", "pool.ntp.org");
        Serial.println("[WiFi] NTP time sync started");
    }

    bool connectWithSaved() {
        Serial.println("[WiFi] Connecting with saved credentials...");
        WiFi.mode(WIFI_STA);
        WiFi.begin();

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            startNTP();
            return true;
        }
        
        Serial.println("[WiFi] Connection failed");
        return false;
    }
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool isTimeSynced() {
    return timeSynced;
}

String getIP() {
    return WiFi.localIP().toString();
}

void startProvisioning() {
    Serial.println("[WiFi] Starting provisioning portal...");
    
    CaptivePortal::Config config;
    config.apName = "EspGrow-Setup";
    config.connectionTimeout = 15000;
    
    CaptivePortal::start(config, [](const String& ssid, const String& password) {
        Serial.printf("[WiFi] Provisioned: %s\n", ssid.c_str());
    });
    
    provisioningActive = true;
}

void init() {
    Serial.println("[WiFi] Initializing...");
    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
    
    wifi_config_t conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK || strlen((char*)conf.sta.ssid) == 0) {
        Serial.println("[WiFi] No saved credentials, starting provisioning...");
        startProvisioning();
        return;
    }

    Serial.printf("[WiFi] Found saved credentials for: %s\n", (char*)conf.sta.ssid);
    if (connectWithSaved()) {
        wasConnected = true;
        return;
    }
    
    Serial.println("[WiFi] Connection failed, starting provisioning...");
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
            startNTP();
        }
        return;
    }

    bool connected = isConnected();

    if (connected) {
        wasConnected = true;
        reconnectFailures = 0;
        return;
    }

    if (!wasConnected) return;

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
    WiFi.begin();
}

}
