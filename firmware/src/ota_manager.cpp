#include "ota_manager.h"
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>

namespace OtaManager {

namespace {
    volatile Status currentStatus = Status::Idle;
    StatusCallback statusCallback;

    static constexpr unsigned long CACHE_TTL = 1800000; // 30 minutes
    static constexpr unsigned long STALL_TIMEOUT = 30000;
    static constexpr size_t MIN_FREE_HEAP = 50000;
    static constexpr uint8_t ESP_IMAGE_MAGIC = 0xE9;
    static constexpr size_t DOWNLOAD_BUFFER_SIZE = 1024;

    struct ReleaseCache {
        String latestVersion;
        String downloadUrl;
        String releaseUrl;
        String publishedAt;
        int size = 0;
        unsigned long lastCheck = 0;
        bool valid = false;
        bool rateLimited = false;
    };
    ReleaseCache releaseCache;

    void emitStatus(Status status, int progress = -1, const char* error = nullptr) {
        currentStatus = status;
        if (statusCallback) {
            StatusEvent event;
            event.status = status;
            event.progress = progress;
            if (error) event.error = error;
            statusCallback(event);
        }
    }

    void scheduleReboot(unsigned long delayMs = 1500) {
        emitStatus(Status::Rebooting);
        delay(delayMs);
        ESP.restart();
    }

    bool isGitHubUrl(const String& url) {
        return url.startsWith("https://github.com/") ||
               url.startsWith("https://objects.githubusercontent.com/");
    }

    size_t getOtaPartitionSize() {
        const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
        return partition ? partition->size : 0;
    }

    bool downloadAndFlash(const String& url, const String& expectedMD5) {
        if (ESP.getFreeHeap() < MIN_FREE_HEAP) {
            Serial.printf("[OTA] Heap too low: %u < %u\n", ESP.getFreeHeap(), MIN_FREE_HEAP);
            emitStatus(Status::Error, -1, "Not enough memory for TLS");
            currentStatus = Status::Idle;
            return false;
        }

        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setUserAgent("EspGrow-OTA/1.0");
        http.setTimeout(30000);
        http.begin(client, url);

        Serial.printf("[OTA] Downloading: %s\n", url.c_str());
        emitStatus(Status::Downloading, 0);

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK) {
            Serial.printf("[OTA] HTTP error: %d\n", httpCode);
            http.end();
            emitStatus(Status::Error, -1, "Download failed");
            currentStatus = Status::Idle;
            return false;
        }

        int contentLength = http.getSize();
        if (contentLength <= 0) {
            Serial.println("[OTA] Invalid content length");
            http.end();
            emitStatus(Status::Error, -1, "Invalid firmware size");
            currentStatus = Status::Idle;
            return false;
        }

        size_t partitionSize = getOtaPartitionSize();
        if (partitionSize > 0 && (size_t)contentLength > partitionSize) {
            Serial.printf("[OTA] Firmware too large: %d > %u\n", contentLength, partitionSize);
            http.end();
            emitStatus(Status::Error, -1, "Firmware too large for partition");
            currentStatus = Status::Idle;
            return false;
        }

        Stream* stream = http.getStreamPtr();
        uint8_t firstByte;
        if (stream->readBytes(&firstByte, 1) != 1) {
            Serial.println("[OTA] Failed to read first byte");
            http.end();
            emitStatus(Status::Error, -1, "Download read error");
            currentStatus = Status::Idle;
            return false;
        }

        if (firstByte != ESP_IMAGE_MAGIC) {
            Serial.printf("[OTA] Bad magic: 0x%02X (expected 0xE9)\n", firstByte);
            http.end();
            emitStatus(Status::Error, -1, "Invalid firmware image");
            currentStatus = Status::Idle;
            return false;
        }

        Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);

        if (expectedMD5.length() > 0 && !Update.setMD5(expectedMD5.c_str())) {
            Serial.println("[OTA] Invalid MD5");
            http.end();
            emitStatus(Status::Error, -1, "Invalid MD5 hash");
            currentStatus = Status::Idle;
            return false;
        }

        if (!Update.begin(contentLength, U_FLASH)) {
            Serial.printf("[OTA] Begin failed: %s\n", Update.errorString());
            http.end();
            emitStatus(Status::Error, -1, Update.errorString());
            currentStatus = Status::Idle;
            return false;
        }

        Update.onProgress([](size_t progress, size_t total) {
            int percent = (total > 0) ? (int)((progress * 100) / total) : 0;
            static int lastEmitted = -1;
            if (percent != lastEmitted && percent % 5 == 0) {
                lastEmitted = percent;
                // Can't call emitStatus here (ISR-like context from Update internals),
                // but we can use Serial for debug. Progress is emitted from the write loop.
            }
        });

        currentStatus = Status::Installing;
        emitStatus(Status::Installing, 0);

        if (Update.write(&firstByte, 1) != 1) {
            Serial.println("[OTA] Write failed (first byte)");
            Update.abort();
            http.end();
            emitStatus(Status::Error, -1, "Flash write failed");
            currentStatus = Status::Idle;
            return false;
        }

        uint8_t buffer[DOWNLOAD_BUFFER_SIZE];
        size_t totalWritten = 1;
        int lastPercent = -1;
        unsigned long lastDataTime = millis();

        while (http.connected() && totalWritten < (size_t)contentLength) {
            size_t available = stream->available();
            if (available) {
                lastDataTime = millis();
                size_t toRead = (available > sizeof(buffer)) ? sizeof(buffer) : available;
                size_t bytesRead = stream->readBytes(buffer, toRead);

                if (Update.write(buffer, bytesRead) != bytesRead) {
                    Serial.println("[OTA] Write failed");
                    Update.abort();
                    http.end();
                    emitStatus(Status::Error, -1, "Flash write failed");
                    currentStatus = Status::Idle;
                    return false;
                }

                totalWritten += bytesRead;
                int percent = (totalWritten * 100) / contentLength;
                if (percent != lastPercent && percent % 5 == 0) {
                    lastPercent = percent;
                    emitStatus(Status::Installing, percent);
                }
            } else if (millis() - lastDataTime > STALL_TIMEOUT) {
                Serial.println("[OTA] Download stalled");
                Update.abort();
                http.end();
                emitStatus(Status::Error, -1, "Download stalled");
                currentStatus = Status::Idle;
                return false;
            }
            delay(1);
        }

        http.end();

        if (totalWritten != (size_t)contentLength) {
            Serial.printf("[OTA] Incomplete: %u/%d bytes\n", totalWritten, contentLength);
            Update.abort();
            emitStatus(Status::Error, -1, "Incomplete download");
            currentStatus = Status::Idle;
            return false;
        }

        if (!Update.end()) {
            Serial.printf("[OTA] End failed: %s\n", Update.errorString());
            emitStatus(Status::Error, -1, Update.errorString());
            currentStatus = Status::Idle;
            return false;
        }

        Serial.println("[OTA] Update successful!");
        emitStatus(Status::Success, 100);
        return true;
    }

    void fetchGitHubRelease() {
        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setUserAgent("EspGrow-OTA/1.0");
        http.setTimeout(10000);

        String url = "https://api.github.com/repos/" + String(GITHUB_REPO) + "/releases/latest";
        http.begin(client, url);
        http.addHeader("Accept", "application/vnd.github.v3+json");

        int httpCode = http.GET();

        if (httpCode == 403 || httpCode == 429) {
            Serial.printf("[OTA] GitHub rate limited: %d\n", httpCode);
            http.end();
            releaseCache.rateLimited = true;
            releaseCache.lastCheck = millis();
            return;
        }

        if (httpCode != HTTP_CODE_OK) {
            Serial.printf("[OTA] GitHub check failed: %d\n", httpCode);
            http.end();
            return;
        }

        JsonDocument filter;
        filter["tag_name"] = true;
        filter["html_url"] = true;
        filter["published_at"] = true;
        filter["assets"][0]["name"] = true;
        filter["assets"][0]["browser_download_url"] = true;
        filter["assets"][0]["size"] = true;

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
        http.end();

        if (error) {
            Serial.printf("[OTA] GitHub JSON parse error: %s\n", error.c_str());
            return;
        }

        releaseCache.latestVersion = doc["tag_name"].as<const char*>();
        releaseCache.releaseUrl = doc["html_url"].as<const char*>();
        releaseCache.publishedAt = doc["published_at"].as<const char*>();
        releaseCache.downloadUrl = "";
        releaseCache.size = 0;
        releaseCache.rateLimited = false;

        String binName = "firmware-" + getChipModel() + ".bin";
        JsonArray assets = doc["assets"];
        for (JsonObject asset : assets) {
            const char* name = asset["name"];
            if (name && String(name) == binName) {
                releaseCache.downloadUrl = asset["browser_download_url"].as<const char*>();
                releaseCache.size = asset["size"].as<int>();
                break;
            }
        }

        releaseCache.lastCheck = millis();
        releaseCache.valid = true;
        Serial.printf("[OTA] GitHub: latest=%s, binary=%s\n",
            releaseCache.latestVersion.c_str(),
            releaseCache.downloadUrl.length() > 0 ? "found" : "not found");
    }
}

String getChipModel() {
    String model = ESP.getChipModel();
    if (model.indexOf("ESP32-C3") >= 0) return "c3";
    if (model.indexOf("ESP32-S3") >= 0) return "s3";
    return "esp32";
}

Status getStatus() {
    return currentStatus;
}

ReleaseInfo getCachedRelease() {
    ReleaseInfo info;
    info.latestVersion = releaseCache.latestVersion;
    info.downloadUrl = releaseCache.downloadUrl;
    info.releaseUrl = releaseCache.releaseUrl;
    info.publishedAt = releaseCache.publishedAt;
    info.size = releaseCache.size;
    info.valid = releaseCache.valid;
    return info;
}

void refreshGitHubCache() {
    xTaskCreate(
        [](void*) {
            fetchGitHubRelease();
            vTaskDelete(NULL);
        },
        "ota_check",
        16384,
        NULL,
        1,
        NULL
    );
}

bool validateRollback() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) return false;

    esp_ota_img_states_t otaState;
    if (esp_ota_get_state_partition(running, &otaState) != ESP_OK) return false;

    if (otaState == ESP_OTA_IMG_PENDING_VERIFY) {
        Serial.println("[OTA] Post-update boot detected, validating...");
        esp_ota_mark_app_valid_cancel_rollback();
        Serial.println("[OTA] Firmware validated — rollback cancelled");
        return true;
    }

    return false;
}

void begin(AsyncWebServer* server, StatusCallback onStatus) {
    statusCallback = onStatus;

    server->on(
        "/api/ota/upload",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {
            bool hasError = Update.hasError();
            AsyncWebServerResponse *response = request->beginResponse(
                hasError ? 500 : 200,
                "application/json",
                hasError
                    ? String("{\"error\":\"") + Update.errorString() + "\"}"
                    : "{\"success\":true}"
            );
            response->addHeader("Connection", "close");
            request->send(response);

            if (!hasError) {
                scheduleReboot();
            } else {
                currentStatus = Status::Idle;
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index,
           uint8_t *data, size_t len, bool final) {
            if (index == 0) {
                if (currentStatus != Status::Idle) {
                    Serial.println("[OTA] Upload rejected: OTA already in progress");
                    return;
                }
                currentStatus = Status::Uploading;

                Serial.printf("[OTA] Upload Start: %s\n", filename.c_str());
                emitStatus(Status::Uploading, 0);

                if (request->hasHeader("X-Firmware-MD5")) {
                    String md5 = request->header("X-Firmware-MD5");
                    if (!Update.setMD5(md5.c_str())) {
                        Serial.println("[OTA] Invalid MD5");
                    } else {
                        Serial.printf("[OTA] MD5: %s\n", md5.c_str());
                    }
                }

                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                    Update.printError(Serial);
                    emitStatus(Status::Error, -1, Update.errorString());
                    currentStatus = Status::Idle;
                }
            }

            if (currentStatus != Status::Uploading) return;

            if (index == 0 && len > 0 && data[0] != ESP_IMAGE_MAGIC) {
                Serial.printf("[OTA] Bad magic: 0x%02X\n", data[0]);
                Update.abort();
                emitStatus(Status::Error, -1, "Invalid firmware image");
                currentStatus = Status::Idle;
                return;
            }

            if (len && !Update.hasError()) {
                if (Update.write(data, len) != len) {
                    Serial.println("[OTA] Write failed");
                    emitStatus(Status::Error, -1, "Flash write failed");
                    currentStatus = Status::Idle;
                }
            }

            if (final) {
                if (Update.end(true)) {
                    Serial.printf("[OTA] Success: %u bytes\n", index + len);
                    emitStatus(Status::Success, 100);
                } else {
                    Update.printError(Serial);
                    emitStatus(Status::Error, -1, Update.errorString());
                    currentStatus = Status::Idle;
                }
            }
        }
    );

    server->on("/api/ota/check", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!releaseCache.valid || millis() - releaseCache.lastCheck > CACHE_TTL) {
            refreshGitHubCache();
        }

        JsonDocument response;
        response["currentVersion"] = FIRMWARE_VERSION;
        response["chipModel"] = getChipModel();

        size_t partitionSize = getOtaPartitionSize();
        if (partitionSize > 0) {
            response["maxFirmwareSize"] = partitionSize;
        }

        if (releaseCache.valid) {
            response["latestVersion"] = releaseCache.latestVersion;
            response["releaseUrl"] = releaseCache.releaseUrl;
            response["publishedAt"] = releaseCache.publishedAt;
            if (releaseCache.downloadUrl.length() > 0) {
                response["downloadUrl"] = releaseCache.downloadUrl;
                response["size"] = releaseCache.size;

                if (partitionSize > 0 && releaseCache.size > 0) {
                    bool fits = (size_t)releaseCache.size <= partitionSize;
                    response["fits"] = fits;
                    if (!fits) {
                        response["sizeWarning"] = "Firmware is too large for this device's partition";
                    }
                }
            }
        }

        if (releaseCache.rateLimited) {
            response["rateLimited"] = true;
        }

        String out;
        serializeJson(response, out);
        request->send(200, "application/json", out);
    });

    AsyncCallbackJsonWebHandler* installHandler = new AsyncCallbackJsonWebHandler("/api/ota/install");
    installHandler->setMethod(HTTP_POST);
    installHandler->setMaxContentLength(1024);

    installHandler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        if (currentStatus != Status::Idle) {
            request->send(409, "application/json", "{\"error\":\"OTA already in progress\"}");
            return;
        }

        JsonObject obj = json.as<JsonObject>();

        if (!obj["url"].is<const char*>()) {
            request->send(400, "application/json", "{\"error\":\"Missing url\"}");
            return;
        }

        String url = obj["url"].as<const char*>();

        if (!isGitHubUrl(url)) {
            request->send(403, "application/json", "{\"error\":\"URL not allowed\"}");
            return;
        }

        String md5 = obj["md5"].is<const char*>() ? obj["md5"].as<const char*>() : "";

        currentStatus = Status::Downloading;
        request->send(202, "application/json", "{\"status\":\"downloading\"}");

        struct TaskParams {
            String url;
            String md5;
        };

        TaskParams* params = new TaskParams;
        params->url = url;
        params->md5 = md5;

        xTaskCreate(
            [](void* p) {
                TaskParams* params = (TaskParams*)p;

                bool success = downloadAndFlash(params->url, params->md5);
                delete params;

                if (success) {
                    scheduleReboot();
                }
                vTaskDelete(NULL);
            },
            "ota_download",
            16384,
            params,
            1,
            NULL
        );
    });

    server->addHandler(installHandler);
}

}
