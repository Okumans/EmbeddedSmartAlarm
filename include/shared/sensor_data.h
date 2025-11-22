#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>

// Data packet structure sent from Sensor Node to Gateway
// Using pragma pack to ensure same size on ESP8266 and ESP32
#pragma pack(push, 1)
typedef struct {
  uint32_t timestamp;    // Timestamp in milliseconds (4 bytes)
  float temperature;     // Temperature in Celsius (4 bytes)
  float humidity;        // Humidity in percentage (4 bytes)
  float pressure;        // Air pressure in hPa (4 bytes)
  float uvIndex;         // UV index (0-11+) (4 bytes)
  uint8_t batteryLevel;  // Battery level (0-100%) (1 byte)
  uint8_t sensorId;      // Sensor node identifier (1 byte)
  char deviceName[16];   // Device name (e.g., "SensorNode01") (16 bytes)
} SensorData;            // Total: 38 bytes
#pragma pack(pop)

// Device identifiers
#define SENSOR_NODE_ID 1

// Device names
#define SENSOR_NODE_NAME "SensorNode01"

#endif  // SENSOR_DATA_H
