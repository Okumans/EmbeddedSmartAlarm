#!/usr/bin/env python3
"""
Alarm Question Generator and Manager
Generates trivia questions using Gemini AI and stores them in a database
Questions are sent to ESP32 via MQTT for display on alarm trigger
"""

import json
import sqlite3
from datetime import datetime
from pathlib import Path
import paho.mqtt.client as mqtt
import time
from google import genai
import random

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "AlarmQuestionGenerator"

# Topics
TOPIC_QUESTION = "smartalarm/question"
TOPIC_ANSWER_VALIDATION = "smartalarm/answer/validation"
TOPIC_ALARM_DEACTIVATE = "smartalarm/alarm/deactivate"

# Gemini API
API_KEY = 'AIzaSyDOX89lRp_WZr_mjGyx4viRIIcZXF_Y-s8'

# Database
DB_FILE = Path(__file__).parent / "alarm_questions.db"

QUESTION_PROMPT = """
Act as a "Wake-Up Challenge" generator. Generate a unique, difficult trivia question in Thai.
Strict Constraints:
High Variety Topics: Do NOT ask about common history dates or mountain heights. Instead, select a random category from: Astronomy, Human Anatomy, Thai Literature, Physics, Video Games, Chemical Elements, or Geometry.
The Answer: Must be STT-Friendly. The answer must be either:
A Number (e.g., speed of light, number of bones).
A Single Distinct Noun (e.g., name of a planet, a specific color, an animal, or a chemical element name).
Difficulty: The user must not be able to answer while half-asleep. They should need to Google it or calculate it.
Format: Output strictly only the question in Thai inside double quotes.
"""


class QuestionDatabase:
    """Manages alarm questions in SQLite database"""
    
    def __init__(self, db_path=DB_FILE):
        self.db_path = db_path
        self.init_database()
    
    def init_database(self):
        """Initialize database schema"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS questions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                question TEXT NOT NULL,
                correct_answer TEXT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                used_count INTEGER DEFAULT 0,
                last_used TIMESTAMP
            )
        """)
        
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS answer_attempts (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                question_id INTEGER,
                user_answer TEXT,
                is_correct BOOLEAN,
                attempt_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (question_id) REFERENCES questions(id)
            )
        """)
        
        conn.commit()
        conn.close()
        print(f"✓ Database initialized at {self.db_path}")
    
    def add_question(self, question, correct_answer=None):
        """Add a new question to database"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute("""
            INSERT INTO questions (question, correct_answer)
            VALUES (?, ?)
        """, (question, correct_answer))
        
        question_id = cursor.lastrowid
        conn.commit()
        conn.close()
        
        print(f"✓ Added question #{question_id}: {question[:50]}...")
        return question_id
    
    def get_random_question(self):
        """Get a random question, preferring less-used ones"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        # Get question with lowest usage count
        cursor.execute("""
            SELECT id, question, correct_answer, used_count
            FROM questions
            ORDER BY used_count ASC, RANDOM()
            LIMIT 1
        """)
        
        result = cursor.fetchone()
        conn.close()
        
        if result:
            return {
                "id": result[0],
                "question": result[1],
                "correct_answer": result[2],
                "used_count": result[3]
            }
        return None
    
    def mark_question_used(self, question_id):
        """Mark question as used"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute("""
            UPDATE questions
            SET used_count = used_count + 1,
                last_used = CURRENT_TIMESTAMP
            WHERE id = ?
        """, (question_id,))
        
        conn.commit()
        conn.close()
    
    def record_answer_attempt(self, question_id, user_answer, is_correct):
        """Record an answer attempt"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute("""
            INSERT INTO answer_attempts (question_id, user_answer, is_correct)
            VALUES (?, ?, ?)
        """, (question_id, user_answer, is_correct))
        
        conn.commit()
        conn.close()
    
    def get_question_count(self):
        """Get total number of questions"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute("SELECT COUNT(*) FROM questions")
        count = cursor.fetchone()[0]
        
        conn.close()
        return count


class GeminiQuestionGenerator:
    """Generate questions using Gemini AI"""
    
    def __init__(self, api_key=API_KEY):
        self.client = genai.Client(api_key=api_key)
    
    def generate_with_retry(self, model, contents, max_attempts=5):
        """Generate content with exponential backoff retry"""
        attempt = 0
        backoff = 1.0
        
        while attempt < max_attempts:
            attempt += 1
            try:
                response = self.client.models.generate_content(
                    model=model,
                    contents=contents,
                )
                return response
            except Exception as e:
                if attempt >= max_attempts:
                    raise
                jitter = random.uniform(0, 0.5 * backoff)
                sleep_time = min(backoff + jitter, 60.0)
                print(f"Request failed (attempt {attempt}): {e}. Retrying in {sleep_time:.1f}s...")
                time.sleep(sleep_time)
                backoff = min(backoff * 2.0, 60.0)
    
    def generate_question(self):
        """Generate a trivia question"""
        print("Generating question with Gemini AI...")
        
        response = self.generate_with_retry(
            model="gemini-2.0-flash-exp",
            contents=QUESTION_PROMPT
        )
        
        question = response.text.strip('"{} \n')
        print(f"✓ Generated: {question}")
        return question
    
    def validate_answer(self, question, answer):
        """Validate if answer is correct"""
        prompt = f"""
        You are a "Wake-Up Challenge" validator. Given the trivia question and its answer, determine if the answer is valid.
        Question: {question}
        Answer: {answer}
        Answer Format: Output strictly only "Valid" or "Invalid" inside double quotes.
        """
        
        response = self.generate_with_retry(
            model="gemini-2.0-flash-exp",
            contents=prompt
        )
        
        result = response.text.strip().replace('"', '')
        is_valid = "Valid" in result and "Invalid" not in result
        
        print(f"Validation: {answer} -> {result} ({'✓' if is_valid else '✗'})")
        return is_valid
    
    def get_correct_answer(self, question):
        """Get the correct answer for a question"""
        prompt = f"""
        What is the correct short answer to this trivia question?
        Question: {question}
        Format: Just the answer text in Thai.
        """
        
        response = self.generate_with_retry(
            model="gemini-2.0-flash-exp",
            contents=prompt
        )
        
        return response.text.strip()


def generate_and_store_questions(count=10):
    """Generate multiple questions and store in database"""
    db = QuestionDatabase()
    generator = GeminiQuestionGenerator()
    
    print(f"\n{'='*70}")
    print(f"  Generating {count} Questions")
    print(f"{'='*70}\n")
    
    for i in range(count):
        print(f"\n[{i+1}/{count}] Generating question...")
        try:
            question = generator.generate_question()
            
            # Optionally get correct answer
            # correct_answer = generator.get_correct_answer(question)
            # db.add_question(question, correct_answer)
            
            db.add_question(question)
            time.sleep(1)  # Rate limiting
            
        except Exception as e:
            print(f"✗ Error generating question: {e}")
            continue
    
    print(f"\n{'='*70}")
    print(f"✓ Generated and stored {count} questions")
    print(f"  Total questions in database: {db.get_question_count()}")
    print(f"{'='*70}\n")


def send_question_to_esp32(question_text):
    """Send question to ESP32 via MQTT"""
    client = mqtt.Client(client_id=MQTT_CLIENT_ID)
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        time.sleep(0.5)
        
        # Send question
        result = client.publish(TOPIC_QUESTION, question_text, qos=1)
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"✓ Sent question to ESP32: {question_text[:50]}...")
            return True
        else:
            print(f"✗ Failed to send question")
            return False
            
    except Exception as e:
        print(f"✗ MQTT Error: {e}")
        return False
    finally:
        client.loop_stop()
        client.disconnect()


def main():
    """Main function"""
    import sys
    
    if len(sys.argv) < 2:
        print("\nUsage:")
        print("  python alarm_question_manager.py generate <count>  - Generate questions")
        print("  python alarm_question_manager.py send              - Send random question to ESP32")
        print("  python alarm_question_manager.py stats             - Show database statistics")
        print("\nExamples:")
        print("  python alarm_question_manager.py generate 10")
        print("  python alarm_question_manager.py send")
        print()
        return
    
    command = sys.argv[1].lower()
    
    if command == "generate":
        count = int(sys.argv[2]) if len(sys.argv) > 2 else 5
        generate_and_store_questions(count)
    
    elif command == "send":
        db = QuestionDatabase()
        question_data = db.get_random_question()
        
        if question_data:
            print(f"\nSelected Question #{question_data['id']}:")
            print(f"  {question_data['question']}")
            print(f"  Used {question_data['used_count']} times\n")
            
            send_question_to_esp32(question_data['question'])
            db.mark_question_used(question_data['id'])
        else:
            print("✗ No questions in database. Generate some first!")
    
    elif command == "stats":
        db = QuestionDatabase()
        conn = sqlite3.connect(db.db_path)
        cursor = conn.cursor()
        
        print(f"\n{'='*70}")
        print("  Database Statistics")
        print(f"{'='*70}\n")
        
        cursor.execute("SELECT COUNT(*) FROM questions")
        total = cursor.fetchone()[0]
        print(f"Total questions: {total}")
        
        cursor.execute("SELECT AVG(used_count) FROM questions")
        avg_used = cursor.fetchone()[0] or 0
        print(f"Average usage: {avg_used:.2f} times")
        
        cursor.execute("SELECT COUNT(*) FROM answer_attempts")
        total_attempts = cursor.fetchone()[0]
        print(f"Total answer attempts: {total_attempts}")
        
        cursor.execute("SELECT COUNT(*) FROM answer_attempts WHERE is_correct = 1")
        correct_attempts = cursor.fetchone()[0]
        
        if total_attempts > 0:
            success_rate = (correct_attempts / total_attempts) * 100
            print(f"Success rate: {success_rate:.1f}%")
        
        print(f"\n{'='*70}\n")
        conn.close()
    
    else:
        print(f"✗ Unknown command: {command}")


if __name__ == "__main__":
    main()
