#ifndef MQTT_TIME_H
#define MQTT_TIME_H

#include <Arduino.h>

// Stores the last time string received over MQTT (topic: smartalarm/time)
extern String mqttTime;
extern bool mqttTimeAvailable;

#endif  // MQTT_TIME_H
