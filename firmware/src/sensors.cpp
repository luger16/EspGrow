#include "sensors.h"
#include <Wire.h>
#include <SensirionI2cSht4x.h>
#include <SensirionI2CScd4x.h>

#define SOIL_SENSOR_PIN 1
#define SOIL_DRY_VALUE 2800
#define SOIL_WET_VALUE 1400

namespace {
    SensirionI2cSht4x sht41;
    SensirionI2CScd4x scd40;
    
    bool sht41Found = false;
    bool scd40Found = false;
    
    float calculateVPD(float tempC, float rhPercent) {
        float svp = 0.6108 * exp((17.27 * tempC) / (tempC + 237.3));
        float avp = svp * (rhPercent / 100.0);
        return svp - avp;
    }
    
    bool readSoilMoisture(float& moisture) {
        int raw = analogRead(SOIL_SENSOR_PIN);
        
        // ADC returns 0 if pin not connected or failed read
        if (raw == 0) {
            return false;
        }
        
        int clamped = constrain(raw, SOIL_WET_VALUE, SOIL_DRY_VALUE);
        moisture = map(clamped, SOIL_DRY_VALUE, SOIL_WET_VALUE, 0, 100);
        return true;
    }
}

namespace Sensors {

bool init(int sda, int scl) {
    if (sda >= 0 && scl >= 0) {
        Wire.begin(sda, scl);
    } else {
        Wire.begin();
    }
    
    sht41.begin(Wire, SHT40_I2C_ADDR_44);
    uint32_t serialNum = 0;
    if (sht41.serialNumber(serialNum) == 0) {
        sht41Found = true;
        Serial.printf("[Sensors] SHT41 found (serial: %u)\n", serialNum);
    } else {
        Serial.println("[Sensors] SHT41 not found");
    }
    
    scd40.begin(Wire);
    scd40.stopPeriodicMeasurement();
    delay(500);
    
    uint16_t serial0, serial1, serial2;
    if (scd40.getSerialNumber(serial0, serial1, serial2) == 0) {
        scd40Found = true;
        Serial.printf("[Sensors] SCD40 found (serial: %04X%04X%04X)\n", serial0, serial1, serial2);
        scd40.startPeriodicMeasurement();
    } else {
        Serial.println("[Sensors] SCD40 not found");
    }
    
    return sht41Found || scd40Found;
}

SensorData read() {
    SensorData data = {0, 0, 0, 0, 0, false};
    
    if (sht41Found) {
        float temp, hum;
        if (sht41.measureHighPrecision(temp, hum) == 0) {
            data.temperature = temp;
            data.humidity = hum;
            data.valid = true;
        }
    }
    
    if (scd40Found) {
        uint16_t co2;
        float temp, hum;
        bool isReady = false;
        
        scd40.getDataReadyFlag(isReady);
        if (isReady && scd40.readMeasurement(co2, temp, hum) == 0) {
            data.co2 = co2;
            
            if (!sht41Found) {
                data.temperature = temp;
                data.humidity = hum;
            }
            data.valid = true;
        }
    }
    
    if (data.valid && data.humidity > 0) {
        data.vpd = calculateVPD(data.temperature, data.humidity);
    }
    
    float soilMoisture;
    if (readSoilMoisture(soilMoisture)) {
        data.soilMoisture = soilMoisture;
        data.valid = true;
    }
    
    return data;
}

bool hasSHT41() { return sht41Found; }
bool hasSCD40() { return scd40Found; }

}
