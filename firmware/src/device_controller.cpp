#include "device_controller.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

namespace DeviceController {

static constexpr int CONTROL_TIMEOUT_MS = 2000;
static constexpr int QUERY_TIMEOUT_MS = 1000;
static constexpr int CONNECT_TIMEOUT_MS = 1000;

static constexpr size_t JOB_QUEUE_SIZE = 8;
static constexpr size_t RESULT_QUEUE_SIZE = 8;
static constexpr size_t WORKER_STACK_SIZE = 4096;
static constexpr UBaseType_t WORKER_PRIORITY = 1;
static constexpr BaseType_t WORKER_CORE = 0;

namespace {

    struct Job {
        char method[16];
        char target[40];
        bool isControl;
        bool on;
    };

    QueueHandle_t jobQueue = nullptr;
    QueueHandle_t resultQueue = nullptr;
    TaskHandle_t workerTask = nullptr;
    ResultCallback resultCallback = nullptr;

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

    QueryResult doControl(const String& method, const String& target, bool on) {
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

    QueryResult doQuery(const String& method, const String& target) {
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

    void workerTaskFn(void* param) {
        Job job;
        for (;;) {
            if (xQueueReceive(jobQueue, &job, portMAX_DELAY) == pdTRUE) {
                QueryResult qr;
                if (job.isControl) {
                    qr = doControl(String(job.method), String(job.target), job.on);
                } else {
                    qr = doQuery(String(job.method), String(job.target));
                }

                AsyncResult ar = {};
                strlcpy(ar.method, job.method, sizeof(ar.method));
                strlcpy(ar.target, job.target, sizeof(ar.target));
                ar.result = qr;
                ar.wasControl = job.isControl;
                ar.requestedState = job.on;

                xQueueSend(resultQueue, &ar, pdMS_TO_TICKS(100));
            }
        }
    }
}

void init() {
    if (!jobQueue) {
        jobQueue = xQueueCreate(JOB_QUEUE_SIZE, sizeof(Job));
    }
    if (!resultQueue) {
        resultQueue = xQueueCreate(RESULT_QUEUE_SIZE, sizeof(AsyncResult));
    }
    if (!workerTask) {
        xTaskCreatePinnedToCore(
            workerTaskFn, "devCtrl", WORKER_STACK_SIZE,
            nullptr, WORKER_PRIORITY, &workerTask, WORKER_CORE);
    }
    Serial.println("[DeviceCtrl] Initialized");
}

void loop() {
    AsyncResult ar;
    while (xQueueReceive(resultQueue, &ar, 0) == pdTRUE) {
        if (resultCallback) {
            resultCallback(ar);
        }
    }
}

void onResult(ResultCallback cb) {
    resultCallback = cb;
}

bool controlAsync(const String& method, const String& target, bool on) {
    if (!jobQueue) return false;
    Job job = {};
    strlcpy(job.method, method.c_str(), sizeof(job.method));
    strlcpy(job.target, target.c_str(), sizeof(job.target));
    job.isControl = true;
    job.on = on;
    return xQueueSend(jobQueue, &job, 0) == pdTRUE;
}

bool queryAsync(const String& method, const String& target) {
    if (!jobQueue) return false;
    Job job = {};
    strlcpy(job.method, method.c_str(), sizeof(job.method));
    strlcpy(job.target, target.c_str(), sizeof(job.target));
    job.isControl = false;
    job.on = false;
    return xQueueSend(jobQueue, &job, 0) == pdTRUE;
}

bool busy() {
    if (!jobQueue) return false;
    return uxQueueMessagesWaiting(jobQueue) > 0;
}

}
