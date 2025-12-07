# LLM/STT Alarm System - Quick Start Guide

## 🚀 Setup (One Time)

### 1. Install Python Dependencies

```bash
cd LLM_STT
pip install -r requirements.txt
```

### 2. Configure Gemini API Key

Edit `alarm_question_manager.py` and `gemini_loop.py`:

```python
API_KEY = 'YOUR_API_KEY_HERE'
```

Get your API key from: https://makersuite.google.com/app/apikey

### 3. Generate Questions

```bash
python alarm_question_manager.py generate 10
```

### 4. Upload ESP32 Firmware

```bash
cd ..
platformio run --target upload --environment env_gateway_esp32
```

## 📝 Daily Usage

### Start the System (3 Terminals)

**Terminal 1**: Answer Verifier (keep running)

```bash
cd LLM_STT
python alarm_answer_verifier.py
```

**Terminal 2**: Send question before sleep

```bash
python alarm_question_manager.py send
```

**Terminal 3**: Set alarm time

```bash
cd ../scripts
python send_alarm_config.py both 07:00
```

## ⏰ When Alarm Triggers

1. **ESP32 plays alarm sound** 📢
2. **OLED displays trivia question** 📺
3. **Hold button (long press)** → Recording starts
4. **Speak your answer** 🎤
5. **Release button** → Recording stops
6. **Wait 2-3 seconds** → Python processes answer
7. **If correct** → Alarm stops ✅
8. **If wrong** → Try again (max 3 attempts)

## 🎯 Button Guide

| Gesture                 | Action          | Description          |
| ----------------------- | --------------- | -------------------- |
| **Long Press** (800ms+) | START Recording | Hold to speak answer |
| **Long Press Release**  | STOP Recording  | Release to submit    |
| Single Click            | _(Future)_      | Skip/Cancel          |
| Double Click            | _(Future)_      | Show hint            |

## 📊 Useful Commands

```bash
# View database stats
python alarm_question_manager.py stats

# Generate more questions
python alarm_question_manager.py generate 20

# Test question immediately
python alarm_question_manager.py send

# Monitor all MQTT messages
cd ../scripts
python mqtt_subscriber.py
```

## 🔧 Hardware Checklist

- [x] Button connected to GPIO 4
- [x] Button connected to GND
- [x] Speaker connected (I2S: GPIO 26, 25, 27)
- [x] SD card inserted with `/alarm.mp3`
- [x] OLED display connected (I2C)
- [x] WiFi configured and connected

## ❓ Quick Troubleshooting

### Button doesn't work

- Check GPIO 4 connection
- Verify ground connection
- Check serial: `[Button] Initialized on pin 4`

### Question not showing on OLED

- Run: `python alarm_question_manager.py send`
- Check ESP32 serial: `[MQTT] Question received`
- Verify MQTT connection

### Answer always marked wrong

- Check Gemini API key is valid
- Speak clearly in Thai
- Ensure quiet environment
- Check Python verifier is running

### Alarm won't stop

- Press button and speak correct answer
- Or manually stop: `python mqtt_send.py smartalarm/commands stop_audio`

## 🎓 Example Question/Answer

**Question**: `ดาวเคราะห์ที่ใหญ่ที่สุดในระบบสุริยะคืออะไร?`  
(What is the largest planet in the solar system?)

**Correct Answer**: `ดาวพฤหัสบดี` (Jupiter)

**How it works**:

1. Alarm triggers → Question shows on OLED
2. You long-press button
3. Say: "ดาวพฤหัสบดี"
4. Release button
5. Python transcribes: "ดาวพฤหัสบดี"
6. Gemini validates: ✓ Correct!
7. Alarm stops → Good morning! ☀️

## 🌟 Tips

- **Speak Clearly**: Enunciate words for better STT accuracy
- **Quiet Room**: Background noise affects transcription
- **Fast WiFi**: Required for Gemini API calls
- **Pre-generate Questions**: Run generate command the night before
- **Keep Verifier Running**: Leave Terminal 1 active overnight
- **Test First**: Try the system in the afternoon before relying on it

## 📱 Mobile App Integration (Future)

Plan to add:

- Question management via mobile app
- Custom question creation
- Difficulty levels
- Category selection
- Answer history
- Statistics dashboard

---

**Need Help?** Check `WORKFLOW_GUIDE.md` for detailed documentation.
