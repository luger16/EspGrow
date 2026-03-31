#pragma once

#include <Arduino.h>

namespace Sensors {

bool init(int sda = -1, int scl = -1);
void read();
float getSensorValue(const char* sensorId);

float getPpfdCalibrationFactor();
void setPpfdCalibrationFactor(float factor);
float getRawPpfd();
bool getSpectralChannels(uint16_t* out, size_t len);

}  // namespace Sensors
