#include "../../include/rtos_tasks.h"

#include "../../include/display_manager.h"
#include "../../include/mqtt_manager.h"
#include "../../include/sensor_manager.h"
#include "audio_manager.h"

// External references to global objects (from main.cpp)
extern AudioManager audio;
extern MQTTManager mqtt;
extern SensorManager localSensors;
extern DisplayManager displayManager;
extern void publishRemoteSensorData();

// Task handles
TaskHandle_t audioDecodeTaskHandle = NULL;
TaskHandle_t audioEncodeTaskHandle = NULL;
TaskHandle_t websocketTaskHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;

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

  // Reserved for future INMP441 Microphone implementation
  vTaskDelete(NULL);
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

  const TickType_t sensorInterval = pdMS_TO_TICKS(2000);    // 2 seconds
  const TickType_t publishInterval = pdMS_TO_TICKS(10000);  // 10 seconds

  TickType_t lastSensorRead = xTaskGetTickCount();
  TickType_t lastPublish = xTaskGetTickCount();

  for (;;) {
    TickType_t now = xTaskGetTickCount();

    // Read sensors every 2 seconds
    if ((now - lastSensorRead) >= sensorInterval) {
      localSensors.readSensors();
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
        extern const char* MQTT_TOPIC_GATEWAY_TEMP;
        extern const char* MQTT_TOPIC_GATEWAY_HUMIDITY;

        localSensors.publishToMQTT(mqtt, MQTT_TOPIC_GATEWAY_TEMP,
                                   MQTT_TOPIC_GATEWAY_HUMIDITY);
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

  // ========== CORE 0: Network & Communication ==========

  // MQTT - HIGH priority on Core 0
  xTaskCreatePinnedToCore(mqttTask, "MQTT", STACK_SIZE_NETWORK, NULL,
                          PRIORITY_MQTT, &mqttTaskHandle,
                          0  // Core 0 (same core as WiFi stack)
  );

  Serial.println("[RTOS] ✓ All tasks created successfully\n");
  Serial.println("========================================");
  Serial.println("Task Assignment:");
  Serial.println("========================================");
  Serial.println("Core 0 (Network):");
  Serial.println("  - WiFi Stack (system)");
  Serial.println("  - MQTT Task (priority 2)");
  Serial.println("\nCore 1 (Audio/Display):");
  Serial.println("  - Audio Decode (priority 3)");
  Serial.println("  - Audio Encode (priority 3, suspended)");
  Serial.println("  - Sensor Task (priority 1)");
  Serial.println("  - Display Task (priority 1)");
  Serial.println("========================================\n");
}
