# LLM/STT Alarm Question System - Complete Workflow

## Overview

This system integrates Gemini AI question generation and speech-to-text (STT) answer verification to create an interactive alarm that requires answering a trivia question to be deactivated.

## System Components

### 1. Question Generation (Python)

- **File**: `LLM_STT/alarm_question_manager.py`
- **Function**: Generate trivia questions using Gemini AI
- **Database**: SQLite database storing questions
- **MQTT**: Sends questions to ESP32

### 2. Answer Verification (Python)

- **File**: `LLM_STT/alarm_answer_verifier.py`
- **Function**: Transcribe audio answers and validate with Gemini AI
- **STT Engine**: Whisper (faster-whisper or OpenAI whisper)
- **MQTT**: Receives audio, sends validation results

### 3. ESP32 Firmware

- **Button Manager**: Detects single/double/long press
- **Alarm Question Handler**: Manages question state
- **Audio Recording**: Records answer via I2S microphone (future)
- **OLED Display**: Shows question and status
- **MQTT Integration**: Communicates with Python services

## Complete Workflow

```
┌─────────────────────────────────────────────────────────────────┐
│ PHASE 1: QUESTION PREPARATION (Before Alarm)                    │
└─────────────────────────────────────────────────────────────────┘

1. Generate Questions:
   python alarm_question_manager.py generate 10
   → Creates 10 trivia questions
   → Stores in alarm_questions.db

2. Send Question to ESP32:
   python alarm_question_manager.py send
   → Selects random unused question
   → Publishes to smartalarm/question
   → ESP32 stores question in memory

┌─────────────────────────────────────────────────────────────────┐
│ PHASE 2: ALARM TRIGGER                                          │
└─────────────────────────────────────────────────────────────────┘

3. Time Matches Alarm:
   → ESP32 checks: current_time == alarm_time
   → Plays /alarm.mp3 on speaker
   → Starts question session
   → Displays question on OLED
   → Status: "Attempt 1/3"

┌─────────────────────────────────────────────────────────────────┐
│ PHASE 3: USER INTERACTION (Button Press)                        │
└─────────────────────────────────────────────────────────────────┘

4. User Presses Button:

   LONG PRESS (800ms+):
   → START recording answer
   → OLED shows: "Recording..."
   → Publishes: smartalarm/recording/start
   → Red LED on (optional)

   LONG PRESS RELEASE:
   → STOP recording
   → OLED shows: "Validating..."
   → Publishes: smartalarm/recording/stop
   → Audio data sent via MQTT chunks

   SINGLE CLICK:
   → Skip/Cancel (future feature)

   DOUBLE CLICK:
   → Show hint (future feature)

┌─────────────────────────────────────────────────────────────────┐
│ PHASE 4: ANSWER VERIFICATION (Python)                           │
└─────────────────────────────────────────────────────────────────┘

5. Answer Verifier Receives Audio:
   → Collects audio chunks
   → Saves to temp_answer.wav
   → Transcribes with Whisper STT
   → Extracted text: e.g., "ดาวเคราะห์พระอาทิตย์"

6. Gemini AI Validates:
   → Compares answer with question
   → Determines: Valid or Invalid
   → Publishes result to smartalarm/answer/validation

┌─────────────────────────────────────────────────────────────────┐
│ PHASE 5: RESULT HANDLING (ESP32)                                │
└─────────────────────────────────────────────────────────────────┘

7A. If CORRECT:
    → Stop alarm audio
    → OLED shows: "CORRECT!"
    → Publish: smartalarm/alarm/deactivated
    → Green LED blink (optional)
    → Reset question state
    → Good morning! ☀️

7B. If WRONG:
    → Increment attempt counter
    → OLED shows: "Wrong! 2/3"
    → Alarm continues playing
    → Wait for next button press

7C. If FAILED (3 attempts):
    → Alarm continues indefinitely
    → OLED shows: "FAILED - Manual Stop"
    → Publish: smartalarm/alarm/failed
    → Requires manual intervention
```

## MQTT Topics Flow

```
┌──────────────────┐
│ Python Generator │
└────────┬─────────┘
         │
         │ smartalarm/question
         │ "ดาวเคราะห์ที่ใหญ่ที่สุดในระบบสุริยะคืออะไร?"
         │
         ▼
    ┌────────┐
    │  ESP32  │◄──── Button Press (Long)
    └────┬───┘
         │
         │ smartalarm/recording/start
         │ smartalarm/answer/audio (chunks)
         │ smartalarm/recording/stop
         │
         ▼
┌─────────────────┐
│ Python Verifier │
└────────┬────────┘
         │
         │ 1. Transcribe: "ดาวพฤหัสบดี"
         │ 2. Validate with Gemini
         │ 3. Result: "valid"
         │
         │ smartalarm/answer/validation
         │
         ▼
    ┌────────┐
    │  ESP32  │──► Stop Alarm ✓
    └────────┘
```

## Button Gestures

### Long Press (Hold 800ms+)

**Action**: START/STOP Recording

```cpp
LONG_PRESS_START  → Begin recording answer
LONG_PRESS_STOP   → End recording, submit for validation
```

**Example**:

1. Hold button → "Recording..." appears
2. Speak answer: "ดาวพฤหัสบดี"
3. Release button → Audio sent for verification

### Single Click (Quick press)

**Action**: Reserved for future features

- Skip question
- Cancel recording
- Show next attempt

### Double Click (Two quick presses)

**Action**: Reserved for future features

- Show hint
- Repeat question audio
- Manual alarm stop

## File Structure

```
EmbeddedSmartAlarm/
├── LLM_STT/
│   ├── alarm_question_manager.py    ★ Generate & send questions
│   ├── alarm_answer_verifier.py     ★ STT & validation
│   ├── gemini_loop.py                 Gemini AI interface
│   ├── transcribe.py                  Whisper STT
│   ├── alarm_questions.db             Question database
│   └── temp_answer.wav                Temporary audio file
│
├── include/gateway_esp32/
│   ├── button_manager.h              ★ Button gesture detection
│   ├── alarm_question_handler.h      ★ Question state management
│   └── ...
│
├── src/gateway_esp32/
│   ├── button_manager.cpp            ★ Button implementation
│   ├── alarm_question_handler.cpp    ★ Question logic
│   ├── mqtt_setup.cpp                 MQTT handlers (updated)
│   ├── rtos_tasks.cpp                 Alarm task (updated)
│   └── ...
│
└── include/shared/
    └── mqtt_topic_config.h            MQTT topics (updated)
```

## Setup Instructions

### 1. Install Python Dependencies

```bash
cd LLM_STT
pip install google-generativeai paho-mqtt faster-whisper torch
```

Or using requirements.txt:

```bash
pip install -r requirements.txt
```

### 2. Generate Questions

```bash
# Generate 10 questions
python alarm_question_manager.py generate 10

# Check database stats
python alarm_question_manager.py stats
```

### 3. Start Services

**Terminal 1** - Answer Verifier (Keep Running):

```bash
python alarm_answer_verifier.py
```

**Terminal 2** - Send Question Before Alarm:

```bash
python alarm_question_manager.py send
```

**Terminal 3** - Monitor (Optional):

```bash
cd ../scripts
python mqtt_subscriber.py
```

### 4. Upload ESP32 Firmware

```bash
platformio run --target upload --environment env_gateway_esp32
```

### 5. Test the System

1. Set alarm for 1 minute ahead:

   ```bash
   python send_alarm_config.py both 14:35
   ```

2. Wait for alarm to trigger

3. Read question on OLED

4. Long-press button → speak answer → release

5. Wait for validation

6. If correct → alarm stops! ✓

## Hardware Connections

### Button (GPIO 4)

```
Button → GPIO 4
Button → GND
(Using INPUT mode with external pull-down)
```

### Optional: Status LED

```
LED Anode → GPIO 2 (or any free pin)
LED Cathode → 220Ω → GND
```

### I2S Microphone (Future - for audio recording)

```
MIC SCK  → GPIO 14
MIC WS   → GPIO 15
MIC SD   → GPIO 32
MIC VDD  → 3.3V
MIC GND  → GND
```

## Configuration

### Gemini API Key

Edit `LLM_STT/alarm_question_manager.py` and `gemini_loop.py`:

```python
API_KEY = 'YOUR_GEMINI_API_KEY_HERE'
```

### Button Pin

Edit `include/gateway_esp32/button_manager.h` or main.cpp:

```cpp
#define BUTTON_PIN 4
const bool BUTTON_ACTIVE_HIGH = true;
```

### Maximum Attempts

Edit `include/gateway_esp32/alarm_question_handler.h`:

```cpp
const int maxAttempts = 3;  // Default: 3 attempts
```

## Troubleshooting

### Question Not Displaying

1. Check Python generator sent question:
   ```bash
   python alarm_question_manager.py send
   ```
2. Verify ESP32 subscribed to `smartalarm/question`
3. Check serial monitor for `[MQTT] Question received`

### Button Not Responding

1. Check wiring: Button → GPIO 4, GND
2. Verify `BUTTON_ACTIVE_HIGH` setting matches wiring
3. Check serial: `[Button] Initialized on pin 4`

### Audio Not Recorded

**Note**: Current implementation expects external audio system.
Future implementation will use I2S microphone directly on ESP32.

### STT Not Working

1. Install dependencies:
   ```bash
   pip install faster-whisper torch
   ```
2. Check CUDA availability for faster processing
3. Test transcription standalone:
   ```bash
   python transcribe.py
   ```

### Validation Always Invalid

1. Check Gemini API key is valid
2. Verify answer language matches question (Thai)
3. Check network connectivity
4. Review Gemini API quotas

## Testing Commands

### Generate Test Questions

```bash
python alarm_question_manager.py generate 5
```

### Send Random Question

```bash
python alarm_question_manager.py send
```

### View Statistics

```bash
python alarm_question_manager.py stats
```

### Manual Validation Test

```bash
cd LLM_STT
python
>>> from gemini_loop import GeminiQuestionGenerator
>>> gen = GeminiQuestionGenerator()
>>> gen.validate_answer("ดาวเคราะห์ที่ใหญ่ที่สุดคืออะไร?", "ดาวพฤหัสบดี")
True
```

## Future Enhancements

- [ ] I2S microphone integration (INMP441)
- [ ] Real-time audio streaming to Python
- [ ] Voice activity detection (VAD)
- [ ] Multi-language support
- [ ] Question difficulty levels
- [ ] Hint system
- [ ] Daily question scheduling
- [ ] Answer history tracking
- [ ] Custom question categories
- [ ] TTS question playback

## Notes

- Questions are stored permanently in SQLite
- Alarm sounds continue until correct answer
- Maximum 3 attempts per alarm trigger
- Button requires physical press (no touch sensor)
- STT works best in quiet environment
- Gemini API requires internet connection
