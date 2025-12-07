#ifndef QUESTION_MANAGER_H
#define QUESTION_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>

#include "mqtt_manager.h"

/**
 * QuestionManager - Manages trivia question cache with LittleFS persistence
 *
 * Features:
 * - Persistent storage of up to 10 questions in /questions.txt
 * - Automatic refill when cache drops below threshold
 * - Random question selection with usage tracking
 * - MQTT integration for requesting questions from Python service
 * - Fallback questions if offline
 */
class QuestionManager {
 private:
  // ============================================================================
  // Configuration
  // ============================================================================
  static const int MAX_QUESTIONS_CACHE = 10;     // Maximum questions to cache
  static const int MIN_QUESTIONS_THRESHOLD = 3;  // Request more when below this
  static const int MAX_QUESTION_LENGTH = 256;    // Max chars per question
  static const char* QUESTIONS_FILE;             // File path: "/questions.txt"
  static const unsigned long REQUEST_COOLDOWN =
      5000;  // Min 5s between requests

  // ============================================================================
  // Fallback Questions (hardcoded for offline use)
  // ============================================================================
  static const char* FALLBACK_QUESTIONS[];
  static const int FALLBACK_COUNT;

  // ============================================================================
  // Cache Storage
  // ============================================================================
  String questionCache[MAX_QUESTIONS_CACHE];
  int questionCount;  // Current number of cached questions
  int lastUsedIndex;  // Track last used question

  // ============================================================================
  // State Tracking
  // ============================================================================
  unsigned long lastRequestTime;  // Timestamp of last MQTT request
  bool requestInProgress;         // Flag to prevent duplicate requests
  bool initialized;               // System initialization status

  // ============================================================================
  // File Operations
  // ============================================================================
  bool loadQuestionsFromFile();
  bool saveQuestionsToFile();
  void addQuestionToCache(const String& question);
  bool isQuestionValid(const String& question);

  // ============================================================================
  // Question Selection
  // ============================================================================
  String selectRandomQuestion();
  void markQuestionUsed(int index);
  String getFallbackQuestion();

  // ============================================================================
  // Request Management
  // ============================================================================
  bool canRequestQuestions();
  void setRequestInProgress(bool inProgress);

 public:
  // ============================================================================
  // Constructor
  // ============================================================================
  QuestionManager();

  // ============================================================================
  // Initialization
  // ============================================================================
  bool begin();  // Load from file, request if needed

  // ============================================================================
  // Cache Management
  // ============================================================================
  int getCachedQuestionCount() const { return questionCount; }
  bool needsMoreQuestions() const {
    return questionCount < MIN_QUESTIONS_THRESHOLD;
  }
  void clearCache();
  bool isCacheEmpty() const { return questionCount == 0; }

  // ============================================================================
  // Question Operations
  // ============================================================================
  String getQuestionForAlarm();  // Main method - gets question, requests more
                                 // if needed
  void receiveQuestion(
      const String& question);  // Called by MQTT handler (single)
  void receiveQuestionBatch(
      const String& batch);  // Called by MQTT handler (batch)

  // ============================================================================
  // MQTT Integration
  // ============================================================================
  void requestQuestionsFromService(
      int count = 5);  // Publish to smartalarm/question/request
  void registerMQTTHandlers(MQTTManager& mqtt);

  // ============================================================================
  // Status & Debug
  // ============================================================================
  void printStatus() const;  // Print cache status to Serial
  void printCache() const;   // Print all cached questions
};

#endif  // QUESTION_MANAGER_H
