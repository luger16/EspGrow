#include "captive_portal.h"
#include "websocket_server.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

namespace CaptivePortal {

namespace {
    DNSServer dnsServer;
    AsyncWebServer* server = nullptr;
    SuccessCallback successCallback;
    Config currentConfig;
    bool portalActive = false;
    
    String scannedNetworks[16];
    int networkCount = 0;
    
    String lastError;
    String pendingSSID;
    String pendingPassword;
    bool connectionPending = false;
    unsigned long connectionStartTime = 0;

    void setupAP();
    void scanNetworks();

    String generateHTML() {
        String html = R"(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EspGrow WiFi</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#111;color:#fff;padding:20px;min-height:100vh}
h1{color:#22c55e;margin-bottom:20px;font-size:22px}
.error{background:#7f1d1d;padding:12px;border-radius:6px;margin-bottom:16px}
select,input,button{width:100%;padding:12px;border-radius:6px;font-size:16px;margin-bottom:12px;border:1px solid #333}
select,input{background:#222;color:#fff}
select:focus,input:focus{outline:none;border-color:#22c55e}
button{background:#22c55e;color:#000;font-weight:600;border:none;cursor:pointer}
button:disabled{opacity:0.5}
.secondary{background:#333;color:#fff}
</style>
</head>
<body>
<h1>EspGrow WiFi Setup</h1>
)";
        if (lastError.length() > 0) {
            html += "<div class=\"error\">" + lastError + "</div>";
        }

        html += R"(<form method="POST" action="/connect">
<select name="ssid" required>
<option value="">Select network...</option>
)";
        for (int i = 0; i < networkCount; i++) {
            html += "<option value=\"" + scannedNetworks[i] + "\">" + scannedNetworks[i] + "</option>";
        }

        html += R"(</select>
<input type="password" name="password" placeholder="Password">
<button type="submit">Connect</button>
</form>
<button class="secondary" onclick="location.href='/scan'">Scan Again</button>
</body>
</html>)";
        
        return html;
    }

    String generateConnectingHTML() {
        return R"(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="5;url=/">
<title>Connecting...</title>
<style>body{font-family:system-ui;background:#111;color:#fff;padding:40px;text-align:center}</style>
</head><body><h2>Connecting...</h2><p>Please wait</p></body></html>)";
    }

    void scanNetworks() {
        Serial.println("[Portal] Scanning...");
        WiFi.mode(WIFI_AP_STA);
        
        int n = WiFi.scanNetworks();
        networkCount = min(n, 16);
        
        int indices[16];
        for (int i = 0; i < networkCount; i++) indices[i] = i;
        for (int i = 0; i < networkCount; i++) {
            for (int j = i + 1; j < networkCount; j++) {
                if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
                    int tmp = indices[i];
                    indices[i] = indices[j];
                    indices[j] = tmp;
                }
            }
        }
        
        for (int i = 0; i < networkCount; i++) {
            scannedNetworks[i] = WiFi.SSID(indices[i]);
        }
        
        WiFi.scanDelete();
        Serial.printf("[Portal] Found %d networks\n", networkCount);
    }

    void setupAP() {
        WiFi.mode(WIFI_AP);
        
        IPAddress apIP(192, 168, 4, 1);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        
        if (currentConfig.apPassword && strlen(currentConfig.apPassword) >= 8) {
            WiFi.softAP(currentConfig.apName, currentConfig.apPassword);
        } else {
            WiFi.softAP(currentConfig.apName);
        }
        
        delay(500);
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(53, "*", apIP);
        
        Serial.printf("[Portal] AP: %s (%s)\n", currentConfig.apName, apIP.toString().c_str());
    }

    void attemptConnection() {
        Serial.printf("[Portal] Trying: %s\n", pendingSSID.c_str());
        
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(pendingSSID.c_str(), pendingPassword.c_str());
        
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < currentConfig.connectionTimeout) {
            delay(100);
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[Portal] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            if (successCallback) {
                successCallback(pendingSSID, pendingPassword);
            }
        } else {
            Serial.println("[Portal] Failed");
            lastError = "Connection failed. Check password.";
            setupAP();
        }
        
        connectionPending = false;
        pendingSSID = "";
        pendingPassword = "";
    }
}

void start(const Config& config, SuccessCallback onSuccess) {
    currentConfig = config;
    successCallback = onSuccess;
    lastError = "";
    connectionPending = false;
    
    Serial.println("[Portal] Starting...");
    scanNetworks();
    setupAP();
    
    server = WebSocketServer::getServer(80);
    
    server->on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
        scanNetworks();
        setupAP();
        request->redirect("/");
    });
    
    server->on("/connect", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("ssid", true)) {
            pendingSSID = request->getParam("ssid", true)->value();
        }
        if (request->hasParam("password", true)) {
            pendingPassword = request->getParam("password", true)->value();
        }
        connectionPending = true;
        connectionStartTime = millis();
        lastError = "";
        
        Serial.printf("[Portal] Connecting to: %s\n", pendingSSID.c_str());
        request->send(200, "text/html", generateConnectingHTML());
    });
    
    server->on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("http://" + WiFi.softAPIP().toString());
    });
    server->on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("http://" + WiFi.softAPIP().toString());
    });
    server->on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("http://" + WiFi.softAPIP().toString());
    });
    
    server->onNotFound([](AsyncWebServerRequest *request){
        request->send(200, "text/html", generateHTML());
    });
    
    portalActive = true;
    Serial.printf("[Portal] Ready: %s\n", config.apName);
}

void stop() {
    if (!portalActive) return;
    Serial.println("[Portal] Stopping...");
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    portalActive = false;
}

void loop() {
    if (!portalActive) return;
    dnsServer.processNextRequest();
    if (connectionPending && (millis() - connectionStartTime) > 500) {
        attemptConnection();
    }
}

bool isActive() {
    return portalActive;
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

}
