#include "../../include/gateway_esp32/alarm_manager.h"

AlarmManager alarmManager;

AlarmManager::AlarmManager() {}

void AlarmManager::setFromPayload(const char* payload, unsigned int length) {
  String s((char*)payload, length);
  alarms.clear();
  triggeredAlarms.clear();  // Clear triggered states when updating alarms

  int start = 0;
  while (start >= 0 && start < s.length()) {
    int comma = s.indexOf(',', start);
    String token;
    if (comma == -1) {
      token = s.substring(start);
      start = -1;
    } else {
      token = s.substring(start, comma);
      start = comma + 1;
    }
    token.trim();
    // Basic validation: expect HH:MM
    if (token.length() == 5 && token.charAt(2) == ':') {
      alarms.push_back(token);
    }
  }
}

String AlarmManager::toCSV() const {
  String out = "";
  for (size_t i = 0; i < alarms.size(); ++i) {
    if (i > 0) out += ",";
    out += alarms[i];
  }
  return out;
}

String AlarmManager::checkAlarms(const String& currentTime) {
  // currentTime should be in HH:MM format
  if (currentTime.length() < 5) {
    return "";  // Invalid time format
  }

  // Extract HH:MM from current time (ignore seconds)
  String currentHHMM = currentTime.substring(0, 5);

  // Check each alarm
  for (const String& alarm : alarms) {
    if (alarm == currentHHMM && !isAlarmTriggered(alarm)) {
      return alarm;  // Match found and not yet triggered
    }
  }

  return "";  // No match
}

bool AlarmManager::isAlarmTriggered(const String& alarmTime) const {
  for (const String& triggered : triggeredAlarms) {
    if (triggered == alarmTime) {
      return true;
    }
  }
  return false;
}

void AlarmManager::setAlarmTriggered(const String& alarmTime, bool triggered) {
  if (triggered) {
    // Add to triggered list if not already there
    if (!isAlarmTriggered(alarmTime)) {
      triggeredAlarms.push_back(alarmTime);
    }
  } else {
    // Remove from triggered list
    for (size_t i = 0; i < triggeredAlarms.size(); ++i) {
      if (triggeredAlarms[i] == alarmTime) {
        triggeredAlarms.erase(triggeredAlarms.begin() + i);
        break;
      }
    }
  }
}

void AlarmManager::clearTriggeredStates() { triggeredAlarms.clear(); }
