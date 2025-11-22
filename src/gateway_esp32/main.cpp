#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <soc/rtc_cntl_reg.h>
#include <soc/soc.h>

#include "../../include/gateway_esp32/audio_manager.h"
#include "../../include/gateway_esp32/display_manager.h"
#include "../../include/gateway_esp32/mqtt_manager.h"
#include "../../include/gateway_esp32/mqtt_setup.h"
#include "../../include/gateway_esp32/rtos_tasks.h"
#include "../../include/gateway_esp32/sd_manager.h"
#include "../../include/gateway_esp32/sensor_manager.h"
#include "../../include/gateway_esp32/wifi_espnow_manager.h"
#include "../../include/shared/sensor_data.h"

// ============================================================================
// Configuration
// ============================================================================

// Centralized configuration
#include "../../include/shared/config.h"
#include "../../include/shared/mqtt_topic_config.h"

// Pin Definitions
#define DHTPIN 4
#define DHTTYPE DHT22
#define I2S_DOUT 27
#define I2S_BCLK 26
#define I2S_LRC 25
#define SDA_PIN 21
#define SCL_PIN 22

// I2C Multiplexer Channels
// BMP/pressure sensor removed from gateway for now
#define TCA_CHANNEL_OLED 1

// Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

// Timing Configuration
#define SENSOR_READ_INTERVAL 2000
#define MQTT_PUBLISH_INTERVAL 10000

// ============================================================================
// Global Objects
// ============================================================================

SensorManager localSensors(0x23);  // BH1750 I2C address
TCA9548A tca;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
MQTTManager mqtt;
AudioManager audio;
SDManager sdManager;
DisplayManager displayManager;

// ============================================================================
// Global Variables
// ============================================================================

// Remote sensor data (from NodeMCU via ESP-NOW)
SensorData remoteSensorData;
bool remoteSensorDataAvailable = false;
unsigned long lastRemoteDataReceived = 0;

// ============================================================================
// Function Declarations
// ============================================================================

// ============================================================================
// Arduino Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n========================================");
  Serial.println("Smart Alarm Clock - Starting");
  Serial.println("========================================\n");

  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize I2C Multiplexer
  tca.begin();
  Serial.println("[System] TCA9548A initialized");

  // Initialize components
  localSensors.begin(&tca, true);
  displayManager.begin(&tca);
  displayManager.showStartup();

  // Initialize SD card first
  if (!sdManager.begin(5)) {
    Serial.println("[System] SD Manager initialization failed!");
  }

  // Initialize audio system
  audio.begin();
  audio.setSDManager(&sdManager);
  audio.setMQTTManager(&mqtt);

  // Set display manager dependencies
  displayManager.setSensorManager(&localSensors);
  displayManager.setSDManager(&sdManager);
  displayManager.setAudioManager(&audio);
  displayManager.setRemoteSensorData(&remoteSensorData);

  // Setup WiFi and MQTT
  setupWiFi();
  setupMQTT();

  // Setup ESP-NOW (after WiFi for channel sync)
  setupESPNow();

  Serial.println("\n[System] Setup complete!\n");

  // Initialize and start FreeRTOS tasks
  initRTOSTasks();
  startRTOSTasks();

  Serial.println("[System] FreeRTOS tasks running!");
  Serial.println(
      "[System] Arduino loop() will be used for WiFi maintenance only\n");
}

// ============================================================================
// Arduino Loop
// ============================================================================

void loop() {
  // WiFi maintenance (everything else runs in FreeRTOS tasks)
  maintainWiFi();

  // Small delay to prevent watchdog issues
  vTaskDelay(pdMS_TO_TICKS(100));
}
