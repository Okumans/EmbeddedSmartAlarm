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

#include "audio_manager.h"

// ============================================================================
// Configuration
// ============================================================================

// WiFi Configuration
const char* WIFI_SSID = "Pleaseconnecttome";
const char* WIFI_PASSWORD = "n1234567!";

// MQTT Configuration
const char* MQTT_SERVER = "broker.hivemq.com";  // Change to your MQTT broker
const int MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "SmartAlarmClock";
const char* MQTT_TOPIC_TEMP = "smartalarm/temperature";
const char* MQTT_TOPIC_HUMIDITY = "smartalarm/humidity";
const char* MQTT_TOPIC_PRESSURE = "smartalarm/pressure";
const char* MQTT_TOPIC_STATUS = "smartalarm/status";

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

// ============================================================================
// Function Declarations
// ============================================================================

void setupWiFi();
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void initSensors();
void initDisplay();
void readSensors();
void publishMQTTData();
void updateDisplay();

// ============================================================================
// WiFi Setup
// ============================================================================

void setupWiFi() {
  Serial.println("\n[WiFi] Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Connection failed!");
  }
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

  Serial.println("[MQTT] Data published");
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
  display.println("Weather Monitor");
  display.println("----------------");

  // Sensor data
  display.printf("Temp: %.1f C\n", currentTemp);
  display.printf("Hum : %.1f %%\n", currentHumidity);
  display.printf("Pres: %.1f hPa\n", currentPressure);

  // WiFi status
  display.println();
  if (WiFi.status() == WL_CONNECTED) {
    display.print("WiFi: OK");
  } else {
    display.print("WiFi: --");
  }

  // MQTT status
  display.print(" MQTT: ");
  if (mqttClient.connected()) {
    display.println("OK");
  } else {
    display.println("--");
  }

  // Simple animation: moving box
  display.fillRect(animationX, 50, 10, 10, SSD1306_WHITE);
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

  // Initial sensor reading
  readSensors();
  updateDisplay();

  Serial.println("\n[System] Setup complete!\n");
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
