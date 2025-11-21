#include "../../include/display_manager.h"

#include <WiFi.h>

#include "../../include/sd_manager.h"
#include "../../include/sensor_data.h"
#include "../../include/sensor_manager.h"
#include "audio_manager.h"

DisplayManager::DisplayManager()
    : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
      tcaMultiplexer(nullptr),
      currentPage(PAGE_SENSORS),
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
  display.setCursor(0, 12);
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