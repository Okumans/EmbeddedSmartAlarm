#include "../../include/gateway_esp32/question_manager.h"

// ============================================================================
// Static Member Initialization
// ============================================================================
const char* QuestionManager::QUESTIONS_FILE = "/questions.txt";

const char* QuestionManager::FALLBACK_QUESTIONS[] = {
    "What is the largest planet in our solar system?",
    "What is the capital city of Thailand?",
    "At what temperature does water boil in Celsius?",
    "How many bones does an adult human have?",
    "How many continents are there on Earth?"};

const int QuestionManager::FALLBACK_COUNT = 5;

// ============================================================================
// Constructor
// ============================================================================
QuestionManager::QuestionManager()
    : questionCount(0),
      lastUsedIndex(-1),
      lastRequestTime(0),
      requestInProgress(false),
      initialized(false) {}

// ============================================================================
// Initialization
// ============================================================================
bool QuestionManager::begin() {
  Serial.println("[QuestionManager] Initializing...");

  // Load questions from file
  if (loadQuestionsFromFile()) {
    Serial.printf("[QuestionManager] ✓ Loaded %d questions from file\n",
                  questionCount);
  } else {
    Serial.println("[QuestionManager] ⚠ No questions file found");
  }

  // Check if we need to request more
  if (needsMoreQuestions()) {
    Serial.printf("[QuestionManager] Cache low (%d/%d), requesting more...\n",
                  questionCount, MIN_QUESTIONS_THRESHOLD);
    requestQuestionsFromService();
  }

  initialized = true;
  printStatus();

  return true;
}

// ============================================================================
// File Operations
// ============================================================================
bool QuestionManager::loadQuestionsFromFile() {
  if (!SPIFFS.exists(QUESTIONS_FILE)) {
    Serial.println("[QuestionManager] Questions file not found");
    return false;
  }

  File file = SPIFFS.open(QUESTIONS_FILE, "r");
  if (!file) {
    Serial.println("[QuestionManager] ✗ Failed to open questions file");
    return false;
  }

  questionCount = 0;
  while (file.available() && questionCount < MAX_QUESTIONS_CACHE) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() > 0 && isQuestionValid(line)) {
      questionCache[questionCount] = line;
      questionCount++;
    }
  }

  file.close();
  return questionCount > 0;
}

bool QuestionManager::saveQuestionsToFile() {
  // Check if SPIFFS is mounted
  if (!SPIFFS.begin()) {
    Serial.println("[QuestionManager] ✗ SPIFFS not mounted, cannot save");
    return false;
  }

  // Open file with FILE_WRITE mode (creates if doesn't exist)
  File file = SPIFFS.open(QUESTIONS_FILE, FILE_WRITE);
  if (!file) {
    Serial.printf("[QuestionManager] ✗ Failed to open %s for writing\n", QUESTIONS_FILE);
    Serial.printf("[QuestionManager] SPIFFS total: %d, used: %d\n", 
                  SPIFFS.totalBytes(), SPIFFS.usedBytes());
    return false;
  }

  // Clear existing content by truncating
  file.seek(0);
  
  // Write questions
  for (int i = 0; i < questionCount; i++) {
    file.println(questionCache[i]);
  }

  file.close();
  Serial.printf("[QuestionManager] ✓ Saved %d questions to file\n",
                questionCount);
  return true;
}

void QuestionManager::addQuestionToCache(const String& question) {
  if (questionCount >= MAX_QUESTIONS_CACHE) {
    Serial.println("[QuestionManager] ⚠ Cache full, skipping question");
    return;
  }

  if (!isQuestionValid(question)) {
    Serial.println("[QuestionManager] ⚠ Invalid question, skipping");
    return;
  }

  // Check for duplicates
  for (int i = 0; i < questionCount; i++) {
    if (questionCache[i] == question) {
      Serial.println("[QuestionManager] ⚠ Duplicate question, skipping");
      return;
    }
  }

  questionCache[questionCount] = question;
  questionCount++;
  Serial.printf("[QuestionManager] ✓ Added question to cache (%d/%d)\n",
                questionCount, MAX_QUESTIONS_CACHE);
}

bool QuestionManager::isQuestionValid(const String& question) {
  // Check if question is not empty and not too long
  return question.length() > 0 && question.length() < MAX_QUESTION_LENGTH;
}

// ============================================================================
// Question Selection
// ============================================================================
String QuestionManager::selectRandomQuestion() {
  if (questionCount == 0) {
    Serial.println("[QuestionManager] ⚠ No questions in cache, using fallback");
    return getFallbackQuestion();
  }

  // Select random question that's different from last used
  int index;
  if (questionCount == 1) {
    index = 0;
  } else {
    do {
      index = random(0, questionCount);
    } while (index == lastUsedIndex && questionCount > 1);
  }

  lastUsedIndex = index;
  return questionCache[index];
}

void QuestionManager::markQuestionUsed(int index) {
  // Simple approach: just track the last used index
  // Could be extended to move used questions to end of array
  lastUsedIndex = index;
}

String QuestionManager::getFallbackQuestion() {
  int index = random(0, FALLBACK_COUNT);
  return String(FALLBACK_QUESTIONS[index]);
}

// ============================================================================
// Cache Management
// ============================================================================
void QuestionManager::clearCache() {
  questionCount = 0;
  lastUsedIndex = -1;
  Serial.println("[QuestionManager] Cache cleared");
}

// ============================================================================
// Question Operations
// ============================================================================
String QuestionManager::getQuestionForAlarm() {
  Serial.println("[QuestionManager] Getting question for alarm...");

  // Select a question
  String question = selectRandomQuestion();

  // Check if we need to request more (async, non-blocking)
  if (needsMoreQuestions() && canRequestQuestions()) {
    Serial.printf("[QuestionManager] Cache low (%d/%d), requesting more...\n",
                  questionCount, MIN_QUESTIONS_THRESHOLD);
    requestQuestionsFromService();
  }

  return question;
}

void QuestionManager::receiveQuestion(const String& question) {
  Serial.println("[QuestionManager] Received single question from MQTT");

  String trimmedQuestion = question;
  trimmedQuestion.trim();

  addQuestionToCache(trimmedQuestion);
  saveQuestionsToFile();

  setRequestInProgress(false);
  printStatus();
}

void QuestionManager::receiveQuestionBatch(const String& batch) {
  Serial.println("[QuestionManager] Received question batch from MQTT");

  int startIndex = 0;
  int newLineIndex;
  int addedCount = 0;

  while ((newLineIndex = batch.indexOf('\n', startIndex)) != -1) {
    String question = batch.substring(startIndex, newLineIndex);
    question.trim();

    if (question.length() > 0) {
      addQuestionToCache(question);
      addedCount++;
    }

    startIndex = newLineIndex + 1;
  }

  // Handle last question (no trailing newline)
  if (startIndex < batch.length()) {
    String question = batch.substring(startIndex);
    question.trim();
    if (question.length() > 0) {
      addQuestionToCache(question);
      addedCount++;
    }
  }

  if (addedCount > 0) {
    saveQuestionsToFile();
  }

  Serial.printf("[QuestionManager] ✓ Added %d questions from batch\n",
                addedCount);

  setRequestInProgress(false);
  printStatus();
}

// ============================================================================
// MQTT Integration
// ============================================================================
void QuestionManager::requestQuestionsFromService(int count) {
  if (!canRequestQuestions()) {
    Serial.println("[QuestionManager] ⚠ Request cooldown active, skipping");
    return;
  }

  // Calculate how many questions we need
  int needed = MAX_QUESTIONS_CACHE - questionCount;
  if (needed <= 0) {
    Serial.println("[QuestionManager] Cache full, no request needed");
    return;
  }

  int requestCount = min(count, needed);

  Serial.printf("[QuestionManager] Requesting %d questions from service...\n",
                requestCount);

  // This will be published by the MQTT manager
  // The actual publish happens in the caller or via MQTT manager
  extern MQTTManager mqtt;

  char payload[8];
  snprintf(payload, sizeof(payload), "%d", requestCount);
  mqtt.publish("smartalarm/question/request", payload, 1);

  lastRequestTime = millis();
  setRequestInProgress(true);
}

void QuestionManager::registerMQTTHandlers(MQTTManager& mqtt) {
  Serial.println("[QuestionManager] Registering MQTT handlers...");

  // Handler for single question (legacy support)
  mqtt.registerAndSubscribe(
      "smartalarm/question",
      [this](MQTTManager& mqtt, const char* topic, byte* payload,
             unsigned int length) -> bool {
        String question((char*)payload, length);
        this->receiveQuestion(question);
        return true;
      },
      "QuestionSingle",
      120  // Priority
  );

  // Handler for batch questions
  mqtt.registerAndSubscribe(
      "smartalarm/question/batch",
      [this](MQTTManager& mqtt, const char* topic, byte* payload,
             unsigned int length) -> bool {
        String batch((char*)payload, length);
        this->receiveQuestionBatch(batch);
        return true;
      },
      "QuestionBatch",
      120  // Priority
  );

  Serial.println("[QuestionManager] ✓ MQTT handlers registered");
}

// ============================================================================
// Request Management
// ============================================================================
bool QuestionManager::canRequestQuestions() {
  unsigned long now = millis();

  // Check cooldown period
  if (requestInProgress && (now - lastRequestTime) < 30000) {  // 30s timeout
    return false;
  }

  // Reset if timeout exceeded
  if (requestInProgress && (now - lastRequestTime) >= 30000) {
    Serial.println("[QuestionManager] Request timeout, resetting flag");
    requestInProgress = false;
  }

  // Check minimum interval between requests
  if ((now - lastRequestTime) < REQUEST_COOLDOWN) {
    return false;
  }

  return true;
}

void QuestionManager::setRequestInProgress(bool inProgress) {
  requestInProgress = inProgress;
  if (!inProgress) {
    Serial.println("[QuestionManager] Request completed");
  }
}

// ============================================================================
// Status & Debug
// ============================================================================
void QuestionManager::printStatus() const {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║     Question Manager Status            ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.printf("║ Cached Questions:  %2d / %2d            ║\n", questionCount,
                MAX_QUESTIONS_CACHE);
  Serial.printf("║ Threshold:         %2d                  ║\n",
                MIN_QUESTIONS_THRESHOLD);
  Serial.printf("║ Needs More:        %s                 ║\n",
                needsMoreQuestions() ? "YES" : "NO ");
  Serial.printf("║ Request Pending:   %s                 ║\n",
                requestInProgress ? "YES" : "NO ");
  Serial.printf("║ Initialized:       %s                 ║\n",
                initialized ? "YES" : "NO ");
  Serial.println("╚════════════════════════════════════════╝");
}

void QuestionManager::printCache() const {
  Serial.println("\n[QuestionManager] Cached Questions:");
  for (int i = 0; i < questionCount; i++) {
    Serial.printf("  %d. %s\n", i + 1, questionCache[i].c_str());
  }
  if (questionCount == 0) {
    Serial.println("  (No questions cached)");
  }
  Serial.println();
}
