#include "audio_manager.h"

AudioManager::AudioManager() {
  out = nullptr;
  file = nullptr;
  id3 = nullptr;
  wav = nullptr;
  mp3 = nullptr;
  initialized = false;
  isPlaying = false;
  currentVolume = 0.5;
}

AudioManager::~AudioManager() { cleanup(); }

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

  Serial.println("[Audio] Initializing SPIFFS...");

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("[Audio] SPIFFS Mount Failed");
    return false;
  }

  Serial.println("[Audio] SPIFFS mounted successfully");
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

  SPIFFS.end();
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
  if (!SPIFFS.exists(filename)) {
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

  file = new AudioFileSourceSPIFFS(filename);
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

  file = new AudioFileSourceSPIFFS(filename);
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
  Serial.println("[Audio] Files in SPIFFS:");
  Serial.println("----------------------------------------");

  File root = SPIFFS.open("/");
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
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();

  Serial.println("[Audio] SPIFFS Info:");
  Serial.printf("  Total: %d bytes (%.2f MB)\n", totalBytes,
                totalBytes / (1024.0 * 1024.0));
  Serial.printf("  Used:  %d bytes (%.2f MB)\n", usedBytes,
                usedBytes / (1024.0 * 1024.0));
  Serial.printf("  Free:  %d bytes (%.2f MB)\n", totalBytes - usedBytes,
                (totalBytes - usedBytes) / (1024.0 * 1024.0));
  Serial.printf("  Usage: %.1f%%\n", (usedBytes * 100.0) / totalBytes);
}
