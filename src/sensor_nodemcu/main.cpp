#include <Arduino.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// ============================================================================
// Configuration
// ============================================================================

// WiFi Configuration
const char* WIFI_SSID = "Pleaseconnecttome";
const char* WIFI_PASSWORD = "n1234567!";

// MQTT Configuration
const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "SensorNode_NodeMCU";
const char* MQTT_TOPIC_TEMP = "smartalarm/sensor/temperature";
const char* MQTT_TOPIC_HUMIDITY = "smartalarm/sensor/humidity";
const char* MQTT_TOPIC_STATUS = "smartalarm/sensor/status";

// Pin Definitions for NodeMCU
#define DHTPIN D4  // GPIO2 (D4 on NodeMCU)
#define DHTTYPE DHT22

// Timing Configuration
#define SENSOR_READ_INTERVAL 5000  // 5 seconds

// ============================================================================
// Global Objects
// ============================================================================

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastSensorRead = 0;

// ============================================================================
// Function Declarations
// ============================================================================

void setupWiFi();
void reconnectMQTT();
void readAndPublishSensors();

// ============================================================================
// Setup Function
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n===========================================");
  Serial.println("Smart Alarm - Sensor Node (NodeMCUv2)");
  Serial.println("===========================================\n");

  // Initialize DHT sensor
  dht.begin();
  Serial.println("✓ DHT22 sensor initialized");

  // Connect to WiFi
  setupWiFi();

  // Setup MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  Serial.println("✓ MQTT configured");

  Serial.println("\n✓ Setup complete!\n");
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
  // Ensure MQTT connection
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // Read and publish sensor data periodically
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = currentMillis;
    readAndPublishSensors();
  }
}

// ============================================================================
// WiFi Functions
// ============================================================================

void setupWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected");
    Serial.print("  IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n✗ WiFi connection failed!");
  }
}

// ============================================================================
// MQTT Functions
// ============================================================================

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker...");

    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println(" connected!");
      mqttClient.publish(MQTT_TOPIC_STATUS, "Sensor Node Online");
    } else {
      Serial.print(" failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" - retrying in 5 seconds");
      delay(5000);
    }
  }
}

// ============================================================================
// Sensor Functions
// ============================================================================

void readAndPublishSensors() {
  // Read DHT22 sensor
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Check if readings are valid
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("✗ Failed to read from DHT sensor!");
    return;
  }

  // Display readings
  Serial.println("─────────────────────────────────");
  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.println(" °C");

  Serial.print("Humidity:    ");
  Serial.print(humidity, 1);
  Serial.println(" %");
  Serial.println("─────────────────────────────────");

  // Publish to MQTT
  char tempStr[8];
  char humStr[8];

  dtostrf(temperature, 6, 2, tempStr);
  dtostrf(humidity, 6, 2, humStr);

  mqttClient.publish(MQTT_TOPIC_TEMP, tempStr);
  mqttClient.publish(MQTT_TOPIC_HUMIDITY, humStr);

  Serial.println("✓ Data published to MQTT\n");
}
