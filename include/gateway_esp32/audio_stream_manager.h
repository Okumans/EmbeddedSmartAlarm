#ifndef AUDIO_STREAM_MANAGER_H
#define AUDIO_STREAM_MANAGER_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <driver/i2s.h>

class AudioStreamManager {
 public:
  AudioStreamManager();

  // Initialize I2S and WebSocket
  bool begin(const char* serverIP, uint16_t serverPort);

  // Control recording
  bool startRecording();
  void stopRecording();
  bool isRecording() const { return recording; }

  // Process audio in loop (call from task)
  void process();

 private:
  // I2S Configuration (INMP441)
  static constexpr int I2S_WS = 33;   // Word Select (LRCL)
  static constexpr int I2S_SD = 34;   // Serial Data (DOUT)
  static constexpr int I2S_SCK = 32;  // Serial Clock (BCLK)
  // Use I2S_NUM_1 for microphone input so playback (I2S_NUM_0) and mic can
  // operate simultaneously without driver conflicts.
  static constexpr i2s_port_t I2S_PORT = I2S_NUM_1;
  static constexpr int SAMPLE_RATE = 16000;
  static constexpr int BITS_PER_SAMPLE = 16;
  static constexpr int I2S_READ_BITS = 32;  // INMP441 outputs 32-bit

  // Buffer configuration
  static constexpr int BUFFER_SIZE = 512;
  static constexpr int DMA_BUF_COUNT = 8;
  static constexpr int DMA_BUF_LEN = 1024;

  // State
  bool recording;
  bool i2sInitialized;
  bool wsConnected;

  // WebSocket client
  WebSocketsClient webSocket;
  String serverIP;
  uint16_t serverPort;

  // Buffers
  int32_t i2sBuffer[BUFFER_SIZE];
  int16_t audioBuffer[BUFFER_SIZE];

  // Pending question metadata to send as first WS TEXT message
  bool pendingQuestion;
  String pendingQuestionText;

  // Private methods
  bool initI2S();
  void deinitI2S();
  bool connectWebSocket();
  void disconnectWebSocket();
  int32_t readI2SSample();
  void convertAndSend();

  // WebSocket event handler
  static void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);

  // Helper: truncate UTF-8 string to maxBytes without breaking multi-byte
  // sequences.
  String truncateUtf8(const String& s, size_t maxBytes);
};

#endif  // AUDIO_STREAM_MANAGER_H
