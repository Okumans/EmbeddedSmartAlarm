#include "audio_manager.h"

// MQTT Manager for remote file upload
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>

#include "../../include/mqtt_manager.h"
#include "../../include/sd_manager.h"

// ---------------- MQTT TOPICS ----------------
#include "../../include/config.h"

#define TOPIC_REQUEST MQTT_TOPIC_AUDIO_REQUEST
#define TOPIC_RESPONSE MQTT_TOPIC_AUDIO_RESPONSE
#define TOPIC_CHUNK MQTT_TOPIC_AUDIO_CHUNK
#define TOPIC_ACK MQTT_TOPIC_AUDIO_ACK
#define TOPIC_STATUS MQTT_TOPIC_AUDIO_STATUS

File fsFile;

AudioManager::AudioManager()
    : out{nullptr},
      file{nullptr},
      id3{nullptr},
      mp3{nullptr},
      initialized{false},
      isPlaying{false},
      currentVolume{0.5},
      mqttManager{nullptr},
      sdManager{nullptr},
      receivingFile{false},
      expectedSize{0},
      receivedSize{0},
      lastChunkTime{0} {
  // Create Recursive Mutex
  audioMutex = xSemaphoreCreateRecursiveMutex();
}

AudioManager::~AudioManager() {
  cleanup();
  vSemaphoreDelete(audioMutex);
}

// cleanup() is private helper, assumes caller holds lock!
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

  // Initialize I2S output
  Serial.println("[Audio] Initializing I2S output...");
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(currentVolume);

  initialized = true;
  Serial.println("[Audio] Audio system initialized");

  return true;
}

void AudioManager::end() {
  cleanup();

  if (out) {
    delete out;
    out = nullptr;
  }

  // Note: SD.end() is now handled by SDManager
  initialized = false;
  Serial.println("[Audio] Audio system stopped");
}

bool AudioManager::playFile(const char* filename) {
  // LOCK
  xSemaphoreTakeRecursive(audioMutex, portMAX_DELAY);

  if (!initialized) {
    Serial.println("[Audio] Not initialized!");
    xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
    return false;
  }

  // Check if SD manager is ready
  if (!sdManager || !sdManager->isReady()) {
    Serial.println("[Audio] SD manager not ready!");
    xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
    return false;
  }

  // Stop any currently playing audio
  stop();

  // Check if file exists
  if (!sdManager->exists(filename)) {
    Serial.printf("[Audio] File not found: %s\n", filename);
    xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
    return false;
  }

  // Determine file type by extension
  String fname = String(filename);
  fname.toLowerCase();

  if (fname.endsWith(".mp3")) {
    bool result = playMP3(filename);
    xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
    return result;
  } else {
    Serial.println("[Audio] Unsupported file format. Use .mp3");
    xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
    return false;
  }
}

bool AudioManager::playMP3(const char* filename) {
  xSemaphoreTakeRecursive(audioMutex, portMAX_DELAY);  // LOCK

  cleanup();  // Safe to call now

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
    xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
    return true;
  } else {
    Serial.println("[Audio] Failed to start MP3 playback");
    cleanup();
    xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
    return false;
  }
}

void AudioManager::stop() {
  xSemaphoreTakeRecursive(audioMutex, portMAX_DELAY);  // LOCK
  cleanup();
  Serial.println("[Audio] Stopped");
  xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
}

// CRITICAL: This runs on Core 1
void AudioManager::loop() {
  // We grab the lock. If Core 0 is busy setting up a song (playFile),
  // we wait here instead of crashing on a bad pointer.
  if (xSemaphoreTakeRecursive(audioMutex, 5) == pdTRUE) {  // Wait max 5 ticks

    if (initialized && isPlaying && mp3 && mp3->isRunning()) {
      if (!mp3->loop()) {
        Serial.println("[Audio] MP3 playback finished");
        cleanup();  // Safe
        // Publish finished status
        if (mqttManager) {
          mqttManager->publish(TOPIC_STATUS, "finished");
          Serial.println("[Audio] Published 'finished' status");
        }
      }
    } else {
      isPlaying = false;
    }

    xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
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
  xSemaphoreTakeRecursive(audioMutex, portMAX_DELAY);  // LOCK

  if (!initialized || !isPlaying) {
    xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
    return false;
  }

  if (mp3 && mp3->isRunning()) {
    xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
    return true;
  }

  isPlaying = false;
  xSemaphoreGiveRecursive(audioMutex);  // UNLOCK
  return false;
}

void AudioManager::listFiles() {
  if (sdManager && sdManager->isReady()) {
    sdManager->listAudioFiles();
  } else {
    Serial.println("[Audio] SD manager not ready");
  }
}

String AudioManager::getFileList() {
  if (sdManager && sdManager->isReady()) {
    return sdManager->listAudioFiles();
  } else {
    return "";
  }
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
    if (sdManager->exists(recvFilename.c_str())) {
      currentAudioSize = sdManager->getFileSize(recvFilename.c_str());
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
  if (!sdManager || !sdManager->isReady()) return true;

  // ---------- START: expected size:uuid ----------
  const char startPrefix[] = "START:";
  if (length > 6 && memcmp(payload, startPrefix, 6) == 0) {
    String num = String((char*)(payload + 6), length - 6);
    // Parse UUID from payload (START:<size>:<uuid>)
    int colonPos = num.indexOf(':');
    if (colonPos != -1) {
      expectedSize = num.substring(0, colonPos).toInt();
      String audioId = num.substring(colonPos + 1);
      // Create filename with UUID: /sound_<uuid>.mp3
      recvFilename = "/sound_" + audioId + ".mp3";
    } else {
      expectedSize = num.toInt();
      recvFilename = "/uploaded.mp3";  // fallback
    }
    receivedSize = 0;

    Serial.printf("[Audio] START Expecting file size: %u bytes, UUID: %s\n",
                  expectedSize, recvFilename.c_str());

    // Delete old file if exists
    if (sdManager->exists(recvFilename.c_str())) {
      sdManager->remove(recvFilename.c_str());
      Serial.println("[Audio] Old file removed.");
    }

    // Open file for writing
    if (!sdManager->openForWrite(recvFilename.c_str())) {
      Serial.println("[Audio] ERROR: Could not open file for writing.");
      Serial.println("[Audio] SD card may be full or not available");
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
      // DELEGATE: Write raw bytes to SDManager
      bool success = sdManager->writeChunk(payload + headerLen, rawLen);

      if (!success) {
        Serial.println("[Audio] Write failed! Aborting.");
        sdManager->closeFile();
        receivingFile = false;
        return true;
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
      // DELEGATE: Safe close and cool-down
      sdManager->closeFile();
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

void AudioManager::setSDManager(SDManager* sd) { sdManager = sd; }

bool AudioManager::isDownloading() { return receivingFile; }

float AudioManager::getDownloadProgress() const {
  if (!receivingFile || expectedSize == 0) return -1.0f;
  return (float)receivedSize / expectedSize;
}
