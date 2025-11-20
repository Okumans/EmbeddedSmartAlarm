#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>

#include "AudioFileSourceID3.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// Forward declarations
class PubSubClient;
class MQTTManager;

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

  void cleanup();

  // Internal handler for audio chunks
  bool handleAudioChunk(MQTTManager& mqtt, byte* payload, unsigned int length);
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

  // Get SD card info
  void printSDInfo();

  // Register MQTT handlers with the manager
  void registerMQTTHandlers(MQTTManager& mqtt);

  // Set MQTT manager for status reporting
  void setMQTTManager(MQTTManager* mqtt);

  // Check if audio file is currently downloading
  bool isDownloading();
};

#endif  // AUDIO_MANAGER_H
