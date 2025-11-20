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

#include "../../include/display_manager.h"
#include "../../include/mqtt_manager.h"
#include "../../include/rtos_tasks.h"
#include "../../include/sd_manager.h"
#include "../../include/sensor_data.h"
#include "../../include/sensor_manager.h"
#include "audio_manager.h"

// ============================================================================
// Configuration
// ============================================================================

// Centralized configuration
#include "../../include/config.h"

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
#define SENSOR_READ_INTERVAL 2000    // 2 seconds
#define MQTT_PUBLISH_INTERVAL 10000  // 10 seconds

// MQTT Topics - Local Sensors (ESP32) - explicit inside topics
const char* MQTT_TOPIC_GATEWAY_TEMP = "smartalarm/gateway/temperature/inside";
const char* MQTT_TOPIC_GATEWAY_HUMIDITY = "smartalarm/gateway/humidity/inside";
const char* MQTT_TOPIC_STATUS = "smartalarm/gateway/status";

// MQTT Topics - Remote Sensors (from NodeMCU via ESP-NOW) - explicit outside
// topics
const char* MQTT_TOPIC_REMOTE_TEMP = "smartalarm/sensor/temperature/outside";
const char* MQTT_TOPIC_REMOTE_HUMIDITY = "smartalarm/sensor/humidity/outside";
const char* MQTT_TOPIC_REMOTE_PRESSURE = "smartalarm/sensor/pressure/outside";
const char* MQTT_TOPIC_REMOTE_UV = "smartalarm/sensor/uvindex/outside";
const char* MQTT_TOPIC_REMOTE_BATTERY = "smartalarm/sensor/battery/outside";
const char* MQTT_TOPIC_REMOTE_STATUS = "smartalarm/sensor/status";

// ============================================================================
// Global Objects
// ============================================================================

SensorManager localSensors(DHTPIN, DHTTYPE);
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

void setupWiFi();
void setupMQTT();
void setupMQTTHandlers();
void setupESPNow();
void onESPNowDataReceived(const uint8_t* mac_addr, const uint8_t* data,
                          int data_len);
void publishRemoteSensorData();
void updateDisplay();

// ============================================================================
// WiFi Setup
// ============================================================================

void setupWiFi() {
  Serial.println("\n[WiFi] Configuring WiFi...");

  WiFi.mode(WIFI_AP_STA);

  Serial.println(
      "[WiFi] Connecting to WiFi network first to detect channel...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  uint8_t wifiChannel = WIFI_CHANNEL;  // Default

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] ✓ Connected to WiFi!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] MAC Address: ");
    Serial.println(WiFi.macAddress());

    // Get the actual WiFi channel
    uint8_t primary;
    wifi_second_chan_t secondary;
    esp_wifi_get_channel(&primary, &secondary);
    wifiChannel = primary;
    Serial.printf("[WiFi] WiFi Channel: %d\n", wifiChannel);

    // Warning if channel mismatch with expected
    if (wifiChannel != WIFI_CHANNEL) {
      Serial.println("\n⚠ INFO: WiFi channel differs from default");
      Serial.printf("⚠ Using actual channel %d for Soft AP\n", wifiChannel);
      Serial.printf("⚠ Update sensor node WIFI_CHANNEL to %d\n\n", wifiChannel);
    }
  } else {
    Serial.println("\n[WiFi] ✗ WiFi connection failed!");
    Serial.printf("[WiFi] Using default channel %d for Soft AP\n",
                  WIFI_CHANNEL);
  }

  // Now create Soft Access Point on the SAME channel as WiFi
  Serial.println("\n[WiFi] Creating Soft Access Point...");
  WiFi.softAP(SOFT_AP_SSID, SOFT_AP_PASSWORD, wifiChannel, 0);

  IPAddress apIP = WiFi.softAPIP();

  Serial.print("[WiFi] ✓ Soft AP Created: ");
  Serial.println(SOFT_AP_SSID);
  Serial.print("[WiFi] AP IP Address: ");
  Serial.println(apIP);
  Serial.printf("[WiFi] AP Channel: %d\n", wifiChannel);

  // Print BOTH MAC addresses - critical for ESP-NOW!
  Serial.println("\n========================================");
  Serial.println("IMPORTANT: MAC Addresses for ESP-NOW");
  Serial.println("========================================");
  Serial.print("Station MAC (WiFi): ");
  Serial.println(WiFi.macAddress());
  Serial.print("AP MAC (Soft AP):   ");
  Serial.println(WiFi.softAPmacAddress());
  Serial.println("\n*** Use the AP MAC for sensor node! ***");
  Serial.printf("*** Configure sensor node to use channel %d ***\n",
                wifiChannel);
  Serial.println("========================================\n");
}

// ============================================================================
// ESP-NOW Setup
// ============================================================================

void onESPNowDataReceived(const uint8_t* mac_addr, const uint8_t* data,
                          int data_len) {
  Serial.println("\n[ESP-NOW] ← Data received!");
  Serial.print("[ESP-NOW] From MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.printf(" | Size: %d bytes\n", data_len);

  if (data_len == sizeof(SensorData)) {
    memcpy(&remoteSensorData, data, sizeof(SensorData));
    remoteSensorDataAvailable = true;
    lastRemoteDataReceived = millis();

    // Display received data
    Serial.println("╔═══════════════════════════════════════╗");
    Serial.printf(" ║  Device: %-24s ║\n", remoteSensorData.deviceName);
    Serial.println("╠═══════════════════════════════════════╣");
    Serial.printf(" ║  Temperature: %6.2f °C            ║\n",
                  remoteSensorData.temperature);
    Serial.printf(" ║  Humidity:    %6.2f %%             ║\n",
                  remoteSensorData.humidity);
    Serial.printf(" ║  Pressure:    %7.2f hPa          ║\n",
                  remoteSensorData.pressure);
    Serial.printf(" ║  UV Index:    %6.2f                ║\n",
                  remoteSensorData.uvIndex);
    Serial.printf(" ║  Battery:     %3d %%                ║\n",
                  remoteSensorData.batteryLevel);
    Serial.println("╚═══════════════════════════════════════╝");

    // Immediately publish to MQTT
    publishRemoteSensorData();
  } else {
    Serial.printf("[ESP-NOW] ✗ Invalid data size! Expected %d, got %d\n",
                  sizeof(SensorData), data_len);
  }
}

void setupESPNow() {
  Serial.println("\n[ESP-NOW] Initializing...");

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] ✗ Initialization failed!");
    return;
  }

  Serial.println("[ESP-NOW] ✓ Initialized successfully");

  // Register receive callback
  esp_now_register_recv_cb(onESPNowDataReceived);

  Serial.printf("[DEBUG] SensorData struct size: %d bytes\n",
                sizeof(SensorData));
  Serial.println("[ESP-NOW] ✓ Ready to receive data from sensor nodes");
}

// ============================================================================
// MQTT Setup
// ============================================================================

void setupMQTT() {
  wifiClient.setTimeout(3000);  // 3 second timeout for MQTT connections
  mqttClient.setBufferSize(4200);
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  mqtt.begin(&mqttClient, MQTT_CLIENT_ID, MQTT_TOPIC_STATUS);
  Serial.println("[MQTT] Client configured with 4200 byte buffer");

  setupMQTTHandlers();
}

void setupMQTTHandlers() {
  Serial.println("\n[MQTT] Registering message handlers...");

  // =======================================================================
  // AUDIO HANDLERS - High Priority (150)
  // =======================================================================

  // Audio playback command
  mqtt.registerHandler(
      "smartalarm/play_audio",
      [](MQTTManager& mqtt, const char* topic, byte* payload,
         unsigned int length) -> bool {
        String filename((char*)payload, length);
        if (!filename.startsWith("/")) {
          filename = "/" + filename;
        }

        bool success = audio.playFile(filename.c_str());
        mqtt.publish("smartalarm/audio/status", success ? "playing" : "error");

        return true;
      },
      "AudioPlayback", 150);

  // =======================================================================
  // SYSTEM COMMANDS - Normal Priority (100)
  // =======================================================================

  mqtt.registerHandler(
      "smartalarm/commands",
      [](MQTTManager& mqtt, const char* topic, byte* payload,
         unsigned int length) -> bool {
        String message((char*)payload, length);
        message.toLowerCase();

        if (message == "stop_audio") {
          audio.stop();
          mqtt.publish("smartalarm/status", "audio_stopped");
          return true;
        } else if (message == "list_files") {
          String fileList = audio.getFileList();
          if (fileList.length() > 0) {
            mqtt.publish("smartalarm/files", fileList);
            mqtt.publish("smartalarm/status", "files_listed");
          } else {
            mqtt.publish("smartalarm/status", "no_files");
          }
          return true;
        } else if (message.startsWith("volume=")) {
          float vol = message.substring(7).toFloat();
          audio.setVolume(vol);
          mqtt.publish("smartalarm/status", "volume:" + String(vol, 2));
          return true;
        } else if (message.startsWith("play:")) {
          String filename = message.substring(5);
          if (!filename.startsWith("/")) {
            filename = "/" + filename;
          }
          bool success = audio.playFile(filename.c_str());
          mqtt.publish("smartalarm/status", success ? "playing" : "error");
          return true;
        } else if (message == "status") {
          String status = "online|audio:";
          if (audio.playing()) {
            status += "playing";
          } else {
            status += "stopped";
          }
          status += "|volume:" + String(audio.getVolume(), 2);
          status += "|wifi:" + String(WiFi.RSSI()) + "dBm";
          mqtt.publish("smartalarm/status", status);
          return true;
        }

        return false;  // Not handled by this handler
      },
      "SystemCommands", 100);

  // Register AudioManager's own handlers
  audio.registerMQTTHandlers(mqtt);

  Serial.println("[MQTT] Handler registration complete\n");
}

// ============================================================================
// Publish Remote Sensor Data to MQTT
// ============================================================================

void publishRemoteSensorData() {
  if (!mqtt.isConnected() || !remoteSensorDataAvailable) {
    return;
  }

  char tempStr[10];
  char humStr[10];
  char pressStr[10];
  char uvStr[10];
  char battStr[5];

  dtostrf(remoteSensorData.temperature, 6, 2, tempStr);
  dtostrf(remoteSensorData.humidity, 6, 2, humStr);
  dtostrf(remoteSensorData.pressure, 7, 2, pressStr);
  dtostrf(remoteSensorData.uvIndex, 5, 2, uvStr);
  snprintf(battStr, sizeof(battStr), "%d", remoteSensorData.batteryLevel);

  mqtt.publish(MQTT_TOPIC_REMOTE_TEMP, tempStr);
  mqtt.publish(MQTT_TOPIC_REMOTE_HUMIDITY, humStr);
  mqtt.publish(MQTT_TOPIC_REMOTE_PRESSURE, pressStr);
  mqtt.publish(MQTT_TOPIC_REMOTE_UV, uvStr);
  mqtt.publish(MQTT_TOPIC_REMOTE_BATTERY, battStr);

  // Publish status with device name
  String statusMsg = String(remoteSensorData.deviceName) + " online";
  mqtt.publish(MQTT_TOPIC_REMOTE_STATUS, statusMsg);

  Serial.println("[MQTT] → Remote sensor data forwarded to MQTT broker");
}

// ============================================================================
// Arduino Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Disable brownout detector to prevent crashes with insufficient power
  // WARNING: Use a proper 5V/2A power supply in production!
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.println("[System] Brownout detector disabled");

  Serial.println("\n\n========================================");
  Serial.println("Smart Alarm Clock - Starting");
  Serial.println("========================================\n");

  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize I2C Multiplexer
  tca.begin();
  Serial.println("[System] TCA9548A initialized");

  // Initialize components
  localSensors.begin(&tca);
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
  Serial.println("========================================");
  Serial.println("Waiting for sensor data via ESP-NOW...");
  Serial.println("========================================\n");

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
  // WiFi reconnection check (everything else runs in FreeRTOS tasks)
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection lost, reconnecting...");
    setupWiFi();
  }

  // Small delay to prevent watchdog issues
  vTaskDelay(pdMS_TO_TICKS(100));
}
