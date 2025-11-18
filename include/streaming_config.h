#ifndef STREAMING_CONFIG_H
#define STREAMING_CONFIG_H

// ============================================================================
// Audio Streaming Configuration
// ============================================================================

// Opus Audio Configuration (VoIP Standard)
#define STREAM_SAMPLE_RATE 48000  // 48kHz - Opus standard
#define STREAM_CHANNELS 1         // Mono - sufficient for voice
#define OPUS_FRAME_SIZE_MS 20     // 20ms - Standard VoIP frame size
#define OPUS_FRAME_SAMPLES 960    // 48kHz * 20ms = 960 samples

// Buffer Configuration
#define BUFFER_SIZE_BYTES 8192         // ~200-300ms jitter buffer
#define STREAM_BUFFER_TRIGGER_LEVEL 1  // Trigger on any data

// WebSocket Configuration
#define WEBSOCKET_SERVER_PORT 81  // WebSocket server port

// Packet Configuration
#define MAX_OPUS_PACKET_SIZE 512  // Maximum size of an Opus packet
#define PACKET_HEADER_SIZE 2      // 2 bytes for packet length (uint16_t)

// Audio States
enum AudioState {
  AUDIO_STATE_IDLE,
  AUDIO_STATE_PLAYING_FILE,
  AUDIO_STATE_STREAMING
};

// Pre-roll Configuration (buffer fill before playback starts)
#define PREROLL_BUFFER_PERCENT 50  // Wait for buffer to be 50% full
#define PREROLL_MIN_BYTES (BUFFER_SIZE_BYTES * PREROLL_BUFFER_PERCENT / 100)

// Timeout Configuration
#define STREAM_BUFFER_TIMEOUT_MS \
  10  // Timeout for reading from buffer (prevents watchdog)
#define SILENCE_BUFFER_SIZE 960  // Size of silence buffer (one frame)

#endif  // STREAMING_CONFIG_H
