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

# Get system status
python mqtt_send.py smartalarm/commands status
```

### `mqtt_subscriber.py` - Monitor MQTT Messages

Subscribe to and monitor MQTT topics in real-time.

**Usage:**
```bash
# Monitor all smartalarm topics
python mqtt_subscriber.py "smartalarm/#"
```

---

## 🔧 Configuration

All scripts use the default MQTT broker `broker.hivemq.com` on port 1883. To use a different broker, modify the broker settings in each script:

```python
MQTT_BROKER = "your-broker.com"
MQTT_PORT = 1883
```

---
