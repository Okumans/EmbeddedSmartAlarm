# Question Manager System - Usage Guide

## Overview

The QuestionManager system provides persistent question caching with automatic refill from a Python service. Questions are stored on ESP32's LittleFS filesystem and managed intelligently.

## Architecture

```
ESP32 (QuestionManager)          Python (question_service.py)
    ↓                                      ↑
Cache: 10 questions                Database (SQLite)
    ↓                                      ↑
Check threshold (< 3)              Generate with Gemini AI
    ↓                                      ↑
Request via MQTT ────────────────────────→ 
    ↓                                      ↓
    ←──────────────────────────── Send batch (5-10 questions)
    ↓
Store to /questions.txt
    ↓
Select random for alarm
```

## Quick Start

### 1. Generate Initial Questions

```bash
cd LLM_STT
python alarm_question_manager.py generate 20
```

### 2. Start Question Service (Keep Running)

```bash
# Terminal 1 - Question Service
cd LLM_STT
python question_service.py
```

Expected output:
```
======================================================================
  Question Service - Daemon Mode
======================================================================
  Broker: broker.hivemq.com:1883
  Database: alarm_questions.db
  Questions in DB: 20
======================================================================

🔌 Connecting to MQTT broker...
✓ Connected to MQTT broker
✓ Subscribed to smartalarm/question/request
```

### 3. Upload ESP32 Firmware

```bash
platformio run --target upload --environment env_gateway_esp32
```

### 4. Monitor ESP32

```bash
platformio device monitor --environment env_gateway_esp32
```

You should see:
```
[QuestionManager] Initializing...
[QuestionManager] ✓ Loaded 0 questions from file
[QuestionManager] Cache low (0/3), requesting more...
[QuestionManager] Requesting 5 questions from service...
```

Then in Python service terminal:
```
📨 Question request received
   Requested: 5 questions
   Database: 20 questions available

📤 Sending 5 questions:
   1. ดาวเคราะห์ที่ใหญ่ที่สุดในระบบสุริยะคืออะไร?...
   2. เมืองหลวงของประเทศไทยคืออะไร?...
   ...
✓ Batch sent successfully
```

ESP32:
```
[QuestionManager] Received question batch from MQTT
[QuestionManager] ✓ Added question to cache (1/10)
[QuestionManager] ✓ Added question to cache (2/10)
...
[QuestionManager] ✓ Saved 5 questions to file
╔════════════════════════════════════════╗
║     Question Manager Status            ║
╠════════════════════════════════════════╣
║ Cached Questions:   5 / 10            ║
║ Threshold:          3                  ║
║ Needs More:        NO                  ║
║ Request Pending:   NO                  ║
║ Initialized:       YES                 ║
╚════════════════════════════════════════╝
```

## Testing

### Test Alarm with Question

```bash
# Terminal 2
cd scripts
python send_alarm_config.py both 14:35  # Set alarm 1 minute ahead
```

When alarm triggers:
```
⏰ ALARM TRIGGERED: 14:35
[QuestionManager] Getting question for alarm...
[QuestionManager] Cache low (2/3), requesting more...
[AlarmQuestion] Question set: ดาวเคราะห์ที่ใหญ่ที่สุดในระบบสุริยะคืออะไร?
[Alarm] Starting question challenge mode
[Alarm] Playing alarm sound: /alarm.mp3
```

### Manual Question Request

In ESP32 serial monitor or via MQTT:
```bash
# Test requesting questions
python -c "
import paho.mqtt.client as mqtt
c = mqtt.Client()
c.connect('broker.hivemq.com', 1883)
c.publish('smartalarm/question/request', '3', qos=1)
"
```

## MQTT Topics

| Topic | Direction | Payload | QoS |
|-------|-----------|---------|-----|
| `smartalarm/question/request` | ESP32 → Python | `"5"` (count) | 1 |
| `smartalarm/question/batch` | Python → ESP32 | `"Q1\nQ2\nQ3"` | 1 |
| `smartalarm/question` | Python → ESP32 | `"Single Q"` | 1 (legacy) |

## Files

### ESP32
- **Header**: `include/gateway_esp32/question_manager.h`
- **Implementation**: `src/gateway_esp32/question_manager.cpp`
- **Storage**: `/questions.txt` (LittleFS)
- **Config**: `include/shared/mqtt_topic_config.h`

### Python
- **Service**: `LLM_STT/question_service.py` (daemon)
- **Manager**: `LLM_STT/alarm_question_manager.py` (CLI tool)
- **Database**: `LLM_STT/alarm_questions.db` (SQLite)

## Configuration

### ESP32 (question_manager.h)
```cpp
static const int MAX_QUESTIONS_CACHE = 10;      // Max cache size
static const int MIN_QUESTIONS_THRESHOLD = 3;    // Request when below this
static const unsigned long REQUEST_COOLDOWN = 5000;  // Min 5s between requests
```

### Python
```python
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
```

## Troubleshooting

### No questions received

1. **Check Python service is running**:
   ```bash
   ps aux | grep question_service
   ```

2. **Check MQTT connectivity**:
   ```bash
   # Monitor MQTT traffic
   cd scripts
   python mqtt_subscriber.py
   ```

3. **Check database**:
   ```bash
   cd LLM_STT
   python alarm_question_manager.py stats
   ```

### Questions not persisting

1. Check LittleFS mounted:
   ```
   [AudioManager] ✓ LittleFS mounted.
   ```

2. Check file permissions:
   ```cpp
   File file = LittleFS.open("/questions.txt", "w");
   ```

### Alarm triggers without question

- This is normal if no questions are cached yet
- Service will request questions automatically
- Fallback questions are used if MQTT unavailable

## Advanced Usage

### Pre-load Questions on Boot

Add to `main.cpp` setup:
```cpp
// Request questions immediately on boot
if (questionManager.getCachedQuestionCount() == 0) {
    questionManager.requestQuestionsFromService(10);
}
```

### Manual Cache Management

Via serial commands (future feature):
```cpp
questionManager.clearCache();
questionManager.printCache();
questionManager.printStatus();
```

### Custom Question Generation

```bash
# Generate specific topic questions
cd LLM_STT
# Edit QUESTION_PROMPT in alarm_question_manager.py
python alarm_question_manager.py generate 10
```

## System Behavior

### Boot Sequence
1. QuestionManager.begin() loads from `/questions.txt`
2. If count < 3, automatically requests via MQTT
3. Python service responds with batch
4. Questions saved to file for next boot

### Alarm Trigger
1. Alarm matches time
2. QuestionManager.getQuestionForAlarm() called
3. Random question selected from cache
4. If cache < 3, async request sent (non-blocking)
5. Question displayed on OLED

### Question Rotation
- Random selection prevents repeats
- Least-used questions prioritized (Python side)
- Simple random on ESP32 side

## Performance

- **Memory**: ~1KB for 10 questions (100 bytes avg each)
- **File I/O**: Only on boot and batch receive
- **MQTT**: ~500 bytes per request/response
- **CPU**: Negligible overhead

## Benefits

✅ **Persistent** - Survives reboots
✅ **Automatic** - Requests when low
✅ **Efficient** - Batch loading reduces MQTT traffic
✅ **Reliable** - Fallback questions if offline
✅ **Scalable** - Easy to add more questions
✅ **Stateless Python** - Service just responds to requests
