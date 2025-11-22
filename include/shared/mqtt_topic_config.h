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

#endif  // MQTT_TOPIC_CONFIG_H