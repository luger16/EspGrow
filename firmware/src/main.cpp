#include <Arduino.h>
#include "storage.h"
#include "wifi_manager.h"
#include "websocket_server.h"
#include "device_controller.h"
#include "sensors.h"
#include "device_modes.h"
#include "devices.h"
#include "energy_tracker.h"
#include "sensor_config.h"
#include "history.h"
#include "ota_manager.h"
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <map>

namespace {
    unsigned long lastBroadcast = 0;
    const unsigned long BROADCAST_INTERVAL = 5000;
    std::map<String, float> cachedSensorReadings;
    bool sensorReadingsDirty = false;

    void broadcastDeviceModes() {
        JsonDocument doc;
        doc["type"] = "device_modes";
        
        String modesJson;
        DeviceModes::getModesJson(modesJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, modesJson);
        doc["data"] = dataDoc.as<JsonArray>();
        
        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }

    void broadcastDayNightConfig() {
        JsonDocument doc;
        doc["type"] = "daynight_config";
        
        String configJson;
        DeviceModes::getDayNightConfigJson(configJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, configJson);
        doc["data"] = dataDoc;
        
        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }

    void broadcastDevices() {
        Devices::computeControlModes();
        
        JsonDocument doc;
        doc["type"] = "devices";
        
        String devicesJson;
        Devices::getDevicesJson(devicesJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, devicesJson);
        doc["data"] = dataDoc.as<JsonArray>();
        
        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }

    void broadcastEnergy() {
        JsonDocument doc;
        doc["type"] = "energy";
        
        String energyJson;
        EnergyTracker::getEnergiesJson(energyJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, energyJson);
        doc["data"] = dataDoc.as<JsonArray>();
        
        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }

    void broadcastSensors() {
        JsonDocument doc;
        doc["type"] = "sensor_config";
        
        String sensorsJson;
        SensorConfig::getSensorsJson(sensorsJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, sensorsJson);
        doc["data"] = dataDoc.as<JsonArray>();
        
        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }

    void sendHistory(const char* sensorId, const char* range) {
        History::Range r;
        if (strcmp(range, "12h") == 0) r = History::RANGE_12H;
        else if (strcmp(range, "24h") == 0) r = History::RANGE_24H;
        else if (strcmp(range, "7d") == 0) r = History::RANGE_7D;
        else return;
        
        uint8_t buffer[History::POINTS_7D * sizeof(History::HistoryPoint)];
        size_t size = History::getHistory(sensorId, r, buffer, sizeof(buffer));
        
        if (size > 0) {
            JsonDocument doc;
            doc["type"] = "history";
            JsonObject histData = doc["data"].to<JsonObject>();
            histData["sensorId"] = sensorId;
            histData["range"] = range;
            histData["pointSize"] = sizeof(History::HistoryPoint);
            histData["count"] = size / sizeof(History::HistoryPoint);
            
            String base64Data;
            size_t encodedLen = (size + 2) / 3 * 4;
            base64Data.reserve(encodedLen + 1);
            
            const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            for (size_t i = 0; i < size; i += 3) {
                uint32_t n = ((uint32_t)buffer[i]) << 16;
                if (i + 1 < size) n |= ((uint32_t)buffer[i + 1]) << 8;
                if (i + 2 < size) n |= buffer[i + 2];
                
                base64Data += b64chars[(n >> 18) & 0x3F];
                base64Data += b64chars[(n >> 12) & 0x3F];
                base64Data += (i + 1 < size) ? b64chars[(n >> 6) & 0x3F] : '=';
                base64Data += (i + 2 < size) ? b64chars[n & 0x3F] : '=';
            }
            
            histData["payload"] = base64Data;
            
            String out;
            serializeJson(doc, out);
            WebSocketServer::broadcast(out);
        }
    }

    void handleMessage(const String& message) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, message);
        if (err) {
            Serial.printf("[Main] JSON parse error: %s\n", err.c_str());
            return;
        }

        const char* type = doc["type"];
        if (!type) return;

        // Extract payload from "data" key (new protocol), fallback to top-level (legacy)
        JsonObject payload = doc["data"].is<JsonObject>() ? doc["data"].as<JsonObject>() : doc.as<JsonObject>();

        if (strcmp(type, "ping") == 0) {
            JsonDocument response;
            response["type"] = "pong";
            response["data"]["timestamp"] = millis();
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "device_control") == 0) {
            const char* method = payload["method"];
            const char* target = payload["target"];
            bool on = payload["on"];
            
            bool success = DeviceController::control(
                method ? method : "",
                target ? target : "",
                on
            );
            
            Devices::Device* device = Devices::findDeviceByTarget(
                method ? method : "", target ? target : "");
            
            if (success && device) {
                Devices::setDeviceState(device->id, on);
            }
            
            JsonDocument response;
            response["type"] = "device_status";
            JsonObject respData = response["data"].to<JsonObject>();
            respData["target"] = target;
            respData["on"] = on;
            respData["success"] = success;
            if (device) {
                respData["deviceId"] = device->id;
            }
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "get_device_modes") == 0) {
            broadcastDeviceModes();
            broadcastDayNightConfig();
        }
        else if (strcmp(type, "set_device_mode") == 0) {
            JsonDocument modeDoc;
            modeDoc["deviceId"] = payload["deviceId"];
            modeDoc["mode"] = payload["mode"];
            if (payload["triggers"].is<JsonArray>()) modeDoc["triggers"] = payload["triggers"];
            if (payload["cycle"].is<JsonObject>()) modeDoc["cycle"] = payload["cycle"];
            if (payload["schedule"].is<JsonObject>()) modeDoc["schedule"] = payload["schedule"];
            
            DeviceModes::setMode(modeDoc);
            broadcastDeviceModes();
            broadcastDevices();
        }
        else if (strcmp(type, "delete_device_mode") == 0) {
            const char* deviceId = payload["deviceId"];
            if (deviceId) {
                DeviceModes::removeMode(deviceId);
                broadcastDeviceModes();
                broadcastDevices();
            }
        }
        else if (strcmp(type, "set_daynight_config") == 0) {
            JsonDocument configDoc;
            if (payload["useSchedule"].is<bool>()) configDoc["useSchedule"] = payload["useSchedule"];
            if (payload["dayStartTime"].is<const char*>()) configDoc["dayStartTime"] = payload["dayStartTime"];
            if (payload["nightStartTime"].is<const char*>()) configDoc["nightStartTime"] = payload["nightStartTime"];
            if (payload["lightThreshold"].is<float>()) configDoc["lightThreshold"] = payload["lightThreshold"];
            if (payload["lightHysteresis"].is<float>()) configDoc["lightHysteresis"] = payload["lightHysteresis"];
            
            DeviceModes::setDayNightConfig(configDoc);
            broadcastDayNightConfig();
        }
        else if (strcmp(type, "get_devices") == 0) {
            broadcastDevices();
        }
        else if (strcmp(type, "add_device") == 0) {
            JsonDocument deviceDoc;
            deviceDoc["id"] = payload["id"];
            deviceDoc["name"] = payload["name"];
            deviceDoc["type"] = payload["deviceType"];
            deviceDoc["controlMethod"] = payload["controlMethod"];
            deviceDoc["ipAddress"] = payload["ipAddress"];
            deviceDoc["controlMode"] = payload["controlMode"];
            deviceDoc["hasEnergyMonitoring"] = payload["hasEnergyMonitoring"] | false;
            
            Devices::addDevice(deviceDoc);
            broadcastDevices();
        }
        else if (strcmp(type, "update_device") == 0) {
            const char* deviceId = payload["id"];
            JsonDocument updates;
            if (payload["name"].is<const char*>()) updates["name"] = payload["name"];
            if (payload["deviceType"].is<const char*>()) updates["type"] = payload["deviceType"];
            if (payload["controlMethod"].is<const char*>()) updates["controlMethod"] = payload["controlMethod"];
            if (payload["ipAddress"].is<const char*>()) updates["ipAddress"] = payload["ipAddress"];
            if (payload["hasEnergyMonitoring"].is<bool>()) updates["hasEnergyMonitoring"] = payload["hasEnergyMonitoring"];
            
            Devices::updateDevice(deviceId, updates);
            broadcastDevices();
        }
        else if (strcmp(type, "remove_device") == 0) {
            const char* deviceId = payload["id"];
            DeviceModes::removeModeForDevice(deviceId);
            Devices::removeDevice(deviceId);
            broadcastDevices();
            broadcastDeviceModes();
        }
        else if (strcmp(type, "get_sensors") == 0) {
            broadcastSensors();
        }
        else if (strcmp(type, "add_sensor") == 0) {
            JsonDocument sensorDoc;
            sensorDoc["id"] = payload["id"];
            sensorDoc["name"] = payload["name"];
            sensorDoc["type"] = payload["sensorType"];
            sensorDoc["unit"] = payload["unit"];
            sensorDoc["hardwareType"] = payload["hardwareType"];
            sensorDoc["address"] = payload["address"];
            sensorDoc["tempSourceId"] = payload["tempSourceId"];
            sensorDoc["humSourceId"] = payload["humSourceId"];
            
            SensorConfig::addSensor(sensorDoc);
            broadcastSensors();
        }
        else if (strcmp(type, "update_sensor") == 0) {
            const char* sensorId = payload["id"];
            JsonDocument updates;
            if (payload["name"].is<const char*>()) updates["name"] = payload["name"];
            if (payload["sensorType"].is<const char*>()) updates["type"] = payload["sensorType"];
            if (payload["unit"].is<const char*>()) updates["unit"] = payload["unit"];
            if (payload["hardwareType"].is<const char*>()) updates["hardwareType"] = payload["hardwareType"];
            if (payload["address"].is<const char*>()) updates["address"] = payload["address"];
            if (payload["tempSourceId"].is<const char*>()) updates["tempSourceId"] = payload["tempSourceId"];
            if (payload["humSourceId"].is<const char*>()) updates["humSourceId"] = payload["humSourceId"];
            
            SensorConfig::updateSensor(sensorId, updates);
            broadcastSensors();
        }
        else if (strcmp(type, "remove_sensor") == 0) {
            const char* sensorId = payload["id"];
            SensorConfig::removeSensor(sensorId);
            broadcastSensors();
        }
        else if (strcmp(type, "get_history") == 0) {
            const char* sensorId = payload["sensorId"];
            const char* range = payload["range"];
            if (sensorId && range) {
                sendHistory(sensorId, range);
            }
        }
        else if (strcmp(type, "get_ppfd_calibration") == 0) {
            JsonDocument response;
            response["type"] = "ppfd_calibration";
            response["data"]["factor"] = Sensors::getPpfdCalibrationFactor();
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "calibrate_ppfd") == 0) {
            float knownPpfd = payload["knownPpfd"] | 0.0f;
            float rawPpfd = Sensors::getRawPpfd();
            
            if (knownPpfd > 0 && !isnan(rawPpfd) && rawPpfd > 0) {
                float factor = knownPpfd / rawPpfd;
                Sensors::setPpfdCalibrationFactor(factor);
                
                JsonDocument response;
                response["type"] = "ppfd_calibration";
                JsonObject respData = response["data"].to<JsonObject>();
                respData["factor"] = factor;
                respData["success"] = true;
                String out;
                serializeJson(response, out);
                WebSocketServer::broadcast(out);
            } else {
                JsonDocument response;
                response["type"] = "ppfd_calibration";
                JsonObject respData = response["data"].to<JsonObject>();
                respData["success"] = false;
                respData["error"] = isnan(rawPpfd) || rawPpfd <= 0 
                    ? "no_reading" : "invalid_value";
                String out;
                serializeJson(response, out);
                WebSocketServer::broadcast(out);
            }
        }
        else if (strcmp(type, "get_energy") == 0) {
            broadcastEnergy();
        }
        else if (strcmp(type, "reset_energy") == 0) {
            const char* deviceId = payload["deviceId"];
            if (deviceId) {
                EnergyTracker::resetEnergy(deviceId);
            } else {
                EnergyTracker::resetAllEnergy();
            }
            broadcastEnergy();
        }
        else if (strcmp(type, "reset_ppfd_calibration") == 0) {
            Sensors::setPpfdCalibrationFactor(1.0f);
            
            JsonDocument response;
            response["type"] = "ppfd_calibration";
            JsonObject respData = response["data"].to<JsonObject>();
            respData["factor"] = 1.0f;
            respData["success"] = true;
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "get_system_info") == 0) {
            JsonDocument response;
            response["type"] = "system_info";
            JsonObject respData = response["data"].to<JsonObject>();
            respData["uptime"] = millis() / 1000;
            respData["freeHeap"] = ESP.getFreeHeap();
            respData["chipModel"] = ESP.getChipModel();
            respData["wifiRssi"] = WiFi.RSSI();
            respData["ipAddress"] = WiFiManager::getIP();
            respData["firmwareVersion"] = FIRMWARE_VERSION;
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
    }

    void broadcastSensorData() {
        if (!WebSocketServer::hasClients()) return;

        Sensors::read();

        size_t sensorCount;
        const char** sensorIds = SensorConfig::getSensorIds(sensorCount);
        
        bool anyValid = false;
        
        JsonDocument doc;
        doc["type"] = "sensors";
        JsonArray data = doc["data"].to<JsonArray>();
        
        for (size_t i = 0; i < sensorCount; i++) {
            float value = Sensors::getSensorValue(sensorIds[i]);
            
            if (!isnan(value)) {
                anyValid = true;
                
                JsonObject entry = data.add<JsonObject>();
                entry["id"] = sensorIds[i];
                SensorConfig::Sensor* cfg = SensorConfig::getSensor(sensorIds[i]);
                if (cfg) entry["type"] = cfg->type;
                entry["value"] = value;

                if (cfg && strcmp(cfg->hardwareType, "as7341") == 0) {
                    uint16_t channels[8];
                    if (Sensors::getSpectralChannels(channels, 8)) {
                        JsonArray ch = entry["channels"].to<JsonArray>();
                        for (int j = 0; j < 8; j++) {
                            ch.add(channels[j]);
                        }
                    }
                }
                
                History::record(sensorIds[i], value);
                cachedSensorReadings[String(sensorIds[i])] = value;
            }
        }
        
        if (!anyValid) return;
        
        sensorReadingsDirty = true;

        String out;
        serializeJson(doc, out);
        WebSocketServer::broadcast(out);
    }
}

void setup() {
    Serial.begin(115200);
    
    while (!Serial && millis() < 5000) {
        delay(10);
    }
    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("       EspGrow Starting...       ");
    Serial.println("=================================");
    Serial.printf("Chip: %s\n", ESP.getChipModel());
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    
    if (!Storage::init()) {
        Serial.println("[ERROR] Storage init failed!");
    }
    
    WiFiManager::init();
    DeviceController::init();
    Sensors::init();
    Devices::init();
    SensorConfig::init();
    History::init();
    DeviceModes::init();
    EnergyTracker::init();
    DeviceModes::setDeviceStateCallback([](const char* deviceId, bool on) {
        JsonDocument response;
        response["type"] = "device_status";
        JsonObject respData = response["data"].to<JsonObject>();
        respData["deviceId"] = deviceId;
        respData["on"] = on;
        respData["success"] = true;
        String out;
        serializeJson(response, out);
        WebSocketServer::broadcast(out);
    });
    
    WebSocketServer::onMessage(handleMessage);
    
    OtaManager::validateRollback();
}

void loop() {
    static bool wsInitialized = false;
    
    WiFiManager::loop();
    
    if (WiFiManager::isConnected()) {
        if (!wsInitialized) {
            if (MDNS.begin("espgrow")) {
                Serial.println("[mDNS] Started: espgrow.local");
                MDNS.addService("http", "tcp", 80);
            }
            
            WebSocketServer::init();
            wsInitialized = true;
        }
        
        WebSocketServer::loop();
        History::loop();
        EnergyTracker::loop();
        
        if (millis() - lastBroadcast >= BROADCAST_INTERVAL) {
            lastBroadcast = millis();
            broadcastSensorData();
            broadcastEnergy();
        }
        
        if (sensorReadingsDirty) {
            sensorReadingsDirty = false;
            DeviceModes::loop(cachedSensorReadings);
        }
    }
}
