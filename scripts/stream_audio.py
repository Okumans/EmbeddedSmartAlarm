"""
Audio Streaming Client for SmartAlarmClock ESP32

This script captures audio from your microphone, encodes it to Opus format,
and streams it to the ESP32 Gateway via WebSocket.

Prerequisites:
    pip install pyaudio opuslib websockets

Usage:
    1. Update ESP32_IP with your ESP32's IP address
    2. Ensure ESP32 is running with WebSocket server active
    3. Run: python stream_audio.py
    4. Speak into your microphone to hear audio from ESP32 speaker
"""

import asyncio
import websockets
import pyaudio
import opuslib
import struct
import sys

# --- Configuration ---
# REPLACE THIS with your ESP32 Gateway's IP Address
ESP32_IP = "172.20.10.4"
PORT = 81

# Audio Constants (Must match ESP32 Settings)
SAMPLE_RATE = 48000
CHANNELS = 1
FRAME_SIZE = 960  # 960 samples = 20ms at 48kHz


async def stream_audio():
    uri = f"ws://{ESP32_IP}:{PORT}"
    print(f"Attempting connection to {uri}...")

    # 1. Initialize PortAudio
    p = pyaudio.PyAudio()

    try:
        # Open Microphone Stream
        stream = p.open(
            format=pyaudio.paInt16,
            channels=CHANNELS,
            rate=SAMPLE_RATE,
            input=True,
            frames_per_buffer=FRAME_SIZE,
        )
        print("Microphone initialized.")
    except Exception as e:
        print(f"Error opening audio stream: {e}")
        print("Ensure PortAudio is installed and a microphone is connected.")
        return

    # 2. Initialize Opus Encoder
    # APPLICATION_VOIP optimizes for speech and low latency
    encoder = opuslib.Encoder(SAMPLE_RATE, CHANNELS, opuslib.APPLICATION_VOIP)

    async with websockets.connect(uri) as websocket:
        print(f"✅ Connected to {ESP32_IP}! Streaming... (Press Ctrl+C to stop)")

        try:
            packet_count = 0
            while True:
                # A. Read raw PCM data (exception_on_overflow=False prevents crashing on lag)
                pcm_data = stream.read(FRAME_SIZE, exception_on_overflow=False)

                # B. Encode to Opus
                opus_packet = encoder.encode(pcm_data, FRAME_SIZE)

                # C. Create Frame Header
                # '<H' means Little Endian Unsigned Short (2 bytes)
                # This matches the ESP32's architecture for reading the size
                packet_len = len(opus_packet)
                header = struct.pack("<H", packet_len)

                # D. Combine and Send
                full_message = header + opus_packet
                await websocket.send(full_message)

                # Print dot every 50 packets to show activity
                packet_count += 1
                if packet_count % 50 == 0:
                    print(".", end="", flush=True)

        except KeyboardInterrupt:
            print("\n🛑 Stopping stream...")
        except Exception as e:
            print(f"\n⚠️ Error during streaming: {e}")
        finally:
            print("Cleaning up resources...")
            stream.stop_stream()
            stream.close()
            p.terminate()


if __name__ == "__main__":
    try:
        asyncio.run(stream_audio())
    except KeyboardInterrupt:
        # Handle Ctrl+C gracefully at the top level
        pass
