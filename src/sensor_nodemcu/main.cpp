#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

extern "C" {
#include <user_interface.h>
}

#include "../../include/sensor_data.h"

// ============================================================================
// Configuration
// ============================================================================

// Soft AP Configuration (from Gateway)
const char* SOFT_AP_SSID = "SmartAlarm-Gateway";
const char* SOFT_AP_PASSWORD = "12345678";

// Gateway ESP32 MAC Address - REPLACE WITH YOUR ESP32's MAC ADDRESS
// IMPORTANT: Use the AP MAC (Soft AP MAC), NOT the Station MAC!
// The gateway will print both - use the "AP MAC (Soft AP)" address
// AP MAC: 28:56:2F:4A:15:0D
uint8_t gatewayAddress[] = {
    0x28, 0x56, 0x2F,
    0x4A, 0x15, 0x0D};  // AP MAC address (last byte is 0D, not 0C)

// WiFi Channel - MUST MATCH THE GATEWAY'S CHANNEL
// The gateway creates a Soft AP on this channel
#define WIFI_CHANNEL 6  // Must match gateway's WIFI_CHANNEL setting

// Timing Configuration
#define SENSOR_READ_INTERVAL 5000  // 5 seconds between transmissions

// ============================================================================
// Global Variables
// ============================================================================

SensorData sensorData;
unsigned long lastSensorRead = 0;
unsigned long transmissionCount = 0;
bool espNowInitialized = false;

// ============================================================================
// Function Declarations
// ============================================================================

void connectToSoftAP();
void initESPNow();
void readMockSensors();
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
// WiFi Connection to Soft AP
// ============================================================================

void connectToSoftAP() {
  Serial.println("\n[WiFi] Connecting to Soft AP...");
  Serial.printf("[WiFi] SSID: %s\n", SOFT_AP_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SOFT_AP_SSID, SOFT_AP_PASSWORD, WIFI_CHANNEL);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] ✓ Connected to Soft AP!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] MAC Address: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("[WiFi] Channel: %d\n", WiFi.channel());
  } else {
    Serial.println("\n[WiFi] ✗ Failed to connect to Soft AP!");
    Serial.println("[WiFi] Check SSID, password, and that gateway is running");
  }
}

// ============================================================================
// ESP-NOW Initialization
// ============================================================================

void initESPNow() {
  Serial.println("\n[ESP-NOW] Initializing...");

  // WiFi is already connected to Soft AP, just verify channel
  Serial.printf("[WiFi] Current channel: %d\n", WiFi.channel());

  // Initialize ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("[ESP-NOW] ✗ Initialization failed!");
    espNowInitialized = false;
    return;
  }
  Serial.println("[ESP-NOW] ✓ ESP-NOW initialized");

  // *** CRITICAL: Set self role BEFORE adding peers ***
  // This ESP8266 is a CONTROLLER (sender)
  if (esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER) != 0) {
    Serial.println("[ESP-NOW] ✗ Failed to set self role!");
    espNowInitialized = false;
    return;
  }
  Serial.println("[ESP-NOW] ✓ Self role set to CONTROLLER");

  // Register send callback
  esp_now_register_send_cb(onDataSent);
  Serial.println("[ESP-NOW] ✓ Send callback registered");

  // Add the peer (the gateway)
  // The peer role is SLAVE (receiver)
  int addPeerResult =
      esp_now_add_peer(gatewayAddress,
                       ESP_NOW_ROLE_SLAVE,  // Gateway acts as SLAVE (receiver)
                       WIFI_CHANNEL,        // Must match gateway's channel
                       NULL, 0);

  // Check if adding the peer was successful
  if (addPeerResult == 0) {
    Serial.print("[ESP-NOW] ✓ Peer added successfully: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", gatewayAddress[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.println();
    espNowInitialized = true;  // Flag success!
  } else {
    Serial.printf("[ESP-NOW] ✗ Failed to add peer! Error code: %d\n",
                  addPeerResult);
    espNowInitialized = false;  // Flag failure
  }
}

// ============================================================================
// Setup Function
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n===========================================");
  Serial.println("Smart Alarm - Sensor Node (NodeMCU ESP8266)");
  Serial.println("ESP-NOW Sensor Transmitter");
  Serial.println("===========================================\n");

  // Connect to Gateway's Soft AP
  connectToSoftAP();

  // Initialize ESP-NOW (after WiFi connection)
  initESPNow();

  // Initialize sensor data structure
  sensorData.sensorId = SENSOR_NODE_ID;
  strncpy(sensorData.deviceName, SENSOR_NODE_NAME,
          sizeof(sensorData.deviceName));
  sensorData.batteryLevel = 100;  // Mock battery at 100%

  Serial.printf("[DEBUG] SensorData struct size: %d bytes\n",
                sizeof(SensorData));

  if (espNowInitialized) {
    Serial.println("\n✓ Setup complete! Starting data transmission...\n");
  } else {
    Serial.println("\n✗ Setup FAILED! ESP-NOW not initialized properly!");
    Serial.println("⚠ Data transmission will fail. Check errors above.\n");
  }
  Serial.println("===========================================");
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
  unsigned long currentMillis = millis();

  // Read sensors and send data periodically
  if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = currentMillis;
    readMockSensors();
    sendSensorData();
  }

  delay(10);  // Small delay to prevent watchdog issues
}

// ============================================================================
// Mock Sensor Functions
// ============================================================================

void readMockSensors() {
  // Generate mock sensor data with realistic variations
  float baseTemp = 25.0;
  float baseHumidity = 60.0;
  float basePressure = 1013.25;
  float baseUV = 5.0;

  // Add some random variation to make it look realistic
  float tempVariation = (random(-50, 50) / 10.0);   // ±5.0°C
  float humVariation = (random(-100, 100) / 10.0);  // ±10.0%
  float pressVariation = (random(-50, 50) / 10.0);  // ±5.0 hPa
  float uvVariation = (random(-20, 20) / 10.0);     // ±2.0 UV index

  // Update sensor data
  sensorData.timestamp = millis();
  sensorData.temperature = baseTemp + tempVariation;
  sensorData.humidity = constrain(baseHumidity + humVariation, 0, 100);
  sensorData.pressure = basePressure + pressVariation;
  sensorData.uvIndex = constrain(baseUV + uvVariation, 0, 15);
  sensorData.batteryLevel =
      100 - (transmissionCount % 100);  // Simulate battery drain

  // Display readings
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.printf(" ║  Transmission #%-4lu                 ║\n",
                transmissionCount);
  Serial.println("╠═══════════════════════════════════════╣");
  Serial.printf(" ║  Temperature: %6.2f °C            ║\n",
                sensorData.temperature);
  Serial.printf(" ║  Humidity:    %6.2f %%             ║\n",
                sensorData.humidity);
  Serial.printf(" ║  Pressure:    %7.2f hPa          ║\n", sensorData.pressure);
  Serial.printf(" ║  UV Index:    %6.2f                ║\n",
                sensorData.uvIndex);
  Serial.printf(" ║  Battery:     %3d %%                ║\n",
                sensorData.batteryLevel);
  Serial.println("╚═══════════════════════════════════════╝");
}

// ============================================================================
// ESP-NOW Send Function
// ============================================================================

void sendSensorData() {
  // !! SAFETY CHECK !!
  // Don't try to send if the peer was never added
  if (!espNowInitialized) {
    Serial.println("[ESP-NOW] ✗ Cannot send - peer not initialized!");
    return;
  }

  // Check WiFi connection status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ESP-NOW] ⚠ Warning: Not connected to Soft AP!");
    Serial.println("[ESP-NOW] Attempting to reconnect...");
    connectToSoftAP();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[ESP-NOW] ✗ Cannot send - WiFi not connected!");
      return;
    }
  }

  // Send data via ESP-NOW
  Serial.print("[ESP-NOW] Sending to MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", gatewayAddress[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  uint8_t result =
      esp_now_send(gatewayAddress, (uint8_t*)&sensorData, sizeof(sensorData));

  if (result == 0) {
    Serial.println("[ESP-NOW] → Data packet queued for transmission");
    transmissionCount++;
  } else {
    Serial.printf("[ESP-NOW] ✗ Error queuing data packet! Error: %d\n", result);
    Serial.println("[ESP-NOW] Common error codes:");
    Serial.println("  - 0xFD (253): Peer not found");
    Serial.println("  - 0x100: Invalid argument");
  }
  Serial.println();
}