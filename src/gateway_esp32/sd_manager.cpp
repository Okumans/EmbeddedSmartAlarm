#include "../../include/gateway_esp32/sd_manager.h"

SDManager::SDManager() : _ready(false), _bytesSinceFlush(0) {}

bool SDManager::begin(int maxRetries) {
  if (_ready) return true;

  Serial.println("[SD] Initializing SD card...");

  // --- STABILITY FIX ---
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(SD_MISO_PIN, INPUT_PULLUP);
  delay(50);
  // ---------------------

  SPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  for (int attempt = 0; attempt < maxRetries; attempt++) {
    Serial.printf("[SD] Mount attempt %d/%d at 4MHz...\n", attempt + 1,
                  maxRetries);

    if (SD.begin(SD_CS_PIN, SPI, 4000000)) {
      _ready = true;
      Serial.println("[SD] Mount Success at 4MHz!");
      return true;
    }

    Serial.println("[SD] 4MHz Mount Failed. Retrying at 1MHz...");
    delay(100);

    if (SD.begin(SD_CS_PIN, SPI, 1000000)) {
      _ready = true;
      Serial.println("[SD] Mount Success at 1MHz!");
      return true;
    }

    if (attempt < maxRetries - 1) {
      Serial.printf("[SD] Attempt %d failed. Waiting before retry...\n",
                    attempt + 1);
      delay(500);
    }
  }

  Serial.printf("[SD] CRITICAL: All %d mount attempts failed!\n", maxRetries);
  _ready = false;
  return false;
}

bool SDManager::isReady() { return _ready; }

// ================= FILE WRITING LOGIC =================

bool SDManager::openForWrite(const char* filename) {
  if (!_ready) return false;

  // Clean up previous file if open
  if (_file) _file.close();

  // Remove existing file to start fresh
  if (SD.exists(filename)) {
    SD.remove(filename);
  }

  _file = SD.open(filename, FILE_WRITE);
  if (!_file) {
    Serial.printf("[SD] Failed to open %s for writing\n", filename);
    return false;
  }

  _bytesSinceFlush = 0;
  Serial.printf("[SD] Opened %s for writing\n", filename);
  return true;
}

bool SDManager::writeChunk(const uint8_t* data, size_t len) {
  if (!_ready || !_file) return false;

  size_t written = _file.write(data, len);

  // Safety Check
  if (written != len) {
    Serial.println("[SD] Write failed! Disk full or error.");
    _file.close();
    return false;
  }

  // Smart Flushing Strategy (Every 32KB)
  _bytesSinceFlush += written;
  if (_bytesSinceFlush >= 32768) {
    _file.flush();
    _bytesSinceFlush = 0;
    Serial.println("[SD] Auto-flush");
  }

  return true;
}

void SDManager::closeFile() {
  if (_file) {
    _file.flush();  // Force final write
    _file.close();

    // CRITICAL COOL-DOWN (Prevents Select Failed)
    delay(500);

    Serial.println("[SD] File closed and saved.");
  }
  _bytesSinceFlush = 0;
}

// ================= FILE MANAGEMENT =================

String SDManager::listAudioFiles() {
  String result = "";
  if (!_ready) return result;

  File root = SD.open("/");
  File file = root.openNextFile();
  bool first = true;

  while (file) {
    if (!file.isDirectory()) {
      String n = file.name();
      if (n.endsWith(".mp3") || n.endsWith(".wav")) {
        if (!first) result += ",";
        result += n;
        first = false;
      }
    }
    file = root.openNextFile();
  }

  return result;
}

bool SDManager::exists(const char* filename) {
  return _ready && SD.exists(filename);
}

void SDManager::remove(const char* filename) {
  if (_ready && SD.exists(filename)) SD.remove(filename);
}

size_t SDManager::getFileSize(const char* filename) {
  if (!_ready || !SD.exists(filename)) return 0;
  File f = SD.open(filename, "r");
  size_t s = f.size();
  f.close();
  return s;
}

void SDManager::printCardInfo() {
  if (!_ready) return;
  Serial.printf("[SD] Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
}