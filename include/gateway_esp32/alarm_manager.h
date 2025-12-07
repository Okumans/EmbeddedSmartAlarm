#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include <Arduino.h>

#include <vector>

class AlarmManager {
 public:
  AlarmManager();

  // Parse a payload like "07:30,08:00,14:30" and store HH:MM entries
  void setFromPayload(const char* payload, unsigned int length);

  // Return alarms as CSV string
  String toCSV() const;

  // Accessor
  const std::vector<String>& getAlarms() const { return alarms; }

  // Check if current time matches any alarm
  // Returns alarm time if matched, empty string otherwise
  String checkAlarms(const String& currentTime);

  // Check if an alarm is currently triggered (to prevent re-triggering)
  bool isAlarmTriggered(const String& alarmTime) const;

  // Mark an alarm as triggered
  void setAlarmTriggered(const String& alarmTime, bool triggered);

  // Clear all triggered states (call at start of new minute)
  void clearTriggeredStates();

  // Get alarm sound file (always returns "/audio.mp3")
  String getAlarmSound() const { return "/audio.mp3"; }

 private:
  std::vector<String> alarms;
  std::vector<String> triggeredAlarms;  // Track which alarms have been triggered
};

// Global instance (defined in .cpp)
extern AlarmManager alarmManager;

#endif  // ALARM_MANAGER_H
