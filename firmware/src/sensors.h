#pragma once

#include <Arduino.h>

namespace Sensors {

// Initialize I2C bus and scan all supported hardware
// sda/scl: I2C pins (default: -1 uses board defaults)
bool init(int sda = -1, int scl = -1);

// Read all detected hardware and cache latest values
void read();

// Get a sensor value by sensor ID (looks up SensorConfig for hardwareType + type)
// Returns NAN if hardware not connected or sensor type not available
float getSensorValue(const char* sensorId);

// Check if a specific hardware type is connected
bool isHardwareConnected(const char* hardwareType);

// Returns true if any hardware was detected during init
bool hasAnySensor();

}  // namespace Sensors
