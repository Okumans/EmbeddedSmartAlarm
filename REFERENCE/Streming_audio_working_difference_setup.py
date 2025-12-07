import asyncio
import websockets
import numpy as np
from faster_whisper import WhisperModel
import time

# --- CONFIG ---
PORT = 4000
SAMPLE_RATE = 16000
SILENCE_THRESHOLD = 500  
MIN_RECORD_SECONDS = 1.0 
MAX_RECORD_SECONDS = 10.0 
SILENCE_DURATION = 1.0   

# --- Load Model ---
print("⏳ Loading Model... (รอสักครู่)")
# ถ้าจะเน้นภาษาอังกฤษอย่างเดียว แนะนำให้ใช้รุ่น "tiny.en" จะแม่นกว่า tiny ปกติ
# แต่ถ้าไม่อยากโหลดใหม่ใช้ "tiny" เหมือนเดิมก็ได้ครับ
model = WhisperModel("tiny", device="cpu", compute_type="int8")
print("✅ Model Loaded! Ready to receive audio.")

async def audio_handler(websocket):
    print(f"🎤 ESP32 Connected from {websocket.remote_address}")
    
    audio_buffer = bytearray()
    silence_start_time = None
    is_speaking = False
    
    try:
        async for message in websocket:
            # รับข้อมูลเสียง
            audio_buffer.extend(message)
            chunk_data = np.frombuffer(message, dtype=np.int16)
            
            # คำนวณความดัง
            if len(chunk_data) > 0:
                amplitude = np.mean(np.abs(chunk_data))
            else:
                amplitude = 0

            # --- Logic การตัดคำ (VAD) ---
            current_time = time.time()
            buffer_duration = len(audio_buffer) / (SAMPLE_RATE * 2) 

            if amplitude > SILENCE_THRESHOLD:
                is_speaking = True
                silence_start_time = None 
            else:
                if silence_start_time is None:
                    silence_start_time = current_time
            
            should_transcribe = False
            
            if is_speaking and silence_start_time and (current_time - silence_start_time > SILENCE_DURATION):
                if buffer_duration >= MIN_RECORD_SECONDS:
                    should_transcribe = True
                    print(f"✂️ Silence Detected ({buffer_duration:.2f}s)")
            
            if buffer_duration >= MAX_RECORD_SECONDS:
                should_transcribe = True
                print("⚠️ Force Cut (Max Limit)")

            # --- เริ่มขั้นตอนการแปล ---
            if should_transcribe:
                # แปลง Int16 -> Float32
                process_buffer = np.frombuffer(audio_buffer, dtype=np.int16).astype(np.float32) / 32768.0
                
                audio_buffer = bytearray()
                is_speaking = False
                silence_start_time = None
                
                start_proc = time.time()
                
                # ---------------------------------------------------------
                # ✅ จุดที่แก้ไข: เปลี่ยน language="th" เป็น "en"
                # ---------------------------------------------------------
                segments, info = model.transcribe(process_buffer, language="en", beam_size=1)
                
                text = " ".join([segment.text for segment in segments]).strip()
                proc_time = time.time() - start_proc

                # กรองคำมั่ว (Hallucination) ที่พบบ่อยในภาษาอังกฤษ
                text_lower = text.lower()
                ignore_words = ["subtitles", "watching", "mbc", "amara", "copyright"]
                
                is_noise = any(word in text_lower for word in ignore_words)

                if text and not is_noise:
                    print(f"✅ Talk: {text} (Time: {proc_time:.2f}s)")
                else:
                    print(f"🔇 (No Speech/Noise)")

    except websockets.exceptions.ConnectionClosed:
        print("🔴 ESP32 Disconnected")
    except Exception as e:
        print(f"❌ Error: {e}")

async def main():
    async with websockets.serve(audio_handler, "0.0.0.0", PORT):
        print(f"🟢 Server Running on ws://0.0.0.0:{PORT}")
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
