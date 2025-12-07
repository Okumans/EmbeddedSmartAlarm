# Alarm Time Synchronization - Implementation Summary

## Overview

Successfully implemented real-world clock synchronization with alarm triggering functionality for the Smart Alarm Clock ESP32 system. The system now:

- Synchronizes time via MQTT or NTP
- Checks configured alarms every second
- Automatically plays audio when alarm time matches current time
- Provides MQTT notifications for alarm events

## Architecture

### ESP32 Components

1. **AlarmManager Class** (`alarm_manager.h/cpp`)

   - Stores alarm times in HH:MM format
   - Validates and parses CSV alarm lists
   - Checks current time against alarms
   - Prevents re-triggering with state management

2. **Alarm RTOS Task** (`rtos_tasks.cpp`)

   - Runs on Core 0 (network core)
   - Checks alarms every second
   - Gets time from MQTT (primary) or NTP (fallback)
   - Triggers audio playback on match
   - Publishes MQTT notifications

3. **Configuration** (`config.h`)
   - Default alarm sound: `/alarm.mp3`
   - Configurable alarm audio file

### Time Sources (Priority Order)

1. **MQTT Time** (Primary)

   - Topic: `smartalarm/time`
   - Format: "HH:MM:SS" or "YYYY-MM-DD HH:MM:SS"
   - Updated via Python scripts

2. **NTP Time** (Fallback)
   - Automatic synchronization
   - Used when MQTT time unavailable

### Python Scripts

1. **mqtt_subscriber.py** (Enhanced)

   - Monitors alarm triggered events
   - Stores alarm data in `alarm_data.json`
   - Displays visual notifications for alarms

2. **send_alarm_config.py** (New)

   - Sends current time to ESP32
   - Configures alarm times
   - Supports single or batch operations

3. **view_alarms.py** (New)

   - Views stored alarm data
   - Clears alarm history

4. **manage_alarms.py** (New)
   - Add/remove alarms manually
   - List configured alarms
   - Useful for testing

## Data Flow

```
┌─────────────────────────────────────────────────────────────┐
│                     MQTT Broker                              │
│              (broker.hivemq.com:1883)                        │
└───────────────────┬─────────────────┬───────────────────────┘
                    │                 │
        ┌───────────▼─────────┐  ┌───▼──────────────────┐
        │  Python Scripts      │  │   ESP32 Gateway      │
        │                      │  │                      │
        │  send_alarm_config   │  │  MQTT Handler        │
        │  mqtt_subscriber     │  │  AlarmManager        │
        │  view_alarms         │  │  Alarm Task          │
        │  manage_alarms       │  │  Audio Manager       │
        └──────────────────────┘  └──────────┬───────────┘
                                             │
                                    ┌────────▼────────┐
                                    │   I2S Speaker   │
                                    │  plays alarm    │
                                    └─────────────────┘
```

## MQTT Topics

### Input (ESP32 subscribes)

- `smartalarm/time` - Time synchronization (HH:MM:SS)
- `smartalarm/alarmlist` - Alarm configuration (CSV: "07:30,08:00,14:30")

### Output (ESP32 publishes)

- `smartalarm/alarm/triggered` - Alarm triggered notification
- `smartalarm/alarm/error` - Alarm error notification
- `smartalarm/alarmlist/status` - Alarm list update confirmation
- `smartalarm/alarmlist/parsed` - Parsed alarm list echo

## Key Features

### 1. Time Synchronization

- **Dual source**: MQTT (priority) or NTP (fallback)
- **Format**: HH:MM:SS (extracts HH:MM for comparison)
- **Update frequency**: On-demand via MQTT, automatic via NTP

### 2. Alarm Checking

- **Frequency**: Every 1 second
- **Comparison**: Current HH:MM vs Alarm HH:MM
- **Anti-retrigger**: Tracks triggered alarms per minute
- **Reset**: Triggered states clear when minute changes

### 3. Audio Playback

- **Trigger**: Automatic when time matches alarm
- **File**: `/alarm.mp3` (configurable in config.h)
- **Location**: SD card
- **Output**: I2S speaker

### 4. Notification System

- **MQTT publish**: Alarm triggered events
- **Python display**: Visual notification in subscriber
- **Serial debug**: Console logging on ESP32

## Files Modified/Created

### ESP32 Firmware

**Modified:**

- `include/gateway_esp32/alarm_manager.h` - Added alarm checking methods
- `src/gateway_esp32/alarm_manager.cpp` - Implemented alarm logic
- `include/gateway_esp32/rtos_tasks.h` - Added alarm task
- `src/gateway_esp32/rtos_tasks.cpp` - Implemented alarm task
- `include/shared/config.h` - Added alarm sound configuration

**Existing (Used):**

- `src/gateway_esp32/mqtt_setup.cpp` - MQTT handlers already in place
- `include/shared/mqtt_time.h` - Time storage
- `src/shared/mqtt_time.cpp` - Time management

### Python Scripts

**Created:**

- `scripts/send_alarm_config.py` - Send time/alarms to ESP32
- `scripts/view_alarms.py` - View stored alarm data
- `scripts/manage_alarms.py` - Manual alarm management
- `scripts/alarm_data.example.json` - Example data structure
- `ALARM_TESTING_GUIDE.md` - Complete testing guide

**Modified:**

- `scripts/mqtt_subscriber.py` - Enhanced with alarm storage and notifications
- `scripts/README.md` - Updated documentation

## Usage Examples

### Quick Setup

```bash
# Terminal 1: Monitor system
python scripts/mqtt_subscriber.py

# Terminal 2: Configure alarms (set for 1 minute in future)
python scripts/send_alarm_config.py both 14:35 14:40
```

### Set Alarm for Tomorrow Morning

```bash
python scripts/send_alarm_config.py alarms 07:00 07:30 08:00
```

### Update Time Only

```bash
python scripts/send_alarm_config.py time
```

### View Current Alarms

```bash
python scripts/view_alarms.py
```

## Testing Checklist

- [x] Alarm manager parses CSV correctly
- [x] Time comparison works (HH:MM format)
- [x] Alarm triggers at exact minute
- [x] No re-triggering within same minute
- [x] Audio plays on trigger
- [x] MQTT notification sent
- [x] Python subscriber displays notification
- [x] Multiple alarms supported
- [x] Alarm list updates dynamically
- [x] Time source fallback works (MQTT → NTP)

## Configuration

### ESP32 (`config.h`)

```cpp
static const char* DEFAULT_ALARM_SOUND = "/alarm.mp3";
```

### Python Scripts

```python
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
```

## Hardware Requirements

- ESP32 (tested on ESP32-DevKitC)
- I2S speaker/amplifier connected to:
  - BCLK: GPIO 26
  - LRC: GPIO 25
  - DOUT: GPIO 27
- SD card with `/alarm.mp3` file
- WiFi connection

## Performance Metrics

- **Memory**: +4KB for alarm task stack
- **CPU**: Negligible (1-second interval check)
- **Network**: Minimal (MQTT notifications only)
- **Latency**: <1 second from trigger to audio start

## Future Enhancements

Possible improvements:

- [ ] Snooze functionality via button/MQTT
- [ ] Different alarm sounds per alarm
- [ ] Recurring alarm patterns (weekdays/weekends)
- [ ] Alarm duration/repeat settings
- [ ] Gradual volume increase
- [ ] Display alarm indicator on OLED
- [ ] Persistent alarm storage (SD card)
- [ ] Web interface for alarm management

## Notes

- Alarms are volatile (cleared on ESP32 restart)
- Time must be synchronized before alarms work correctly
- Alarm sound file must exist on SD card
- Python alarm storage is for monitoring/reference only
- Each alarm triggers once per minute maximum
