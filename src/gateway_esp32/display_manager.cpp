#include "../../include/gateway_esp32/display_manager.h"

#include <WiFi.h>
#include <time.h>

#include "../../include/gateway_esp32/audio_manager.h"
#include "../../include/gateway_esp32/sd_manager.h"
#include "../../include/gateway_esp32/sensor_manager.h"
#include "../../include/shared/sensor_data.h"
#include "../../include/shared/mqtt_time.h"
#include "../../include/shared/time_sync.h"

DisplayManager::DisplayManager()
    : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
      tcaMultiplexer(nullptr),
      currentPage(PAGE_SENSORS),
      showingAlarmQuestion(false),
      lastAttempt(-1),
      sensorManager(nullptr),
      sdManager(nullptr),
      audioManager(nullptr),
      remoteSensorData(nullptr) {}

bool DisplayManager::begin(TCA9548A* tca) {
  tcaMultiplexer = tca;

  Serial.println("[DisplayManager] Initializing OLED...");

  tcaMultiplexer->openChannel(TCA_CHANNEL_OLED);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[DisplayManager] SSD1306 not found!");
    tcaMultiplexer->closeChannel(TCA_CHANNEL_OLED);
    return false;
  }

  display.clearDisplay();
  tcaMultiplexer->closeChannel(TCA_CHANNEL_OLED);
  Serial.println("[DisplayManager] OLED initialized");
  return true;
}

void DisplayManager::update() {
  if (!tcaMultiplexer) return;

  tcaMultiplexer->openChannel(TCA_CHANNEL_OLED);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Skip normal pages if showing alarm question
  if (showingAlarmQuestion) {
    display.display();
    tcaMultiplexer->closeChannel(TCA_CHANNEL_OLED);
    return;
  }

  switch (currentPage) {
    case PAGE_SENSORS:
      drawPageSensors();
      break;
    case PAGE_NETWORK:
      drawPageNetwork();
      break;
    case PAGE_STATUS:
      drawPageStatus();
      break;
    case PAGE_AUDIO:
      drawPageAudio();
      break;
  }

  display.display();
  tcaMultiplexer->closeChannel(TCA_CHANNEL_OLED);
}

void DisplayManager::nextPage() {
  currentPage = static_cast<DisplayPage>((currentPage + 1) % 4);
}

void DisplayManager::setSensorManager(SensorManager* sensorMgr) {
  sensorManager = sensorMgr;
}

void DisplayManager::setSDManager(SDManager* sdMgr) { sdManager = sdMgr; }

void DisplayManager::setAudioManager(AudioManager* audioMgr) {
  audioManager = audioMgr;
}

void DisplayManager::setRemoteSensorData(SensorData* remoteData) {
  remoteSensorData = remoteData;
}

void DisplayManager::showStartup() {
  if (!tcaMultiplexer) return;

  tcaMultiplexer->openChannel(TCA_CHANNEL_OLED);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Smart Alarm Clock");
  display.println("================");
  display.println();
  display.println("Initializing...");
  display.display();

  tcaMultiplexer->closeChannel(TCA_CHANNEL_OLED);
}

void DisplayManager::drawHeader(const char* title) {
  display.setCursor(0, 0);
  display.println(title);
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
  // Draw current time on the right side of the header
  drawTime();
  display.setCursor(0, 12);
}

void DisplayManager::drawTime() {
  struct tm timeinfo;
  char timeBuf[16];
  const char* statusBuf;

  // Prefer time received over MQTT if available
  if (mqttTimeAvailable && mqttTime.length() > 0) {
    // Use the mqttTime string (trim to HH:MM:SS if longer)
    String t = mqttTime;
    t.trim();
    if (t.length() > 8) t = t.substring(t.length() - 8);  // last HH:MM:SS
    strncpy(timeBuf, t.c_str(), sizeof(timeBuf) - 1);
    timeBuf[sizeof(timeBuf) - 1] = '\0';
    statusBuf = "MQT";  // indicate source
  } else {
    // Fallback to local NTP time
    if (getLocalTime(&timeinfo, 1000)) {
      snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
               timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
      snprintf(timeBuf, sizeof(timeBuf), "--:--:--");
    }
    // Use shared flag to display sync status
    statusBuf = (timeSynced ? "NTP" : "--");
  }

  // Calculate positions: time + small status to the right
  int timeChars = strlen(timeBuf);
  int statusChars = strlen(statusBuf);
  int charWidth = 6 * 1;  // 6 pixels per char at size 1

  int timeWidth = timeChars * charWidth;
  int statusWidth = statusChars * charWidth;

  int padding = 4;
  int startX = SCREEN_WIDTH - (timeWidth + statusWidth + padding);
  if (startX < 0) startX = 0;

  display.setCursor(startX, 0);
  display.println(timeBuf);

  display.setCursor(startX + timeWidth + 2, 0);
  display.println(statusBuf);
}

void DisplayManager::drawPageSensors() {
  drawHeader("Sensors");

  if (sensorManager) {
    display.printf("Local: %.0f lux\n", sensorManager->getLightIntensity());
  } else {
    display.println("Local: -- lux");
  }

  display.println();

  if (remoteSensorData) {
    display.printf("Remote: %.1fC %.1f%%\n", remoteSensorData->temperature,
                   remoteSensorData->humidity);
    display.printf("Battery: %d%%\n", remoteSensorData->batteryLevel);
  } else {
    display.println("Remote: No Data");
  }
}

void DisplayManager::drawPageNetwork() {
  drawHeader("Network");

  display.printf("SSID: %s\n", WiFi.SSID().c_str());
  display.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  display.printf("RSSI: %d dBm\n", WiFi.RSSI());
  display.printf("MAC: %s\n", WiFi.macAddress().c_str());
}

void DisplayManager::drawPageStatus() {
  drawHeader("Status");

  if (sdManager && sdManager->isReady()) {
    display.println("SD: MOUNTED");
  } else {
    display.println("SD: NO DISK");
  }

  if (audioManager) {
    if (audioManager->playing()) {
      display.println("Audio: PLAYING");
    } else {
      display.println("Audio: IDLE");
    }
  } else {
    display.println("Audio: --");
  }

  unsigned long uptime = millis() / 1000;
  display.printf("Uptime: %lu s\n", uptime);
}

void DisplayManager::drawPageAudio() {
  drawHeader("Audio");

  if (audioManager) {
    if (audioManager->isDownloading()) {
      display.println("RECEIVING...");
      float progress = audioManager->getDownloadProgress();
      if (progress >= 0.0f) {
        int percent = (int)(progress * 100.0f);
        display.printf("%d%%\n", percent);

        // Draw progress bar
        int barWidth = SCREEN_WIDTH - 20;  // Leave some margin
        int filledWidth = (int)(progress * barWidth);
        display.drawRect(10, 40, barWidth, 8, SSD1306_WHITE);
        display.fillRect(10, 40, filledWidth, 8, SSD1306_WHITE);
      } else {
        display.println("Starting...");
      }
    } else if (audioManager->playing()) {
      display.println("PLAYING");
      // Simple visualizer bars
      for (int i = 0; i < 8; i++) {
        int height = random(5, 20);
        display.fillRect(i * 16, SCREEN_HEIGHT - height, 10, height,
                         SSD1306_WHITE);
      }
    } else {
      display.println("IDLE");
    }
  } else {
    display.println("Audio: --");
  }
}

void DisplayManager::showAlarmQuestion(const String& question, int attempt, int maxAttempts, const String& status) {
  if (!tcaMultiplexer) return;

  // Only redraw if something changed
  if (showingAlarmQuestion && 
      lastQuestionText == question && 
      lastAttempt == attempt && 
      lastStatus == status) {
    return;  // No change, skip redraw
  }

  // Update cache
  showingAlarmQuestion = true;
  lastQuestionText = question;
  lastAttempt = attempt;
  lastStatus = status;
  
  tcaMultiplexer->openChannel(TCA_CHANNEL_OLED);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Title
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("ALARM QUESTION");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
  
  // Question (wrap text at ~21 chars per line)
  display.setCursor(0, 14);
  display.setTextSize(1);
  
  // Simple text wrapping
  int lineY = 14;
  int charCount = 0;
  for (int i = 0; i < question.length(); i++) {
    if (charCount >= 21 || question[i] == '\n') {
      lineY += 10;
      charCount = 0;
      display.setCursor(0, lineY);
    }
    if (lineY < 45) {  // Don't overflow into bottom area
      display.print(question[i]);
      charCount++;
    }
  }
  
  // Status line
  display.setCursor(0, 48);
  display.setTextSize(1);
  display.println(status);
  
  // Attempt counter
  display.setCursor(0, 56);
  display.printf("Attempt: %d/%d", attempt, maxAttempts);
  
  display.display();
  tcaMultiplexer->closeChannel(TCA_CHANNEL_OLED);
}

void DisplayManager::returnToNormalDisplay() {
  showingAlarmQuestion = false;
  lastQuestionText = "";
  lastAttempt = -1;
  lastStatus = "";
  currentPage = PAGE_SENSORS;  // Reset to first page
}