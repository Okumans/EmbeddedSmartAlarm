#include "../../include/mqtt_manager.h"

#include <algorithm>

MQTTManager* MQTTManager::instance = nullptr;

MQTTManager::MQTTManager()
    : client(nullptr), firstConnection(true), lastReconnectAttempt(0) {
  instance = this;
}

MQTTManager::~MQTTManager() { instance = nullptr; }

void MQTTManager::begin(PubSubClient* mqttClient, const String& clientID,
                        const String& statusTopic) {
  client = mqttClient;
  client->setCallback(globalCallback);
  clientId = clientID;
  this->statusTopic = statusTopic;
  firstConnection = true;
  Serial.println("[MQTTManager] Initialized");
}

void MQTTManager::registerHandler(const String& topicPattern,
                                  MQTTHandlerFunc callback, const String& name,
                                  uint8_t priority) {
  handlers.push_back(MQTTHandler(topicPattern, callback, name, priority));

  // Sort by priority (descending - higher priority first)
  std::sort(handlers.begin(), handlers.end(),
            [](const MQTTHandler& a, const MQTTHandler& b) {
              return a.priority > b.priority;
            });

  Serial.printf(
      "[MQTTManager] Registered handler '%s' for pattern '%s' (priority: %d)\n",
      name.c_str(), topicPattern.c_str(), priority);
}

void MQTTManager::registerAndSubscribe(const String& topicPattern,
                                       MQTTHandlerFunc callback,
                                       const String& name, uint8_t priority) {
  registerHandler(topicPattern, callback, name, priority);
  subscribe(topicPattern);
}

void MQTTManager::unregisterHandler(const String& topicPattern) {
  auto initialSize = handlers.size();
  handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
                                [&topicPattern](const MQTTHandler& h) {
                                  return h.topicPattern == topicPattern;
                                }),
                 handlers.end());

  if (handlers.size() < initialSize) {
    Serial.printf("[MQTTManager] Unregistered handler for pattern '%s'\n",
                  topicPattern.c_str());
  }
}

bool MQTTManager::topicMatches(const String& pattern, const String& topic) {
  // MQTT wildcard matching
  // + = single level wildcard (e.g., "smartalarm/+/temp")
  // # = multi-level wildcard (e.g., "smartalarm/#")

  // Exact match - fast path
  if (pattern == topic) return true;

  // No wildcards - exact match only
  if (pattern.indexOf('+') == -1 && pattern.indexOf('#') == -1) {
    return false;
  }

  // Split by '/' and compare level by level
  int patternIdx = 0, topicIdx = 0;
  int patternLen = pattern.length(), topicLen = topic.length();

  while (patternIdx < patternLen && topicIdx < topicLen) {
    // Check for multi-level wildcard
    if (pattern[patternIdx] == '#') {
      // # must be last character or followed by '/'
      if (patternIdx == patternLen - 1 || pattern[patternIdx + 1] == '/') {
        return true;  // # matches everything remaining
      }
    }

    // Find next separator
    int patternSep = pattern.indexOf('/', patternIdx);
    int topicSep = topic.indexOf('/', topicIdx);

    if (patternSep == -1) patternSep = patternLen;
    if (topicSep == -1) topicSep = topicLen;

    String patternLevel = pattern.substring(patternIdx, patternSep);
    String topicLevel = topic.substring(topicIdx, topicSep);

    // Single level wildcard
    if (patternLevel == "+") {
      // Matches any single level
    }
    // Exact match required
    else if (patternLevel != topicLevel) {
      return false;
    }

    // Move to next level
    patternIdx = patternSep + 1;
    topicIdx = topicSep + 1;
  }

  // Both should be fully consumed
  return patternIdx >= patternLen && topicIdx >= topicLen;
}

void MQTTManager::dispatch(const char* topic, byte* payload,
                           unsigned int length) {
  Serial.printf("[MQTTManager] Dispatching message on topic: %s (%u bytes)\n",
                topic, length);

  String topicStr = String(topic);
  bool handled = false;

  // Try each handler in priority order
  for (auto& handler : handlers) {
    if (topicMatches(handler.topicPattern, topicStr)) {
      Serial.printf(
          "[MQTTManager] → Trying handler '%s' (pattern: '%s', priority: %d)\n",
          handler.name.c_str(), handler.topicPattern.c_str(), handler.priority);

      // Pass *this (MQTTManager reference) to the handler
      if (handler.callback(*this, topic, payload, length)) {
        Serial.printf("[MQTTManager] ✓ Handled by '%s'\n",
                      handler.name.c_str());
        handled = true;
        break;  // Stop at first handler that returns true
      }
    }
  }

  if (!handled) {
    Serial.printf("[MQTTManager] ⚠ No handler processed topic: %s\n", topic);
  }
}

void MQTTManager::globalCallback(char* topic, byte* payload,
                                 unsigned int length) {
  if (instance) {
    instance->dispatch(topic, payload, length);
  } else {
    Serial.println("[MQTTManager] ERROR: No instance available for callback!");
  }
}

bool MQTTManager::subscribe(const String& topic) {
  if (client && client->connected()) {
    bool result = client->subscribe(topic.c_str());
    if (result) {
      // Track subscribed topics
      if (std::find(subscribedTopics.begin(), subscribedTopics.end(), topic) ==
          subscribedTopics.end()) {
        subscribedTopics.push_back(topic);
      }
    }
    Serial.printf("[MQTTManager] Subscribe to '%s': %s\n", topic.c_str(),
                  result ? "✓ OK" : "✗ FAILED");
    return result;
  }

  Serial.printf("[MQTTManager] Cannot subscribe to '%s': not connected\n",
                topic.c_str());
  return false;
}

void MQTTManager::subscribeAll() {
  Serial.println(
      "[MQTTManager] Subscribing to all registered handler topics...");

  for (const auto& handler : handlers) {
    subscribe(handler.topicPattern);
  }
}

bool MQTTManager::unsubscribe(const String& topic) {
  if (client && client->connected()) {
    bool result = client->unsubscribe(topic.c_str());
    Serial.printf("[MQTTManager] Unsubscribe from '%s': %s\n", topic.c_str(),
                  result ? "✓ OK" : "✗ FAILED");
    return result;
  }
  return false;
}

bool MQTTManager::publish(const String& topic, const String& message,
                          bool retain) {
  return publish(topic, message.c_str(), retain);
}

bool MQTTManager::publish(const String& topic, const char* message,
                          bool retain) {
  if (client && client->connected()) {
    bool result = client->publish(topic.c_str(), message, retain);
    if (!result) {
      Serial.printf("[MQTTManager] ✗ Failed to publish to '%s'\n",
                    topic.c_str());
    }
    return result;
  }
  Serial.printf("[MQTTManager] Cannot publish to '%s': not connected\n",
                topic.c_str());
  return false;
}

bool MQTTManager::publish(const String& topic, byte* payload,
                          unsigned int length, bool retain) {
  if (client && client->connected()) {
    bool result = client->publish(topic.c_str(), payload, length, retain);
    if (!result) {
      Serial.printf("[MQTTManager] ✗ Failed to publish %u bytes to '%s'\n",
                    length, topic.c_str());
    }
    return result;
  }
  Serial.printf("[MQTTManager] Cannot publish to '%s': not connected\n",
                topic.c_str());
  return false;
}

bool MQTTManager::isConnected() const { return client && client->connected(); }

// ============================================================================
// Connection Management
// ============================================================================

bool MQTTManager::reconnect() {
  if (!client) {
    Serial.println("[MQTTManager] ERROR: Client not initialized!");
    return false;
  }

  if (client->connected()) {
    return true;  // Already connected
  }

  Serial.print("[MQTTManager] Attempting connection");
  if (clientId.length() > 0) {
    Serial.printf(" (Client ID: %s)", clientId.c_str());
  }
  Serial.print("...");

  bool connected = false;
  if (clientId.length() > 0) {
    connected = client->connect(clientId.c_str());
  } else {
    connected = client->connect("ESP32Client");
  }

  if (connected) {
    Serial.println(" connected!");

    // Publish online status if topic is set
    if (statusTopic.length() > 0) {
      publish(statusTopic, "online", true);  // Retained message
      Serial.printf("[MQTTManager] Published status to '%s'\n",
                    statusTopic.c_str());
    }

    // Subscribe to all registered handler topics on first connection
    if (firstConnection) {
      subscribeAll();
      firstConnection = false;
    } else {
      // Resubscribe to previously subscribed topics on reconnection
      Serial.println(
          "[MQTTManager] Resubscribing to topics after reconnection...");
      for (const auto& topic : subscribedTopics) {
        client->subscribe(topic.c_str());
        Serial.printf("[MQTTManager] Resubscribed to '%s'\n", topic.c_str());
      }
    }

    return true;
  } else {
    Serial.print(" failed, rc=");
    Serial.println(client->state());
    return false;
  }
}

void MQTTManager::loop() {
  if (!client) {
    return;
  }

  // Maintain MQTT connection
  if (!client->connected()) {
    unsigned long now = millis();
    // Try to reconnect every 5 seconds
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      reconnect();
    }
  } else {
    // Process MQTT messages
    client->loop();
  }
}
