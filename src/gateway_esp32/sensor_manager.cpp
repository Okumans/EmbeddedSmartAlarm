#include "../../include/sensor_manager.h"

#include "../../include/mqtt_manager.h"

SensorManager::SensorManager(uint8_t dhtPin, uint8_t dhtType)
    : dht(dhtPin, dhtType),
      tca(nullptr),
      currentTemp(0.0),
      currentHumidity(0.0),
      lastReadTime(0),
      temperatureValid(false),
      humidityValid(false) {}

void SensorManager::begin(TCA9548A* tcaMultiplexer) {
  Serial.println("[SensorManager] Initializing...");

  tca = tcaMultiplexer;

  // Initialize DHT22
  dht.begin();
  Serial.println("[SensorManager] DHT22 initialized");

  // Initial read
  readSensors();

  Serial.println("[SensorManager] Ready");
}

void SensorManager::readSensors() {
  // Read DHT22
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // Validate and update temperature
  if (!isnan(temp) && temp > -40 && temp < 80) {
    currentTemp = temp;
    temperatureValid = true;
  } else {
    temperatureValid = false;
    Serial.println("[SensorManager] Invalid temperature reading");
  }

  // Validate and update humidity
  if (!isnan(hum) && hum >= 0 && hum <= 100) {
    currentHumidity = hum;
    humidityValid = true;
  } else {
    humidityValid = false;
    Serial.println("[SensorManager] Invalid humidity reading");
  }

  lastReadTime = millis();

  // Log readings
  if (hasValidData()) {
    Serial.printf("[SensorManager] T=%.1f°C, H=%.1f%%\n", currentTemp,
                  currentHumidity);
  }
}

float SensorManager::getTemperature() const { return currentTemp; }

float SensorManager::getHumidity() const { return currentHumidity; }

bool SensorManager::isTemperatureValid() const { return temperatureValid; }

bool SensorManager::isHumidityValid() const { return humidityValid; }

bool SensorManager::hasValidData() const {
  return temperatureValid && humidityValid;
}

unsigned long SensorManager::getLastReadTime() const { return lastReadTime; }

void SensorManager::publishToMQTT(MQTTManager& mqtt, const char* tempTopic,
                                  const char* humTopic) {
  if (!mqtt.isConnected() || !hasValidData()) {
    return;
  }

  char tempStr[10];
  char humStr[10];
  dtostrf(currentTemp, 4, 1, tempStr);
  dtostrf(currentHumidity, 4, 1, humStr);

  mqtt.publish(tempTopic, tempStr);
  mqtt.publish(humTopic, humStr);

  Serial.println("[SensorManager] Data published to MQTT");
}
