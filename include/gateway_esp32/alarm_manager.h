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

 private:
  std::vector<String> alarms;
};

// Global instance (defined in .cpp)
extern AlarmManager alarmManager;

#endif  // ALARM_MANAGER_H
