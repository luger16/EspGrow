#include <Arduino.h>
#include "storage.h"
#include "wifi_manager.h"
#include "websocket_server.h"
#include "device_controller.h"
#include "sensors.h"
#include "device_modes.h"
#include "devices.h"
#include "energy_tracker.h"
#include "dli_tracker.h"
#include "sensor_config.h"
#include "history.h"
#include "climate_config.h"
#include "event_log.h"
#include "ota_manager.h"
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <map>

namespace {
    unsigned long lastBroadcast = 0;
    unsigned long lastDevicePoll = 0;
    const unsigned long BROADCAST_INTERVAL = 5000;
    const unsigned long DEVICE_POLL_INTERVAL = 30000;
    unsigned int pollCycle = 0;
    const unsigned int OFFLINE_RECHECK_INTERVAL = 3;
    std::map<String, float> cachedSensorReadings;
    bool sensorReadingsDirty = false;

    void sendMessage(const String& message, uint32_t clientId = 0) {
        if (clientId) {
            WebSocketServer::sendTo(clientId, message);
        } else {
            WebSocketServer::broadcast(message);
        }
    }

    void sendDeviceModes(uint32_t clientId = 0) {
        JsonDocument doc;
        doc["type"] = "device_modes";
        
        String modesJson;
        DeviceModes::getModesJson(modesJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, modesJson);
        doc["data"] = dataDoc.as<JsonArray>();
        
        String out;
        serializeJson(doc, out);
        sendMessage(out, clientId);
    }

    void sendDayNightConfig(uint32_t clientId = 0) {
        JsonDocument doc;
        doc["type"] = "daynight_config";
        
        String configJson;
        DeviceModes::getDayNightConfigJson(configJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, configJson);
        doc["data"] = dataDoc;
        
        String out;
        serializeJson(doc, out);
        sendMessage(out, clientId);
    }

    void sendDevices(uint32_t clientId = 0) {
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
        sendMessage(out, clientId);
    }

    void sendClimateConfig(uint32_t clientId = 0) {
        JsonDocument doc;
        doc["type"] = "climate_config";

        String configJson;
        ClimateConfig::getConfigJson(configJson);

        JsonDocument dataDoc;
        deserializeJson(dataDoc, configJson);
        doc["data"] = dataDoc;

        String out;
        serializeJson(doc, out);
        sendMessage(out, clientId);
    }

    void sendEvents(uint32_t clientId = 0) {
        JsonDocument doc;
        doc["type"] = "events";

        String eventsJson;
        EventLog::getEventsJson(eventsJson);

        JsonDocument dataDoc;
        deserializeJson(dataDoc, eventsJson);
        doc["data"] = dataDoc.as<JsonArray>();

        String out;
        serializeJson(doc, out);
        sendMessage(out, clientId);
    }

    void sendEnergy(uint32_t clientId = 0) {
        JsonDocument doc;
        doc["type"] = "energy";
        
        String energyJson;
        EnergyTracker::getEnergiesJson(energyJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, energyJson);
        doc["data"] = dataDoc.as<JsonArray>();
        
        String out;
        serializeJson(doc, out);
        sendMessage(out, clientId);
    }

    void sendDli(uint32_t clientId = 0) {
        JsonDocument doc;
        doc["type"] = "dli";
        
        String dliJson;
        DliTracker::getDliJson(dliJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, dliJson);
        doc["data"] = dataDoc;
        
        String out;
        serializeJson(doc, out);
        sendMessage(out, clientId);
    }

    void sendSensors(uint32_t clientId = 0) {
        JsonDocument doc;
        doc["type"] = "sensor_config";
        
        String sensorsJson;
        SensorConfig::getSensorsJson(sensorsJson);
        
        JsonDocument dataDoc;
        deserializeJson(dataDoc, sensorsJson);
        doc["data"] = dataDoc.as<JsonArray>();
        
        String out;
        serializeJson(doc, out);
        sendMessage(out, clientId);
    }

    void sendHistory(const char* sensorId, const char* range, uint32_t clientId = 0) {
        History::Range r;
        if (strcmp(range, "6h") == 0) r = History::RANGE_6H;
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
            
            size_t encodedLen = (size + 2) / 3 * 4;
            char* b64buf = new char[encodedLen + 1];
            
            const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            size_t pos = 0;
            for (size_t i = 0; i < size; i += 3) {
                uint32_t n = ((uint32_t)buffer[i]) << 16;
                if (i + 1 < size) n |= ((uint32_t)buffer[i + 1]) << 8;
                if (i + 2 < size) n |= buffer[i + 2];
                
                b64buf[pos++] = b64chars[(n >> 18) & 0x3F];
                b64buf[pos++] = b64chars[(n >> 12) & 0x3F];
                b64buf[pos++] = (i + 1 < size) ? b64chars[(n >> 6) & 0x3F] : '=';
                b64buf[pos++] = (i + 2 < size) ? b64chars[n & 0x3F] : '=';
            }
            b64buf[pos] = '\0';
            
            histData["payload"] = (const char*)b64buf;
            
            String out;
            serializeJson(doc, out);
            delete[] b64buf;
            sendMessage(out, clientId);
        }
    }

    void pollAllDevices() {
        size_t count = Devices::getDeviceCount();
        if (count == 0) return;
        
        bool recheckOffline = (pollCycle % OFFLINE_RECHECK_INTERVAL == 0);
        pollCycle++;
        
        for (size_t i = 0; i < count; i++) {
            Devices::Device* device = Devices::getDeviceByIndex(i);
            if (!device || strlen(device->ipAddress) == 0) continue;
            
            if (!device->isOnline && !recheckOffline) continue;
            
            DeviceController::queryAsync(device->controlMethod, device->ipAddress);
        }
    }

    void handleMessage(uint32_t clientId, const String& message) {
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

        // Request-response: reply only to requesting client
        if (strcmp(type, "ping") == 0) {
            JsonDocument response;
            response["type"] = "pong";
            response["data"]["timestamp"] = millis();
            String out;
            serializeJson(response, out);
            sendMessage(out, clientId);
        }
        else if (strcmp(type, "get_init") == 0) {
            sendSensors(clientId);
            sendDevices(clientId);
            sendDeviceModes(clientId);
            sendDayNightConfig(clientId);
            sendClimateConfig(clientId);
            sendEnergy(clientId);
            sendDli(clientId);
            
            JsonDocument ppfdResp;
            ppfdResp["type"] = "ppfd_calibration";
            ppfdResp["data"]["factor"] = Sensors::getPpfdCalibrationFactor();
            String ppfdOut;
            serializeJson(ppfdResp, ppfdOut);
            sendMessage(ppfdOut, clientId);
        }
        else if (strcmp(type, "get_device_modes") == 0) {
            sendDeviceModes(clientId);
            sendDayNightConfig(clientId);
        }
        else if (strcmp(type, "get_devices") == 0) {
            sendDevices(clientId);
        }
        else if (strcmp(type, "get_sensors") == 0) {
            sendSensors(clientId);
        }
        else if (strcmp(type, "get_history") == 0) {
            const char* sensorId = payload["sensorId"];
            const char* range = payload["range"];
            if (sensorId && range) {
                sendHistory(sensorId, range, clientId);
            }
        }
        else if (strcmp(type, "get_ppfd_calibration") == 0) {
            JsonDocument response;
            response["type"] = "ppfd_calibration";
            response["data"]["factor"] = Sensors::getPpfdCalibrationFactor();
            String out;
            serializeJson(response, out);
            sendMessage(out, clientId);
        }
        else if (strcmp(type, "get_energy") == 0) {
            sendEnergy(clientId);
        }
        else if (strcmp(type, "get_dli") == 0) {
            sendDli(clientId);
        }
        else if (strcmp(type, "get_climate_config") == 0) {
            sendClimateConfig(clientId);
        }
        else if (strcmp(type, "get_events") == 0) {
            sendEvents(clientId);
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
            sendMessage(out, clientId);
        }
        // Mutations: broadcast state changes to all clients
        else if (strcmp(type, "device_control") == 0) {
            const char* method = payload["method"];
            const char* target = payload["target"];
            bool on = payload["on"];
            
            DeviceController::controlAsync(
                method ? method : "",
                target ? target : "",
                on
            );
        }
        else if (strcmp(type, "set_device_mode") == 0) {
            JsonDocument modeDoc;
            modeDoc["deviceId"] = payload["deviceId"];
            modeDoc["mode"] = payload["mode"];
            if (payload["triggers"].is<JsonArray>()) modeDoc["triggers"] = payload["triggers"];
            if (payload["cycle"].is<JsonObject>()) modeDoc["cycle"] = payload["cycle"];
            if (payload["schedule"].is<JsonObject>()) modeDoc["schedule"] = payload["schedule"];
            
            DeviceModes::setMode(modeDoc);
            sendDeviceModes();
            sendDevices();
        }
        else if (strcmp(type, "delete_device_mode") == 0) {
            const char* deviceId = payload["deviceId"];
            if (deviceId) {
                DeviceModes::removeMode(deviceId);
                sendDeviceModes();
                sendDevices();
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
            sendDayNightConfig();
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
            sendDevices();
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
            sendDevices();
        }
        else if (strcmp(type, "remove_device") == 0) {
            const char* deviceId = payload["id"];
            DeviceModes::removeModeForDevice(deviceId);
            Devices::removeDevice(deviceId);
            sendDevices();
            sendDeviceModes();
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
            if (payload["leafTempOffset"].is<float>()) sensorDoc["leafTempOffset"] = payload["leafTempOffset"];
            
            SensorConfig::addSensor(sensorDoc);
            sendSensors();
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
            if (payload["leafTempOffset"].is<float>()) updates["leafTempOffset"] = payload["leafTempOffset"];
            
            SensorConfig::updateSensor(sensorId, updates);
            sendSensors();
        }
        else if (strcmp(type, "remove_sensor") == 0) {
            const char* sensorId = payload["id"];
            SensorConfig::removeSensor(sensorId);
            sendSensors();
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
        else if (strcmp(type, "reset_energy") == 0) {
            const char* deviceId = payload["deviceId"];
            if (deviceId) {
                EnergyTracker::resetEnergy(deviceId);
            } else {
                EnergyTracker::resetAllEnergy();
            }
            sendEnergy();
        }
        else if (strcmp(type, "reset_dli") == 0) {
            DliTracker::resetDli();
            sendDli();
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
        else if (strcmp(type, "set_climate_phase") == 0) {
            const char* phase = payload["phase"];
            const char* startDate = payload["phaseStartDate"];
            if (phase) {
                ClimateConfig::setPhase(phase, startDate);
                sendClimateConfig();
            }
        }
        else if (strcmp(type, "set_climate_targets") == 0) {
            const char* phase = payload["phase"];
            if (phase && payload["targets"].is<JsonObject>()) {
                JsonObject targets = payload["targets"].as<JsonObject>();
                ClimateConfig::setTargets(phase, targets);
                sendClimateConfig();
            }
        }
        else if (strcmp(type, "reset_climate_targets") == 0) {
            const char* phase = payload["phase"];
            if (phase) {
                ClimateConfig::resetTargets(phase);
                sendClimateConfig();
            }
        }
        else if (strcmp(type, "clear_history") == 0) {
            History::clearAll();
            JsonDocument response;
            response["type"] = "clear_history";
            response["data"]["success"] = true;
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
        else if (strcmp(type, "restart") == 0) {
            JsonDocument response;
            response["type"] = "restart";
            response["data"]["success"] = true;
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
            delay(500);
            ESP.restart();
        }
    }

    void readAndRecordSensors() {
        Sensors::read();

        size_t sensorCount;
        const char** sensorIds = SensorConfig::getSensorIds(sensorCount);
        
        bool anyValid = false;
        
        for (size_t i = 0; i < sensorCount; i++) {
            float value = Sensors::getSensorValue(sensorIds[i]);
            
            if (!isnan(value)) {
                anyValid = true;
                History::record(sensorIds[i], value);
                cachedSensorReadings[String(sensorIds[i])] = value;
            }
        }

        size_t deviceCount = Devices::getDeviceCount();
        for (size_t i = 0; i < deviceCount; i++) {
            Devices::Device* device = Devices::getDeviceByIndex(i);
            if (device && device->isOnline) {
                History::record(device->id, device->isOn ? 1.0f : 0.0f, History::LAST_VALUE);
            }
        }
        
        if (anyValid) sensorReadingsDirty = true;
    }

    void broadcastSensorData() {
        if (!WebSocketServer::hasClients()) return;

        size_t sensorCount;
        const char** sensorIds = SensorConfig::getSensorIds(sensorCount);
        
        JsonDocument doc;
        doc["type"] = "sensors";
        JsonArray data = doc["data"].to<JsonArray>();
        
        bool anyValid = false;
        
        for (size_t i = 0; i < sensorCount; i++) {
            auto it = cachedSensorReadings.find(String(sensorIds[i]));
            if (it == cachedSensorReadings.end()) continue;
            
            anyValid = true;
            float value = it->second;
            
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
        }
        
        if (!anyValid) return;

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
    DeviceController::onResult([](const DeviceController::AsyncResult& ar) {
        Devices::Device* device = Devices::findDeviceByTarget(ar.method, ar.target);
        if (!device) return;

        bool changed = false;

        if (ar.result.reachable) {
            if (!device->isOnline) {
                Devices::setDeviceOnline(device->id, true);
                changed = true;
                Serial.printf("[DeviceCtrl] %s came online\n", device->name);

                char desc[128];
                snprintf(desc, sizeof(desc), "%s is reachable", device->name);
                EventLog::pushEvent("system", "Device online", desc);
            }
            if (device->isOn != ar.result.isOn) {
                Devices::setDeviceState(device->id, ar.result.isOn);
                changed = true;
            }
        } else {
            if (device->isOnline) {
                Devices::setDeviceOnline(device->id, false);
                changed = true;
                Serial.printf("[DeviceCtrl] %s went offline\n", device->name);

                char desc[128];
                snprintf(desc, sizeof(desc), "%s is unreachable", device->name);
                EventLog::pushEvent("system", "Device offline", desc, "warning");
            }
        }

        if (ar.wasControl || changed) {
            JsonDocument response;
            response["type"] = "device_status";
            JsonObject respData = response["data"].to<JsonObject>();
            respData["deviceId"] = device->id;
            respData["target"] = ar.target;
            respData["on"] = ar.result.reachable ? ar.result.isOn : ar.requestedState;
            respData["success"] = ar.result.reachable;
            String out;
            serializeJson(response, out);
            WebSocketServer::broadcast(out);
        }
    });
    Sensors::init();
    Devices::init();
    SensorConfig::init();
    History::init();
    DeviceModes::init();
    EnergyTracker::init();
    DliTracker::init();
    ClimateConfig::init();
    EventLog::init();
    
    WebSocketServer::onMessage(handleMessage);
    
    OtaManager::validateRollback();
}

void loop() {
    static bool wsInitialized = false;
    static bool wasConnected = false;
    
    WiFiManager::loop();
    DeviceController::loop();
    
    bool connected = WiFiManager::isConnected();
    
    if (connected) {
        if (!wsInitialized) {
            WebSocketServer::init();
            wsInitialized = true;
        }
        
        // Register/re-register mDNS on every WiFi (re)connect
        if (!wasConnected) {
            MDNS.end();
            if (MDNS.begin("espgrow")) {
                Serial.println("[mDNS] Started: espgrow.local");
                MDNS.addService("http", "tcp", 80);
            }
            pollAllDevices();
            lastDevicePoll = millis();
        }
        
        WebSocketServer::loop();
        EnergyTracker::loop();
        DliTracker::loop();
        
        if (millis() - lastDevicePoll >= DEVICE_POLL_INTERVAL) {
            lastDevicePoll = millis();
            pollAllDevices();
        }
    }
    
    // Sensor reading, history, and automation run regardless of WiFi
    History::loop();
    
    if (millis() - lastBroadcast >= BROADCAST_INTERVAL) {
        lastBroadcast = millis();
        readAndRecordSensors();
        if (connected) {
            broadcastSensorData();
            sendEnergy();
            sendDli();
        }
    }
    
    if (sensorReadingsDirty) {
        sensorReadingsDirty = false;
        DeviceModes::loop(cachedSensorReadings);
        EventLog::loop(cachedSensorReadings);
    }
    
    wasConnected = connected;
}
