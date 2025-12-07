#include "../../include/gateway_esp32/mqtt_setup.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include "../../include/gateway_esp32/audio_manager.h"
#include "../../include/gateway_esp32/mqtt_manager.h"
#include "../../include/shared/config.h"
#include "../../include/shared/sensor_data.h"
#include "../../include/shared/mqtt_time.h"
#include "../../include/shared/time_sync.h"
#include "../../include/gateway_esp32/alarm_manager.h"
#include "../../include/gateway_esp32/alarm_question_handler.h"

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

  // Try an immediate connection if WiFi is already up. This gives faster
  // feedback on startup instead of waiting for the MQTT task's reconnect
  // interval. Useful when debugging connectivity issues.
  Serial.printf("[MQTT] WiFi status: %d | IP: %s\n", WiFi.status(),
                WiFi.localIP().toString().c_str());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[MQTT] Attempting immediate connect...");
    if (mqtt.reconnect()) {
      Serial.println("[MQTT] Immediate connection successful");
    } else {
      Serial.println("[MQTT] Immediate connection failed (will retry in task loop)");
    }
  } else {
    Serial.println("[MQTT] WiFi not connected; skipping immediate MQTT connect");
  }

  setupMQTTHandlers();
}

void setupMQTTHandlers() {
  Serial.println("\n[MQTT] Registering message handlers...");

  // =======================================================================
  // AUDIO HANDLERS - High Priority (150)
  // =======================================================================

  // Audio playback command
  mqtt.registerAndSubscribe(
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

  mqtt.registerAndSubscribe(
      "smartalarm/commands",
      [](MQTTManager& mqtt, const char* topic, byte* payload,
         unsigned int length) -> bool {
        String message((char*)payload, length);
        message.toLowerCase();

        Serial.printf("[MQTT] Received system command: %s\n",
                      message.c_str());

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
        } else if (message == "list_alarms") {
          // Send current alarm list
          String alarmList = alarmManager.toCSV();
          if (alarmList.length() > 0) {
            mqtt.publish("smartalarm/alarms", alarmList);
            mqtt.publish("smartalarm/status", "alarms_listed");
          } else {
            mqtt.publish("smartalarm/status", "no_alarms");
          }
          return true;
        }

        return false;  // Not handled by this handler
      },
      "SystemCommands", 100);

  // Register AudioManager's own handlers
  audio.registerMQTTHandlers(mqtt);

  // =======================================================================
  // MQTT TIME HANDLER - Normal Priority (100)
  // Topic: smartalarm/time
  // Payload: plain time string (e.g., "2025-12-06 07:30:00" or "07:30:00")
  // =======================================================================
  mqtt.registerAndSubscribe(
      "smartalarm/time",
      [](MQTTManager& mqtt, const char* topic, byte* payload,
         unsigned int length) -> bool {
        String t((char*)payload, length);
        t.trim();
        if (t.length() > 0) {
          mqttTime = t;
          mqttTimeAvailable = true;
          Serial.printf("[MQTT] Received time via MQTT: %s\n", t.c_str());
          // Optionally acknowledge
          mqtt.publish("smartalarm/time/status", "received");
          return true;
        }
        return false;
      },
      "MQTTTime", 100);

  // =======================================================================
  // ALARM LIST HANDLER - Normal Priority (100)
  // Payload: CSV of HH:MM values, e.g. "07:30,08:00,14:30"
  // =======================================================================
  mqtt.registerAndSubscribe(
      "smartalarm/alarmlist",
      [](MQTTManager& mqtt, const char* topic, byte* payload,
         unsigned int length) -> bool {
        // Parse and store alarms
        alarmManager.setFromPayload((const char*)payload, length);

        // Acknowledge by publishing status and the parsed list
        mqtt.publish("smartalarm/alarmlist/status", "ok");
        mqtt.publish("smartalarm/alarmlist/parsed", alarmManager.toCSV());

        Serial.printf("[MQTT] Alarms updated: %s\n", alarmManager.toCSV().c_str());
        return true;
      },
      "AlarmList", 100);

  // =======================================================================
  // ALARM SOUND HANDLER - Normal Priority (100)
  // Topic: smartalarm/alarm/sound
  // Payload: Sound file path (e.g., "/sound_EbBW3i3L.mp3" or "sound_123.mp3")
  // =======================================================================
  mqtt.registerAndSubscribe(
      "smartalarm/alarm/sound",
      [](MQTTManager& mqtt, const char* topic, byte* payload,
         unsigned int length) -> bool {
        String soundFile((char*)payload, length);
        soundFile.trim();
        
        // Ensure leading slash
        if (!soundFile.startsWith("/")) {
          soundFile = "/" + soundFile;
        }
        
        // Update alarm sound
        alarmManager.setAlarmSound(soundFile);
        
        // Acknowledge
        mqtt.publish("smartalarm/alarm/sound/status", "ok");
        mqtt.publish("smartalarm/alarm/sound/current", soundFile);
        
        Serial.printf("[MQTT] Alarm sound updated to: %s\n", soundFile.c_str());
        return true;
      },
      "AlarmSound", 100);

  // =======================================================================
  // ALARM QUESTION HANDLER - Normal Priority (100)
  // Topic: smartalarm/question
  // Payload: Question text in Thai
  // =======================================================================
  mqtt.registerAndSubscribe(
      "smartalarm/question",
      [](MQTTManager& mqtt, const char* topic, byte* payload,
         unsigned int length) -> bool {
        String question((char*)payload, length);
        alarmQuestion.setQuestion(question);
        
        mqtt.publish("smartalarm/question/status", "received");
        Serial.printf("[MQTT] Question received: %s\n", question.c_str());
        return true;
      },
      "AlarmQuestion", 100);

  // =======================================================================
  // ANSWER VALIDATION HANDLER - Normal Priority (100)
  // Topic: smartalarm/answer/validation
  // Payload: "valid" or "invalid"
  // =======================================================================
  mqtt.registerAndSubscribe(
      "smartalarm/answer/validation",
      [](MQTTManager& mqtt, const char* topic, byte* payload,
         unsigned int length) -> bool {
        String result((char*)payload, length);
        result.toLowerCase();
        
        bool isCorrect = result.indexOf("valid") >= 0 && result.indexOf("invalid") < 0;
        alarmQuestion.setValidationResult(isCorrect);
        
        Serial.printf("[MQTT] Answer validation: %s (%s)\n",
                      result.c_str(), isCorrect ? "CORRECT" : "WRONG");
        return true;
      },
      "AnswerValidation", 100);

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