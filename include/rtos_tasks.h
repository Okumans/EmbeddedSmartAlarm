#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// Task priorities (higher = more important)
#define PRIORITY_AUDIO_DECODE 2    // High: decode audio for playback
#define PRIORITY_AUDIO_ENCODE 2    // High: encode audio for streaming
#define PRIORITY_WEBSOCKET 2       // High: WebSocket I/O
#define PRIORITY_MQTT 1            // Normal: MQTT communication
#define PRIORITY_SENSOR_READ 1     // Normal: sensor reading
#define PRIORITY_DISPLAY 1         // Normal: display updates
#define PRIORITY_SENSOR_PUBLISH 1  // Normal: sensor publishing

// Stack sizes (in words, not bytes!) - Reduced to prevent power issues
#define STACK_SIZE_AUDIO 3072    // Audio processing
#define STACK_SIZE_NETWORK 3072  // WebSocket/MQTT networking
#define STACK_SIZE_SENSOR 2048   // Sensors
#define STACK_SIZE_DISPLAY 2048  // Display

// Queue sizes for audio streaming
#define AUDIO_TX_QUEUE_SIZE 5  // Outgoing audio packets (reduced)
#define AUDIO_RX_QUEUE_SIZE 5  // Incoming audio packets (reduced)
#define MQTT_QUEUE_SIZE 3      // MQTT messages (reduced)

// Audio streaming packet structure
struct AudioPacket {
  uint8_t data[256];  // Opus frame (reduced from 512 to save memory)
  size_t length;
  uint32_t timestamp;
  bool isValid;
};

// Task handles (for suspend/resume control)
extern TaskHandle_t audioDecodeTaskHandle;
extern TaskHandle_t audioEncodeTaskHandle;
extern TaskHandle_t websocketTaskHandle;
extern TaskHandle_t mqttTaskHandle;
extern TaskHandle_t sensorTaskHandle;
extern TaskHandle_t displayTaskHandle;

// Queues for inter-task communication
extern QueueHandle_t audioTxQueue;  // Local mic → WebSocket → Server
extern QueueHandle_t audioRxQueue;  // Server → WebSocket → Speaker
extern QueueHandle_t mqttQueue;     // MQTT messages

// Task functions
void audioDecodeTask(void* parameter);  // Decode incoming audio & play
void audioEncodeTask(void* parameter);  // Encode mic input for streaming
void websocketTask(void* parameter);    // Handle WebSocket communication
void mqttTask(void* parameter);         // Handle MQTT communication
void sensorTask(void* parameter);       // Read sensors periodically
void displayTask(void* parameter);      // Update display periodically

// Initialization
void initRTOSTasks();
void startRTOSTasks();

#endif  // RTOS_TASKS_H
