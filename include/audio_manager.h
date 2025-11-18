#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <opus.h>

#include "AudioFileSourceID3.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "streaming_config.h"

// Forward declarations
class PubSubClient;
class MQTTManager;

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

  bool initialized;
  bool isPlaying;
  float currentVolume;

  // Streaming state
  AudioState currentState;
  OpusDecoder* opusDecoder;
  uint8_t opusPacketBuffer[MAX_OPUS_PACKET_SIZE];
  int16_t pcmBuffer[OPUS_FRAME_SAMPLES];
  int16_t silenceBuffer[SILENCE_BUFFER_SIZE];
  bool prerollComplete;

  void cleanup();
  void cleanupStreaming();

  // Internal handler for audio chunks
  bool handleAudioChunk(MQTTManager& mqtt, byte* payload, unsigned int length);
  bool handleAudioRequest(MQTTManager& mqtt, byte* payload,
                          unsigned int length);

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

  // Streaming methods
  bool startStreaming();
  void stopStreaming();
  bool isStreaming() const;
  void processStreamingAudio();  // Called from audio decode task

  // Volume control (0.0 to 1.0)
  void setVolume(float volume);
  float getVolume();

  // Check if audio is currently playing
  bool playing();

  // List all audio files in SPIFFS
  void listFiles();

  // Get SPIFFS info
  void printSPIFFSInfo();

  // Register MQTT handlers with the manager
  void registerMQTTHandlers(MQTTManager& mqtt);
};

#endif  // AUDIO_MANAGER_H
