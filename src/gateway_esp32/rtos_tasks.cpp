#include "../../include/gateway_esp32/rtos_tasks.h"

#include <time.h>

#include "../../include/gateway_esp32/alarm_manager.h"
#include "../../include/gateway_esp32/alarm_question_handler.h"
#include "../../include/gateway_esp32/audio_manager.h"
#include "../../include/gateway_esp32/audio_stream_manager.h"
#include "../../include/gateway_esp32/button_manager.h"
#include "../../include/gateway_esp32/display_manager.h"
#include "../../include/gateway_esp32/mqtt_manager.h"
#include "../../include/gateway_esp32/question_manager.h"
#include "../../include/gateway_esp32/sensor_manager.h"
#include "../../include/shared/config.h"
#include "../../include/shared/mqtt_time.h"
#include "../../include/shared/sensor_data.h"
#include "../../include/shared/time_sync.h"

// External references to global objects (from main.cpp)
extern AudioManager audio;
extern AudioStreamManager audioStream;
extern ButtonManager button;
extern MQTTManager mqtt;
extern SensorManager localSensors;
extern DisplayManager displayManager;
extern QuestionManager questionManager;
extern void publishRemoteSensorData();
extern SensorData remoteSensorData;
extern bool remoteSensorDataAvailable;
extern unsigned long lastRemoteDataReceived;

// Task handles
TaskHandle_t audioDecodeTaskHandle = NULL;
TaskHandle_t audioEncodeTaskHandle = NULL;
TaskHandle_t websocketTaskHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t alarmTaskHandle = NULL;

// Queues
QueueHandle_t audioTxQueue = NULL;
QueueHandle_t audioRxQueue = NULL;
QueueHandle_t mqttQueue = NULL;

// ============================================================================
// AUDIO DECODE TASK - Handle audio playback
// ============================================================================
void audioDecodeTask(void* parameter) {
  Serial.println("[RTOS] Audio Decode Task started on Core 1");

  for (;;) {
    // Handle MP3 playback
    audio.loop();
    vTaskDelay(pdMS_TO_TICKS(10));  // 10ms for MP3 playback
  }
}

// ============================================================================
// AUDIO ENCODE TASK - Encode microphone input for streaming
// ============================================================================
void audioEncodeTask(void* parameter) {
  Serial.println("[RTOS] Audio Encode Task started on Core 1");

  for (;;) {
    // Process audio streaming (I2S read + WebSocket send)
    audioStream.process();
    vTaskDelay(pdMS_TO_TICKS(10));  // 10ms loop for smooth audio streaming
  }
}

// ============================================================================
// MQTT TASK - Handle MQTT communication
// ============================================================================
void mqttTask(void* parameter) {
  Serial.println("[RTOS] MQTT Task started on Core 0");

  for (;;) {
    // Process MQTT messages
    mqtt.loop();

    // Run at 10Hz (every 100ms) - reduced frequency to prevent watchdog
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ============================================================================
// SENSOR TASK - Read sensors and update display
// ============================================================================
void sensorTask(void* parameter) {
  Serial.println("[RTOS] Sensor Task started on Core 1");

  const TickType_t sensorInterval = pdMS_TO_TICKS(10000);   // 10 seconds
  const TickType_t publishInterval = pdMS_TO_TICKS(10000);  // 10 seconds

  TickType_t lastSensorRead = xTaskGetTickCount();
  TickType_t lastPublish = xTaskGetTickCount();

  for (;;) {
    TickType_t now = xTaskGetTickCount();

    // Read sensors every 2 seconds
    if ((now - lastSensorRead) >= sensorInterval) {
      localSensors.readSensors();
      // Print sensor values and current time to Serial for verification
      // Determine time source: prefer MQTT time if available
      struct tm timeinfo;
      char timeBuf[32];
      const char* timeSource = "--";

      if (mqttTimeAvailable && mqttTime.length() > 0) {
        // Use mqttTime (take last 8 chars if it contains date+time)
        String t = mqttTime;
        t.trim();
        if (t.length() > 8) t = t.substring(t.length() - 8);
        strncpy(timeBuf, t.c_str(), sizeof(timeBuf) - 1);
        timeBuf[sizeof(timeBuf) - 1] = '\0';
        timeSource = "MQT";
      } else if (getLocalTime(&timeinfo, 1000)) {
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", timeinfo.tm_hour,
                 timeinfo.tm_min, timeinfo.tm_sec);
        timeSource = (timeSynced ? "NTP" : "unsynced");
      } else {
        snprintf(timeBuf, sizeof(timeBuf), "--:--:--");
        timeSource = "unsynced";
      }

      Serial.printf("[Data] Time: %s (%s)\n", timeBuf, timeSource);

      // Also print MQTT connectivity
      Serial.printf("[Data] MQTT connected: %s | MQTT time avail: %s\n",
                    (mqtt.isConnected() ? "yes" : "no"),
                    (mqttTimeAvailable ? "yes" : "no"));

      // Local sensor data
      Serial.printf("[Data] Local Light: %.1f lux (lastRead %lu ms)\n",
                    localSensors.getLightIntensity(),
                    localSensors.getLastReadTime());

      // Remote sensor data if available
      if (remoteSensorDataAvailable) {
        unsigned long age = (millis() - lastRemoteDataReceived) / 1000;
        Serial.printf(
            "[Data] Remote: %s | Temp: %.2f C | Hum: %.2f %% | Press: %.2f hPa "
            "| UV: %.2f | Batt: %d%% | age: %lus\n",
            remoteSensorData.deviceName, remoteSensorData.temperature,
            remoteSensorData.humidity, remoteSensorData.pressure,
            remoteSensorData.uvIndex, remoteSensorData.batteryLevel, age);
      } else {
        Serial.println("[Data] Remote: No data available");
      }

      // Audio download status
      if (audio.isDownloading()) {
        float progress = audio.getDownloadProgress();
        if (progress >= 0) {
          Serial.printf("[Data] Audio Download: In Progress (%.1f%%)\n",
                        progress * 100);
        } else {
          Serial.println("[Data] Audio Download: In Progress");
        }
      } else {
        Serial.println("[Data] Audio Download: Idle");
      }

      delay(1000);  // Small delay for Serial output clarity
      lastSensorRead = now;
    }

    // Publish to MQTT every 10 seconds
    if ((now - lastPublish) >= publishInterval) {
      // If we are downloading audio, DO NOT publish sensors.
      // This prevents Core 1 (Sensors) from fighting Core 0 (Network) for the
      // MQTT client.
      if (audio.isDownloading()) {
        Serial.println(
            "[Sensors] Skipping publish (Audio Download in progress)");
      } else {
        localSensors.publishToMQTT(mqtt, MQTT_TOPIC_GATEWAY_LIGHT);
        publishRemoteSensorData();
      }

      lastPublish = now;
    }

    // Run at 100Hz (every 10ms)
    vTaskDelay(pdMS_TO_TICKS(50));  // Increased from 10ms to 50ms to save power
  }
}

// ============================================================================
// DISPLAY TASK - Update OLED display
// ============================================================================
void displayTask(void* parameter) {
  Serial.println("[RTOS] Display Task started on Core 1");

  int pageCounter = 0;
  const int PAGE_SWITCH_INTERVAL = 25;  // 5 seconds (25 * 200ms)

  for (;;) {
    // Update display
    displayManager.update();

    // Cycle to next page every 5 seconds
    pageCounter++;
    if (pageCounter >= PAGE_SWITCH_INTERVAL) {
      displayManager.nextPage();
      pageCounter = 0;
    }

    // Delay 200ms (5 FPS refresh rate)
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// ============================================================================
// ALARM TASK - Check alarms and trigger audio
// ============================================================================
void alarmTask(void* parameter) {
  Serial.println("[RTOS] Alarm Task started on Core 0");

  String lastMinute = "";

  for (;;) {
    // Get current time
    String currentTime = "";
    struct tm timeinfo;
    char timeBuf[32];

    // Prefer MQTT time, fallback to NTP time
    if (mqttTimeAvailable && mqttTime.length() > 0) {
      // Use MQTT time (extract last 8 chars if it contains date+time)
      String t = mqttTime;
      t.trim();
      if (t.length() > 8) {
        currentTime = t.substring(t.length() - 8);  // HH:MM:SS
      } else {
        currentTime = t;
      }
    } else if (getLocalTime(&timeinfo, 1000)) {
      // Use NTP time
      snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", timeinfo.tm_hour,
               timeinfo.tm_min, timeinfo.tm_sec);
      currentTime = String(timeBuf);
    }

    // Only proceed if we have a valid time
    if (currentTime.length() >= 5) {
      String currentMinute = currentTime.substring(0, 5);  // HH:MM

      // Clear triggered states when minute changes
      if (currentMinute != lastMinute) {
        alarmManager.clearTriggeredStates();
        lastMinute = currentMinute;
      }

      // Check if any alarm matches current time
      String matchedAlarm = alarmManager.checkAlarms(currentTime);
      if (matchedAlarm.length() > 0 && !alarmQuestion.isActive()) {
        // Alarm triggered!
        Serial.printf("⏰ ALARM TRIGGERED: %s\n", matchedAlarm.c_str());

        // Mark this alarm as triggered to prevent re-triggering
        alarmManager.setAlarmTriggered(matchedAlarm, true);

        // Request question if not already available
        String question = questionManager.getQuestionForAlarm();
        if (question.length() == 0) {
          Serial.println("[Alarm] Requesting question from server...");
          mqtt.publish("smartalarm/question/request", "");
          // Wait briefly for question
          vTaskDelay(pdMS_TO_TICKS(500));
          question = questionManager.getQuestionForAlarm();
        }

        if (question.length() > 0) {
          alarmQuestion.setQuestion(question);
          Serial.println("[Alarm] Starting question challenge mode");
          alarmQuestion.startQuestionSession();

          // Show question on OLED
          displayManager.showAlarmQuestion(
              question, alarmQuestion.getCurrentAttempt(),
              alarmQuestion.getMaxAttempts(), "Press button to answer");
        } else {
          Serial.println(
              "[Alarm] No question available, alarm will play without "
              "challenge");
        }

        // Play alarm sound at 100% volume
        audio.setVolume(1.0);
        String soundFile = alarmManager.getAlarmSound();
        if (audio.playFile(soundFile.c_str())) {
          Serial.printf("[Alarm] Playing alarm sound: %s\n", soundFile.c_str());
          mqtt.publish("smartalarm/alarm/triggered",
                       "Alarm triggered at " + matchedAlarm);
        } else {
          Serial.println("[Alarm] ERROR: Failed to play alarm sound!");
          mqtt.publish("smartalarm/alarm/error", "Failed to play alarm sound");
        }
      }

      // Handle alarm question states
      if (alarmQuestion.isActive()) {
        // Check timeout (50 seconds no activity)
        if (alarmQuestion.shouldTimeout()) {
          Serial.println("[Alarm] Timeout! Counting as wrong attempt");
          alarmQuestion.setValidationResult(false);  // Mark as wrong
          alarmQuestion.updateActivity();            // Reset timer
        }

        // Update display with current status
        displayManager.showAlarmQuestion(
            alarmQuestion.getQuestion(), alarmQuestion.getCurrentAttempt(),
            alarmQuestion.getMaxAttempts(), alarmQuestion.getStatusMessage());

        // Check if should stop alarm
        if (alarmQuestion.shouldDeactivateAlarm() && audio.playing()) {
          Serial.println(
              "[Alarm] Question answered correctly - Stopping alarm");
          audio.stop();
          mqtt.publish("smartalarm/alarm/deactivated",
                       "Question answered correctly");

          // Show success message
          displayManager.showAlarmQuestion(
              alarmQuestion.getQuestion(), alarmQuestion.getCurrentAttempt(),
              alarmQuestion.getMaxAttempts(), "CORRECT! ✓");

          vTaskDelay(pdMS_TO_TICKS(3000));  // Show for 3 seconds
          displayManager.returnToNormalDisplay();
          alarmQuestion.reset();
        }

        // Check if failed all attempts
        if (alarmQuestion.getState() == QUESTION_FAILED && audio.playing()) {
          Serial.println("[Alarm] Max attempts reached - Stopping alarm");
          audio.stop();
          mqtt.publish("smartalarm/alarm/deactivated", "Max attempts reached");

          // Show failure message
          displayManager.showAlarmQuestion(
              alarmQuestion.getQuestion(), alarmQuestion.getCurrentAttempt(),
              alarmQuestion.getMaxAttempts(), "Max attempts reached");

          vTaskDelay(pdMS_TO_TICKS(3000));  // Show for 3 seconds
          displayManager.returnToNormalDisplay();
          alarmQuestion.reset();
        }

        // Handle button for recording (only during alarm)
        static bool wasPressed = false;
        bool isPressed = button.isPressed();

        if (isPressed && !wasPressed) {
          // Button just pressed - start recording
          Serial.println("[Button] Pressed - Starting recording");

          // Update activity time
          alarmQuestion.updateActivity();

          // Turn on LED
          digitalWrite(2, HIGH);

          // Reduce volume to 30% during recording
          audio.setVolume(0.3);

          // Start recording
          alarmQuestion.startRecording();
          audioStream.startRecording();
          wasPressed = true;

          // Update display
          displayManager.showAlarmQuestion(
              alarmQuestion.getQuestion(), alarmQuestion.getCurrentAttempt(),
              alarmQuestion.getMaxAttempts(), "Recording...");
        } else if (!isPressed && wasPressed) {
          // Button just released - stop recording
          Serial.println("[Button] Released - Stopping recording");

          // Turn off LED
          digitalWrite(2, LOW);

          // Restore volume to 100%
          audio.setVolume(1.0);

          // Stop recording
          audioStream.stopRecording();
          alarmQuestion.stopRecording();
          wasPressed = false;

          // Update display
          displayManager.showAlarmQuestion(
              alarmQuestion.getQuestion(), alarmQuestion.getCurrentAttempt(),
              alarmQuestion.getMaxAttempts(), "Validating...");
        }
      }
    }

    // Check every 100ms for responsive button handling
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void initRTOSTasks() {
  Serial.println("\n[RTOS] Initializing task queues...");

  // Create queues for audio streaming
  mqttQueue =
      xQueueCreate(MQTT_QUEUE_SIZE, sizeof(void*));  // Pointer to message

  if (audioTxQueue == NULL || audioRxQueue == NULL || mqttQueue == NULL) {
    Serial.println("[RTOS] ERROR: Failed to create queues!");
    return;
  }

  Serial.println("[RTOS] ✓ Queues created successfully");
}

void startRTOSTasks() {
  Serial.println("\n[RTOS] Starting tasks...\n");

  // ========== CORE 1: Audio & Display ==========

  // Audio decode - CRITICAL priority on Core 1
  xTaskCreatePinnedToCore(audioDecodeTask, "AudioDecode", STACK_SIZE_AUDIO,
                          NULL, PRIORITY_AUDIO_DECODE, &audioDecodeTaskHandle,
                          1  // Core 1
  );

  // Audio encode - CRITICAL priority on Core 1 (suspended until needed)
  xTaskCreatePinnedToCore(audioEncodeTask, "AudioEncode", STACK_SIZE_AUDIO,
                          NULL, PRIORITY_AUDIO_ENCODE, &audioEncodeTaskHandle,
                          1  // Core 1
  );

  // Sensor reading - NORMAL priority on Core 1
  xTaskCreatePinnedToCore(sensorTask, "Sensors", STACK_SIZE_SENSOR, NULL,
                          PRIORITY_SENSOR_READ, &sensorTaskHandle,
                          1  // Core 1
  );

  // Display updates - NORMAL priority on Core 1
  xTaskCreatePinnedToCore(displayTask, "Display", STACK_SIZE_DISPLAY, NULL,
                          PRIORITY_DISPLAY, &displayTaskHandle,
                          1  // Core 1
  );

  // Alarm checking - NORMAL priority on Core 0
  xTaskCreatePinnedToCore(alarmTask, "AlarmCheck", STACK_SIZE_ALARM, NULL,
                          PRIORITY_ALARM_CHECK, &alarmTaskHandle,
                          0  // Core 0 (same core as MQTT/Network)
  );

  // ========== CORE 0: Network & Communication ==========

  // MQTT - HIGH priority on Core 0
  xTaskCreatePinnedToCore(mqttTask, "MQTT", STACK_SIZE_NETWORK, NULL,
                          PRIORITY_MQTT, &mqttTaskHandle,
                          0  // Core 0 (same core as WiFi stack)
  );
}
