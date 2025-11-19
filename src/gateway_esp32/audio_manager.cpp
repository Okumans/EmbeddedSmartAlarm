#include "audio_manager.h"

// MQTT Manager for remote file upload
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <freertos/FreeRTOS.h>

#include "../../include/mqtt_manager.h"

// ---------------- MQTT TOPICS ----------------
#include "../../include/config.h"

#define TOPIC_REQUEST MQTT_TOPIC_AUDIO_REQUEST
#define TOPIC_RESPONSE MQTT_TOPIC_AUDIO_RESPONSE
#define TOPIC_CHUNK MQTT_TOPIC_AUDIO_CHUNK
#define TOPIC_ACK MQTT_TOPIC_AUDIO_ACK

File fsFile;
String recvFilename = "/sound.mp3";
bool receivingFile = false;
size_t expectedSize = 0;
size_t receivedSize = 0;
unsigned long lastChunkTime = 0;  // Track upload timeout

AudioManager::AudioManager()
    : out{nullptr},
      file{nullptr},
      id3{nullptr},
      mp3{nullptr},
      initialized{nullptr},
      isPlaying{false},
      currentVolume{0.5},
      currentMode{MODE_IDLE},
      streamActive{false} {
  initADPCM();
}

AudioManager::~AudioManager() { cleanup(); }

void AudioManager::cleanup() {
  if (mp3) {
    if (mp3->isRunning()) mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }

  if (id3) {
    delete id3;
    id3 = nullptr;
  }

  if (file) {
    delete file;
    file = nullptr;
  }

  isPlaying = false;
}

bool AudioManager::begin() {
  if (initialized) {
    return true;
  }

  Serial.println("[Audio] Initializing filesystem...");
  Serial.println("[Audio] Mounting LittleFS (will format if needed)...");

  if (!LittleFS.begin()) {
    Serial.println("[Audio] LittleFS mount failed — attempting format...");

    if (!LittleFS.format()) {
      Serial.println("[Audio] LittleFS format failed!");
      return false;
    }

    delay(100);

    if (!LittleFS.begin()) {
      Serial.println("[Audio] LittleFS mount failed even after format");
      return false;
    }
  }

  Serial.println("[Audio] LittleFS mounted successfully");
  printSPIFFSInfo();

  // Initialize I2S output
  Serial.println("[Audio] Initializing I2S output...");
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(currentVolume);

  initialized = true;
  Serial.println("[Audio] Audio system initialized");

  listFiles();

  return true;
}

void AudioManager::end() {
  cleanup();

  if (out) {
    delete out;
    out = nullptr;
  }

  LittleFS.end();
  initialized = false;
  Serial.println("[Audio] Audio system stopped");
}

bool AudioManager::playFile(const char* filename) {
  if (!initialized) {
    Serial.println("[Audio] Not initialized!");
    return false;
  }

  // Stop any currently playing audio
  stop();

  // Check if file exists
  if (!LittleFS.exists(filename)) {
    Serial.printf("[Audio] File not found: %s\n", filename);
    return false;
  }

  // Determine file type by extension
  String fname = String(filename);
  fname.toLowerCase();

  if (fname.endsWith(".mp3")) {
    return playMP3(filename);
  } else {
    Serial.println("[Audio] Unsupported file format. Use .mp3");
    return false;
  }
}

bool AudioManager::playMP3(const char* filename) {
  if (!initialized) {
    Serial.println("[Audio] Not initialized!");
    return false;
  }

  cleanup();

  Serial.printf("[Audio] Playing MP3: %s\n", filename);

  file = new AudioFileSourceLittleFS(filename);
  id3 = new AudioFileSourceID3(file);
  mp3 = new AudioGeneratorMP3();

  if (mp3->begin(id3, out)) {
    isPlaying = true;
    return true;
  } else {
    Serial.println("[Audio] Failed to start MP3 playback");
    cleanup();
    return false;
  }
}

void AudioManager::stop() {
  cleanup();
  Serial.println("[Audio] Stopped");
}

void AudioManager::loop() {
  if (!initialized || !isPlaying) {
    return;
  }

  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      Serial.println("[Audio] MP3 playback finished");
      cleanup();
    }
  } else {
    isPlaying = false;
  }
}

void AudioManager::setVolume(float volume) {
  currentVolume = constrain(volume, 0.0, 1.0);
  if (out) {
    out->SetGain(currentVolume);
  }
  Serial.printf("[Audio] Volume set to %.2f\n", currentVolume);
}

float AudioManager::getVolume() { return currentVolume; }

bool AudioManager::playing() {
  if (!initialized || !isPlaying) {
    return false;
  }

  if (mp3 && mp3->isRunning()) {
    return true;
  }

  isPlaying = false;
  return false;
}

void AudioManager::listFiles() {
  Serial.println("[Audio] Files in filesystem:");
  Serial.println("----------------------------------------");

  File root = LittleFS.open("/");
  File file = root.openNextFile();
  int count = 0;
  while (file) {
    if (!file.isDirectory()) {
      String filename = file.name();
      if (filename.endsWith(".mp3") || filename.endsWith(".MP3")) {
        Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
        count++;
      }
    }
    file = root.openNextFile();
  }

  if (count == 0) {
    Serial.println("  No audio files found");
  } else {
    Serial.printf("Total: %d audio file(s)\n", count);
  }
  Serial.println("----------------------------------------");
}

void AudioManager::printSPIFFSInfo() {
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();

  Serial.println("[Audio] LittleFS Info:");
  Serial.printf("  Total: %d bytes (%.2f MB)\n", totalBytes,
                totalBytes / (1024.0 * 1024.0));
  Serial.printf("  Used:  %d bytes (%.2f MB)\n", usedBytes,
                usedBytes / (1024.0 * 1024.0));
  Serial.printf("  Free:  %d bytes (%.2f MB)\n", totalBytes - usedBytes,
                (totalBytes - usedBytes) / (1024.0 * 1024.0));
  Serial.printf("  Usage: %.1f%%\n", (usedBytes * 100.0) / totalBytes);
}

// Helper: parse numeric substring from payload
static int parseNumberFromPayload(const byte* payload, int start, int end) {
  char buf[16];
  int len = end - start;
  if (len <= 0 || len >= (int)sizeof(buf)) return 0;
  memcpy(buf, payload + start, len);
  buf[len] = '\0';
  return atoi(buf);
}

// ============================================================================
// MQTT Handler Registration
// ============================================================================

void AudioManager::registerMQTTHandlers(MQTTManager& mqtt) {
  Serial.println("[Audio] Registering MQTT handlers...");

  // Handler for audio upload requests
  mqtt.registerHandler(
      "esp32/audio_request",
      [this](MQTTManager& mqtt, const char* topic, byte* payload,
             unsigned int length) -> bool {
        return this->handleAudioRequest(mqtt, payload, length);
      },
      "AudioRequest",
      150  // High priority
  );

  // Handler for audio chunks
  mqtt.registerHandler(
      "esp32/audio_chunk",
      [this](MQTTManager& mqtt, const char* topic, byte* payload,
             unsigned int length) -> bool {
        return this->handleAudioChunk(mqtt, payload, length);
      },
      "AudioChunk",
      150  // High priority
  );

  Serial.println("[Audio] MQTT handlers registered");
}

// ============================================================================
// STREAM MODE METHODS
// ============================================================================

bool AudioManager::beginStreamRX(int port) {
  if (!initialized) {
    Serial.println("[Audio] Not initialized!");
    return false;
  }

  if (streamActive) {
    Serial.println("[Audio] Stream already active");
    return true;
  }

  Serial.printf("[Audio] Starting UDP listener on port %d\n", port);
  if (udp.begin(port)) {
    streamActive = true;
    currentMode = MODE_STREAM_RX;
    initADPCM();
    // Configure I2S for ADPCM streaming: 16kHz, 16-bit, Stereo
    if (out) {
      out->stop();          // Stop any previous playback to reset buffers
      out->SetRate(16000);  // Set Sample Rate for ADPCM
      out->begin();         // Restart the I2S driver with new settings
      Serial.println("[Audio] I2S Re-configured: 16kHz for ADPCM streaming");
    }
    Serial.println("[Audio] Stream RX mode activated");
    return true;
  } else {
    Serial.println("[Audio] Failed to start UDP listener");
    return false;
  }
}

void AudioManager::stopStream() {
  if (streamActive) {
    udp.stop();
    streamActive = false;
    currentMode = MODE_IDLE;
    // Reset I2S to default settings for MP3 playback
    if (out) {
      out->stop();          // Stop streaming playback
      out->SetRate(44100);  // Reset to MP3 sample rate
      out->begin();         // Restart the I2S driver with MP3 settings
      Serial.println("[Audio] I2S reset to 44.1kHz for MP3 playback");
    }
    Serial.println("[Audio] Stream stopped");
  }
}

void AudioManager::processStream() {
  if (!streamActive || !initialized) {
    return;
  }

  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    Serial.printf("[Audio] Received UDP packet: %d bytes\n", packetSize);
    // Read UDP packet
    uint8_t buffer[512];
    int len = udp.read(buffer, sizeof(buffer));
    if (len > 0) {
      Serial.printf("[Audio] Processing %d bytes of ADPCM data\n", len);
      // Decode ADPCM and play - limit processing to avoid stack overflow
      int maxSamples =
          256;  // Process max 256 samples per call to prevent blocking
      for (int i = 0; i < len && i < maxSamples; i++) {
        // Each byte contains 2 nibbles (4-bit ADPCM samples)
        int16_t sample1 = decodeADPCMNibble(buffer[i] >> 4);
        int16_t sample2 = decodeADPCMNibble(buffer[i] & 0x0F);

        // Debug: Check if decoder is producing valid samples (much less
        // frequent)
        static int debug_count = 0;
        if (debug_count++ % 5000 == 0) {  // Every 5000 samples instead of 1000
          Serial.printf("[Debug] PCM Samples: %d, %d\n", sample1, sample2);
        }

        if (out) {
          // Send mono samples to BOTH Left and Right channels for stereo I2S
          out->ConsumeSample(&sample1);  // Left channel
          out->ConsumeSample(&sample1);  // Right channel
          out->ConsumeSample(&sample2);  // Left channel
          out->ConsumeSample(&sample2);  // Right channel
        }
      }

      // If we processed the full packet, check for more packets
      if (len >= 512) {
        // Yield to prevent blocking the RTOS task
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
  }
}

// ============================================================================
// ADPCM DECODER
// ============================================================================

void AudioManager::initADPCM() {
  adpcmPredictor = 0;
  adpcmStepIndex = 0;
}

int16_t AudioManager::decodeADPCMNibble(uint8_t nibble) {
  // IMA ADPCM step table
  static const int16_t stepTable[89] = {
      7,     8,     9,     10,    11,    12,    13,    14,    16,    17,
      19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
      50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
      130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
      337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
      876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
      2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
      5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
      15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

  // IMA ADPCM index table
  static const int8_t indexTable[16] = {-1, -1, -1, -1, 2, 4, 6, 8,
                                        -1, -1, -1, -1, 2, 4, 6, 8};

  int16_t step = stepTable[adpcmStepIndex];
  int16_t diff = step >> 3;

  if (nibble & 4) diff += step;
  if (nibble & 2) diff += step >> 1;
  if (nibble & 1) diff += step >> 2;

  if (nibble & 8) {
    adpcmPredictor -= diff;
  } else {
    adpcmPredictor += diff;
  }

  // Clamp predictor
  if (adpcmPredictor > 32767) adpcmPredictor = 32767;
  if (adpcmPredictor < -32768) adpcmPredictor = -32768;

  // Update step index
  adpcmStepIndex += indexTable[nibble];
  if (adpcmStepIndex < 0) adpcmStepIndex = 0;
  if (adpcmStepIndex > 88) adpcmStepIndex = 88;

  return adpcmPredictor;
}

// ============================================================================
// Audio Request Handler (FREE_SPACE query)
// ============================================================================

bool AudioManager::handleAudioRequest(MQTTManager& mqtt, byte* payload,
                                      unsigned int length) {
  const char req[] = "REQUEST_FREE_SPACE";

  if (length == (sizeof(req) - 1) && memcmp(payload, req, length) == 0) {
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    size_t freeSpace = total - used;

    // Check if current audio file exists and get its size
    size_t currentAudioSize = 0;
    if (LittleFS.exists(recvFilename)) {
      File f = LittleFS.open(recvFilename, "r");
      if (f) {
        currentAudioSize = f.size();
        f.close();
      }
    }

    // Send response: FREE:<freeSpace>:<currentAudioSize>
    String reply = "FREE:" + String(freeSpace) + ":" + String(currentAudioSize);
    mqtt.publish(TOPIC_RESPONSE, reply);
    Serial.printf("[Audio] Responded - Free: %u bytes, Current: %u bytes\n",
                  freeSpace, currentAudioSize);
    return true;
  }
  return false;
}

// ============================================================================
// Audio Chunk Handler (file upload)
// ============================================================================

bool AudioManager::handleAudioChunk(MQTTManager& mqtt, byte* payload,
                                    unsigned int length) {
  // ---------- START: expected size ----------
  const char startPrefix[] = "START:";
  if (length > 6 && memcmp(payload, startPrefix, 6) == 0) {
    String num = String((char*)(payload + 6), length - 6);
    expectedSize = num.toInt();
    receivedSize = 0;

    Serial.printf("[Audio] START Expecting file size: %u bytes\n",
                  expectedSize);

    // Delete old file
    if (LittleFS.exists(recvFilename)) {
      LittleFS.remove(recvFilename);
      Serial.println("[Audio] Old file removed.");
    }

    fsFile = LittleFS.open(recvFilename, "w");
    if (!fsFile) {
      Serial.println("[Audio] ERROR: Could not open file for writing.");
      size_t total = LittleFS.totalBytes();
      size_t used = LittleFS.usedBytes();
      size_t freeBytes = total - used;
      Serial.printf("[Audio] LittleFS total=%u used=%u free=%u\n", total, used,
                    freeBytes);

      receivingFile = false;
      return true;
    }

    Serial.printf("[Audio] File opened: %s\n", recvFilename.c_str());
    Serial.printf("[Audio] Ready to receive %u bytes\n", expectedSize);
    receivingFile = true;
    lastChunkTime = millis();
    return true;
  }

  // ---------- CHUNK: header then raw bytes ----------
  const char chunkPrefix[] = "CHUNK:";
  if (length > 6 && memcmp(payload, chunkPrefix, 6) == 0) {
    if (!receivingFile) {
      Serial.println(
          "[Audio] WARNING: Received chunk but not in receiving mode!");
      return true;
    }

    // Parse header: CHUNK:<index>:<total>:<binary_data>
    int firstColon = -1;
    int secondColon = -1;
    for (unsigned int i = 6; i < length; ++i) {
      if (payload[i] == ':') {
        if (firstColon == -1)
          firstColon = i;
        else {
          secondColon = i;
          break;
        }
      }
    }

    if (firstColon == -1 || secondColon == -1) {
      Serial.println("[Audio] WARNING: Malformed chunk header!");
      return true;
    }

    int chunkIndex = parseNumberFromPayload(payload, 6, firstColon);
    int headerLen = secondColon + 1;
    int rawLen = (int)length - headerLen;

    if (rawLen > 0) {
      size_t bytesWritten = fsFile.write(payload + headerLen, rawLen);
      if (bytesWritten != (size_t)rawLen) {
        Serial.printf("[Audio] WARNING: Wrote %u bytes but expected %d!\n",
                      bytesWritten, rawLen);
      }
      receivedSize += rawLen;
      float progress = (receivedSize * 100.0) / expectedSize;
      Serial.printf("[Audio] Chunk %d: %d bytes | Total: %u/%u (%.1f%%)\n",
                    chunkIndex, rawLen, receivedSize, expectedSize, progress);

      // Send ACK for this chunk
      char ackBuf[32];
      snprintf(ackBuf, sizeof(ackBuf), "ACK:%d", chunkIndex);
      bool published = mqtt.publish(TOPIC_ACK, ackBuf);
      if (published) {
        Serial.printf("[Audio] ✓ ACK sent for chunk %d\n", chunkIndex);
      } else {
        Serial.printf("[Audio] ✗ Failed to send ACK for chunk %d\n",
                      chunkIndex);
      }

      lastChunkTime = millis();
    }
    return true;
  }

  // ---------- END ----------
  const char endMsg[] = "END";
  if (length == 3 && memcmp(payload, endMsg, 3) == 0) {
    if (receivingFile) {
      fsFile.close();
      receivingFile = false;
      Serial.println("[Audio] ============================================");
      Serial.printf("[Audio] FILE UPLOAD COMPLETE!\n");
      Serial.printf("[Audio] Total received: %u bytes (expected: %u)\n",
                    receivedSize, expectedSize);
      Serial.println("[Audio] ============================================");

      // Publish completion status
      mqtt.publish(TOPIC_RESPONSE, "UPLOAD_COMPLETE");
    } else {
      Serial.println(
          "[Audio] WARNING: Received END but not in receiving mode!");
    }
    return true;
  }

  return false;
}
