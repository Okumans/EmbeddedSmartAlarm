#include "../../include/gateway_esp32/alarm_manager.h"
#include <SPIFFS.h>

AlarmManager alarmManager;

AlarmManager::AlarmManager() : alarmSoundFile("/alarm.mp3") {}

void AlarmManager::begin() {
  Serial.println("[AlarmManager] Initializing...");
  
  // Load alarm sound from file
  if (loadAlarmSoundFromFile()) {
    Serial.printf("[AlarmManager] ✓ Loaded alarm sound: %s\n", alarmSoundFile.c_str());
  } else {
    Serial.printf("[AlarmManager] Using default alarm sound: %s\n", alarmSoundFile.c_str());
  }
}

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

void AlarmManager::clearTriggeredStates() {
  triggeredAlarms.clear();
}

void AlarmManager::setAlarmSound(const String& soundFile) {
  alarmSoundFile = soundFile;
  Serial.printf("[AlarmManager] Alarm sound set to: %s\n", soundFile.c_str());
  
  // Save to file for persistence
  saveAlarmSoundToFile();
}

String AlarmManager::getAlarmSound() const {
  return alarmSoundFile;
}

bool AlarmManager::loadAlarmSoundFromFile() {
  if (!SPIFFS.begin()) {
    Serial.println("[AlarmManager] ✗ SPIFFS not mounted");
    return false;
  }

  const char* filepath = "/alarm_sound.txt";
  
  if (!SPIFFS.exists(filepath)) {
    return false;
  }

  File file = SPIFFS.open(filepath, "r");
  if (!file) {
    Serial.println("[AlarmManager] ✗ Failed to open alarm sound file");
    return false;
  }

  String line = file.readStringUntil('\n');
  line.trim();
  file.close();

  if (line.length() > 0) {
    alarmSoundFile = line;
    return true;
  }

  return false;
}

bool AlarmManager::saveAlarmSoundToFile() {
  if (!SPIFFS.begin()) {
    Serial.println("[AlarmManager] ✗ SPIFFS not mounted, cannot save");
    return false;
  }

  const char* filepath = "/alarm_sound.txt";
  
  File file = SPIFFS.open(filepath, FILE_WRITE);
  if (!file) {
    Serial.println("[AlarmManager] ✗ Failed to save alarm sound file");
    return false;
  }

  file.println(alarmSoundFile);
  file.close();
  
  Serial.printf("[AlarmManager] ✓ Saved alarm sound to file: %s\n", alarmSoundFile.c_str());
  return true;
}
