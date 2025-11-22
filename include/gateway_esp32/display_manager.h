#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Forward declarations
class TCA9548A;
class SensorManager;
class SDManager;
class AudioManager;
#include "../shared/sensor_data.h"

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
#define TCA_CHANNEL_OLED 1

// Enum for display pages
enum DisplayPage { PAGE_SENSORS, PAGE_NETWORK, PAGE_STATUS, PAGE_AUDIO };

class DisplayManager {
 public:
  DisplayManager();

  // Initialization
  bool begin(TCA9548A* tca);

  // Update display
  void update();

  // Page navigation
  void nextPage();

  // Dependency injection
  void setSensorManager(SensorManager* sensorMgr);
  void setSDManager(SDManager* sdMgr);
  void setAudioManager(AudioManager* audioMgr);
  void setRemoteSensorData(SensorData* remoteData);

  // Startup screen
  void showStartup();

 private:
  Adafruit_SSD1306 display;
  TCA9548A* tcaMultiplexer;

  DisplayPage currentPage;

  // Injected dependencies
  SensorManager* sensorManager;
  SDManager* sdManager;
  AudioManager* audioManager;
  SensorData* remoteSensorData;

  // Private drawing methods
  void drawPageSensors();
  void drawPageNetwork();
  void drawPageStatus();
  void drawPageAudio();
  void drawHeader(const char* title);
};

#endif  // DISPLAY_MANAGER_H