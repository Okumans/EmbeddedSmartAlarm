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

Subscribe to and monitor MQTT topics in real-time. **Now includes alarm data storage!**

**Features:**

- Real-time monitoring of sensor data (temperature, humidity, pressure, UV index, battery)
- **Alarm data storage**: Automatically stores alarm times received from the gateway
- **Persistence**: Saves alarm data to `alarm_data.json` for offline access
- Audio system status tracking
- Periodic status summaries

**Usage:**

```bash
# Monitor all smartalarm topics and store alarm data
python mqtt_subscriber.py
```

**Alarm Data:**

- Alarm times are automatically parsed and stored in memory
- Data is persisted to `alarm_data.json` in the scripts directory
- Format: CSV string like "07:30,08:00,14:30" parsed into individual alarms
- Use `view_alarms.py` to view stored alarm data

### `view_alarms.py` - View Stored Alarm Data

View and manage alarm data collected by `mqtt_subscriber.py`.

**Usage:**

```bash
# View current alarms
python view_alarms.py

# Clear all alarm data
python view_alarms.py clear
```

**Output Example:**

```
📋 Alarm Information:
   Total Alarms: 3
   Last Update:  2025-12-06 14:30:45
   Raw Payload:  07:30,08:00,14:30

⏰ Active Alarm Times:
   1. 07:30
   2. 08:00
   3. 14:30
```

### `manage_alarms.py` - Manual Alarm Management

Manually add, remove, or modify alarm times without needing MQTT messages. Useful for testing.

**Usage:**

```bash
# List all alarms
python manage_alarms.py list

# Add an alarm
python manage_alarms.py add 07:30

# Remove an alarm
python manage_alarms.py remove 14:00

# Clear all alarms (with confirmation)
python manage_alarms.py clear
```

### `send_alarm_config.py` - Send Alarm Configuration to ESP32

Send time synchronization and alarm configurations to the ESP32 gateway via MQTT.

**Usage:**

```bash
# Send current system time to ESP32
python send_alarm_config.py time

# Send alarm list to ESP32
python send_alarm_config.py alarms 07:30 08:00 14:30

# Send both time and alarms
python send_alarm_config.py both 07:30 08:00 14:30
```

**How It Works:**

- The ESP32 continuously checks current time against configured alarms
- When a match is found, it plays `/alarm.mp3` from the SD card
- Alarm notifications are published to `smartalarm/alarm/triggered`
- The subscriber script will display a visual notification when alarms trigger

---

## 🔧 Configuration

All scripts use the default MQTT broker `broker.hivemq.com` on port 1883. To use a different broker, modify the broker settings in each script:

```python
MQTT_BROKER = "your-broker.com"
MQTT_PORT = 1883
```

---
