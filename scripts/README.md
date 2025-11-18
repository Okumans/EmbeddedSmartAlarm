# SmartAlarmClock Python Scripts

This directory contains Python utility scripts for interacting with the SmartAlarmClock ESP32 Gateway.

## 📋 Prerequisites

Install required dependencies:

```bash
pip install pyaudio opuslib websockets paho-mqtt
```

### System Dependencies for Audio (PortAudio)

- **macOS**: `brew install portaudio`
- **Linux/Raspberry Pi**: `sudo apt install portaudio19-dev`
- **Windows**: Usually handled by pip automatically

---

## 🎵 Audio Streaming Scripts

### `stream_audio.py` - Real-time Audio Streaming

Captures audio from your microphone, encodes it to Opus format, and streams it to the ESP32 Gateway via WebSocket for real-time playback.

**Usage:**

1. Update `ESP32_IP` in the script with your ESP32's IP address
2. Ensure ESP32 is powered on and connected to WiFi
3. Optionally, send MQTT command to start streaming mode:
   ```bash
   python mqtt_send.py smartalarm/stream/control start
   ```
4. Run the streaming script:
   ```bash
   python stream_audio.py
   ```
5. Speak into your microphone - you should hear your voice from the ESP32 speaker with ~200-500ms latency

**Technical Details:**
- Sample Rate: 48kHz (Opus standard)
- Channels: Mono
- Frame Size: 20ms (960 samples)
- Codec: Opus (optimized for VoIP)
- Transport: WebSocket (port 81)

---

## 📤 Audio Upload Scripts

### `mqtt_audiochunkupload.py` - Upload Audio Files

Uploads MP3/WAV audio files to the ESP32's LittleFS filesystem via MQTT.

**Usage:**
```bash
python mqtt_audiochunkupload.py path/to/audiofile.mp3
```

### `audio_controller.py` - Audio Playback Control

Control audio playback on the ESP32 via MQTT commands.

**Usage:**
```bash
# Play a file
python audio_controller.py play /sound.mp3

# Stop playback
python audio_controller.py stop

# Set volume (0.0 - 1.0)
python audio_controller.py volume 0.5

# List files
python audio_controller.py list
```

---

## 📡 MQTT Utility Scripts

### `mqtt_send.py` - Send MQTT Commands

General-purpose MQTT command sender.

**Usage:**
```bash
# Send custom command
python mqtt_send.py smartalarm/commands "stop_audio"

# Start streaming mode
python mqtt_send.py smartalarm/stream/control start

# Stop streaming mode
python mqtt_send.py smartalarm/stream/control stop

# Get system status
python mqtt_send.py smartalarm/commands status
```

### `mqtt_subscriber.py` - Monitor MQTT Messages

Subscribe to and monitor MQTT topics in real-time.

**Usage:**
```bash
# Monitor all smartalarm topics
python mqtt_subscriber.py "smartalarm/#"

# Monitor specific topic
python mqtt_subscriber.py "smartalarm/stream/status"
```

---

## 🔧 Configuration

All scripts use the default MQTT broker `broker.hivemq.com` on port 1883. To use a different broker, modify the broker settings in each script:

```python
MQTT_BROKER = "your-broker.com"
MQTT_PORT = 1883
```

For WebSocket streaming, update the ESP32 IP address:

```python
ESP32_IP = "192.168.1.100"  # Replace with your ESP32's IP
PORT = 81
```

---

## 📝 Notes

- **Audio Quality**: Streaming uses Opus codec which provides excellent quality at low bitrates
- **Latency**: Expect 200-500ms latency depending on network conditions and buffer settings
- **Buffer Settings**: Adjust `BUFFER_SIZE_BYTES` in ESP32's `streaming_config.h` if you experience choppy audio
- **Microphone Selection**: PyAudio uses the system default microphone. Change input device in the script if needed

---

## 🐛 Troubleshooting

**Issue: "No module named pyaudio"**
- Solution: Install PortAudio system library first, then `pip install pyaudio`

**Issue: "Connection refused" when streaming**
- Verify ESP32 IP address is correct
- Check that ESP32 is on the same network
- Ensure WebSocket server is running (check ESP32 serial monitor)

**Issue: Robotic or choppy audio**
- Increase `BUFFER_SIZE_BYTES` in ESP32's `streaming_config.h`
- Check WiFi signal strength (both ESP32 and PC)
- Reduce network congestion

**Issue: High latency**
- Decrease `BUFFER_SIZE_BYTES` (trade-off: may increase choppiness)
- Ensure ESP32 and PC are on the same local network (not bridged/VPN)
