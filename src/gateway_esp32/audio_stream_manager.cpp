#include "gateway_esp32/audio_stream_manager.h"

#include "../../include/gateway_esp32/alarm_question_handler.h"

// Static instance pointer for WebSocket callback
static AudioStreamManager* instancePtr = nullptr;

AudioStreamManager::AudioStreamManager()
    : recording(false),
      i2sInitialized(false),
      wsConnected(false),
      serverPort(0),
      pendingQuestion(false),
      pendingQuestionText(""),
      questionSentForConnection(false) {
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
  // Prepare question text to send as the FIRST TEXT message over WebSocket.
  // The protocol requires: first message = plain TEXT question (UTF-8,
  // max 128 bytes). Subsequent messages are binary audio frames.
  extern AlarmQuestionHandler alarmQuestion;
  String question = alarmQuestion.getQuestion();
  // Truncate safely to 128 bytes (preserve UTF-8 sequences)
  String questionTrunc = truncateUtf8(question, 128);

  // Make sure the stored/displayed question matches the text we will send.
  // Update the alarmQuestion if truncation changed the text so OLED and
  // validation service see the exact same string.
  if (questionTrunc != question) {
    Serial.printf(
        "[AudioStream] Truncating question for WS/display: '%s' -> '%s'\n",
        question.c_str(), questionTrunc.c_str());
    alarmQuestion.setQuestion(questionTrunc);
  }

  // If already connected, send immediately; otherwise buffer and send on
  // connect event.
  pendingQuestion = true;
  pendingQuestionText = questionTrunc;
  if (wsConnected) {
    webSocket.sendTXT(pendingQuestionText);
    Serial.printf("[AudioStream] Sent question TEXT: %s\n",
                  pendingQuestionText.c_str());
    pendingQuestion = false;
    pendingQuestionText = "";
    questionSentForConnection = true;
  } else {
    Serial.printf(
        "[AudioStream] Buffered question (will send on connect): %s\n",
        pendingQuestionText.c_str());
  }
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

  // Ensure we've sent the required per-connection question TEXT before
  // emitting any binary audio frames on this connection. If not yet sent,
  // skip this iteration to give the CONNECTED handler time to transmit it.
  if (!questionSentForConnection) {
    static bool warned = false;
    if (!warned) {
      Serial.println(
          "[AudioStream] Waiting to send audio until question TEXT is sent for this connection");
      warned = true;
    }
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
      instancePtr->questionSentForConnection = false;
      break;

    case WStype_CONNECTED:
      Serial.printf("[AudioStream] WebSocket connected to: %s\n", payload);
      instancePtr->wsConnected = true;
      // If we are currently recording, ensure the server receives the
      // per-connection TEXT question before any binary frames. Send the
      // current question (truncate safely) now so a reconnect during
      // recording doesn't result in the server receiving audio frames
      // without the required leading TEXT message.
      if (instancePtr->recording) {
        extern AlarmQuestionHandler alarmQuestion;
        String q = alarmQuestion.getQuestion();
        String qTrunc = instancePtr->truncateUtf8(q, 128);
        if (qTrunc != q) {
          alarmQuestion.setQuestion(qTrunc);
        }
        instancePtr->webSocket.sendTXT(qTrunc);
        Serial.printf(
            "[AudioStream] Sent question TEXT on reconnect while recording: %s\n",
            qTrunc.c_str());
        instancePtr->questionSentForConnection = true;
        // Clear any buffered question since we've just sent the canonical one
        instancePtr->pendingQuestion = false;
        instancePtr->pendingQuestionText = "";
      } else if (instancePtr->pendingQuestion &&
                 instancePtr->pendingQuestionText.length() > 0) {
        // Not recording but had a pending question (record started before
        // connect finished). Send that buffered question now.
        instancePtr->webSocket.sendTXT(instancePtr->pendingQuestionText);
        Serial.printf(
            "[AudioStream] Sent buffered question TEXT on connect: %s\n",
            instancePtr->pendingQuestionText.c_str());
        instancePtr->pendingQuestion = false;
        instancePtr->pendingQuestionText = "";
        instancePtr->questionSentForConnection = true;
      } else {
        // Nothing to send now; mark as not-yet-sent so future reconnects will
        // trigger resend if recording begins.
        instancePtr->questionSentForConnection = false;
      }
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

String AudioStreamManager::truncateUtf8(const String& s, size_t maxBytes) {
  const char* data = s.c_str();
  size_t len = strlen(data);
  if (len <= maxBytes) return s;

  size_t i = 0;
  size_t used = 0;
  while (i < len) {
    unsigned char c = (unsigned char)data[i];
    size_t charLen = 1;
    if ((c & 0x80) == 0x00)
      charLen = 1;
    else if ((c & 0xE0) == 0xC0)
      charLen = 2;
    else if ((c & 0xF0) == 0xE0)
      charLen = 3;
    else if ((c & 0xF8) == 0xF0)
      charLen = 4;
    else
      charLen = 1;  // fallback

    if (used + charLen > maxBytes) break;
    used += charLen;
    i += charLen;
  }

  String out = String(data).substring(0, used);
  Serial.printf("[AudioStream] Question truncated to %u bytes\n",
                (unsigned)used);
  return out;
}
