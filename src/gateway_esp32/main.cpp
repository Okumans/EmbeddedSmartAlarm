#include <Adafruit_BMP085.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <TCA9548A.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "../../include/sensor_data.h"
#include "audio_manager.h"

// ============================================================================
// Configuration
// ============================================================================

// WiFi Configuration
const char* WIFI_SSID = "Pleaseconnecttome";
const char* WIFI_PASSWORD = "n1234567!";

// Soft Access Point Configuration (for ESP-NOW communication)
const char* SOFT_AP_SSID = "SmartAlarm-Gateway";
const char* SOFT_AP_PASSWORD = "12345678";
#define WIFI_CHANNEL 6  // Fixed WiFi channel for ESP-NOW

// MQTT Configuration
const char* MQTT_SERVER = "broker.hivemq.com";  // Change to your MQTT broker
const int MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "SmartAlarmClock";

// MQTT Topics - Local Sensors (ESP32)
const char* MQTT_TOPIC_TEMP = "smartalarm/gateway/temperature";
const char* MQTT_TOPIC_HUMIDITY = "smartalarm/gateway/humidity";
const char* MQTT_TOPIC_PRESSURE = "smartalarm/gateway/pressure";
const char* MQTT_TOPIC_STATUS = "smartalarm/gateway/status";

// MQTT Topics - Remote Sensors (from NodeMCU via ESP-NOW)
const char* MQTT_TOPIC_REMOTE_TEMP = "smartalarm/sensor/temperature";
const char* MQTT_TOPIC_REMOTE_HUMIDITY = "smartalarm/sensor/humidity";
const char* MQTT_TOPIC_REMOTE_PRESSURE = "smartalarm/sensor/pressure";
const char* MQTT_TOPIC_REMOTE_UV = "smartalarm/sensor/uvindex";
const char* MQTT_TOPIC_REMOTE_BATTERY = "smartalarm/sensor/battery";
const char* MQTT_TOPIC_REMOTE_STATUS = "smartalarm/sensor/status";

// Pin Definitions
#define DHTPIN 4
#define DHTTYPE DHT22
#define I2S_DOUT 27
#define I2S_BCLK 26
#define I2S_LRC 25
#define SDA_PIN 21
#define SCL_PIN 22

// I2C Multiplexer Channels
#define TCA_CHANNEL_BMP 0
#define TCA_CHANNEL_OLED 1

// Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

// Timing Configuration
#define SENSOR_READ_INTERVAL 2000    // 2 seconds
#define MQTT_PUBLISH_INTERVAL 10000  // 10 seconds

// ============================================================================
// Global Objects
// ============================================================================

DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP085 bmp;
TCA9548A tca;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
AudioManager audio;

// ============================================================================
// Global Variables
// ============================================================================

unsigned long lastSensorRead = 0;
unsigned long lastMqttPublish = 0;
float currentTemp = 0.0;
float currentHumidity = 0.0;
float currentPressure = 0.0;
int animationX = 0;

// Remote sensor data (from NodeMCU via ESP-NOW)
SensorData remoteSensorData;
bool remoteSensorDataAvailable = false;
unsigned long lastRemoteDataReceived = 0;

// ============================================================================
// Function Declarations
// ============================================================================

void setupWiFi();
void setupMQTT();
void setupESPNow();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void onESPNowDataReceived(const uint8_t* mac_addr, const uint8_t* data,
                          int data_len);
void initSensors();
void initDisplay();
void readSensors();
void publishMQTTData();
void publishRemoteSensorData();
void updateDisplay();

// ============================================================================
// WiFi Setup
// ============================================================================

void setupWiFi() {
  Serial.println("\n[WiFi] Configuring WiFi...");

  // CRITICAL: Set to AP+STA mode for ESP-NOW to work properly
  // The gateway acts as both an Access Point (for ESP-NOW) and Station (for
  // internet)
  WiFi.mode(WIFI_AP_STA);

  // First, connect to WiFi network to get the channel
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
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  Serial.println("[MQTT] Client configured");
}

void reconnectMQTT() {
  if (!mqttClient.connected()) {
    Serial.print("[MQTT] Attempting connection...");

    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println("connected!");
      mqttClient.publish(MQTT_TOPIC_STATUS, "online");
      // Subscribe to command topics
      mqttClient.subscribe("smartalarm/commands");
      mqttClient.subscribe("smartalarm/play_audio");
      Serial.println("[MQTT] Subscribed to command topics");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Message received on topic: ");
  Serial.println(topic);

  // Convert payload to string
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("[MQTT] Payload: ");
  Serial.println(message);

  // Handle incoming MQTT messages
  String topicStr = String(topic);

  if (topicStr == "smartalarm/play_audio") {
    // Play audio file from SPIFFS
    // Example: /alarm1.mp3 or /wake_up.wav
    String filename = message;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    audio.playFile(filename.c_str());

  } else if (topicStr == "smartalarm/commands") {
    message.toLowerCase();

    if (message == "stop_audio") {
      audio.stop();

    } else if (message == "list_files") {
      audio.listFiles();

    } else if (message.startsWith("volume=")) {
      // Set volume: volume=0.5
      float vol = message.substring(7).toFloat();
      audio.setVolume(vol);

    } else if (message.startsWith("play:")) {
      // Play specific file: play:/alarm.mp3
      String filename = message.substring(5);
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }
      audio.playFile(filename.c_str());
    }
  }
}

void publishMQTTData() {
  if (!mqttClient.connected()) {
    return;
  }

  char tempStr[10];
  char humStr[10];
  char pressStr[10];

  dtostrf(currentTemp, 4, 1, tempStr);
  dtostrf(currentHumidity, 4, 1, humStr);
  dtostrf(currentPressure, 6, 1, pressStr);

  mqttClient.publish(MQTT_TOPIC_TEMP, tempStr);
  mqttClient.publish(MQTT_TOPIC_HUMIDITY, humStr);
  mqttClient.publish(MQTT_TOPIC_PRESSURE, pressStr);

  Serial.println("[MQTT] Gateway sensor data published");
}

// ============================================================================
// Publish Remote Sensor Data to MQTT
// ============================================================================

void publishRemoteSensorData() {
  if (!mqttClient.connected() || !remoteSensorDataAvailable) {
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

  mqttClient.publish(MQTT_TOPIC_REMOTE_TEMP, tempStr);
  mqttClient.publish(MQTT_TOPIC_REMOTE_HUMIDITY, humStr);
  mqttClient.publish(MQTT_TOPIC_REMOTE_PRESSURE, pressStr);
  mqttClient.publish(MQTT_TOPIC_REMOTE_UV, uvStr);
  mqttClient.publish(MQTT_TOPIC_REMOTE_BATTERY, battStr);

  // Publish status with device name
  String statusMsg = String(remoteSensorData.deviceName) + " online";
  mqttClient.publish(MQTT_TOPIC_REMOTE_STATUS, statusMsg.c_str());

  Serial.println("[MQTT] → Remote sensor data forwarded to MQTT broker");
}

// ============================================================================
// Sensor Initialization
// ============================================================================

void initSensors() {
  Serial.println("[Sensors] Initializing...");

  // Initialize DHT22
  dht.begin();
  Serial.println("[Sensors] DHT22 initialized");

  // Initialize I2C Multiplexer
  tca.begin();
  Serial.println("[Sensors] TCA9548A initialized");

  // Initialize BMP085 on channel 0
  tca.openChannel(TCA_CHANNEL_BMP);
  if (bmp.begin()) {
    Serial.println("[Sensors] BMP085 initialized");
  } else {
    Serial.println("[Sensors] BMP085 initialization failed!");
  }
  tca.closeChannel(TCA_CHANNEL_BMP);
}

// ============================================================================
// Display Initialization
// ============================================================================

void initDisplay() {
  Serial.println("[Display] Initializing OLED...");

  tca.openChannel(TCA_CHANNEL_OLED);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[Display] SSD1306 not found!");
    tca.closeChannel(TCA_CHANNEL_OLED);
    while (1);
  }

  // Show startup screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Smart Alarm Clock");
  display.println("================");
  display.println();
  display.println("Initializing...");
  display.display();

  tca.closeChannel(TCA_CHANNEL_OLED);
  Serial.println("[Display] OLED initialized");
}

// ============================================================================
// Sensor Reading
// ============================================================================

void readSensors() {
  // Read DHT22
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // Read BMP085
  tca.openChannel(TCA_CHANNEL_BMP);
  float pressure = bmp.readPressure() / 100.0;  // Convert to hPa
  tca.closeChannel(TCA_CHANNEL_BMP);

  // Update global variables if readings are valid
  if (!isnan(temp)) currentTemp = temp;
  if (!isnan(hum)) currentHumidity = hum;
  if (pressure > 0) currentPressure = pressure;

  Serial.printf("[Sensors] T=%.1f°C, H=%.1f%%, P=%.1fhPa\n", currentTemp,
                currentHumidity, currentPressure);
}

// ============================================================================
// Display Update
// ============================================================================

void updateDisplay() {
  tca.openChannel(TCA_CHANNEL_OLED);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  // Title
  display.println("Smart Alarm Gateway");
  display.println("------------------");

  // Local sensor data (left side)
  display.printf("L:%.0fC %.0f%%", currentTemp, currentHumidity);

  // Remote sensor data indicator (right side)
  if (remoteSensorDataAvailable &&
      (millis() - lastRemoteDataReceived < 30000)) {
    display.printf(" R:OK\n");
    display.printf("R:%.0fC %.0f%% UV:%.1f\n", remoteSensorData.temperature,
                   remoteSensorData.humidity, remoteSensorData.uvIndex);
    display.printf("Batt:%d%%\n", remoteSensorData.batteryLevel);
  } else {
    display.println(" R:--");
    display.println("Remote: No Data");
  }

  // WiFi and MQTT status
  display.println();
  if (WiFi.status() == WL_CONNECTED) {
    display.print("WiFi:OK");
  } else {
    display.print("WiFi:--");
  }

  display.print(" MQTT:");
  if (mqttClient.connected()) {
    display.println("OK");
  } else {
    display.println("--");
  }

  display.print("ESPNow:OK");

  // Animation
  display.fillRect(animationX, 56, 10, 8, SSD1306_WHITE);
  animationX += 4;
  if (animationX > SCREEN_WIDTH) animationX = 0;

  display.display();
  tca.closeChannel(TCA_CHANNEL_OLED);
}

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

  // Initialize components
  initSensors();
  initDisplay();

  // Initialize audio system
  audio.begin();

  // Setup WiFi and MQTT
  setupWiFi();
  setupMQTT();

  // Setup ESP-NOW (after WiFi for channel sync)
  setupESPNow();

  // Initial sensor reading
  readSensors();
  updateDisplay();

  Serial.println("\n[System] Setup complete!\n");
  Serial.println("========================================");
  Serial.println("Waiting for sensor data via ESP-NOW...");
  Serial.println("========================================\n");
}

// ============================================================================
// Arduino Loop
// ============================================================================

void loop() {
  unsigned long currentMillis = millis();

  // Maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection lost, reconnecting...");
    setupWiFi();
  }

  // Maintain MQTT connection
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    reconnectMQTT();
  }

  // Process MQTT messages
  mqttClient.loop();

  // Handle audio playback
  audio.loop();

  // Read sensors periodically
  if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = currentMillis;
    readSensors();
    updateDisplay();
  }

  // Publish data to MQTT periodically
  if (currentMillis - lastMqttPublish >= MQTT_PUBLISH_INTERVAL) {
    lastMqttPublish = currentMillis;
    publishMQTTData();
  }

  delay(10);  // Small delay to prevent watchdog issues
}
