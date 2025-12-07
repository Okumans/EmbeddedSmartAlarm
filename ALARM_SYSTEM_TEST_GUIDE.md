# Alarm Answer System - Testing Guide

## System Overview

Complete integration of audio streaming + STT + AI validation for alarm questions.

## Architecture

```
ESP32 Gateway                    Python Services
┌─────────────────┐             ┌──────────────────────┐
│ Alarm Triggers  │             │  question_service.py │
│       ↓         │  MQTT req   │  (already running)   │
│ Request Q  ─────┼────────────→│  Sends question      │
│       ↓         │←────────────┤                      │
│ Show Q on OLED  │  MQTT       │                      │
│       ↓         │             └──────────────────────┘
│ Play Alarm      │             
│       ↓         │             ┌─────────────────────────┐
│ User presses    │             │ alarm_answer_validator  │
│ button          │  WebSocket  │ .py (new)               │
│       ↓         │  audio      │                         │
│ Record Audio────┼────────────→│  1. Save WAV            │
│       ↓         │             │  2. Whisper STT         │
│ Wait for result │             │  3. Gemini validate     │
│       ↓         │←────────────┤  4. Send via MQTT       │
│ Receive valid/  │  MQTT       │                         │
│ invalid         │             └─────────────────────────┘
│       ↓         │
│ IF valid: Stop  │
│ IF invalid:     │
│   Retry (max 20)│
└─────────────────┘
```

## Prerequisites

### Python Services
```bash
cd LLM_STT

# Install dependencies
uv pip install websockets paho-mqtt faster-whisper google-generativeai

# Set Gemini API key
export GEMINI_API_KEY="your-api-key-here"
```

### ESP32
- Firmware uploaded
- INMP441 mic connected (GPIO 33/34/32)
- Button on GPIO 15
- LED on GPIO 2

## Step-by-Step Test

### 1. Start Python Services

**Terminal 1 - Question Service:**
```bash
cd LLM_STT
uv run question_service.py
```

**Terminal 2 - Answer Validator:**
```bash
cd LLM_STT
uv run alarm_answer_validator.py
```

You should see:
```
Alarm Answer Validator Service
WebSocket: 0.0.0.0:4000
MQTT: broker.hivemq.com:1883
Server listening on ws://0.0.0.0:4000
```

### 2. Set Test Alarm

**Terminal 3 - Set Alarm:**
```bash
cd scripts

# Set alarm for 2 minutes from now
# Example: if current time is 19:30, set for 19:32
uv run mqtt_send.py setalarms 19:32
```

### 3. Wait for Alarm

When alarm time arrives, you should see:

**ESP32 Serial Monitor:**
```
⏰ ALARM TRIGGERED: 19:32
[Alarm] Requesting question from server...
[Alarm] Starting question challenge mode
[Alarm] Playing alarm sound: /alarm.mp3
```

**OLED Display:**
```
ALARM QUESTION
─────────────
[Question text]

Press button to answer
Attempt: 1/20
```

### 4. Answer the Question

1. **Press and hold button**
   - LED turns ON
   - Volume drops to 30%
   - Display shows "Recording..."
   - Audio streams to Python server

2. **Speak your answer**
   - Clear and loud
   - In Thai or English

3. **Release button**
   - LED turns OFF
   - Volume restores to 100%
   - Display shows "Validating..."

4. **Wait for validation** (~3-5 seconds)

**Python Validator Output:**
```
[WebSocket] Client connected from 172.20.10.X
[Recorder] Started recording...
[Processing] Transcribed text: "ดาวพฤหัสบดี"
[Processing] Validating with Gemini AI...
[Validator] AI result: valid
[Processing] ✓ Validation result sent: valid
```

### 5. Check Result

**IF CORRECT:**
- Alarm stops
- Display shows "CORRECT! ✓"
- Returns to normal display after 3s

**IF WRONG:**
- Display shows "Wrong! Try again (2/20)"
- Alarm continues
- Can try again (up to 20 attempts)

**IF TIMEOUT (50s no press):**
- Counts as wrong attempt
- Display updates attempt counter

**IF MAX ATTEMPTS (20):**
- Alarm stops anyway
- Display shows "Max attempts reached"
- Returns to normal display after 3s

## Troubleshooting

### No audio received
- Check WebSocket connection: `[WebSocket] Client connected`
- Check I2S pins (33/34/32)
- Check AUDIO_SERVER_IP in config.h (172.20.10.3)

### Validation always fails
- Check Gemini API key
- Check question is in current_question
- Look at transcribed text in Python output

### Button not working
- LED should turn ON when pressed
- Check GPIO 15 connection
- Monitor serial for button debug output

### Alarm doesn't trigger
- Check time sync (NTP or MQTT time)
- Verify alarm set correctly: `uv run mqtt_send.py alarms`
- Check serial for "ALARM TRIGGERED"

## Testing Scenarios

### Scenario 1: Correct Answer First Try
1. Set alarm
2. Wait for trigger
3. Press button, answer correctly
4. ✓ Alarm stops immediately

### Scenario 2: Multiple Wrong Attempts
1. Set alarm
2. Answer incorrectly 3 times
3. ✓ Display shows attempt counter
4. Answer correctly on 4th try
5. ✓ Alarm stops

### Scenario 3: Timeout Handling
1. Set alarm
2. Don't press button for 50 seconds
3. ✓ Counts as wrong attempt
4. Try again

### Scenario 4: Max Attempts
1. Set alarm
2. Answer wrong 20 times (or timeout 20 times)
3. ✓ Alarm stops anyway
4. ✓ Display shows "Max attempts reached"

### Scenario 5: Volume Control
1. During alarm, press button
2. ✓ Volume drops to 30%
3. Release button
4. ✓ Volume restores to 100%

## Success Criteria

- [  ] Alarm triggers at correct time
- [  ] Question displays on OLED
- [  ] Alarm sound plays at 100% volume
- [  ] Button press starts recording (LED ON, volume 30%)
- [  ] Button release stops recording (LED OFF, volume 100%)
- [  ] Audio streams to Python server
- [  ] Whisper transcribes audio
- [  ] Gemini validates answer
- [  ] Validation result sent via MQTT
- [  ] ESP32 receives validation result
- [  ] Correct answer stops alarm
- [  ] Wrong answer increments attempt counter
- [  ] Timeout (50s) counts as wrong attempt
- [  ] Max attempts (20) stops alarm anyway
- [  ] OLED returns to normal display after completion

## Next Steps

Once basic flow works:
1. Tune timeout value (currently 50s)
2. Adjust max attempts (currently 20)
3. Improve STT accuracy (language model, noise filtering)
4. Add answer hints after failed attempts
5. Track statistics (attempts per alarm, success rate)
