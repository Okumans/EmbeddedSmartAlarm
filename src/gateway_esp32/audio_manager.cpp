#include "audio_manager.h"

// MQTT / WiFi for remote file upload
#include <LittleFS.h>
#include <PubSubClient.h>
#include <WiFi.h>

// ---------------- MQTT TOPICS ----------------
// (audio topics centralized in include/config.h; legacy defines kept for
// backward compatibility in case other modules still use them)
#define TOPIC_REQUEST MQTT_TOPIC_AUDIO_REQUEST
#define TOPIC_RESPONSE MQTT_TOPIC_AUDIO_RESPONSE
#define TOPIC_CHUNK MQTT_TOPIC_AUDIO_CHUNK
#define TOPIC_ACK MQTT_TOPIC_AUDIO_ACK

// ---------------- WIFI / MQTT SETTINGS ---------------
#include "../../include/config.h"

// AudioManager uses the gateway's shared PubSubClient; the pointer is set
// via `setMQTTClient()` from main.cpp. Do not create a separate client here.

// ---------------- FILE SAVE SETTINGS ----------
File fsFile;
String recvFilename = "/sound.mp3";
bool receivingFile = false;
size_t expectedSize = 0;
size_t receivedSize = 0;
unsigned long lastChunkTime = 0;  // Track upload timeout

// (No forward declarations needed here — MQTT is handled by main gateway)

AudioManager::AudioManager() {
  out = nullptr;
  file = nullptr;
  id3 = nullptr;
  wav = nullptr;
  mp3 = nullptr;
  mqttClientPtr = nullptr;
  initialized = false;
  isPlaying = false;
  currentVolume = 0.5;
}

AudioManager::~AudioManager() { cleanup(); }

void AudioManager::setMQTTClient(PubSubClient* client) {
  mqttClientPtr = client;
}

void AudioManager::cleanup() {
  if (wav) {
    if (wav->isRunning()) wav->stop();
    delete wav;
    wav = nullptr;
  }
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
  // Try mounting first. If mount fails, attempt a format then remount.
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

  // MQTT client is injected from main. AudioManager does not manage the
  // connection or set a global callback. The gateway's mqtt callback will
  // forward audio-related messages to `handleMQTTMessage()`.

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

  if (fname.endsWith(".wav")) {
    return playWAV(filename);
  } else if (fname.endsWith(".mp3")) {
    return playMP3(filename);
  } else {
    Serial.println("[Audio] Unsupported file format. Use .wav or .mp3");
    return false;
  }
}

bool AudioManager::playWAV(const char* filename) {
  if (!initialized) {
    Serial.println("[Audio] Not initialized!");
    return false;
  }

  cleanup();

  Serial.printf("[Audio] Playing WAV: %s\n", filename);

  file = new AudioFileSourceLittleFS(filename);
  wav = new AudioGeneratorWAV();

  if (wav->begin(file, out)) {
    isPlaying = true;
    return true;
  } else {
    Serial.println("[Audio] Failed to start WAV playback");
    cleanup();
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

  // Keep audio playing
  if (wav && wav->isRunning()) {
    if (!wav->loop()) {
      Serial.println("[Audio] WAV playback finished");
      cleanup();
    }
  } else if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      Serial.println("[Audio] MP3 playback finished");
      cleanup();
    }
  } else {
    isPlaying = false;
  }

  // AudioManager relies on the gateway's MQTT client and callback forwarding.
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

  if (wav && wav->isRunning()) {
    return true;
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
      if (filename.endsWith(".wav") || filename.endsWith(".mp3") ||
          filename.endsWith(".WAV") || filename.endsWith(".MP3")) {
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

// Handle audio-related MQTT messages forwarded from main's callback.
bool AudioManager::handleMQTTMessage(const char* topic, byte* payload,
                                     unsigned int length) {
  // Handle simple ASCII header-based protocol where binary data follows header
  Serial.printf("[Audio MQTT] Topic: %s, len=%u\n", topic, length);

  String topicStr = String(topic);
  if (topicStr != TOPIC_REQUEST && topicStr != TOPIC_CHUNK) {
    return false;  // not handled here
  }

  // ---------- 1. PC asks for free space ----------
  if (topicStr == TOPIC_REQUEST) {
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
      String reply =
          "FREE:" + String(freeSpace) + ":" + String(currentAudioSize);
      if (mqttClientPtr) {
        mqttClientPtr->publish(TOPIC_RESPONSE, reply.c_str());
        Serial.printf(
            "[MQTT] Responded - Free: %u bytes, Current audio: %u bytes\n",
            freeSpace, currentAudioSize);
      }
      Serial.printf("[Audio] Free Space: %u bytes\n", freeSpace);
      Serial.printf("[Audio] Current audio file size: %u bytes\n",
                    currentAudioSize);
      return true;
    }
    return false;
  }

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

      // If no free space, attempt a format and retry
      if (freeBytes == 0) {
        Serial.println(
            "[Audio] No free space — attempting LittleFS.format() and retry");
        if (LittleFS.format()) {
          delay(100);
          if (LittleFS.begin()) {
            fsFile = LittleFS.open(recvFilename, "w");
            if (fsFile) {
              Serial.println("[Audio] File opened after format.");
              receivingFile = true;
              return true;
            }
          }
        }
        Serial.println("[Audio] Format retry failed.");
      } else {
        // Try a quick reopen attempt
        Serial.println(
            "[Audio] Attempting to reopen file after short delay...");
        delay(50);
        fsFile = LittleFS.open(recvFilename, "w");
        if (fsFile) {
          receivingFile = true;
          return true;
        }
      }

      receivingFile = false;
      return true;
    }
    Serial.printf("[Audio] File opened successfully: %s\n",
                  recvFilename.c_str());
    Serial.printf("[Audio] Ready to receive %u bytes\n", expectedSize);
    receivingFile = true;
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
      // Send ACK for this chunk so the uploader can proceed
      if (mqttClientPtr) {
        char ackBuf[32];
        snprintf(ackBuf, sizeof(ackBuf), "ACK:%d", chunkIndex);
        bool published = mqttClientPtr->publish(TOPIC_ACK, ackBuf);
        if (published) {
          Serial.printf("[MQTT] ✓ ACK sent for chunk %d\n", chunkIndex);
        } else {
          Serial.printf("[MQTT] ✗ Failed to send ACK for chunk %d\n",
                        chunkIndex);
        }
      } else {
        Serial.println("[MQTT] ERROR: mqttClientPtr is NULL!");
      }
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
    } else {
      Serial.println(
          "[Audio] WARNING: Received END but not in receiving mode!");
    }
    return true;
  }

  return false;
}
