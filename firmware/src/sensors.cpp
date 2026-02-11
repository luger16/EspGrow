#include "sensors.h"
#include "sensor_config.h"
#include <Wire.h>
#include <SensirionI2cSht3x.h>
#include <SensirionI2cSht4x.h>
#include <SensirionI2CScd4x.h>
#include <Adafruit_AS7341.h>
#include <cmath>

namespace {

    struct TempHumReading {
        float temperature = NAN;
        float humidity = NAN;
        bool valid = false;
    };

    struct Co2Reading {
        float temperature = NAN;
        float humidity = NAN;
        uint16_t co2 = 0;
        bool valid = false;
    };

    struct LightReading {
        float ppfd = NAN;
        bool valid = false;
    };

    SensirionI2cSht3x sht3x;
    SensirionI2cSht4x sht4x;
    SensirionI2CScd4x scd4x;
    Adafruit_AS7341 as7341;

    bool sht3xFound = false;
    bool sht4xFound = false;
    bool scd4xFound = false;
    bool as7341Found = false;

    TempHumReading sht3xData;
    TempHumReading sht4xData;
    Co2Reading scd4xData;
    LightReading as7341Data;

    float calculateVPD(float tempC, float rhPercent) {
        float svp = 0.6108f * expf((17.27f * tempC) / (tempC + 237.3f));
        float avp = svp * (rhPercent / 100.0f);
        return svp - avp;
    }

    float getValueFromHardware(const char* hardwareType, const char* sensorType) {
        if (strcmp(hardwareType, "sht3x") == 0) {
            if (!sht3xFound || !sht3xData.valid) return NAN;
            if (strcmp(sensorType, "temperature") == 0) return sht3xData.temperature;
            if (strcmp(sensorType, "humidity") == 0) return sht3xData.humidity;
        }
        else if (strcmp(hardwareType, "sht4x") == 0) {
            if (!sht4xFound || !sht4xData.valid) return NAN;
            if (strcmp(sensorType, "temperature") == 0) return sht4xData.temperature;
            if (strcmp(sensorType, "humidity") == 0) return sht4xData.humidity;
        }
        else if (strcmp(hardwareType, "scd4x") == 0) {
            if (!scd4xFound || !scd4xData.valid) return NAN;
            if (strcmp(sensorType, "temperature") == 0) return scd4xData.temperature;
            if (strcmp(sensorType, "humidity") == 0) return scd4xData.humidity;
            if (strcmp(sensorType, "co2") == 0) return (float)scd4xData.co2;
        }
        else if (strcmp(hardwareType, "as7341") == 0) {
            if (!as7341Found || !as7341Data.valid) return NAN;
            if (strcmp(sensorType, "light") == 0) return as7341Data.ppfd;
        }
        return NAN;
    }
}

namespace Sensors {

bool init(int sda, int scl) {
    if (sda >= 0 && scl >= 0) {
        Wire.begin(sda, scl);
    } else {
        Wire.begin();
    }

    sht3x.begin(Wire, 0x44);
    uint16_t sht3xStatus = 0;
    if (sht3x.readStatusRegister(sht3xStatus) == 0) {
        sht3xFound = true;
        Serial.println("[Sensors] SHT3x found at 0x44");
    } else {
        Serial.println("[Sensors] SHT3x not found");
    }

    uint8_t sht4xAddr = sht3xFound ? 0x45 : 0x44;
    sht4x.begin(Wire, sht4xAddr);
    uint32_t sht4xSerial = 0;
    if (sht4x.serialNumber(sht4xSerial) == 0) {
        sht4xFound = true;
        Serial.printf("[Sensors] SHT4x found at 0x%02X (serial: %u)\n", sht4xAddr, sht4xSerial);
    } else {
        Serial.println("[Sensors] SHT4x not found");
    }

    scd4x.begin(Wire);
    scd4x.stopPeriodicMeasurement();
    delay(500);

    uint16_t serial0, serial1, serial2;
    if (scd4x.getSerialNumber(serial0, serial1, serial2) == 0) {
        scd4xFound = true;
        Serial.printf("[Sensors] SCD4x found (serial: %04X%04X%04X)\n", serial0, serial1, serial2);
        scd4x.startPeriodicMeasurement();
    } else {
        Serial.println("[Sensors] SCD4x not found");
    }

    if (as7341.begin()) {
        as7341Found = true;
        as7341.setATIME(100);
        as7341.setASTEP(999);
        as7341.setGain(AS7341_GAIN_256X);
        Serial.println("[Sensors] AS7341 found");
    } else {
        Serial.println("[Sensors] AS7341 not found");
    }

    return sht3xFound || sht4xFound || scd4xFound || as7341Found;
}

void read() {
    if (sht3xFound) {
        float temp, hum;
        if (sht3x.measureSingleShot(REPEATABILITY_HIGH, false, temp, hum) == 0) {
            sht3xData.temperature = temp;
            sht3xData.humidity = hum;
            sht3xData.valid = true;
        } else {
            sht3xData.valid = false;
        }
    }

    if (sht4xFound) {
        float temp, hum;
        if (sht4x.measureHighPrecision(temp, hum) == 0) {
            sht4xData.temperature = temp;
            sht4xData.humidity = hum;
            sht4xData.valid = true;
        } else {
            sht4xData.valid = false;
        }
    }

    if (scd4xFound) {
        bool isReady = false;
        scd4x.getDataReadyFlag(isReady);
        if (isReady) {
            uint16_t co2;
            float temp, hum;
            if (scd4x.readMeasurement(co2, temp, hum) == 0) {
                scd4xData.co2 = co2;
                scd4xData.temperature = temp;
                scd4xData.humidity = hum;
                scd4xData.valid = true;
            } else {
                scd4xData.valid = false;
            }
        }
    }

    if (as7341Found) {
        if (as7341.readAllChannels()) {
            uint16_t clear = as7341.getChannel(AS7341_CHANNEL_CLEAR);
            // clear-channel-to-PPFD approximation factor
            as7341Data.ppfd = clear * 0.05f;
            as7341Data.valid = true;
        } else {
            as7341Data.valid = false;
        }
    }
}

float getSensorValue(const char* sensorId) {
    SensorConfig::Sensor* cfg = SensorConfig::getSensor(sensorId);
    if (!cfg) return NAN;

    if (strcmp(cfg->hardwareType, "calculated") == 0) {
        if (strcmp(cfg->type, "vpd") == 0) {
            float temp = NAN;
            float hum = NAN;

            if (cfg->tempSourceId[0] != '\0') {
                temp = getSensorValue(cfg->tempSourceId);
            }
            if (cfg->humSourceId[0] != '\0') {
                hum = getSensorValue(cfg->humSourceId);
            }

            if (!isnan(temp) && !isnan(hum) && hum > 0) {
                return calculateVPD(temp, hum);
            }
            return NAN;
        }
        return NAN;
    }

    return getValueFromHardware(cfg->hardwareType, cfg->type);
}

bool isHardwareConnected(const char* hardwareType) {
    if (strcmp(hardwareType, "sht3x") == 0) return sht3xFound;
    if (strcmp(hardwareType, "sht4x") == 0) return sht4xFound;
    if (strcmp(hardwareType, "scd4x") == 0) return scd4xFound;
    if (strcmp(hardwareType, "as7341") == 0) return as7341Found;
    if (strcmp(hardwareType, "calculated") == 0) return true;
    return false;
}

bool hasAnySensor() {
    return sht3xFound || sht4xFound || scd4xFound || as7341Found;
}

}
