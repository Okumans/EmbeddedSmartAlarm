#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// SD Card Pins
#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_CLK_PIN 18

class SDManager {
 public:
  SDManager();

  // Hardware Init
  bool begin(int maxRetries = 1);
  bool isReady();
  bool checkAndRemount();  // Auto-remount if SD card failed

  // File Writing (For MQTT Uploads)
  bool openForWrite(const char* filename);
  bool writeChunk(const uint8_t* data, size_t len);
  void closeFile();

  // File Management
  String listAudioFiles();  // Returns comma-separated list of MP3/WAV files
  bool exists(const char* filename);
  void remove(const char* filename);
  size_t getFileSize(const char* filename);

  // Info
  void printCardInfo();

 private:
  bool _ready;
  File _file;               // Current active file for writing
  size_t _bytesSinceFlush;  // For efficient flushing
  SemaphoreHandle_t _mutex; // Protect SD access
  uint8_t _errorCount;      // Track consecutive errors
  unsigned long _lastErrorTime;
  
  bool _lock(uint32_t timeout_ms = 1000);
  void _unlock();
  bool _reinitialize();     // Try to recover SD card
};

#endif  // SD_MANAGER_H