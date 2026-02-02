#pragma once

#include <Arduino.h>

namespace Sensors {

struct SensorData {
    float temperature;  // Celsius (from SHT41, falls back to SCD40)
    float humidity;     // % RH (from SHT41, falls back to SCD40)
    uint16_t co2;       // ppm (from SCD40)
    float vpd;          // kPa (calculated)
    bool valid;         // true if at least one sensor read succeeded
};

// Initialize I2C and sensors
// sda/scl: I2C pins (default: -1 uses board defaults)
bool init(int sda = -1, int scl = -1);

// Read all sensors and return combined data
// Returns data with valid=false if all sensors failed
SensorData read();

// Individual sensor status
bool hasSHT41();
bool hasSCD40();

}  // namespace Sensors
