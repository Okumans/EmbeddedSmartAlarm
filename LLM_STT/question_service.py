#!/usr/bin/env -S uv run
# /// script
# dependencies = [
#   "paho-mqtt",
# ]
# ///

"""
Question Service - Responds to ESP32 question requests
Runs as a daemon that listens for question requests and sends batches
"""

import sqlite3
from pathlib import Path
import paho.mqtt.client as mqtt
import time
from alarm_question_manager import QuestionDatabase, GeminiQuestionGenerator

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "QuestionService"

# Topics
TOPIC_QUESTION_REQUEST = "smartalarm/question/request"
TOPIC_QUESTION_BATCH = "smartalarm/question/batch"
TOPIC_QUESTION_SINGLE = "smartalarm/question"  # Legacy support

# Database
DB_FILE = Path(__file__).parent / "alarm_questions.db"

class QuestionService:
    """Service that responds to ESP32 question requests"""
    
    def __init__(self):
        self.db = QuestionDatabase(DB_FILE)
        self.generator = GeminiQuestionGenerator()
        self.client = mqtt.Client(client_id=MQTT_CLIENT_ID, callback_api_version=mqtt.CallbackAPIVersion.VERSION1)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        
    def on_connect(self, client, userdata, flags, rc):
        """Callback for when client connects to broker"""
        if rc == 0:
            print("✓ Connected to MQTT broker")
            # Subscribe to question request topic
            client.subscribe(TOPIC_QUESTION_REQUEST)
            print(f"✓ Subscribed to {TOPIC_QUESTION_REQUEST}")
        else:
            print(f"✗ Connection failed with code {rc}")
    
    def on_message(self, client, userdata, msg):
        """Callback for when a message is received"""
        try:
            if msg.topic == TOPIC_QUESTION_REQUEST:
                self.handle_question_request(msg)
        except Exception as e:
            print(f"✗ Error handling message: {e}")
    
    def handle_question_request(self, msg):
        """Handle question request from ESP32"""
        print(f"\n📨 Question request received")
        
        # Parse requested count (default 5)
        try:
            count = int(msg.payload.decode().strip())
            if count <= 0 or count > 10:
                count = 5
        except:
            count = 5
        
        print(f"   Requested: {count} questions")
        
        # Check if we have enough questions in database
        db_count = self.db.get_question_count()
        print(f"   Database: {db_count} questions available")
        
        # Generate more if needed
        if db_count < count:
            needed = count - db_count
            print(f"   Generating {needed} more questions...")
            try:
                self.generate_questions(needed)
            except Exception as e:
                print(f"   ⚠ Generation failed: {e}")
        
        # Get random questions from database
        questions = self.get_random_questions(count)
        
        if questions:
            # Send as batch (newline-separated)
            batch = "\n".join(questions)
            
            print(f"\n📤 Sending {len(questions)} questions:")
            for i, q in enumerate(questions, 1):
                print(f"   {i}. {q[:60]}...")
            
            result = self.client.publish(TOPIC_QUESTION_BATCH, batch, qos=1)
            result.wait_for_publish(timeout=2.0)
            
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                print(f"✓ Batch sent successfully\n")
            else:
                print(f"✗ Failed to send batch\n")
        else:
            print("✗ No questions available\n")
    
    def get_random_questions(self, count):
        """Get multiple random questions from database"""
        conn = sqlite3.connect(self.db.db_path)
        cursor = conn.cursor()
        
        # Get questions with lowest usage count
        cursor.execute("""
            SELECT question
            FROM questions
            ORDER BY used_count ASC, RANDOM()
            LIMIT ?
        """, (count,))
        
        results = cursor.fetchall()
        conn.close()
        
        return [row[0] for row in results]
    
    def generate_questions(self, count):
        """Generate new questions and store in database"""
        for i in range(count):
            try:
                question = self.generator.generate_question()
                self.db.add_question(question)
                time.sleep(1)  # Rate limiting
            except Exception as e:
                print(f"   ✗ Generation error: {e}")
                raise
    
    def run(self):
        """Run the service"""
        print("\n" + "="*70)
        print("  Question Service - Daemon Mode")
        print("="*70)
        print(f"  Broker: {MQTT_BROKER}:{MQTT_PORT}")
        print(f"  Database: {self.db.db_path}")
        print(f"  Questions in DB: {self.db.get_question_count()}")
        print("="*70 + "\n")
        
        print("🔌 Connecting to MQTT broker...")
        
        try:
            self.client.connect(MQTT_BROKER, MQTT_PORT, 60)
            self.client.loop_forever()
        except KeyboardInterrupt:
            print("\n\n⏹  Service stopped by user")
        except Exception as e:
            print(f"\n✗ Service error: {e}")
        finally:
            self.client.disconnect()


def main():
    """Main entry point"""
    import sys
    
    if len(sys.argv) > 1 and sys.argv[1] == "--help":
        print("\nQuestion Service - Daemon for ESP32 question requests")
        print("\nUsage:")
        print("  python question_service.py           - Run service daemon")
        print("  python question_service.py --help    - Show this help")
        print("\nThe service will:")
        print("  - Listen for question requests on smartalarm/question/request")
        print("  - Respond with question batches on smartalarm/question/batch")
        print("  - Generate new questions if database runs low")
        print("\nPress Ctrl+C to stop the service\n")
        return
    
    service = QuestionService()
    service.run()


if __name__ == "__main__":
    main()
