#include "../../include/gateway_esp32/mqtt_setup.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include "../../include/gateway_esp32/audio_manager.h"
#include "../../include/gateway_esp32/mqtt_manager.h"
#include "../../include/shared/config.h"
#include "../../include/shared/sensor_data.h"

// External declarations
extern WiFiClient wifiClient;
extern PubSubClient mqttClient;
extern MQTTManager mqtt;
extern AudioManager audio;
extern SensorData remoteSensorData;
extern bool remoteSensorDataAvailable;

// MQTT Topics are now included via config.h

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