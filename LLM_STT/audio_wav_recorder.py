#!/usr/bin/env -S uv run
# /// script
# dependencies = [
#   "websockets",
# ]
# ///

"""
Audio WAV Recorder Server
Receives 16-bit PCM audio from ESP32 via WebSocket and saves as WAV files.
"""

import asyncio
import websockets
import wave
import struct
from datetime import datetime
from pathlib import Path
import sys

# Configuration
SAMPLE_RATE = 16000
SAMPLE_WIDTH = 2  # 16-bit = 2 bytes
CHANNELS = 1      # Mono
HOST = "0.0.0.0"
PORT = 4000

# Output directory
RECORDINGS_DIR = Path(__file__).parent / "recordings"
RECORDINGS_DIR.mkdir(exist_ok=True)

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
        """Close WAV file and print statistics."""
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
            print(f"  Bytes received: {self.bytes_received}")
            print(f"  File size: {file_size / 1024:.2f} KB")
            print(f"  Expected size: {(self.frames_received * SAMPLE_WIDTH + 44) / 1024:.2f} KB")
            
            self.wav_file = None

async def handle_client(websocket):
    """Handle WebSocket connection from ESP32."""
    client_addr = websocket.remote_address
    print(f"\n[WebSocket] Client connected from {client_addr[0]}:{client_addr[1]}")
    
    # Create recorder with timestamp filename
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = RECORDINGS_DIR / f"recording_{timestamp}.wav"
    recorder = AudioRecorder(filename)
    recorder.start()
    
    try:
        async for message in websocket:
            # Receive binary audio data
            if isinstance(message, bytes):
                print(f"[WebSocket] Received {len(message)} bytes")
                recorder.write_frames(message)
                
                # Print progress every 16000 frames (~1 second at 16kHz)
                if recorder.frames_received % 16000 == 0:
                    duration = recorder.frames_received / SAMPLE_RATE
                    print(f"[Progress] Recording... {duration:.1f}s")
            else:
                print(f"[WebSocket] Received text message: {message}")
                
    except websockets.exceptions.ConnectionClosed:
        print(f"\n[WebSocket] Client disconnected")
    except Exception as e:
        print(f"\n[Error] {type(e).__name__}: {e}")
    finally:
        recorder.stop()

async def main():
    """Start WebSocket server."""
    print("=" * 60)
    print("Audio WAV Recorder Server")
    print("=" * 60)
    print(f"Configuration:")
    print(f"  Host: {HOST}:{PORT}")
    print(f"  Sample Rate: {SAMPLE_RATE} Hz")
    print(f"  Bit Depth: {SAMPLE_WIDTH * 8}-bit")
    print(f"  Channels: {CHANNELS} (Mono)")
    print(f"  Output Directory: {RECORDINGS_DIR}")
    print("=" * 60)
    print(f"\nServer listening on ws://{HOST}:{PORT}")
    print("Waiting for ESP32 connection...\n")
    
    async with websockets.serve(handle_client, HOST, PORT, max_size=None):
        await asyncio.Future()  # Run forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n[Server] Stopped by user")
        sys.exit(0)
