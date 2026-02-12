#pragma once

#include <Arduino.h>

namespace Sensors {

bool init(int sda = -1, int scl = -1);
void read();
float getSensorValue(const char* sensorId);
bool isHardwareConnected(const char* hardwareType);
bool hasAnySensor();

float getPpfdCalibrationFactor();
void setPpfdCalibrationFactor(float factor);
float getRawPpfd();

}  // namespace Sensors
