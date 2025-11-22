// Centralized configuration for WiFi, MQTT, and system settings
#ifndef SMARTALARM_CONFIG_H
#define SMARTALARM_CONFIG_H

#include <Arduino.h>

#include "mqtt_topic_config.h"

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

#endif  // SMARTALARM_CONFIG_H
