// Centralized configuration for WiFi, MQTT, and system settings
#ifndef SMARTALARM_CONFIG_H
#define SMARTALARM_CONFIG_H

#include <Arduino.h>

// WiFi (station) credentials
static const char* WIFI_SSID = "Pleaseconnecttome";
static const char* WIFI_PASSWORD = "n1234567!";

// Soft AP (for ESP-NOW) credentials
static const char* SOFT_AP_SSID = "SmartAlarm-Gateway";
static const char* SOFT_AP_PASSWORD = "12345678";

// WiFi channel used for ESP-NOW (default)
#define WIFI_CHANNEL 6

// MQTT broker
static const char* MQTT_SERVER = "broker.hivemq.com";
static const int MQTT_PORT = 1883;
static const char* MQTT_CLIENT_ID = "SmartAlarmClock";

// Audio upload topics (gateway <-> uploader)
static const char* MQTT_TOPIC_AUDIO_REQUEST =
    "esp32/audio_request";  // uploader -> gateway (REQUEST_FREE_SPACE)
static const char* MQTT_TOPIC_AUDIO_CHUNK =
    "esp32/audio_chunk";  // uploader -> gateway (START/CHUNK/END)
static const char* MQTT_TOPIC_AUDIO_RESPONSE =
    "esp32/audio_response";  // gateway -> uploader (FREE:xxx)
static const char* MQTT_TOPIC_AUDIO_ACK =
    "esp32/audio_ack";  // gateway -> uploader (ACK:<chunk_index>)

// Audio status topics (gateway -> server)
static const char* MQTT_TOPIC_AUDIO_STATUS =
    "esp32/audio_status";  // gateway -> server (playing/finished)

#endif  // SMARTALARM_CONFIG_H
