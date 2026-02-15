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
    int scannedRSSI[16];
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
*{margin:0;padding:0;box-sizing:border-box}
body{font:16px system-ui,sans-serif;padding:20px;background:#fff;color:#000}
h1{font-size:20px;margin-bottom:20px}
.error{background:#f88;padding:10px;margin-bottom:15px;border-radius:8px}
.network{padding:14px 16px;margin-bottom:10px;border:2px solid #e5e5e5;border-radius:12px;cursor:pointer;display:flex;justify-content:space-between;align-items:center;transition:all 0.2s}
.network:hover{background:#f9f9f9;border-color:#d0d0d0}
.network.selected{background:#f5f5f5;border-color:#999;font-weight:500}
.network span:first-child{font-weight:500}
.network span:last-child{font-size:14px;color:#666}
input,button{width:100%;padding:12px;margin-bottom:10px;border:2px solid #e5e5e5;font-size:16px;border-radius:8px}
input:focus{outline:none;border-color:#000}
button{background:#000;color:#fff;cursor:pointer;border:none;font-weight:500}
button:hover{background:#333}
.secondary{background:#fff;color:#000;border:2px solid #e5e5e5}
.secondary:hover{background:#f9f9f9;border-color:#d0d0d0}
</style>
<script>
function select(ssid,el){
document.querySelectorAll('.network').forEach(n=>n.classList.remove('selected'));
el.classList.add('selected');
document.getElementById('ssid').value=ssid;
}
</script>
</head>
<body>
<h1>WiFi Setup</h1>
)";
        if (lastError.length() > 0) {
            html += "<div class=\"error\">" + lastError + "</div>";
        }
        html += "<form method=\"POST\" action=\"/connect\"><div>";
        
        for (int i = 0; i < networkCount; i++) {
            html += "<div class=\"network\" onclick=\"select('" + scannedNetworks[i] + "',this)\"><span>" + scannedNetworks[i] + "</span><span>" + String(scannedRSSI[i]) + " dBm</span></div>";
        }

        html += R"(</div>
<input type="hidden" id="ssid" name="ssid" required>
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
<title>Connecting...</title>
<style>body{font-family:system-ui;background:#fff;color:#000;padding:40px;text-align:center}h2{margin-bottom:16px}p{color:#666;margin-top:20px}</style>
</head><body><h2>Connecting...</h2><p>Device will restart</p><p style="margin-top:30px">Visit <strong>espgrow.local</strong> once connected</p></body></html>)";
    }



    void scanNetworks() {
        Serial.println("[Portal] Scanning...");
        WiFi.mode(WIFI_AP_STA);
        
        int n = WiFi.scanNetworks();
        
        int indices[16];
        int validCount = 0;
        for (int i = 0; i < n && i < 16; i++) {
            if (WiFi.RSSI(i) > -75) {
                indices[validCount++] = i;
            }
        }
        
        for (int i = 0; i < validCount; i++) {
            for (int j = i + 1; j < validCount; j++) {
                if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
                    int tmp = indices[i];
                    indices[i] = indices[j];
                    indices[j] = tmp;
                }
            }
        }
        
        networkCount = min(validCount, 5);
        for (int i = 0; i < networkCount; i++) {
            scannedNetworks[i] = WiFi.SSID(indices[i]);
            scannedRSSI[i] = WiFi.RSSI(indices[i]);
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
        WiFi.persistent(true);
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
        if (!portalActive) { request->send(404); return; }
        scanNetworks();
        setupAP();
        request->redirect("/");
    });
    
    server->on("/connect", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!portalActive) { request->send(404); return; }
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
        if (!portalActive) { request->send(404); return; }
        request->redirect("http://" + WiFi.softAPIP().toString());
    });
    server->on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!portalActive) { request->send(404); return; }
        request->redirect("http://" + WiFi.softAPIP().toString());
    });
    server->on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!portalActive) { request->send(404); return; }
        request->redirect("http://" + WiFi.softAPIP().toString());
    });
    
    server->onNotFound([](AsyncWebServerRequest *request){
        if (!portalActive) { request->send(404); return; }
        request->send(200, "text/html", generateHTML());
    });
    
    server->begin();
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
