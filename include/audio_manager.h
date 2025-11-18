#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>

#include "AudioFileSourceID3.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// Forward declare PubSubClient to avoid including its header in this public
// header
class PubSubClient;

// I2S Configuration
#define I2S_BCLK 26
#define I2S_LRC 25
#define I2S_DOUT 27

class AudioManager {
 private:
  AudioOutputI2S* out;
  AudioFileSourceLittleFS* file;
  AudioFileSourceID3* id3;
  AudioGeneratorWAV* wav;
  AudioGeneratorMP3* mp3;
  PubSubClient* mqttClientPtr;

  bool initialized;
  bool isPlaying;
  float currentVolume;

  void cleanup();

 public:
  AudioManager();
  ~AudioManager();

  // Initialize audio system and SPIFFS
  bool begin();

  // Stop and cleanup
  void end();

  // Play audio file from SPIFFS
  bool playFile(const char* filename);

  // Play WAV file
  bool playWAV(const char* filename);

  // Play MP3 file
  bool playMP3(const char* filename);

  // Stop currently playing audio
  void stop();

  // Update - call this in loop() to keep audio playing
  void loop();

  // Volume control (0.0 to 1.0)
  void setVolume(float volume);
  float getVolume();

  // Check if audio is currently playing
  bool playing();

  // List all audio files in SPIFFS
  void listFiles();

  // Get SPIFFS info
  void printSPIFFSInfo();

  // Inject shared MQTT client (from main) so AudioManager can publish/subscribe
  void setMQTTClient(PubSubClient* client);

  // Handle MQTT messages forwarded from the global callback in main.cpp.
  // Returns true if the message was handled by AudioManager.
  bool handleMQTTMessage(const char* topic, byte* payload, unsigned int length);
};

#endif  // AUDIO_MANAGER_H
