#!/usr/bin/env python3
"""
Alarm Answer Verification System
Listens for audio recordings, transcribes them, validates answers with Gemini AI
"""

import paho.mqtt.client as mqtt
import time
import os
import sys
from pathlib import Path

# Add LLM_STT to path
sys.path.append(str(Path(__file__).parent))

from transcribe import transcribe_audio
from gemini_loop import GeminiQuestionGenerator, QuestionDatabase

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "AlarmAnswerVerifier"

# Topics
TOPIC_AUDIO_RECORDING = "smartalarm/answer/audio"
TOPIC_ANSWER_TEXT = "smartalarm/answer/text"
TOPIC_VALIDATION_RESULT = "smartalarm/answer/validation"
TOPIC_RECORDING_START = "smartalarm/recording/start"
TOPIC_RECORDING_STOP = "smartalarm/recording/stop"

# Temporary audio file
TEMP_AUDIO_FILE = Path(__file__).parent / "temp_answer.wav"


class AnswerVerifier:
    """Handles answer transcription and validation"""
    
    def __init__(self):
        self.generator = GeminiQuestionGenerator()
        self.db = QuestionDatabase()
        self.current_question = None
        self.current_question_id = None
        self.mqtt_client = None
    
    def set_mqtt_client(self, client):
        """Set MQTT client for publishing"""
        self.mqtt_client = client
    
    def set_current_question(self, question, question_id=None):
        """Set the current question being answered"""
        self.current_question = question
        self.current_question_id = question_id
        print(f"\n[Verifier] Current question: {question}")
    
    def process_audio_answer(self, audio_data):
        """Process audio recording and validate answer"""
        if not self.current_question:
            print("[Verifier] ⚠ No question set!")
            return False
        
        try:
            # Save audio data to temporary file
            print("[Verifier] Saving audio recording...")
            with open(TEMP_AUDIO_FILE, 'wb') as f:
                f.write(audio_data)
            
            # Transcribe audio
            print("[Verifier] Transcribing audio...")
            transcribed_text = transcribe_audio(str(TEMP_AUDIO_FILE), model_size="base")
            transcribed_text = transcribed_text.strip()
            
            print(f"[Verifier] Transcribed: {transcribed_text}")
            
            # Publish transcribed text
            if self.mqtt_client:
                self.mqtt_client.publish(TOPIC_ANSWER_TEXT, transcribed_text)
            
            # Validate answer
            print("[Verifier] Validating answer with Gemini...")
            is_correct = self.generator.validate_answer(self.current_question, transcribed_text)
            
            # Publish validation result
            result = "valid" if is_correct else "invalid"
            if self.mqtt_client:
                self.mqtt_client.publish(TOPIC_VALIDATION_RESULT, result)
            
            # Record attempt in database
            if self.current_question_id:
                self.db.record_answer_attempt(
                    self.current_question_id,
                    transcribed_text,
                    is_correct
                )
            
            print(f"[Verifier] Result: {result.upper()}")
            
            # Clean up
            if TEMP_AUDIO_FILE.exists():
                TEMP_AUDIO_FILE.unlink()
            
            return is_correct
            
        except Exception as e:
            print(f"[Verifier] ✗ Error: {e}")
            if self.mqtt_client:
                self.mqtt_client.publish(TOPIC_VALIDATION_RESULT, "error")
            return False


class MQTTAnswerListener:
    """MQTT client for listening to answer recordings"""
    
    def __init__(self):
        self.verifier = AnswerVerifier()
        self.client = mqtt.Client(client_id=MQTT_CLIENT_ID)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.verifier.set_mqtt_client(self.client)
        
        self.audio_chunks = []
        self.recording_active = False
    
    def on_connect(self, client, userdata, flags, rc):
        """Callback when connected to MQTT broker"""
        if rc == 0:
            print(f"✓ Connected to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
            print("=" * 70)
            
            # Subscribe to topics
            client.subscribe(TOPIC_AUDIO_RECORDING)
            print(f"  Subscribed to: {TOPIC_AUDIO_RECORDING}")
            
            client.subscribe(TOPIC_RECORDING_START)
            print(f"  Subscribed to: {TOPIC_RECORDING_START}")
            
            client.subscribe(TOPIC_RECORDING_STOP)
            print(f"  Subscribed to: {TOPIC_RECORDING_STOP}")
            
            client.subscribe("smartalarm/question")
            print(f"  Subscribed to: smartalarm/question")
            
            print("=" * 70)
            print("\nWaiting for answer recordings... (Press Ctrl+C to exit)\n")
        else:
            print(f"✗ Connection failed with code {rc}")
            sys.exit(1)
    
    def on_message(self, client, userdata, msg):
        """Callback when a message is received"""
        topic = msg.topic
        
        if topic == "smartalarm/question":
            # New question received
            question = msg.payload.decode("utf-8")
            self.verifier.set_current_question(question)
        
        elif topic == TOPIC_RECORDING_START:
            # Start collecting audio chunks
            print("\n[Recording] START")
            self.audio_chunks = []
            self.recording_active = True
        
        elif topic == TOPIC_RECORDING_STOP:
            # Stop recording and process audio
            print("[Recording] STOP")
            self.recording_active = False
            
            if self.audio_chunks:
                # Combine all chunks
                audio_data = b''.join(self.audio_chunks)
                print(f"[Recording] Received {len(audio_data)} bytes")
                
                # Process answer
                self.verifier.process_audio_answer(audio_data)
                
                # Clear chunks
                self.audio_chunks = []
        
        elif topic == TOPIC_AUDIO_RECORDING:
            # Receive audio chunk
            if self.recording_active:
                self.audio_chunks.append(msg.payload)
                print(f"[Recording] Chunk {len(self.audio_chunks)} ({len(msg.payload)} bytes)")
    
    def start(self):
        """Start MQTT listener"""
        print("\n" + "=" * 70)
        print("  Smart Alarm Clock - Answer Verification System")
        print("=" * 70)
        print(f"Broker: {MQTT_BROKER}:{MQTT_PORT}")
        print()
        
        try:
            print(f"Connecting to {MQTT_BROKER}:{MQTT_PORT}...")
            self.client.connect(MQTT_BROKER, MQTT_PORT, 60)
            
            # Start the loop
            self.client.loop_forever()
            
        except KeyboardInterrupt:
            print("\n\n✓ Shutting down gracefully...")
            self.client.disconnect()
            print("✓ Disconnected from MQTT broker")
        
        except Exception as e:
            print(f"\n✗ Error: {e}")
            sys.exit(1)


def main():
    """Main function"""
    listener = MQTTAnswerListener()
    listener.start()


if __name__ == "__main__":
    main()
