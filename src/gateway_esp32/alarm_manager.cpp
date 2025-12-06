#include "../../include/gateway_esp32/alarm_manager.h"

AlarmManager alarmManager;

AlarmManager::AlarmManager() {}

void AlarmManager::setFromPayload(const char* payload, unsigned int length) {
  String s((char*)payload, length);
  alarms.clear();

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
