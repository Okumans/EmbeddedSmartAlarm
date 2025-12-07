# Audio Streaming System Guide

## Overview

Phase 1 implementation: Audio streaming from ESP32 INMP441 microphone to Python server for WAV recording.

## Hardware Setup

### INMP441 Microphone Connections
- **WS (Word Select/LRCL)** → GPIO 33
- **SD (Serial Data/DOUT)** → GPIO 34  
- **SCK (Serial Clock/BCLK)** → GPIO 32
- **VDD** → 3.3V
- **GND** → GND

### Button (Optional)
- **Button** → GPIO 15 (with internal pull-up)
- **GND** → GND

## Configuration

### ESP32 Configuration
Edit `include/shared/config.h`:
```cpp
static const char* AUDIO_SERVER_IP = "192.168.1.100";  // Your computer's IP
static const int AUDIO_SERVER_PORT = 4000;
```

**Important:** Update `AUDIO_SERVER_IP` to your computer's local IP address.

### Finding Your Computer's IP
```bash
# Linux/Mac
ip addr show | grep inet

# Or
ifconfig | grep inet
```

## Usage

### Method 1: MQTT Commands (Testing)

1. Start the Python WAV recorder server:
```bash
cd LLM_STT
uv run audio_wav_recorder.py
```

2. Start recording:
```bash
cd scripts
uv run mqtt_send.py smartalarm/commands start_recording
```

3. Stop recording (after speaking):
```bash
uv run mqtt_send.py smartalarm/commands stop_recording
```

Recordings are saved in `LLM_STT/recordings/` with timestamp filenames.

### Method 2: Button Control (Production)

**Automatic during alarm:**
1. When alarm triggers and plays sound
2. Long press button → Volume reduces to 30%, recording starts
3. Speak your answer
4. Release button → Volume restores, recording stops

**Key Features:**
- Button only works when alarm is actively playing
- Volume automatically reduces during recording for better microphone input
- Integrated with alarm question validation system

## Audio Format

- **Sample Rate:** 16 kHz
- **Bit Depth:** 16-bit PCM
- **Channels:** Mono
- **Format:** WAV (uncompressed)
- **Transport:** WebSocket binary frames

## Architecture

### Decoupled Design
```
┌─────────────────────────┐
│  AudioStreamManager     │  ← Core (no dependencies)
│  - startRecording()     │
│  - stopRecording()      │
│  - process()            │
└─────────────────────────┘
           ↑
           │
    ┌──────┴──────┐
    │             │
┌───────┐   ┌──────────┐
│ MQTT  │   │  Button  │  ← Adapters (easily swapped)
│ Cmds  │   │  Handler │
└───────┘   └──────────┘
```

### Migration Path
MQTT commands are just thin adapters (3 lines each) in `mqtt_setup.cpp`:
```cpp
else if (message == "start_recording") {
    audioStream.startRecording();
}
```

To remove MQTT testing layer later: Just delete these 3 lines per command.

## Python Server Details

### Requirements
Install dependencies:
```bash
cd LLM_STT
uv pip install websockets
```

### Server Operation
- Listens on WebSocket port 4000
- Accepts binary audio chunks (16-bit PCM)
- Assembles into WAV file
- Auto-saves with timestamp when recording stops
- Shows real-time progress

### Output Files
```
LLM_STT/recordings/
├── audio_20251207_190345.wav
├── audio_20251207_190512.wav
└── ...
```

## Troubleshooting

### "WebSocket connection timeout"
- Check `AUDIO_SERVER_IP` in config.h
- Verify Python server is running: `uv run audio_wav_recorder.py`
- Ensure firewall allows port 4000
- Confirm ESP32 and computer are on same network

### "I2S initialization failed"
- Check INMP441 wiring (WS=33, SD=34, SCK=32)
- Verify 3.3V power supply
- Check for GPIO conflicts with other peripherals

### No audio recorded
- Test microphone: Tap it, should see activity
- Check I2S pin connections
- Verify WebSocket connection established
- Monitor serial output for errors

### Button not working
- Only works during alarm playback
- Requires long press (800ms)
- Check button wiring to GPIO 15
- Monitor serial for "[Button] Long press detected"

## Next Steps (Phase 2)

Future enhancements:
1. STT integration (Whisper)
2. Answer validation (Gemini AI)
3. Multi-language support
4. Voice activity detection (VAD)

## Technical Details

### I2S Configuration
- Mode: Master RX
- Bits: 32-bit input → 16-bit output conversion
- Channel: Left only (mono)
- DMA buffers: 8 × 1024 bytes

### FreeRTOS Integration
- Task: `audioEncodeTask` on Core 1
- Priority: Real-time audio processing
- Loop: 10ms intervals for smooth streaming

### Memory Usage
- I2S buffer: 512 samples × 4 bytes = 2 KB
- Audio buffer: 512 samples × 2 bytes = 1 KB
- Total: ~3 KB additional RAM
