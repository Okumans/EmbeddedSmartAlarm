#include "../../include/gateway_esp32/wifi_espnow_manager.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "../../include/shared/config.h"
#include "../../include/shared/sensor_data.h"

// External declarations
extern SensorData remoteSensorData;
extern bool remoteSensorDataAvailable;
extern unsigned long lastRemoteDataReceived;
extern void publishRemoteSensorData();

// ESP-NOW callback
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

    // Immediately publish to MQTT
    publishRemoteSensorData();
  } else {
    Serial.printf("[ESP-NOW] ✗ Invalid data size! Expected %d, got %d\n",
                  sizeof(SensorData), data_len);
  }
}

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

void maintainWiFi() {
  // WiFi reconnection check
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection lost, reconnecting...");
    setupWiFi();
  }
}