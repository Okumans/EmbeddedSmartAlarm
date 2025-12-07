#ifndef ALARM_QUESTION_HANDLER_H
#define ALARM_QUESTION_HANDLER_H

#include <Arduino.h>

// Alarm question state
enum AlarmQuestionState {
  QUESTION_IDLE,              // No question active
  QUESTION_DISPLAYING,        // Showing question on OLED
  QUESTION_RECORDING,         // Recording user's answer
  QUESTION_VALIDATING,        // Validating answer via MQTT
  QUESTION_CORRECT,           // Answer was correct
  QUESTION_WRONG,             // Answer was wrong
  QUESTION_FAILED             // Failed all attempts
};

class AlarmQuestionHandler {
 public:
  AlarmQuestionHandler();

  // Set question text (from MQTT)
  void setQuestion(const String& question);

  // Get current question
  String getQuestion() const { return currentQuestion; }

  // Get current state
  AlarmQuestionState getState() const { return state; }

  // Get current attempt number
  int getCurrentAttempt() const { return currentAttempt; }

  // Get max attempts
  int getMaxAttempts() const { return maxAttempts; }

  // Start question session (called when alarm triggers)
  void startQuestionSession();

  // Start recording answer
  void startRecording();

  // Stop recording and submit answer
  void stopRecording();

  // Set answer validation result (from MQTT)
  void setValidationResult(bool isCorrect);

  // Check if question session is active
  bool isActive() const { return state != QUESTION_IDLE; }

  // Check if alarm should be deactivated (correct answer)
  bool shouldDeactivateAlarm() const { return state == QUESTION_CORRECT; }

  // Reset to idle state
  void reset();

  // Get status message for display
  String getStatusMessage() const;

 private:
  String currentQuestion;
  AlarmQuestionState state;
  int currentAttempt;
  const int maxAttempts = 3;
  unsigned long recordingStartTime;
  bool recordingActive;
};

// Global instance
extern AlarmQuestionHandler alarmQuestion;

#endif  // ALARM_QUESTION_HANDLER_H
