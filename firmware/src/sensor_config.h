#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace SensorConfig {

struct Sensor {
    char id[24];
    char name[32];
    char type[20];
    char unit[8];
    char hardwareType[20];
    char address[12];
    char tempSourceId[24];
    char humSourceId[24];
};

void init();

bool addSensor(JsonDocument& doc);
bool updateSensor(const char* sensorId, JsonDocument& doc);
bool removeSensor(const char* sensorId);

void getSensorsJson(String& out);
Sensor* getSensor(const char* sensorId);
size_t getSensorCount();
const char** getSensorIds(size_t& count);

}
