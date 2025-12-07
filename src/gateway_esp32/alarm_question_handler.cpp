#include "../../include/gateway_esp32/alarm_question_handler.h"

AlarmQuestionHandler alarmQuestion;

AlarmQuestionHandler::AlarmQuestionHandler()
    : state(QUESTION_IDLE),
      currentAttempt(0),
      recordingStartTime(0),
      lastActivityTime(0),
      recordingActive(false) {}

void AlarmQuestionHandler::setQuestion(const String& question) {
  currentQuestion = question;
  Serial.printf("[AlarmQuestion] Question set: %s\n", question.c_str());
}

void AlarmQuestionHandler::startQuestionSession() {
  state = QUESTION_DISPLAYING;
  currentAttempt = 1;
  recordingActive = false;
  lastActivityTime = millis();
  
  Serial.println("[AlarmQuestion] Question session started");
  Serial.printf("[AlarmQuestion] Question: %s\n", currentQuestion.c_str());
  Serial.printf("[AlarmQuestion] Attempt 1/%d\n", maxAttempts);
}

void AlarmQuestionHandler::startRecording() {
  if (state == QUESTION_DISPLAYING || state == QUESTION_WRONG) {
    state = QUESTION_RECORDING;
    recordingStartTime = millis();
    lastActivityTime = millis();
    recordingActive = true;
    
    Serial.printf("[AlarmQuestion] Recording started (Attempt %d/%d)\n",
                  currentAttempt, maxAttempts);
  }
}

void AlarmQuestionHandler::stopRecording() {
  if (state == QUESTION_RECORDING) {
    recordingActive = false;
    state = QUESTION_VALIDATING;
    
    unsigned long duration = millis() - recordingStartTime;
    Serial.printf("[AlarmQuestion] Recording stopped (Duration: %lu ms)\n", duration);
    Serial.println("[AlarmQuestion] Waiting for validation...");
  }
}

void AlarmQuestionHandler::setValidationResult(bool isCorrect) {
  if (state == QUESTION_VALIDATING) {
    if (isCorrect) {
      state = QUESTION_CORRECT;
      Serial.println("[AlarmQuestion] ✓ CORRECT! Alarm will be deactivated.");
    } else {
      currentAttempt++;
      
      if (currentAttempt > maxAttempts) {
        state = QUESTION_FAILED;
        Serial.println("[AlarmQuestion] ✗ FAILED all attempts!");
      } else {
        state = QUESTION_WRONG;
        Serial.printf("[AlarmQuestion] ✗ Wrong answer. Try again (Attempt %d/%d)\n",
                      currentAttempt, maxAttempts);
      }
    }
  }
}

void AlarmQuestionHandler::reset() {
  state = QUESTION_IDLE;
  currentQuestion = "";
  currentAttempt = 0;
  recordingActive = false;
  
  Serial.println("[AlarmQuestion] Reset to idle");
}

String AlarmQuestionHandler::getStatusMessage() const {
  switch (state) {
    case QUESTION_IDLE:
      return "Ready";
    
    case QUESTION_DISPLAYING:
      return String("Attempt ") + String(currentAttempt) + "/" + String(maxAttempts);
    
    case QUESTION_RECORDING:
      return "Recording...";
    
    case QUESTION_VALIDATING:
      return "Validating...";
    
    case QUESTION_CORRECT:
      return "CORRECT!";
    
    case QUESTION_WRONG:
      return String("Wrong! ") + String(currentAttempt) + "/" + String(maxAttempts);
    
    case QUESTION_FAILED:
      return "FAILED";
    
    default:
      return "Unknown";
  }
}

bool AlarmQuestionHandler::shouldTimeout() const {
  if (state == QUESTION_IDLE || state == QUESTION_CORRECT || state == QUESTION_FAILED) {
    return false;
  }
  
  // Timeout if no activity for 50 seconds
  return (millis() - lastActivityTime) > timeoutMs;
}

void AlarmQuestionHandler::updateActivity() {
  lastActivityTime = millis();
}
