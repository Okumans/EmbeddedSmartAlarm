#!/usr/bin/env python3
"""
SmartAlarm Audio Stream Sender
Streams audio from file or captures PC audio, encodes to IMA ADPCM, and sends via UDP to ESP32
"""

import pyaudio
import socket
import audioop
import sys
import time
import argparse
from pydub import AudioSegment

# Audio configuration
SAMPLE_RATE = 16000
CHANNELS = 1  # Mono
SAMPLE_WIDTH = 2  # 16-bit
CHUNK_SIZE = 1024  # Audio frames per buffer
UDP_CHUNK_SIZE = 512  # Max UDP payload size

# ADPCM step table
STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
]

# ADPCM index table
INDEX_TABLE = [
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
]

class ADPCMEncoder:
    def __init__(self):
        self.predictor = 0
        self.step_index = 0

    def encode_sample(self, sample):
        """Encode a 16-bit PCM sample to 4-bit ADPCM"""
        step = STEP_TABLE[self.step_index]
        diff = sample - self.predictor

        # Calculate ADPCM nibble
        nibble = 0
        if diff < 0:
            nibble = 8
            diff = -diff

        temp_step = step
        if diff >= temp_step:
            nibble |= 4
            diff -= temp_step
        temp_step >>= 1
        if diff >= temp_step:
            nibble |= 2
            diff -= temp_step
        temp_step >>= 1
        if diff >= temp_step:
            nibble |= 1
            diff -= temp_step

        # Update predictor
        delta = step >> 3
        if nibble & 4:
            delta += step
        if nibble & 2:
            delta += step >> 1
        if nibble & 1:
            delta += step >> 2

        if nibble & 8:
            self.predictor -= delta
        else:
            self.predictor += delta

        # Clamp predictor
        if self.predictor > 32767:
            self.predictor = 32767
        elif self.predictor < -32768:
            self.predictor = -32768

        # Update step index
        self.step_index += INDEX_TABLE[nibble]
        if self.step_index < 0:
            self.step_index = 0
        elif self.step_index > 88:
            self.step_index = 88

        return nibble & 0x0F

def main():
    parser = argparse.ArgumentParser(description='SmartAlarm Audio Stream Sender')
    parser.add_argument('--host', default='192.168.1.100', help='ESP32 IP address')
    parser.add_argument('--port', type=int, default=12345, help='UDP port')
    parser.add_argument('--file', help='Audio file to stream (instead of microphone)')
    parser.add_argument('--loop', action='store_true', help='Loop the audio file continuously')
    parser.add_argument('--device', type=int, help='Input device index (for microphone mode)')
    args = parser.parse_args()

    print(f"SmartAlarm Audio Stream Sender")
    print(f"Target: {args.host}:{args.port}")
    print(f"Sample Rate: {SAMPLE_RATE} Hz, Mono, 16-bit")

    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Initialize ADPCM encoder
    encoder = ADPCMEncoder()

    if args.file:
        # File streaming mode
        print(f"Loading audio file: {args.file}")
        try:
            # Load audio file with pydub
            audio = AudioSegment.from_file(args.file)
            
            # Convert to required format
            audio = audio.set_frame_rate(SAMPLE_RATE)
            audio = audio.set_channels(CHANNELS)
            audio = audio.set_sample_width(SAMPLE_WIDTH)
            
            print(f"Audio duration: {len(audio) / 1000:.1f} seconds")
            print(f"Looping: {'Yes' if args.loop else 'No'}")
            
            print("\nStreaming audio file... Press Ctrl+C to stop")
            
            buffer = bytearray()
            while True:
                # Get raw audio data
                raw_data = audio.raw_data
                
                # Convert to 16-bit samples
                pcm_samples = []
                for i in range(0, len(raw_data), SAMPLE_WIDTH):
                    sample = int.from_bytes(raw_data[i:i+SAMPLE_WIDTH], byteorder='little', signed=True)
                    pcm_samples.append(sample)
                
                # Process in chunks
                for i in range(0, len(pcm_samples), CHUNK_SIZE):
                    chunk_samples = pcm_samples[i:i+CHUNK_SIZE]
                    
                    # Encode to ADPCM
                    adpcm_buffer = bytearray()
                    for sample in chunk_samples:
                        nibble = encoder.encode_sample(sample)
                        adpcm_buffer.append(nibble)
                    
                    # Pack two nibbles into bytes
                    packed_buffer = bytearray()
                    for j in range(0, len(adpcm_buffer) - 1, 2):
                        byte_val = (adpcm_buffer[j] << 4) | adpcm_buffer[j + 1]
                        packed_buffer.append(byte_val)
                    
                    # If odd number of nibbles, pad with 0
                    if len(adpcm_buffer) % 2 == 1:
                        byte_val = adpcm_buffer[-1] << 4
                        packed_buffer.append(byte_val)
                    
                    buffer.extend(packed_buffer)
                    
                    # Send in chunks of UDP_CHUNK_SIZE
                    while len(buffer) >= UDP_CHUNK_SIZE:
                        chunk = buffer[:UDP_CHUNK_SIZE]
                        sock.sendto(bytes(chunk), (args.host, args.port))
                        buffer = buffer[UDP_CHUNK_SIZE:]
                    
                    # Small delay to match real-time playback
                    time.sleep(len(chunk_samples) / SAMPLE_RATE)
                
                if not args.loop:
                    break
                    
                # Reset ADPCM encoder for looping
                encoder = ADPCMEncoder()
            
        except Exception as e:
            print(f"Error loading audio file: {e}")
            return
            
    else:
        # Microphone streaming mode
        # Initialize PyAudio
        audio = pyaudio.PyAudio()

        # List devices if requested
        if args.device is None:
            print("\nAvailable input devices:")
            for i in range(audio.get_device_count()):
                info = audio.get_device_info_by_index(i)
                if info.get('maxInputChannels') > 0:
                    print(f"  {i}: {info.get('name')}")

            default_device = audio.get_default_input_device_info()
            print(f"\nUsing default device: {default_device.get('name')}")
            device_index = default_device.get('index')
        else:
            device_index = args.device

        # Open audio stream
        try:
            stream = audio.open(
                format=audio.get_format_from_width(SAMPLE_WIDTH),
                channels=CHANNELS,
                rate=SAMPLE_RATE,
                input=True,
                input_device_index=device_index,
                frames_per_buffer=CHUNK_SIZE
            )
        except Exception as e:
            print(f"Error opening audio stream: {e}")
            return

        print("\nStreaming from microphone... Press Ctrl+C to stop")

        try:
            buffer = bytearray()
            while True:
                # Read audio data
                data = stream.read(CHUNK_SIZE, exception_on_overflow=False)

                # Convert to 16-bit samples
                samples = audioop.byteswap(data, SAMPLE_WIDTH)  # Convert to little-endian if needed
                pcm_samples = []
                for i in range(0, len(samples), SAMPLE_WIDTH):
                    sample = int.from_bytes(samples[i:i+SAMPLE_WIDTH], byteorder='little', signed=True)
                    pcm_samples.append(sample)

                # Encode to ADPCM
                adpcm_buffer = bytearray()
                for sample in pcm_samples:
                    nibble = encoder.encode_sample(sample)
                    adpcm_buffer.append(nibble)

                # Pack two nibbles into bytes
                packed_buffer = bytearray()
                for i in range(0, len(adpcm_buffer) - 1, 2):
                    byte_val = (adpcm_buffer[i] << 4) | adpcm_buffer[i + 1]
                    packed_buffer.append(byte_val)

                # If odd number of nibbles, pad with 0
                if len(adpcm_buffer) % 2 == 1:
                    byte_val = adpcm_buffer[-1] << 4
                    packed_buffer.append(byte_val)

                buffer.extend(packed_buffer)

                # Send in chunks of UDP_CHUNK_SIZE
                while len(buffer) >= UDP_CHUNK_SIZE:
                    chunk = buffer[:UDP_CHUNK_SIZE]
                    sock.sendto(bytes(chunk), (args.host, args.port))
                    buffer = buffer[UDP_CHUNK_SIZE:]

                # Small delay to prevent overwhelming the network
                time.sleep(0.01)

        except KeyboardInterrupt:
            print("\nStopping stream...")

        finally:
            stream.stop_stream()
            stream.close()
            audio.terminate()

    sock.close()
    print("Stream stopped")

if __name__ == "__main__":
    main()