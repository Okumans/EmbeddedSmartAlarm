#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>

#include <functional>

// Forward declaration
class MQTTManager;

// Handler function signature
// Returns true if message was handled, false otherwise
// Includes MQTTManager reference so handlers can publish responses
using MQTTHandlerFunc = std::function<bool(MQTTManager& mqtt, const char* topic,
                                           byte* payload, unsigned int length)>;

// Handler metadata
struct MQTTHandler {
  String
      topicPattern;  // Can use wildcards: "smartalarm/+/temp", "smartalarm/#"
  MQTTHandlerFunc callback;
  String name;       // For debugging/logging
  uint8_t priority;  // Higher = processed first (0-255)

  MQTTHandler(const String& pattern, MQTTHandlerFunc cb,
              const String& handlerName = "", uint8_t prio = 100)
      : topicPattern(pattern),
        callback(cb),
        name(handlerName.length() > 0 ? handlerName : pattern),
        priority(prio) {}
};

#endif  // MQTT_HANDLER_H
