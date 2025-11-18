#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <DHT.h>
#include <TCA9548A.h>

class SensorManager {
 public:
  // Constructor
  SensorManager(uint8_t dhtPin, uint8_t dhtType);

  // Initialization
  void begin(TCA9548A* tcaMultiplexer = nullptr);

  // Sensor reading
  void readSensors();

  // Getters for sensor data
  float getTemperature() const;
  float getHumidity() const;

  // Data validation
  bool isTemperatureValid() const;
  bool isHumidityValid() const;
  bool hasValidData() const;

  // Timing
  unsigned long getLastReadTime() const;

  // MQTT Publishing
  void publishToMQTT(class MQTTManager& mqtt, const char* tempTopic,
                     const char* humTopic);

 private:
  DHT dht;
  TCA9548A* tca;

  // Sensor readings
  float currentTemp;
  float currentHumidity;

  // State tracking
  unsigned long lastReadTime;
  bool temperatureValid;
  bool humidityValid;
};

#endif  // SENSOR_MANAGER_H
