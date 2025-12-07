#include "../../include/gateway_esp32/audio_manager.h"

// MQTT Manager for remote file upload
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>

#include "../../include/gateway_esp32/mqtt_manager.h"
#include "../../include/gateway_esp32/sd_manager.h"

// ---------------- MQTT TOPICS ----------------
#include "../../include/shared/config.h"

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
      lastChunkTime{0},
      downloadingInProgress{false} {
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

  // Don't initialize I2S yet - wait until playback needed
  Serial.println("[Audio] Audio manager initialized (I2S deferred)");

  initialized = true;
  i2sInitialized = false;
  return true;
}

bool AudioManager::ensureI2SInitialized() {
  if (i2sInitialized) {
    return true;
  }

  Serial.println("[Audio] Initializing I2S hardware...");

  // Initialize I2S output
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(currentVolume);

  i2sInitialized = true;
  Serial.println("[Audio] I2S hardware initialized");
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

  // Initialize I2S if not already done
  if (!ensureI2SInitialized()) {
    Serial.println("[Audio] Failed to initialize I2S!");
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

  // Handler for audio download commands
  mqtt.registerAndSubscribe(
      "esp32/audio_download_cmd",
      [this](MQTTManager& mqtt, const char* topic, byte* payload,
             unsigned int length) -> bool {
        return this->handleDownloadCommand(mqtt, payload, length);
      },
      "AudioDownloadCmd",
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
// Audio Download Command Handler
// ============================================================================

bool AudioManager::handleDownloadCommand(MQTTManager& mqtt, byte* payload,
                                         unsigned int length) {
  String payloadStr = String((char*)payload, length);
  Serial.printf("[Audio] Received download command (length: %u)\n", length);
  Serial.printf("[Audio] Raw payload: %s\n", payloadStr.c_str());

  // URL decode the payload (replace %7C with |)
  // Supports both simple URLs and presigned URLs with query parameters
  // Example:
  // http://172.20.10.2:9000/audio/file.mp3?X-Amz-Algorithm=...%7CsoundId
  payloadStr.replace("%7C", "|");
  Serial.printf("[Audio] Decoded payload: %s\n", payloadStr.c_str());

  // Parse payload: "{url}|{soundId}"
  // URL can be simple or presigned with query parameters
  int separatorIndex = payloadStr.indexOf('|');
  if (separatorIndex == -1) {
    Serial.println(
        "[Audio] ERROR: Invalid payload format - missing | separator");
    mqtt.publish("esp32/audio/status", "download_failed|unknown");
    return true;
  }

  String url = payloadStr.substring(0, separatorIndex);
  String idStr = payloadStr.substring(separatorIndex + 1);

  // Trim whitespace
  url.trim();
  idStr.trim();

  // Validate we have both URL and ID
  if (url.length() == 0 || idStr.length() == 0) {
    Serial.printf(
        "[Audio] ERROR: Invalid payload - URL length: %d, ID length: %d\n",
        url.length(), idStr.length());
    mqtt.publish("esp32/audio/status", "download_failed|invalid");
    return true;
  }

  // Use fixed filename for alarm downloads to ensure single active audio file
  // Always save downloaded audio as /audio.mp3
  String filename = "/audio.mp3";

  Serial.printf("[Audio] Sound ID: %s\n", idStr.c_str());
  Serial.printf("[Audio] Download URL: %s\n", url.c_str());
  Serial.printf("[Audio] Target file: %s\n", filename.c_str());

  // Call downloadFile
  bool success = downloadFile(url.c_str(), filename.c_str());

  // Publish status with soundId for backend validation
  String statusMessage;
  if (success) {
    statusMessage = "download_success|" + idStr;
    Serial.println("[Audio] Download completed successfully");
  } else {
    statusMessage = "download_failed|" + idStr;
    Serial.println("[Audio] Download failed");
  }

  mqtt.publish("esp32/audio/status", statusMessage);
  Serial.printf("[Audio] Published status: %s\n", statusMessage.c_str());

  return true;
}

// ============================================================================
// HTTP Download Implementation
// ============================================================================

bool AudioManager::downloadFile(const char* url, const char* filename) {
  if (!sdManager || !sdManager->isReady()) {
    Serial.println("[Audio] ERROR: SD Manager not ready");
    return false;
  }

  Serial.printf("[Audio] Connecting to: %s\n", url);
  Serial.printf("[Audio] ESP32 IP: %s\n", WiFi.localIP().toString().c_str());

  HTTPClient http;

  // Increase timeout for slow connections
  http.setTimeout(15000);  // 15 seconds

  if (!http.begin(url)) {
    Serial.println("[Audio] ERROR: Failed to begin HTTP connection");
    return false;
  }

  Serial.println("[Audio] Sending HTTP GET request...");
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[Audio] HTTP GET failed, code: %d\n", httpCode);
    if (httpCode == -1) {
      Serial.println("[Audio] Error -1 means: Connection failed");
      Serial.println("[Audio] Possible causes:");
      Serial.println("[Audio]   - Server not running");
      Serial.println("[Audio]   - Wrong IP address");
      Serial.println("[Audio]   - Firewall blocking port");
      Serial.println("[Audio]   - ESP32 and server on different networks");
    }
    http.end();
    return false;
  }

  Serial.printf("[Audio] HTTP OK! Content length: %d bytes\n", http.getSize());

  // Open file for writing
  if (!sdManager->openForWrite(filename)) {
    Serial.println("[Audio] ERROR: Could not open file for writing");
    http.end();
    return false;
  }

  // Allocate buffer
  const size_t bufferSize = 1024;
  uint8_t* buffer = (uint8_t*)malloc(bufferSize);
  if (!buffer) {
    Serial.println("[Audio] ERROR: Memory allocation failed");
    sdManager->closeFile();
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  int len = http.getSize();  // Get content length
  size_t totalBytes = 0;

  downloadingInProgress = true;

  // Read and write in loop
  while (http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if (size) {
      int c =
          stream->readBytes(buffer, ((size > bufferSize) ? bufferSize : size));
      if (!sdManager->writeChunk(buffer, c)) {
        Serial.println("[Audio] ERROR: Write to SD failed");
        free(buffer);
        sdManager->closeFile();
        http.end();
        downloadingInProgress = false;
        return false;
      }

      if (len > 0) len -= c;
      totalBytes += c;

      if (totalBytes % 1000 == 0) {
        Serial.printf("[Audio] Downloaded %d bytes\n", totalBytes);
      }
    }

    // 2. ENSURE DELAY IS SUFFICIENT (Keep this)
    delay(1);
  }

  // Cleanup
  free(buffer);
  sdManager->closeFile();
  http.end();
  downloadingInProgress = false;

  // 3. VERIFY FILE SIZE (Optional but recommended check)
  if (totalBytes == 0) {
    Serial.println("[Audio] Error: Downloaded 0 bytes");
    return false;
  }

  Serial.printf("[Audio] Download complete: %u bytes written to %s\n",
                totalBytes, filename);
  return true;
}

// ============================================================================
// Additional Methods
// ============================================================================

bool AudioManager::playFileFromSD(const char* filename) {
  return playFile(filename);
}

void AudioManager::setMQTTManager(MQTTManager* mqtt) { mqttManager = mqtt; }

void AudioManager::setSDManager(SDManager* sd) { sdManager = sd; }

bool AudioManager::isDownloading() { return downloadingInProgress; }

float AudioManager::getDownloadProgress() const {
  if (!receivingFile || expectedSize == 0) return -1.0f;
  return (float)receivedSize / expectedSize;
}
