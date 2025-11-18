#include "../../include/rtos_tasks.h"

#include <WebSocketsServer.h>

#include "../../include/mqtt_manager.h"
#include "../../include/sensor_manager.h"
#include "audio_manager.h"

// External references to global objects (from main.cpp)
extern AudioManager audio;
extern MQTTManager mqtt;
extern SensorManager localSensors;
extern void updateDisplay();
extern void publishRemoteSensorData();

// WebSocket Server (Port 81)
WebSocketsServer webSocketServer(WEBSOCKET_SERVER_PORT);

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

// StreamBuffer for Opus audio streaming
StreamBufferHandle_t opusStreamBuffer = NULL;

// ============================================================================
// AUDIO DECODE TASK - Play incoming audio stream
// ============================================================================
void audioDecodeTask(void* parameter) {
  Serial.println("[RTOS] Audio Decode Task started on Core 1");

  AudioPacket packet;

  for (;;) {
    // Check if we're in streaming mode
    if (audio.isStreaming()) {
      // Process streaming audio (Opus decoding + I2S output)
      audio.processStreamingAudio();
    } else {
      // Normal MP3 playback mode
      audio.loop();
    }

    // Check for incoming audio packets from queue (future use for bidirectional
    // streaming)
    if (audioRxQueue != NULL &&
        xQueueReceive(audioRxQueue, &packet, 0) == pdTRUE) {
      if (packet.isValid) {
        Serial.printf("[Audio RX] Received %d bytes at %lu\n", packet.length,
                      packet.timestamp);
        // This could be used for future bidirectional audio
      }
    }

    // Delay to prevent power issues and watchdog timeout
    // When streaming, run faster for low-latency
    if (audio.isStreaming()) {
      vTaskDelay(pdMS_TO_TICKS(5));  // 5ms for streaming (200Hz)
    } else {
      vTaskDelay(pdMS_TO_TICKS(10));  // 10ms for MP3 playback
    }
  }
}

// ============================================================================
// AUDIO ENCODE TASK - Encode microphone input for streaming
// ============================================================================
void audioEncodeTask(void* parameter) {
  Serial.println("[RTOS] Audio Encode Task started on Core 1");

  // This task will be suspended until streaming is enabled
  vTaskSuspend(NULL);

  for (;;) {
    // TODO: Implement when adding microphone input
    // 1. Read I2S microphone data
    // 2. Encode to Opus
    // 3. Send to audioTxQueue
    // 4. WebSocket task picks it up

    vTaskDelay(pdMS_TO_TICKS(20));  // 20ms frames (typical for Opus)
  }
}

// ============================================================================
// WEBSOCKET EVENT CALLBACK - The "Producer"
// ============================================================================
void webSocketEvent(uint8_t clientNum, WStype_t type, uint8_t* payload,
                    size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Client #%u disconnected\n", clientNum);
      break;

    case WStype_CONNECTED: {
      IPAddress ip = webSocketServer.remoteIP(clientNum);
      Serial.printf("[WebSocket] Client #%u connected from %d.%d.%d.%d\n",
                    clientNum, ip[0], ip[1], ip[2], ip[3]);
      break;
    }

    case WStype_BIN: {
      // Binary data received - this is our Opus audio stream!
      if (opusStreamBuffer == NULL) {
        Serial.println("[WebSocket] ERROR: StreamBuffer not initialized!");
        return;
      }

      // Check if we have space in the buffer
      size_t availableSpace = xStreamBufferSpacesAvailable(opusStreamBuffer);
      size_t requiredSpace = PACKET_HEADER_SIZE + length;

      if (availableSpace < requiredSpace) {
        // Buffer overflow - drop packet (better than crashing)
        Serial.printf(
            "[WebSocket] Buffer full! Dropping %u bytes (available: %u)\n",
            length, availableSpace);
        return;
      }

      // Write packet length header (2 bytes, little-endian)
      uint16_t packetLength = (uint16_t)length;
      size_t written = xStreamBufferSend(opusStreamBuffer, &packetLength,
                                         PACKET_HEADER_SIZE, 0);

      if (written != PACKET_HEADER_SIZE) {
        Serial.println("[WebSocket] ERROR: Failed to write packet header!");
        return;
      }

      // Write the actual Opus payload
      written = xStreamBufferSend(opusStreamBuffer, payload, length, 0);

      if (written != length) {
        Serial.printf(
            "[WebSocket] WARNING: Partial write! Expected %u, wrote %u\n",
            length, written);
        return;
      }

      // Success - packet is now in the buffer for the Audio Decode Task
      // (Uncomment for debugging - will be very verbose)
      // Serial.printf("[WebSocket] Received %u bytes, buffer level: %u/%u\n",
      //               length, BUFFER_SIZE_BYTES - availableSpace,
      //               BUFFER_SIZE_BYTES);
      break;
    }

    case WStype_TEXT:
      // Text messages not used in this implementation
      Serial.printf("[WebSocket] Text message from #%u: %s\n", clientNum,
                    payload);
      break;

    default:
      break;
  }
}

// ============================================================================
// WEBSOCKET TASK - Handle bidirectional audio streaming
// ============================================================================
void websocketTask(void* parameter) {
  Serial.println("[RTOS] WebSocket Task started on Core 0");

  // Initialize WebSocket server
  webSocketServer.begin();
  webSocketServer.onEvent(webSocketEvent);
  Serial.printf("[WebSocket] Server started on port %d\n",
                WEBSOCKET_SERVER_PORT);

  for (;;) {
    // Process WebSocket events (handles incoming connections and data)
    webSocketServer.loop();

    // TODO: Handle outgoing audio (microphone streaming)
    // This will be implemented when adding microphone support
    /*
    AudioPacket txPacket;
    if (xQueueReceive(audioTxQueue, &txPacket, 0) == pdTRUE) {
      // Send to all connected clients
      webSocketServer.broadcastBIN(txPacket.data, txPacket.length);
    }
    */

    // Run at 200Hz (5ms) for low-latency streaming
    vTaskDelay(pdMS_TO_TICKS(5));
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

    // Run at 50Hz (every 20ms) - reduced from 5ms to save power
    vTaskDelay(pdMS_TO_TICKS(20));
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
      extern const char* MQTT_TOPIC_GATEWAY_TEMP;
      extern const char* MQTT_TOPIC_GATEWAY_HUMIDITY;

      localSensors.publishToMQTT(mqtt, MQTT_TOPIC_GATEWAY_TEMP,
                                 MQTT_TOPIC_GATEWAY_HUMIDITY);
      publishRemoteSensorData();
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

  const TickType_t displayInterval = pdMS_TO_TICKS(200);  // 5Hz (every 200ms)
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    // Update display
    updateDisplay();

    // Run at 10Hz (every 100ms)
    vTaskDelayUntil(&xLastWakeTime, displayInterval);
  }
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void initRTOSTasks() {
  Serial.println("\n[RTOS] Initializing task queues...");

  // Create queues for audio streaming
  audioTxQueue = xQueueCreate(AUDIO_TX_QUEUE_SIZE, sizeof(AudioPacket));
  audioRxQueue = xQueueCreate(AUDIO_RX_QUEUE_SIZE, sizeof(AudioPacket));
  mqttQueue =
      xQueueCreate(MQTT_QUEUE_SIZE, sizeof(void*));  // Pointer to message

  if (audioTxQueue == NULL || audioRxQueue == NULL || mqttQueue == NULL) {
    Serial.println("[RTOS] ERROR: Failed to create queues!");
    return;
  }

  Serial.println("[RTOS] ✓ Queues created successfully");

  // Create StreamBuffer for Opus audio streaming
  Serial.println("[RTOS] Initializing StreamBuffer...");
  opusStreamBuffer =
      xStreamBufferCreate(BUFFER_SIZE_BYTES, STREAM_BUFFER_TRIGGER_LEVEL);

  if (opusStreamBuffer == NULL) {
    Serial.println("[RTOS] ERROR: Failed to create StreamBuffer!");
    return;
  }

  Serial.printf("[RTOS] ✓ StreamBuffer created (%d bytes, trigger: %d)\n",
                BUFFER_SIZE_BYTES, STREAM_BUFFER_TRIGGER_LEVEL);
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

  // WebSocket - HIGH priority on Core 0 (now active for streaming)
  xTaskCreatePinnedToCore(websocketTask, "WebSocket", STACK_SIZE_NETWORK, NULL,
                          PRIORITY_WEBSOCKET, &websocketTaskHandle,
                          0  // Core 0 (same core as WiFi stack)
  );

  Serial.println("[RTOS] ✓ All tasks created successfully\n");
  Serial.println("========================================");
  Serial.println("Task Assignment:");
  Serial.println("========================================");
  Serial.println("Core 0 (Network):");
  Serial.println("  - WiFi Stack (system)");
  Serial.println("  - MQTT Task (priority 2)");
  Serial.println("  - WebSocket Task (priority 2, ACTIVE)");
  Serial.println("\nCore 1 (Audio/Display):");
  Serial.println("  - Audio Decode (priority 3)");
  Serial.println("  - Audio Encode (priority 3, suspended)");
  Serial.println("  - Sensor Task (priority 1)");
  Serial.println("  - Display Task (priority 1)");
  Serial.println("========================================\n");
}
