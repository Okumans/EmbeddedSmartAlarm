#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <BH1750.h>
#include <TCA9548A.h>

class SensorManager {
 public:
  // Constructor
  SensorManager(uint8_t bh1750Address = 0x23);

  // Initialization
  void begin(TCA9548A* tcaMultiplexer = nullptr, bool quiet = false);

  // Sensor reading
  void readSensors();

  // Getters for sensor data
  float getLightIntensity() const;

  // Data validation
  bool isLightValid() const;
  bool hasValidData() const;

  // Timing
  unsigned long getLastReadTime() const;

  // MQTT Publishing
  void publishToMQTT(class MQTTManager& mqtt, const char* lightTopic);

 private:
  BH1750 lightSensor;
  TCA9548A* tca;

  bool isQuiet;

  // Sensor readings
  float currentLightIntensity;

  // State tracking
  unsigned long lastReadTime;
  bool lightValid;
};

#endif  // SENSOR_MANAGER_H
