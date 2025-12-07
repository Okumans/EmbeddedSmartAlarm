#include "gateway_esp32/audio_stream_manager.h"

// Static instance pointer for WebSocket callback
static AudioStreamManager* instancePtr = nullptr;

AudioStreamManager::AudioStreamManager()
    : recording(false),
      i2sInitialized(false),
      wsConnected(false),
      serverPort(0) {
  instancePtr = this;
}

bool AudioStreamManager::begin(const char* serverIP, uint16_t serverPort) {
  this->serverIP = String(serverIP);
  this->serverPort = serverPort;

  Serial.println("[AudioStream] Initialized with server: " + this->serverIP +
                 ":" + String(serverPort));
  return true;
}

bool AudioStreamManager::initI2S() {
  if (i2sInitialized) {
    Serial.println("[AudioStream] I2S already initialized");
    return true;
  }

  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,  // INMP441 outputs 32-bit
      .channel_format =
          I2S_CHANNEL_FMT_ONLY_RIGHT,  // INMP441 typically on RIGHT channel
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = DMA_BUF_COUNT,
      .dma_buf_len = DMA_BUF_LEN,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  i2s_pin_config_t pin_config = {.bck_io_num = I2S_SCK,
                                 .ws_io_num = I2S_WS,
                                 .data_out_num = I2S_PIN_NO_CHANGE,
                                 .data_in_num = I2S_SD};

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[AudioStream] Failed to install I2S driver: %d\n", err);
    return false;
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("[AudioStream] Failed to set I2S pins: %d\n", err);
    i2s_driver_uninstall(I2S_PORT);
    return false;
  }

  // Clear DMA buffers
  i2s_zero_dma_buffer(I2S_PORT);

  // Give I2S time to stabilize
  delay(100);

  i2sInitialized = true;
  Serial.println("[AudioStream] I2S initialized successfully");
  return true;
}

void AudioStreamManager::deinitI2S() {
  if (i2sInitialized) {
    i2s_driver_uninstall(I2S_PORT);
    i2sInitialized = false;
    Serial.println("[AudioStream] I2S deinitialized");
  }
}

bool AudioStreamManager::connectWebSocket() {
  if (wsConnected) {
    return true;
  }

  // Ensure clean state - disconnect any lingering connection
  webSocket.disconnect();
  delay(100);

  Serial.printf("[AudioStream] Connecting to WebSocket %s:%d\n",
                serverIP.c_str(), serverPort);

  webSocket.begin(serverIP, serverPort, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  // Wait for connection (with timeout)
  unsigned long startTime = millis();
  while (!wsConnected && (millis() - startTime) < 5000) {
    webSocket.loop();
    delay(10);
  }

  if (!wsConnected) {
    Serial.println("[AudioStream] WebSocket connection timeout");
    return false;
  }

  Serial.println("[AudioStream] WebSocket connected");
  return true;
}

void AudioStreamManager::disconnectWebSocket() {
  if (wsConnected) {
    webSocket.disconnect();
    wsConnected = false;
    delay(50);  // Give time for proper cleanup
    Serial.println("[AudioStream] WebSocket disconnected");
  }
}

bool AudioStreamManager::startRecording() {
  if (recording) {
    Serial.println("[AudioStream] Already recording");
    return false;
  }

  Serial.println("[AudioStream] Starting recording...");

  // Initialize I2S if needed
  if (!initI2S()) {
    Serial.println("[AudioStream] Failed to initialize I2S");
    return false;
  }

  // Connect WebSocket
  if (!connectWebSocket()) {
    Serial.println("[AudioStream] Failed to connect WebSocket");
    deinitI2S();
    return false;
  }

  recording = true;
  Serial.println("[AudioStream] Recording started");
  return true;
}

void AudioStreamManager::stopRecording() {
  if (!recording) {
    return;
  }

  Serial.println("[AudioStream] Stopping recording...");
  recording = false;

  // Send any remaining data
  webSocket.loop();
  delay(100);

  disconnectWebSocket();
  deinitI2S();

  Serial.println("[AudioStream] Recording stopped");
}

void AudioStreamManager::process() {
  if (!recording) {
    return;
  }

  // Keep WebSocket alive
  webSocket.loop();

  if (!wsConnected) {
    Serial.println("[AudioStream] WebSocket disconnected during recording");
    stopRecording();
    return;
  }

  // Read audio samples from I2S
  size_t bytesRead = 0;
  esp_err_t result = i2s_read(I2S_PORT, i2sBuffer, sizeof(i2sBuffer),
                              &bytesRead, portMAX_DELAY);

  if (result != ESP_OK) {
    Serial.printf("[AudioStream] I2S read error: %d\n", result);
    return;
  }

  if (bytesRead == 0) {
    return;
  }

  // Convert 32-bit samples to 16-bit
  int samplesRead = bytesRead / sizeof(int32_t);
  for (int i = 0; i < samplesRead; i++) {
    // INMP441 sends 24-bit data in 32-bit frames (upper 24 bits)
    // Shift right by 16 to get 16-bit audio (discards lower 8 bits)
    int32_t sample32 = i2sBuffer[i];
    int16_t sample16 = (int16_t)(sample32 >> 16);

    audioBuffer[i] = sample16;
  }

  // Debug: Print first sample occasionally
  static int debugCounter = 0;
  if (debugCounter++ % 100 == 0 && samplesRead > 0) {
    Serial.printf("[AudioStream] Sample: %d (raw: 0x%08X)\n", audioBuffer[0],
                  i2sBuffer[0]);
  }

  // Send to WebSocket as binary data
  if (samplesRead > 0) {
    webSocket.sendBIN((uint8_t*)audioBuffer, samplesRead * sizeof(int16_t));
  }
}

void AudioStreamManager::webSocketEvent(WStype_t type, uint8_t* payload,
                                        size_t length) {
  if (!instancePtr) return;

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[AudioStream] WebSocket disconnected");
      instancePtr->wsConnected = false;
      break;

    case WStype_CONNECTED:
      Serial.printf("[AudioStream] WebSocket connected to: %s\n", payload);
      instancePtr->wsConnected = true;
      break;

    case WStype_TEXT:
      Serial.printf("[AudioStream] Received text: %s\n", payload);
      break;

    case WStype_ERROR:
      Serial.printf("[AudioStream] WebSocket error: %s\n", payload);
      break;

    case WStype_BIN:
      // Not expecting binary from server
      break;

    default:
      break;
  }
}
