#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>

#include <vector>

#include "mqtt_handler.h"

class MQTTManager {
 private:
  PubSubClient* client;
  std::vector<MQTTHandler> handlers;
  std::vector<String> subscribedTopics;

  String clientId;
  String statusTopic;
  bool firstConnection;
  unsigned long lastReconnectAttempt;

  // Wildcard matching helper
  bool topicMatches(const String& pattern, const String& topic);

  // Static callback bridge (required by PubSubClient)
  static void globalCallback(char* topic, byte* payload, unsigned int length);
  static MQTTManager* instance;

 public:
  MQTTManager();
  ~MQTTManager();

  // Setup - initialize with client and optional connection settings
  void begin(PubSubClient* mqttClient, const String& clientID = "ESP32Client",
             const String& statusTopic = "");

  // Loop - call this in main loop() to maintain connection
  void loop();

  // Register a handler for a topic pattern
  void registerHandler(const String& topicPattern, MQTTHandlerFunc callback,
                       const String& name = "", uint8_t priority = 100);

  // Convenience: Register and subscribe in one call
  void registerAndSubscribe(const String& topicPattern,
                            MQTTHandlerFunc callback, const String& name = "",
                            uint8_t priority = 100);

  // Remove a handler by pattern
  void unregisterHandler(const String& topicPattern);

  // Manual message dispatch (useful for testing)
  void dispatch(const char* topic, byte* payload, unsigned int length);

  // Subscribe/Unsubscribe to topics
  bool subscribe(const String& topic);
  bool unsubscribe(const String& topic);

  // Subscribe to all registered handler topics (called after connection)
  void subscribeAll();

  // Publish helpers - handlers can call these
  bool publish(const String& topic, const String& message, bool retain = false);
  bool publish(const String& topic, const char* message, bool retain = false);
  bool publish(const String& topic, byte* payload, unsigned int length,
               bool retain = false);

  // Connection status
  bool isConnected() const;

  // Reconnect if disconnected (called internally by loop())
  bool reconnect();

  // Get underlying client (for advanced usage like setBufferSize)
  PubSubClient* getClient() { return client; }
};

#endif  // MQTT_MANAGER_H
