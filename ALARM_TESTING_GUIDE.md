# Alarm System Testing Guide

This guide explains how to test the Smart Alarm Clock's time synchronization and alarm triggering features.

## Prerequisites

1. **Hardware Setup:**

   - ESP32 gateway connected and running
   - Speaker connected to I2S pins (BCLK: 26, LRC: 25, DOUT: 27)
   - SD card inserted with `/alarm.mp3` file

2. **Software Setup:**
   - Python 3.x installed
   - Required packages: `paho-mqtt`
   - MQTT broker accessible (default: broker.hivemq.com)

## Testing Steps

### Step 1: Start the MQTT Subscriber

Open a terminal and start the subscriber to monitor alarm events:

```bash
cd scripts
python mqtt_subscriber.py
```

You should see:

- Connection confirmation
- Subscription to topics
- Periodic sensor data summaries

### Step 2: Send Current Time to ESP32

In another terminal, send the current system time:

```bash
python send_alarm_config.py time
```

**Expected Output:**

```
✓ Connected to MQTT broker: broker.hivemq.com:1883
✓ Sent current time: 14:30:45
✓ Done!
```

**In the subscriber terminal, you should see:**

```
[Data] Time: 14:30:45 (MQT)
[Data] MQTT time avail: yes
```

### Step 3: Set Alarm Times

Set one or more alarm times. For testing, set an alarm 1-2 minutes in the future:

```bash
# Example: If current time is 14:30, set alarm for 14:32
python send_alarm_config.py alarms 14:32 14:35
```

**Expected Output:**

```
✓ Sent alarm list: 14:32,14:35
  Total alarms: 2
```

**In the subscriber terminal:**

```
📨 Alarm list received at 2025-12-06 14:30:45
Topic: smartalarm/alarmlist
Payload: 14:32,14:35
Alarm Count: 2
Alarm Times: 14:32, 14:35
✓ Saved 2 alarm(s) to alarm_data.json
```

### Step 4: Wait for Alarm to Trigger

Wait until the current time matches one of your alarm times.

**When alarm triggers, you should:**

1. **Hear audio** from the speaker (alarm.mp3 playing)

2. **See in subscriber terminal:**

```
🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔
⏰ ALARM TRIGGERED at 2025-12-06 14:32:00
Message: Alarm triggered at 14:32
🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔🔔
```

3. **See in ESP32 Serial Monitor:**

```
⏰ ALARM TRIGGERED: 14:32
[Alarm] Playing alarm sound: /alarm.mp3
```

## Advanced Testing

### Test with Both Time and Alarms

Send both time and alarms in one command:

```bash
python send_alarm_config.py both 14:45 15:00
```

### Verify Alarm Storage

View stored alarms:

```bash
python view_alarms.py
```

### Stop Alarm Audio

To stop the alarm audio once it's playing:

```bash
python mqtt_send.py smartalarm/commands stop_audio
```

### Check Alarm Status via Serial Monitor

Monitor the ESP32 serial output to see:

- Time synchronization
- Alarm checking (every second)
- Alarm trigger events
- Audio playback status

## Troubleshooting

### Alarm Not Triggering

1. **Check time synchronization:**

   - Verify ESP32 received time: Look for `[MQTT] Received time via MQTT` in serial
   - Check subscriber shows `Time: HH:MM:SS (MQT)`

2. **Verify alarm list:**

   - Confirm alarms were received: Check for `[MQTT] Alarms updated` in serial
   - Use `python view_alarms.py` to see stored alarms

3. **Check alarm file:**
   - Ensure `/alarm.mp3` exists on SD card
   - Try playing manually: `python mqtt_send.py smartalarm/play_audio /alarm.mp3`

### No Audio Output

1. **Check speaker connections:**

   - BCLK → GPIO 26
   - LRC → GPIO 25
   - DOUT → GPIO 27
   - GND → GND
   - VCC → 5V or 3.3V (depending on speaker)

2. **Verify SD card:**

   - Check SD card is properly inserted
   - Verify `/alarm.mp3` file exists
   - Check serial for SD card initialization messages

3. **Test audio playback:**
   ```bash
   python mqtt_send.py smartalarm/commands play:/alarm.mp3
   ```

### Time Not Updating

1. **MQTT connection:**

   - Check broker connectivity
   - Verify topic subscription in serial monitor

2. **Send time again:**

   ```bash
   python send_alarm_config.py time
   ```

3. **Check NTP fallback:**
   - ESP32 should fall back to NTP time if MQTT time unavailable
   - Verify WiFi connection

## Testing Scenarios

### Scenario 1: Quick Test (1 minute)

```bash
# Get current time, add 1 minute
# If now is 14:30, set alarm for 14:31
python send_alarm_config.py both 14:31
```

### Scenario 2: Multiple Alarms

```bash
python send_alarm_config.py alarms 07:00 07:30 08:00
```

### Scenario 3: Update Existing Alarms

```bash
# First set
python send_alarm_config.py alarms 10:00 11:00

# Update (replaces previous)
python send_alarm_config.py alarms 10:30 11:30 12:00
```

## Expected Behavior

1. **Alarm triggers at exact time:**

   - When current time (HH:MM) matches alarm time
   - Only triggers once per minute (prevents re-triggering)

2. **Audio plays automatically:**

   - `/alarm.mp3` starts playing
   - Continues until stopped or file ends

3. **MQTT notification sent:**

   - Topic: `smartalarm/alarm/triggered`
   - Payload: "Alarm triggered at HH:MM"

4. **Alarm resets:**
   - Triggered state clears when minute changes
   - Same alarm can trigger again next day

## Notes

- Alarms are stored in ESP32 memory (cleared on restart)
- Python scripts store alarms in `alarm_data.json` for reference
- Time source priority: MQTT time → NTP time
- Alarm checks run every second
- Default alarm sound: `/alarm.mp3` (configurable in `config.h`)
