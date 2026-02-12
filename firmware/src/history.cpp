#include "history.h"
#include "sensor_config.h"
#include <LittleFS.h>
#include <map>
#include <string>
#include <time.h>

namespace History {

namespace {
    struct CircularBuffer {
        HistoryPoint* points;
        size_t capacity;
        uint32_t head;
        uint32_t count;
        uint32_t interval;
        uint32_t lastWrite;
    };
    
    struct SensorAccumulator {
        float sum;
        uint32_t sampleCount;
        uint32_t lastSample;
    };
    
    struct SensorHistory {
        CircularBuffer buffers[3];
        SensorAccumulator accumulators[3];
        bool initialized;
    };
    
    std::map<std::string, SensorHistory> histories;
    
    const char* HISTORY_DIR = "/history";
    
    size_t getCapacity(Range range) {
        switch (range) {
            case RANGE_12H: return POINTS_12H;
            case RANGE_24H: return POINTS_24H;
            case RANGE_7D: return POINTS_7D;
            default: return 0;
        }
    }
    
    uint32_t getInterval(Range range) {
        switch (range) {
            case RANGE_12H: return INTERVAL_12H;
            case RANGE_24H: return INTERVAL_24H;
            case RANGE_7D: return INTERVAL_7D;
            default: return 0;
        }
    }
    
    String getFilePath(const char* sensorId, Range range) {
        const char* suffix[] = {"12h", "24h", "7d"};
        return String(HISTORY_DIR) + "/" + sensorId + "_" + suffix[range] + ".bin";
    }
    
    void saveBuffer(const char* sensorId, Range range, CircularBuffer& buf) {
        String path = getFilePath(sensorId, range);
        File file = LittleFS.open(path, "w");
        if (!file) return;
        
        uint8_t header[12];
        memcpy(header, &buf.head, 4);
        memcpy(header + 4, &buf.count, 4);
        memcpy(header + 8, &buf.lastWrite, 4);
        file.write(header, 12);
        
        file.write((uint8_t*)buf.points, buf.capacity * sizeof(HistoryPoint));
        file.close();
    }
    
    bool loadBuffer(const char* sensorId, Range range, CircularBuffer& buf) {
        String path = getFilePath(sensorId, range);
        if (!LittleFS.exists(path)) return false;
        
        File file = LittleFS.open(path, "r");
        if (!file) return false;
        
        uint8_t header[12];
        if (file.read(header, 12) != 12) {
            file.close();
            return false;
        }
        
        memcpy(&buf.head, header, 4);
        memcpy(&buf.count, header + 4, 4);
        memcpy(&buf.lastWrite, header + 8, 4);
        
        size_t expectedSize = buf.capacity * sizeof(HistoryPoint);
        if (file.read((uint8_t*)buf.points, expectedSize) != expectedSize) {
            buf.head = 0;
            buf.count = 0;
            buf.lastWrite = 0;
            file.close();
            return false;
        }
        
        file.close();
        return true;
    }
    
    void initSensorHistory(const char* sensorId) {
        if (histories.find(sensorId) != histories.end()) return;
        
        SensorHistory& sh = histories[sensorId];
        
        sh.buffers[RANGE_12H].points = new HistoryPoint[POINTS_12H];
        sh.buffers[RANGE_12H].capacity = POINTS_12H;
        sh.buffers[RANGE_12H].interval = INTERVAL_12H;
        
        sh.buffers[RANGE_24H].points = new HistoryPoint[POINTS_24H];
        sh.buffers[RANGE_24H].capacity = POINTS_24H;
        sh.buffers[RANGE_24H].interval = INTERVAL_24H;
        
        sh.buffers[RANGE_7D].points = new HistoryPoint[POINTS_7D];
        sh.buffers[RANGE_7D].capacity = POINTS_7D;
        sh.buffers[RANGE_7D].interval = INTERVAL_7D;
        
        for (int i = 0; i < 3; i++) {
            sh.buffers[i].head = 0;
            sh.buffers[i].count = 0;
            sh.buffers[i].lastWrite = 0;
            
            sh.accumulators[i].sum = 0;
            sh.accumulators[i].sampleCount = 0;
            sh.accumulators[i].lastSample = 0;
            
            loadBuffer(sensorId, (Range)i, sh.buffers[i]);
        }
        
        sh.initialized = true;
        Serial.printf("[History] Initialized sensor: %s\n", sensorId);
    }
    
    void addPoint(CircularBuffer& buf, uint32_t timestamp, float value) {
        buf.points[buf.head].timestamp = timestamp;
        buf.points[buf.head].value = value;
        
        buf.head = (buf.head + 1) % (uint32_t)buf.capacity;
        if (buf.count < (uint32_t)buf.capacity) buf.count++;
        buf.lastWrite = timestamp;
    }
    
    unsigned long lastSaveTime = 0;
    const unsigned long SAVE_INTERVAL = 60000;
}

void init() {
    if (!LittleFS.exists(HISTORY_DIR)) {
        if (LittleFS.mkdir(HISTORY_DIR)) {
            Serial.println("[History] Created /history directory");
        } else {
            Serial.println("[History] Failed to create /history directory");
        }
    }
    
    size_t sensorCount;
    const char** sensorIds = SensorConfig::getSensorIds(sensorCount);
    
    for (size_t i = 0; i < sensorCount; i++) {
        initSensorHistory(sensorIds[i]);
    }
    
    Serial.println("[History] Initialized");
}

void loop() {
    if (millis() - lastSaveTime < SAVE_INTERVAL) return;
    lastSaveTime = millis();
    
    for (auto& pair : histories) {
        for (int i = 0; i < 3; i++) {
            if (pair.second.buffers[i].count > 0) {
                saveBuffer(pair.first.c_str(), (Range)i, pair.second.buffers[i]);
            }
        }
    }
}

void record(const char* sensorId, float value) {
    if (histories.find(sensorId) == histories.end()) {
        initSensorHistory(sensorId);
    }
    
    SensorHistory& sh = histories[sensorId];
    uint32_t now = (uint32_t)time(nullptr);
    
    for (int i = 0; i < 3; i++) {
        SensorAccumulator& acc = sh.accumulators[i];
        CircularBuffer& buf = sh.buffers[i];
        
        acc.sum += value;
        acc.sampleCount++;
        
        if (now - buf.lastWrite >= buf.interval) {
            if (acc.sampleCount > 0) {
                float avg = acc.sum / acc.sampleCount;
                addPoint(buf, now, avg);
                
                acc.sum = 0;
                acc.sampleCount = 0;
            }
        }
    }
}

size_t getHistory(const char* sensorId, Range range, uint8_t* buffer, size_t bufferSize) {
    auto it = histories.find(sensorId);
    if (it == histories.end()) return 0;
    
    CircularBuffer& buf = it->second.buffers[range];
    
    size_t pointSize = sizeof(HistoryPoint);
    size_t maxPoints = bufferSize / pointSize;
    size_t pointsToReturn = min((size_t)buf.count, maxPoints);
    
    if (pointsToReturn == 0) return 0;
    
    size_t startIdx;
    if (buf.count >= (uint32_t)buf.capacity) {
        startIdx = buf.head;
    } else {
        startIdx = 0;
    }
    
    uint8_t* ptr = buffer;
    for (size_t i = 0; i < pointsToReturn; i++) {
        size_t idx = (startIdx + i) % buf.capacity;
        memcpy(ptr, &buf.points[idx], pointSize);
        ptr += pointSize;
    }
    
    return pointsToReturn * pointSize;
}

size_t getPointCount(Range range) {
    return getCapacity(range);
}

void removeSensor(const char* sensorId) {
    auto it = histories.find(sensorId);
    if (it == histories.end()) return;
    
    for (int i = 0; i < 3; i++) {
        delete[] it->second.buffers[i].points;
        
        String path = getFilePath(sensorId, (Range)i);
        if (LittleFS.exists(path)) {
            LittleFS.remove(path);
        }
    }
    
    histories.erase(it);
    Serial.printf("[History] Removed sensor: %s\n", sensorId);
}

}
