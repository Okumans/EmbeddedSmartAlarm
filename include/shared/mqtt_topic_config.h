// MQTT Topic Configuration
// Centralized definitions for all MQTT topics used in the Smart Alarm Clock
// system
#ifndef MQTT_TOPIC_CONFIG_H
#define MQTT_TOPIC_CONFIG_H

#include <Arduino.h>

// ============================================================================
// GATEWAY SENSORS (ESP32) - Local/Inside Sensors
// ============================================================================
static const char* MQTT_TOPIC_GATEWAY_TEMP =
    "smartalarm/gateway/temperature/inside";
static const char* MQTT_TOPIC_GATEWAY_HUMIDITY =
    "smartalarm/gateway/humidity/inside";
static const char* MQTT_TOPIC_GATEWAY_LIGHT = "smartalarm/gateway/light/inside";
static const char* MQTT_TOPIC_STATUS = "smartalarm/gateway/status";

// ============================================================================
// REMOTE SENSORS (NodeMCU) - Outside Sensors
// ============================================================================
static const char* MQTT_TOPIC_REMOTE_TEMP =
    "smartalarm/sensor/temperature/outside";
static const char* MQTT_TOPIC_REMOTE_HUMIDITY =
    "smartalarm/sensor/humidity/outside";
static const char* MQTT_TOPIC_REMOTE_PRESSURE =
    "smartalarm/sensor/pressure/outside";
static const char* MQTT_TOPIC_REMOTE_UV = "smartalarm/sensor/uvindex/outside";
static const char* MQTT_TOPIC_REMOTE_BATTERY =
    "smartalarm/sensor/battery/outside";
static const char* MQTT_TOPIC_REMOTE_STATUS = "smartalarm/sensor/status";

// ============================================================================
// AUDIO UPLOAD TOPICS (Gateway <-> Uploader Communication)
// ============================================================================
static const char* MQTT_TOPIC_AUDIO_REQUEST =
    "esp32/audio_request";  // uploader -> gateway (REQUEST_FREE_SPACE)
static const char* MQTT_TOPIC_AUDIO_CHUNK =
    "esp32/audio_chunk";  // uploader -> gateway (START/CHUNK/END)
static const char* MQTT_TOPIC_AUDIO_RESPONSE =
    "esp32/audio_response";  // gateway -> uploader (FREE:xxx)
static const char* MQTT_TOPIC_AUDIO_ACK =
    "esp32/audio_ack";  // gateway -> uploader (ACK:<chunk_index>)

// ============================================================================
// AUDIO STATUS TOPICS (Gateway -> Server)
// ============================================================================
static const char* MQTT_TOPIC_AUDIO_STATUS =
    "esp32/audio_status";  // gateway -> server (playing/finished)

// ============================================================================
// ALARM QUESTION TOPICS (LLM/STT Integration)
// ============================================================================
static const char* MQTT_TOPIC_QUESTION =
    "smartalarm/question";  // server -> gateway (trivia question)
static const char* MQTT_TOPIC_QUESTION_STATUS =
    "smartalarm/question/status";  // gateway -> server (received)
static const char* MQTT_TOPIC_ANSWER_AUDIO =
    "smartalarm/answer/audio";  // gateway -> server (audio recording chunks)
static const char* MQTT_TOPIC_ANSWER_TEXT =
    "smartalarm/answer/text";  // server -> gateway (transcribed text)
static const char* MQTT_TOPIC_ANSWER_VALIDATION =
    "smartalarm/answer/validation";  // server -> gateway (valid/invalid)
static const char* MQTT_TOPIC_RECORDING_START =
    "smartalarm/recording/start";  // gateway -> server (start recording)
static const char* MQTT_TOPIC_RECORDING_STOP =
    "smartalarm/recording/stop";  // gateway -> server (stop recording)
static const char* MQTT_TOPIC_ALARM_DEACTIVATE =
    "smartalarm/alarm/deactivate";  // system (alarm stopped)

#endif  // MQTT_TOPIC_CONFIG_H