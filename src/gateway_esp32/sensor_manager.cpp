#include "../../include/sensor_manager.h"

#include "../../include/mqtt_manager.h"

SensorManager::SensorManager(uint8_t bh1750Address)
    : lightSensor(bh1750Address),
      tca(nullptr),
      currentLightIntensity(0.0),
      lastReadTime(0),
      lightValid(false),
      isQuiet(false) {}

void SensorManager::begin(TCA9548A* tcaMultiplexer, bool quiet) {
  Serial.println("[SensorManager] Initializing...");

  isQuiet = quiet;
  tca = tcaMultiplexer;

  // Initialize BH1750 via TCA9548A if multiplexer is provided
  bool sensorInit = false;
  if (tca) {
    // Try to initialize on different TCA channels (0-7)
    for (uint8_t channel = 0; channel < 8; channel++) {
      tca->openChannel(channel);
      delay(10);  // Small delay for channel switching

      if (lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
        Serial.printf("[SensorManager] BH1750 initialized on TCA channel %d\n",
                      channel);
        sensorInit = true;
        break;
      }
    }
    if (!sensorInit) {
      Serial.println(
          "[SensorManager] Failed to initialize BH1750 on any TCA channel");
      // Direct I2C initialization
      sensorInit = lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
      if (sensorInit) {
        Serial.println("[SensorManager] BH1750 initialized (direct I2C)");
      } else {
        Serial.println("[SensorManager] Failed to initialize BH1750");
      }
    }

    // Initial read
    readSensors();

    Serial.println("[SensorManager] Ready");
  }
}

void SensorManager::readSensors() {
  // Read BH1750 light sensor
  float lux = lightSensor.readLightLevel();

  // Validate and update light intensity
  if (!isnan(lux) && lux >= 0 && lux <= 65535) {  // BH1750 range: 0-65535 lux
    currentLightIntensity = lux;
    lightValid = true;
  } else {
    lightValid = false;
    Serial.println("[SensorManager] Invalid light intensity reading");
  }

  lastReadTime = millis();

  // Log readings
  if (hasValidData() && !isQuiet) {
    Serial.printf("[SensorManager] Light=%.1f lux\n", currentLightIntensity);
  }
}

float SensorManager::getLightIntensity() const { return currentLightIntensity; }

bool SensorManager::isLightValid() const { return lightValid; }

bool SensorManager::hasValidData() const { return lightValid; }

unsigned long SensorManager::getLastReadTime() const { return lastReadTime; }

void SensorManager::publishToMQTT(MQTTManager& mqtt, const char* lightTopic) {
  if (!mqtt.isConnected() || !hasValidData()) {
    return;
  }

  char lightStr[10];
  dtostrf(currentLightIntensity, 6, 1, lightStr);

  mqtt.publish(lightTopic, lightStr);

  if (!isQuiet) {
    Serial.println("[SensorManager] Light data published to MQTT");
  }
}
