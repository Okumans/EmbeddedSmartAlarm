#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>

#include "AudioFileSourceID3.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "mqtt_manager.h"
#include "sd_manager.h"

// Forward declarations
class PubSubClient;

// I2S Configuration
#define I2S_BCLK 26
#define I2S_LRC 25
#define I2S_DOUT 27

// SD Card Configuration
#define SD_CS 5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_CLK 18

class AudioManager {
 private:
  AudioOutputI2S* out;
  AudioFileSourceSD* file;
  AudioFileSourceID3* id3;
  AudioGeneratorMP3* mp3;

  bool initialized;
  bool isPlaying;
  float currentVolume;

  MQTTManager* mqttManager;  // For status reporting
  SDManager* sdManager;      // For file operations

  // NEW: Mutex for thread safety
  SemaphoreHandle_t audioMutex;

  // Download state
  bool receivingFile;
  size_t expectedSize;
  size_t receivedSize;
  unsigned long lastChunkTime;
  String recvFilename;
  bool downloadingInProgress;

  void cleanup();

  // Internal handler for audio chunks
  bool handleAudioRequest(MQTTManager& mqtt, byte* payload,
                          unsigned int length);

 public:
  AudioManager();
  ~AudioManager();

  // Initialize audio system and SD card
  bool begin();

  // Stop and cleanup
  void end();

  // Play audio file from SD card
  bool playFile(const char* filename);

  // Play MP3 file from SD card
  bool playMP3(const char* filename);

  // Play file from SD card (alias for playFile)
  bool playFileFromSD(const char* filename);

  // Stop currently playing audio
  void stop();

  // Update - call this in loop() to keep audio playing
  void loop();

  // Volume control (0.0 to 1.0)
  void setVolume(float volume);
  float getVolume();

  // Check if audio is currently playing
  bool playing();

  // List all audio files in SD card
  void listFiles();
  String getFileList();

  // Get SD card info
  void printSDInfo();

  // Register MQTT handlers with the manager
  void registerMQTTHandlers(MQTTManager& mqtt);

  // Set MQTT manager for status reporting
  void setMQTTManager(MQTTManager* mqtt);

  // Set SD manager for file operations
  void setSDManager(SDManager* sd);

  // Check if audio file is currently downloading
  bool isDownloading();

  // Get download progress (0.0 to 1.0, or -1 if not downloading)
  float getDownloadProgress() const;

  // New methods for HTTP download
  bool handleDownloadCommand(MQTTManager& mqtt, byte* payload,
                             unsigned int length);
  bool downloadFile(const char* url, const char* filename);
};

#endif  // AUDIO_MANAGER_H
