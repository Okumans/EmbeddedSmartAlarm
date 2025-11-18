#include "audio_manager.h"

// MQTT Manager for remote file upload
#include <LittleFS.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

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

AudioManager::AudioManager() {
  out = nullptr;
  file = nullptr;
  id3 = nullptr;
  mp3 = nullptr;
  initialized = false;
  isPlaying = false;
  currentVolume = 0.5;

  // Streaming state
  currentState = AUDIO_STATE_IDLE;
  opusDecoder = nullptr;
  prerollComplete = false;

  // Initialize silence buffer
  memset(silenceBuffer, 0, sizeof(silenceBuffer));
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
// STREAMING METHODS
// ============================================================================

void AudioManager::cleanupStreaming() {
  if (opusDecoder) {
    opus_decoder_destroy(opusDecoder);
    opusDecoder = nullptr;
    Serial.println("[Audio] Opus decoder destroyed");
  }
}

bool AudioManager::startStreaming() {
  if (!initialized) {
    Serial.println("[Audio] Cannot start streaming - not initialized!");
    return false;
  }

  Serial.println("[Audio] ========================================");
  Serial.println("[Audio] Starting Streaming Mode");
  Serial.println("[Audio] ========================================");

  // Stop any currently playing file
  if (isPlaying) {
    Serial.println("[Audio] Stopping file playback...");
    stop();
  }

  // Cleanup any existing streaming resources
  cleanupStreaming();

  // Create Opus decoder
  int error;
  opusDecoder =
      opus_decoder_create(STREAM_SAMPLE_RATE, STREAM_CHANNELS, &error);

  if (error != OPUS_OK || opusDecoder == nullptr) {
    Serial.printf("[Audio] ERROR: Failed to create Opus decoder! Error: %d\n",
                  error);
    return false;
  }

  Serial.println("[Audio] ✓ Opus decoder created");

  // Reconfigure I2S for 48kHz
  Serial.println("[Audio] Reconfiguring I2S for 48kHz...");
  i2s_set_sample_rates(I2S_NUM_0, STREAM_SAMPLE_RATE);
  Serial.println("[Audio] ✓ I2S configured for 48kHz");

  // Reset StreamBuffer (extern declaration)
  extern StreamBufferHandle_t opusStreamBuffer;
  if (opusStreamBuffer) {
    xStreamBufferReset(opusStreamBuffer);
    Serial.println("[Audio] ✓ StreamBuffer reset");
  } else {
    Serial.println("[Audio] WARNING: StreamBuffer not available!");
  }

  // Set state
  currentState = AUDIO_STATE_STREAMING;
  prerollComplete = false;

  Serial.println("[Audio] ✓ Streaming mode active!");
  Serial.println("[Audio] ========================================");

  return true;
}

void AudioManager::stopStreaming() {
  if (currentState != AUDIO_STATE_STREAMING) {
    return;
  }

  Serial.println("[Audio] ========================================");
  Serial.println("[Audio] Stopping Streaming Mode");
  Serial.println("[Audio] ========================================");

  // Cleanup decoder
  cleanupStreaming();

  // Restore I2S to default 44.1kHz for MP3 playback
  Serial.println("[Audio] Restoring I2S to 44.1kHz...");
  i2s_set_sample_rates(I2S_NUM_0, 44100);

  // Write silence to prevent pops
  size_t bytes_written;
  i2s_write(I2S_NUM_0, silenceBuffer, sizeof(silenceBuffer), &bytes_written,
            portMAX_DELAY);

  currentState = AUDIO_STATE_IDLE;
  prerollComplete = false;

  Serial.println("[Audio] ✓ Streaming stopped");
  Serial.println("[Audio] ========================================");
}

bool AudioManager::isStreaming() const {
  return currentState == AUDIO_STATE_STREAMING;
}

void AudioManager::processStreamingAudio() {
  if (currentState != AUDIO_STATE_STREAMING || opusDecoder == nullptr) {
    return;
  }

  extern StreamBufferHandle_t opusStreamBuffer;
  if (opusStreamBuffer == nullptr) {
    return;
  }

  // Check for pre-roll (wait for buffer to fill up before starting playback)
  if (!prerollComplete) {
    size_t bytesAvailable = xStreamBufferBytesAvailable(opusStreamBuffer);
    if (bytesAvailable < PREROLL_MIN_BYTES) {
      // Buffer not full enough yet - write silence to I2S
      size_t bytes_written;
      i2s_write(I2S_NUM_0, silenceBuffer, sizeof(silenceBuffer), &bytes_written,
                0);
      return;
    } else {
      prerollComplete = true;
      Serial.printf("[Audio] Pre-roll complete! Buffer filled: %u bytes\n",
                    bytesAvailable);
    }
  }

  // Step 1: Read packet length header (2 bytes)
  uint16_t packetLength = 0;
  size_t received =
      xStreamBufferReceive(opusStreamBuffer, &packetLength, PACKET_HEADER_SIZE,
                           pdMS_TO_TICKS(STREAM_BUFFER_TIMEOUT_MS));

  if (received != PACKET_HEADER_SIZE) {
    // No data available or timeout - write silence to keep I2S running
    size_t bytes_written;
    i2s_write(I2S_NUM_0, silenceBuffer, sizeof(silenceBuffer), &bytes_written,
              0);
    return;
  }

  // Validate packet length
  if (packetLength == 0 || packetLength > MAX_OPUS_PACKET_SIZE) {
    Serial.printf("[Audio] ERROR: Invalid packet length: %u\n", packetLength);
    // Try to recover by reading silence
    size_t bytes_written;
    i2s_write(I2S_NUM_0, silenceBuffer, sizeof(silenceBuffer), &bytes_written,
              0);
    return;
  }

  // Step 2: Read the actual Opus packet
  received =
      xStreamBufferReceive(opusStreamBuffer, opusPacketBuffer, packetLength,
                           pdMS_TO_TICKS(STREAM_BUFFER_TIMEOUT_MS));

  if (received != packetLength) {
    Serial.printf("[Audio] ERROR: Incomplete packet! Expected %u, got %u\n",
                  packetLength, received);
    // Write silence and continue
    size_t bytes_written;
    i2s_write(I2S_NUM_0, silenceBuffer, sizeof(silenceBuffer), &bytes_written,
              0);
    return;
  }

  // Step 3: Decode Opus packet to PCM
  int decodedSamples = opus_decode(opusDecoder, opusPacketBuffer, packetLength,
                                   pcmBuffer, OPUS_FRAME_SAMPLES, 0);

  if (decodedSamples < 0) {
    Serial.printf("[Audio] ERROR: Opus decode failed! Error: %d\n",
                  decodedSamples);
    // Write silence on error
    size_t bytes_written;
    i2s_write(I2S_NUM_0, silenceBuffer, sizeof(silenceBuffer), &bytes_written,
              0);
    return;
  }

  // Step 4: Write PCM data to I2S
  size_t bytes_written;
  size_t bytesToWrite = decodedSamples * sizeof(int16_t);
  i2s_write(I2S_NUM_0, pcmBuffer, bytesToWrite, &bytes_written, portMAX_DELAY);

  if (bytes_written != bytesToWrite) {
    Serial.printf("[Audio] WARNING: I2S partial write! Expected %u, wrote %u\n",
                  bytesToWrite, bytes_written);
  }

  // Success! (Uncomment for debugging - will be very verbose)
  // Serial.printf("[Audio] Decoded %d samples, wrote %u bytes\n",
  // decodedSamples, bytes_written);
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
