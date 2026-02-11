#pragma once

#include <Arduino.h>

namespace History {

enum Range {
    RANGE_12H = 0,
    RANGE_24H = 1,
    RANGE_7D = 2
};

struct HistoryPoint {
    uint32_t timestamp;
    float value;
};

constexpr size_t POINTS_12H = 144;
constexpr size_t POINTS_24H = 144;
constexpr size_t POINTS_7D = 168;

constexpr uint32_t INTERVAL_12H = 5 * 60;
constexpr uint32_t INTERVAL_24H = 10 * 60;
constexpr uint32_t INTERVAL_7D = 60 * 60;

void init();
void loop();

void record(const char* sensorId, float value);
void removeSensor(const char* sensorId);

size_t getHistory(const char* sensorId, Range range, uint8_t* buffer, size_t bufferSize);
size_t getPointCount(Range range);

}
