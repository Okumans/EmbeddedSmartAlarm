#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

extern "C" {
#include <user_interface.h>
}

#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <DHT.h>

#include "../../include/sensor_data.h"

// ============================================================================
// Sensor Pin Definitions
// ==========================================        ==================================
#define DHTPIN D4          // D4 (GPIO2)
#define DHTTYPE DHT22
#define UV_PIN A0         // GUVA-S12SD UV Sensor

DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP085 bmp;

// ============================================================================
// Soft AP Configuration
// ============================================================================
const char* SOFT_AP_SSID     = "SmartAlarm-Gateway";
const char* SOFT_AP_PASSWORD = "12345678";

// Gateway AP MAC Address (MUST USE ESP32's Soft AP MAC)
uint8_t gatewayAddress[] = { 0x28, 0x56, 0x2F, 0x4A, 0x15, 0x0D };

#define WIFI_CHANNEL 6
#define SENSOR_READ_INTERVAL 5000  // 5 seconds

// ============================================================================
// Global Variables
// ============================================================================

SensorData sensorData;
unsigned long lastSensorRead = 0;
unsigned long transmissionCount = 0;
bool espNowInitialized = false;
bool bmpInitialized = false;

// ============================================================================
// Function Declarations
// ============================================================================
void connectToSoftAP();
void initESPNow();
void readRealSensors();
void sendSensorData();
void onDataSent(uint8_t* mac_addr, uint8_t sendStatus);

// ============================================================================
// ESP-NOW Callback
// ============================================================================
void onDataSent(uint8_t* mac_addr, uint8_t sendStatus) {
  Serial.print("[ESP-NOW] Packet sent to: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.print(" | Status: ");
  Serial.println(sendStatus == 0 ? "✓ Success" : "✗ Failed");
}

// ============================================================================
// WiFi Connect
// ============================================================================
void connectToSoftAP() {
  Serial.println("\n[WiFi] Connecting to Soft AP...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SOFT_AP_SSID, SOFT_AP_PASSWORD, WIFI_CHANNEL);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] ✓ Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] ✗ Failed to connect!");
  }
}

// ============================================================================
// ESP-NOW Init
// ============================================================================
void initESPNow() {
  Serial.println("\n[ESP-NOW] Initializing...");

  if (esp_now_init() != 0) {
    Serial.println("[ESP-NOW] ✗ Initialization failed");
    espNowInitialized = false;
    return;
  }
  Serial.println("[ESP-NOW] ✓ Initialized");

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(onDataSent);

  int addPeerResult = esp_now_add_peer(
      gatewayAddress,
      ESP_NOW_ROLE_SLAVE,
      WIFI_CHANNEL,
      NULL, 0);

  if (addPeerResult == 0) {
    Serial.println("[ESP-NOW] ✓ Peer added");
    espNowInitialized = true;
  } else {
    Serial.printf("[ESP-NOW] ✗ Peer add failed! Code: %d\n", addPeerResult);
    espNowInitialized = false;
  }
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== Smart Alarm - Sensor Node ===\n");

  dht.begin();

  // Explicitly initialize I2C pins for ESP8266 (SDA=D2/GPIO4, SCL=D1/GPIO5)
  Wire.begin(D2, D1);
  bmpInitialized = bmp.begin();
  if (bmpInitialized) {
    Serial.println("[BMP] ✓ BMP initialized");
  } else {
    Serial.println("[BMP] ✗ BMP initialization failed");
  }

  connectToSoftAP();
  initESPNow();

  sensorData.sensorId = SENSOR_NODE_ID;
  strncpy(sensorData.deviceName, SENSOR_NODE_NAME, sizeof(sensorData.deviceName));
  sensorData.batteryLevel = 100;

  Serial.println("[System] Ready.\n");
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    readRealSensors();
    sendSensorData();
  }

  delay(10);
}

// ============================================================================
// Real Sensor Reader
// ============================================================================
void readRealSensors() {
  Serial.println("╔════════ REAL SENSOR DATA ═══════════════╗");

  // -------- DHT22 --------
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(t)) sensorData.temperature = t;
  if (!isnan(h)) sensorData.humidity = h;

  Serial.printf(" ║ Temp:       %6.2f °C                  ║\n", sensorData.temperature);
  Serial.printf(" ║ Humidity:   %6.2f %%                   ║\n", sensorData.humidity);

  // -------- BMP180 --------
  if (bmpInitialized) {
    sensorData.pressure = bmp.readPressure() / 100.0;
  } else {
    sensorData.pressure = 0.0;
  }

  Serial.printf(" ║ Pressure:   %7.2f hPa                 ║\n", sensorData.pressure);

  // -------- GUVA-S12SD UV Sensor --------
  int raw = analogRead(UV_PIN);
  float voltage = raw * (3.3 / 1023.0);  // ESP8266 ADC is 0–1V unless divider used

  float uvIndex = voltage / 0.1;  // Approximated UVA → UV Index
  sensorData.uvIndex = constrain(uvIndex, 0, 15);

  Serial.printf(" ║ UV Index:   %6.2f                      ║\n", sensorData.uvIndex);

  // -------- Battery Simulation --------
  sensorData.batteryLevel = 100 - (transmissionCount % 100);

  Serial.printf(" ║ Battery:    %3d %%                      ║\n", sensorData.batteryLevel);
  Serial.println("╚═════════════════════════════════════════╝");

  sensorData.timestamp = millis();
}

// ============================================================================
// ESP-NOW Send
// ============================================================================
void sendSensorData() {
  if (!espNowInitialized) {
    Serial.println("[ESP-NOW] ✗ Cannot send - not initialized!");
    return;
  }

  uint8_t result = esp_now_send(
      gatewayAddress,
      (uint8_t*)&sensorData,
      sizeof(sensorData));

  if (result == 0) {
    Serial.println("[ESP-NOW] ✓ Packet sent (queued)");
    transmissionCount++;
  } else {
    Serial.printf("[ESP-NOW] ✗ Send error: %d\n", result);
  }
}
