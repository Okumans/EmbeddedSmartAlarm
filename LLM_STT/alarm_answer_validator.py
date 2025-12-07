#!/usr/bin/env -S uv run
# /// script
# dependencies = [
#   "websockets",
#   "paho-mqtt",
#   "faster-whisper",
#   "google-generativeai",
# ]
# ///

"""
Alarm Answer Validator Service
Receives audio via WebSocket, transcribes with Whisper, validates with Gemini AI,
and sends result back via MQTT.
"""

import asyncio
import websockets
import wave
import paho.mqtt.client as mqtt
from datetime import datetime
from pathlib import Path
import sys
import os

# Import from existing modules
sys.path.append(str(Path(__file__).parent))
from alarm_question_manager import QuestionDatabase, GeminiQuestionGenerator
from transcribe import transcribe_audio_file

# Configuration
SAMPLE_RATE = 16000
SAMPLE_WIDTH = 2  # 16-bit
CHANNELS = 1      # Mono
WS_HOST = "0.0.0.0"
WS_PORT = 4000

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "AlarmAnswerValidator"

# Output directory
RECORDINGS_DIR = Path(__file__).parent / "recordings"
RECORDINGS_DIR.mkdir(exist_ok=True)

# Global state
current_question = None
mqtt_client = None

class AudioRecorder:
    def __init__(self, filename):
        self.filename = filename
        self.wav_file = None
        self.frames_received = 0
        self.bytes_received = 0
        self.start_time = datetime.now()
        
    def start(self):
        """Initialize WAV file for writing."""
        self.wav_file = wave.open(str(self.filename), 'wb')
        self.wav_file.setnchannels(CHANNELS)
        self.wav_file.setsampwidth(SAMPLE_WIDTH)
        self.wav_file.setframerate(SAMPLE_RATE)
        print(f"[Recorder] Started recording to: {self.filename}")
        
    def write_frames(self, data):
        """Write audio frames to WAV file."""
        if self.wav_file:
            self.wav_file.writeframes(data)
            self.bytes_received += len(data)
            self.frames_received += len(data) // SAMPLE_WIDTH
            
    def stop(self):
        """Close WAV file and return info."""
        if self.wav_file:
            self.wav_file.close()
            duration = (datetime.now() - self.start_time).total_seconds()
            file_size = self.filename.stat().st_size
            audio_duration = self.frames_received / SAMPLE_RATE
            
            print(f"[Recorder] Recording saved:")
            print(f"  File: {self.filename}")
            print(f"  Wall-clock duration: {duration:.2f} seconds")
            print(f"  Audio duration: {audio_duration:.2f} seconds")
            print(f"  Frames: {self.frames_received}")
            print(f"  File size: {file_size / 1024:.2f} KB")
            
            self.wav_file = None
            return str(self.filename)

def validate_answer_with_gemini(question: str, user_answer: str) -> bool:
    """Validate user's answer against the question using Gemini AI."""
    try:
        generator = GeminiQuestionGenerator()
        
        prompt = f"""
You are validating an answer to a trivia question.

Question: {question}

User's Answer: {user_answer}

Is the user's answer correct? Respond with ONLY "valid" or "invalid".

Consider:
- Spelling variations
- Synonyms
- Partial answers that contain the core correct information
- Language variations (English/Thai)

Response (valid or invalid):
"""
        
        response = generator.model.generate_content(prompt)
        result = response.text.strip().lower()
        
        print(f"[Validator] Question: {question}")
        print(f"[Validator] User answer: {user_answer}")
        print(f"[Validator] AI result: {result}")
        
        return "valid" in result and "invalid" not in result
        
    except Exception as e:
        print(f"[Validator] Error: {e}")
        return False

def on_mqtt_connect(client, userdata, flags, rc):
    """MQTT connection callback."""
    if rc == 0:
        print("[MQTT] Connected successfully")
        # Subscribe to question topic to track current question
        client.subscribe("smartalarm/question")
    else:
        print(f"[MQTT] Connection failed with code {rc}")

def on_mqtt_message(client, userdata, msg):
    """MQTT message callback to track current question."""
    global current_question
    
    if msg.topic == "smartalarm/question":
        current_question = msg.payload.decode()
        print(f"[MQTT] Current question updated: {current_question}")

def setup_mqtt():
    """Initialize MQTT client."""
    global mqtt_client
    
    mqtt_client = mqtt.Client(MQTT_CLIENT_ID)
    mqtt_client.on_connect = on_mqtt_connect
    mqtt_client.on_message = on_mqtt_message
    
    print(f"[MQTT] Connecting to {MQTT_BROKER}:{MQTT_PORT}...")
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()

async def handle_audio_session(websocket):
    """Handle WebSocket connection and process audio."""
    client_addr = websocket.remote_address
    print(f"\n[WebSocket] Client connected from {client_addr[0]}:{client_addr[1]}")
    
    # Create recorder with timestamp filename
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = RECORDINGS_DIR / f"answer_{timestamp}.wav"
    recorder = AudioRecorder(filename)
    recorder.start()
    
    try:
        async for message in websocket:
            # Receive binary audio data
            if isinstance(message, bytes):
                print(f"[WebSocket] Received {len(message)} bytes")
                recorder.write_frames(message)
            else:
                print(f"[WebSocket] Received text message: {message}")
                
    except websockets.exceptions.ConnectionClosed:
        print(f"\n[WebSocket] Client disconnected")
    except Exception as e:
        print(f"\n[Error] {type(e).__name__}: {e}")
    finally:
        # Stop recording and get filename
        wav_file = recorder.stop()
        
        # Process the recording
        await process_answer(wav_file)

async def process_answer(wav_file: str):
    """Transcribe and validate the answer."""
    global current_question, mqtt_client
    
    print(f"\n[Processing] Starting validation...")
    
    # Step 1: Transcribe audio with Whisper
    print("[Processing] Transcribing audio with Whisper...")
    try:
        transcribed_text = transcribe_audio_file(wav_file, language="th")
        print(f"[Processing] Transcribed text: {transcribed_text}")
    except Exception as e:
        print(f"[Processing] Transcription error: {e}")
        transcribed_text = ""
    
    # Step 2: Validate answer
    if not transcribed_text:
        print("[Processing] No transcription - marking as invalid")
        is_valid = False
    elif not current_question:
        print("[Processing] No current question - cannot validate")
        is_valid = False
    else:
        print("[Processing] Validating answer with Gemini AI...")
        is_valid = validate_answer_with_gemini(current_question, transcribed_text)
    
    # Step 3: Send result via MQTT
    result_message = "valid" if is_valid else "invalid"
    print(f"[Processing] Sending result via MQTT: {result_message}")
    
    if mqtt_client:
        mqtt_client.publish("smartalarm/answer/validation", result_message, qos=1)
        print(f"[Processing] ✓ Validation result sent: {result_message}")
    else:
        print("[Processing] ✗ MQTT client not available!")
    
    print("[Processing] Validation complete\n")

async def main():
    """Start the validator service."""
    print("=" * 60)
    print("Alarm Answer Validator Service")
    print("=" * 60)
    print(f"WebSocket: {WS_HOST}:{WS_PORT}")
    print(f"MQTT: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"Recordings: {RECORDINGS_DIR}")
    print("=" * 60)
    
    # Setup MQTT
    setup_mqtt()
    
    # Wait for MQTT connection
    await asyncio.sleep(2)
    
    print(f"\nServer listening on ws://{WS_HOST}:{WS_PORT}")
    print("Waiting for ESP32 audio...\n")
    
    async with websockets.serve(handle_audio_session, WS_HOST, WS_PORT, max_size=None):
        await asyncio.Future()  # Run forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n[Server] Stopped by user")
        if mqtt_client:
            mqtt_client.loop_stop()
            mqtt_client.disconnect()
        sys.exit(0)
