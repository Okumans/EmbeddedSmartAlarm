#include "audio_manager.h"

// MQTT Manager for remote file upload
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>

#include "../../include/mqtt_manager.h"

// ---------------- MQTT TOPICS ----------------
#include "../../include/config.h"

#define TOPIC_REQUEST MQTT_TOPIC_AUDIO_REQUEST
#define TOPIC_RESPONSE MQTT_TOPIC_AUDIO_RESPONSE
#define TOPIC_CHUNK MQTT_TOPIC_AUDIO_CHUNK
#define TOPIC_ACK MQTT_TOPIC_AUDIO_ACK
#define TOPIC_STATUS MQTT_TOPIC_AUDIO_STATUS

File fsFile;
String recvFilename = "/question.mp3";
bool receivingFile = false;
size_t expectedSize = 0;
size_t receivedSize = 0;
unsigned long lastChunkTime = 0;  // Track upload timeout

AudioManager::AudioManager()
    : out{nullptr},
      file{nullptr},
      id3{nullptr},
      mp3{nullptr},
      initialized{false},
      isPlaying{false},
      currentVolume{0.5},
      mqttManager{nullptr} {}

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

  Serial.println("[Audio] Initializing SD card...");
  Serial.println(
      "[Audio] Mounting SD card (CS=5, MOSI=23, MISO=19, CLK=18)...");

  SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);

  int attempts = 0;
  const int maxAttempts = 10;  // Maximum 10 attempts
  while (!SD.begin(SD_CS, SPI, 1000000) && attempts < maxAttempts) {
    attempts++;
    Serial.printf("[Audio] SD card mount failed! Attempt %d/%d\n", attempts,
                  maxAttempts);
    delay(1000);  // Wait 1 second before retrying
  }

  if (attempts >= maxAttempts) {
    Serial.println("[Audio] SD card mount failed after maximum attempts!");
    return false;
  }

  Serial.println("[Audio] SD card mounted successfully");
  printSDInfo();

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

  SD.end();
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
  if (!SD.exists(filename)) {
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

  file = new AudioFileSourceSD(filename);
  id3 = new AudioFileSourceID3(file);
  mp3 = new AudioGeneratorMP3();

  if (mp3->begin(id3, out)) {
    isPlaying = true;
    // Publish playing status
    if (mqttManager) {
      mqttManager->publish(TOPIC_STATUS, "playing");
      Serial.println("[Audio] Published 'playing' status");
    }
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
      // Publish finished status
      if (mqttManager) {
        mqttManager->publish(TOPIC_STATUS, "finished");
        Serial.println("[Audio] Published 'finished' status");
      }
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
  Serial.println("[Audio] Files in SD card:");
  Serial.println("----------------------------------------");

  File root = SD.open("/");
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

void AudioManager::printSDInfo() {
  Serial.println("[Audio] SD Card Info:");
  Serial.println("  SD card is mounted and ready");
  // Note: SD library doesn't provide total/used bytes like LittleFS
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
// Audio Request Handler (FREE_SPACE query)
// ============================================================================

bool AudioManager::handleAudioRequest(MQTTManager& mqtt, byte* payload,
                                      unsigned int length) {
  const char req[] = "REQUEST_FREE_SPACE";

  if (length == (sizeof(req) - 1) && memcmp(payload, req, length) == 0) {
    // For SD card, we don't have easy free space calculation
    // Return a large dummy value indicating SD is available
    size_t freeSpace = 100000000;  // 100MB dummy value

    // Check if current audio file exists and get its size
    size_t currentAudioSize = 0;
    if (SD.exists(recvFilename)) {
      File f = SD.open(recvFilename, "r");
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
  static size_t bytesSinceFlush = 0;  // Track bytes written since last flush

  // --- DEBUG: Print Header Preview ---
  char debugHead[10];
  int copyLen = length < 9 ? length : 9;
  memcpy(debugHead, payload, copyLen);
  debugHead[copyLen] = '\0';
  // Print hex values of first 6 bytes to check for hidden chars/corruption
  Serial.printf(
      "[Audio] RX Payload Preview: '%s' (Hex: %02X %02X %02X %02X %02X %02X)\n",
      debugHead, payload[0], payload[1], payload[2], payload[3], payload[4],
      payload[5]);

  // ---------- START: expected size ----------
  const char startPrefix[] = "START:";
  if (length > 6 && memcmp(payload, startPrefix, 6) == 0) {
    String num = String((char*)(payload + 6), length - 6);
    expectedSize = num.toInt();
    receivedSize = 0;

    Serial.printf("[Audio] START Expecting file size: %u bytes\n",
                  expectedSize);

    // Delete old file
    if (SD.exists(recvFilename)) {
      SD.remove(recvFilename);
      Serial.println("[Audio] Old file removed.");
    }

    fsFile = SD.open(recvFilename, "w");
    if (!fsFile) {
      Serial.println("[Audio] ERROR: Could not open file for writing.");
      // For SD, we can't easily get free space, so just report error
      Serial.println("[Audio] SD card may be full or not available");
      receivingFile = false;
      return true;
    }

    Serial.printf("[Audio] File opened: %s\n", recvFilename.c_str());
    Serial.printf("[Audio] Ready to receive %u bytes\n", expectedSize);

    bytesSinceFlush = 0;  // RESET FLUSH COUNTER HERE

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

      // --- NEW FLUSH LOGIC ---
      bytesSinceFlush += rawLen;
      if (bytesSinceFlush >= 32768) {  // Flush every 32KB
        fsFile.flush();
        bytesSinceFlush = 0;
        // Optional debug print (comment out if too noisy)
        // Serial.println("[Audio] Intermediate Flush (32KB saved)");
      }
      // -----------------------

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
      // --- FIX: Ensure physical write and cool-down ---
      Serial.println("[Audio] Finalizing write to SD card...");
      fsFile.flush();  // Force data to physical card
      fsFile.close();  // Close file handle

      // CRITICAL: Give SD card time to update FAT table
      // 'Select Failed' happens if we access it while it's busy internally.
      delay(500);
      // ------------------------------------------------

      receivingFile = false;

      Serial.println("[Audio] ============================================");
      Serial.printf("[Audio] FILE UPLOAD COMPLETE!\n");
      Serial.printf("[Audio] Total received: %u bytes (expected: %u)\n",
                    receivedSize, expectedSize);
      Serial.println("[Audio] ============================================");

      // Publish completion status
      if (mqttManager) {  // Use the pointer directly
        mqttManager->publish(TOPIC_RESPONSE, "UPLOAD_COMPLETE");
      }
    } else {
      Serial.println(
          "[Audio] WARNING: Received END but not in receiving mode!");
    }
    return true;
  }

  return false;
}

// ============================================================================
// Additional Methods
// ============================================================================

bool AudioManager::playFileFromSD(const char* filename) {
  return playFile(filename);
}

void AudioManager::setMQTTManager(MQTTManager* mqtt) { mqttManager = mqtt; }

bool AudioManager::isDownloading() {
  extern bool
      receivingFile;  // Access the global variable defined at top of file
  return receivingFile;
}
