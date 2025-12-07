# Smart Alarm Clock - Quick Reference

## 🚀 Quick Start

### 1. Upload to ESP32

```bash
platformio run --target upload --environment env_gateway_esp32
```

### 2. Monitor Serial Output

```bash
platformio device monitor --environment env_gateway_esp32
```

### 3. Start MQTT Subscriber

```bash
cd scripts
python mqtt_subscriber.py
```

### 4. Set Alarm (Example: 2 minutes from now)

```bash
python send_alarm_config.py both 14:35 14:40
```

## 📋 Essential Commands

### Time Synchronization

```bash
# Send current time to ESP32
python send_alarm_config.py time
```

### Alarm Configuration

```bash
# Set single alarm
python send_alarm_config.py alarms 07:30

# Set multiple alarms
python send_alarm_config.py alarms 07:00 07:30 08:00

# Set time and alarms together
python send_alarm_config.py both 07:00 08:00
```

### Audio Control

```bash
# Play alarm manually
python mqtt_send.py smartalarm/play_audio /alarm.mp3

# Stop audio
python mqtt_send.py smartalarm/commands stop_audio

# Set volume (0.0 to 1.0)
python mqtt_send.py smartalarm/commands volume=0.8
```

### Alarm Management

```bash
# View stored alarms
python view_alarms.py

# List alarms
python manage_alarms.py list

# Add alarm
python manage_alarms.py add 07:30

# Remove alarm
python manage_alarms.py remove 07:30

# Clear all
python manage_alarms.py clear
```

## 🔔 MQTT Topics

### Subscribe (ESP32 → Receive)

| Topic                   | Payload       | Description     |
| ----------------------- | ------------- | --------------- |
| `smartalarm/time`       | `HH:MM:SS`    | Time sync       |
| `smartalarm/alarmlist`  | `07:30,08:00` | Alarm config    |
| `smartalarm/play_audio` | `/alarm.mp3`  | Play audio      |
| `smartalarm/commands`   | `stop_audio`  | System commands |

### Publish (ESP32 → Send)

| Topic                         | Payload                    | Description     |
| ----------------------------- | -------------------------- | --------------- |
| `smartalarm/alarm/triggered`  | `Alarm triggered at 07:30` | Alarm fired     |
| `smartalarm/alarm/error`      | Error message              | Alarm error     |
| `smartalarm/alarmlist/status` | `ok`                       | Config received |
| `smartalarm/audio/status`     | `playing`                  | Audio status    |

## 🎵 Audio Files

### Required File

- **Location**: SD card root
- **Filename**: `/alarm.mp3`
- **Format**: MP3 or WAV
- **Recommended**: 16-bit, 44.1kHz

### Upload Audio

```bash
python mqtt_audiochunkupload.py path/to/alarm.mp3
```

## 🔧 Hardware Setup

### I2S Speaker Connections

```
ESP32 Pin → Speaker/Amplifier
GPIO 26   → BCLK (Bit Clock)
GPIO 25   → LRC (Word Select)
GPIO 27   → DOUT (Data Out)
GND       → GND
5V        → VCC (or 3.3V)
```

### SD Card

- **CS Pin**: GPIO 5
- **MOSI**: GPIO 23
- **MISO**: GPIO 19
- **CLK**: GPIO 18

## 🐛 Troubleshooting

### Alarm Not Triggering

1. Check time sync: `[MQTT] Received time via MQTT` in serial
2. Verify alarm list: `[MQTT] Alarms updated` in serial
3. Ensure `/alarm.mp3` exists on SD card

### No Audio

1. Check speaker connections (BCLK=26, LRC=25, DOUT=27)
2. Test manual playback: `python mqtt_send.py smartalarm/play_audio /alarm.mp3`
3. Verify SD card mounted: Look for SD init messages in serial

### Time Not Updating

1. Check MQTT connection
2. Resend time: `python send_alarm_config.py time`
3. Verify WiFi connected

## 📊 Status Monitoring

### ESP32 Serial Monitor Shows:

```
[Data] Time: 14:30:45 (MQT)
[Data] MQTT connected: yes | MQTT time avail: yes
⏰ ALARM TRIGGERED: 14:30
[Alarm] Playing alarm sound: /alarm.mp3
```

### Python Subscriber Shows:

```
🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔
⏰ ALARM TRIGGERED at 2025-12-06 14:30:00
Message: Alarm triggered at 14:30
🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔
```

## 🎯 Testing Workflow

### 1-Minute Quick Test

```bash
# Terminal 1: Monitor
python mqtt_subscriber.py

# Terminal 2: Set alarm (adjust time +1 minute from now)
python send_alarm_config.py both 14:31

# Wait 1 minute → Alarm triggers!
```

### Production Setup

```bash
# Set morning alarms
python send_alarm_config.py alarms 07:00 07:30 08:00

# Verify
python view_alarms.py
```

## 📁 File Locations

### ESP32 Configuration

- `include/shared/config.h` - Alarm sound path
- `src/gateway_esp32/rtos_tasks.cpp` - Alarm task
- `src/gateway_esp32/alarm_manager.cpp` - Alarm logic

### Python Scripts

- `scripts/send_alarm_config.py` - Configure alarms
- `scripts/mqtt_subscriber.py` - Monitor events
- `scripts/view_alarms.py` - View data
- `scripts/manage_alarms.py` - Manage alarms
- `scripts/alarm_data.json` - Stored alarms

### Documentation

- `ALARM_IMPLEMENTATION.md` - Technical details
- `ALARM_TESTING_GUIDE.md` - Complete testing guide
- `scripts/README.md` - Script documentation

## 🔐 Default Settings

```python
# MQTT
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883

# Alarm
DEFAULT_ALARM_SOUND = "/alarm.mp3"

# WiFi
WIFI_SSID = "Pleaseconnecttome"
```

## 💡 Tips

- **Test first**: Set alarm 1-2 minutes ahead for quick testing
- **Volume check**: Ensure speaker volume is audible
- **SD card**: Format as FAT32, files must start with `/`
- **Time format**: Always use HH:MM (24-hour format)
- **Alarm updates**: New alarm list replaces old one completely
- **Multiple alarms**: Separate with commas, no spaces

## 🆘 Need Help?

1. Check serial monitor for debug messages
2. Verify MQTT connection in subscriber
3. Test audio playback manually
4. Review `ALARM_TESTING_GUIDE.md` for detailed steps
5. Check `ALARM_IMPLEMENTATION.md` for technical details
