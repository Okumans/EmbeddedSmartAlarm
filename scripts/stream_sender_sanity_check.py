#!/usr/bin/env python3
"""
SmartAlarm Audio Stream Sender
Streams audio from file or captures PC audio, encodes to IMA ADPCM, and sends via UDP to ESP32.
Features:
- 7-Byte Header (Seq + Predictor + Index) for stateless decoding
- Precision Timing Loop to prevent clock drift (audio lag)
"""

import pyaudio
import socket
import sys
import time
import argparse
import struct
from pydub import AudioSegment

# Audio configuration
SAMPLE_RATE = 16000
CHANNELS = 1  # Mono
SAMPLE_WIDTH = 2  # 16-bit
FRAME_MS = 60  # Duration of one packet
FRAME_SAMPLES = int(SAMPLE_RATE * FRAME_MS / 1000)
# For IMA ADPCM (4 bits per sample)
FRAME_PAYLOAD_BYTES = (FRAME_SAMPLES + 1) // 2

CHUNK_SIZE = 1024

# ADPCM Tables
STEP_TABLE = [
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    16,
    17,
    19,
    21,
    23,
    25,
    28,
    31,
    34,
    37,
    41,
    45,
    50,
    55,
    60,
    66,
    73,
    80,
    88,
    97,
    107,
    118,
    130,
    143,
    157,
    173,
    190,
    209,
    230,
    253,
    279,
    307,
    337,
    371,
    408,
    449,
    494,
    544,
    598,
    658,
    724,
    796,
    876,
    963,
    1060,
    1166,
    1282,
    1411,
    1552,
    1707,
    1878,
    2066,
    2272,
    2499,
    2749,
    3024,
    3327,
    3660,
    4026,
    4428,
    4871,
    5358,
    5894,
    6484,
    7132,
    7845,
    8630,
    9493,
    10442,
    11487,
    12635,
    13899,
    15289,
    16818,
    18500,
    20350,
    22385,
    24623,
    27086,
    29794,
    32767,
]

INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


class ADPCMEncoder:
    def __init__(self):
        self.predictor = 0
        self.step_index = 0

    def encode_sample(self, sample):
        step = STEP_TABLE[self.step_index]
        diff = sample - self.predictor

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

        if self.predictor > 32767:
            self.predictor = 32767
        elif self.predictor < -32768:
            self.predictor = -32768

        self.step_index += INDEX_TABLE[nibble]
        if self.step_index < 0:
            self.step_index = 0
        elif self.step_index > 88:
            self.step_index = 88

        return nibble & 0x0F


def main():
    parser = argparse.ArgumentParser(description="SmartAlarm Audio Stream Sender")
    parser.add_argument("--host", default="192.168.1.100", help="ESP32 IP address")
    parser.add_argument("--port", type=int, default=12345, help="UDP port")
    parser.add_argument("--file", help="Audio file to stream")
    parser.add_argument("--loop", action="store_true", help="Loop the audio file")
    parser.add_argument("--device", type=int, help="Input device index (Mic mode)")
    args = parser.parse_args()

    print(f"SmartAlarm Audio Stream Sender")
    print(f"Target: {args.host}:{args.port}")
    print(f"Sample Rate: {SAMPLE_RATE} Hz, Mono, 16-bit")
    print(f"Header: 7 Bytes (Seq+Pred+Idx)")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    encoder = ADPCMEncoder()

    if args.file:
        # --- FILE MODE ---
        print(f"Loading audio file: {args.file}")
        try:
            audio = AudioSegment.from_file(args.file)
            audio = audio.set_frame_rate(SAMPLE_RATE)
            audio = audio.set_channels(CHANNELS)
            audio = audio.set_sample_width(SAMPLE_WIDTH)

            print(f"Audio duration: {len(audio) / 1000:.1f} seconds")
            print("\nStreaming audio file... Press Ctrl+C to stop")

            seq = 0
            raw_data = audio.raw_data
            pcm_samples = []
            for i in range(0, len(raw_data), SAMPLE_WIDTH):
                sample = int.from_bytes(
                    raw_data[i : i + SAMPLE_WIDTH], byteorder="little", signed=True
                )
                pcm_samples.append(sample)

            idx = 0

            # Timing variables
            start_time = time.time()
            frames_sent = 0

            while True:
                if idx >= len(pcm_samples):
                    if not args.loop:
                        break
                    idx = 0
                    encoder = ADPCMEncoder()
                    # Reset timing logic on loop
                    start_time = time.time()
                    frames_sent = 0

                frame = pcm_samples[idx : idx + FRAME_SAMPLES]
                if len(frame) < FRAME_SAMPLES:
                    frame.extend([0] * (FRAME_SAMPLES - len(frame)))

                # --- CAPTURE STATE ---
                current_pred = encoder.predictor
                current_idx = encoder.step_index
                # ---------------------

                adpcm_nibbles = bytearray()
                for sample in frame:
                    nibble = encoder.encode_sample(sample)
                    adpcm_nibbles.append(nibble)

                payload = bytearray()
                for j in range(0, len(adpcm_nibbles) - 1, 2):
                    payload.append((adpcm_nibbles[j] << 4) | adpcm_nibbles[j + 1])
                if len(adpcm_nibbles) % 2 == 1:
                    payload.append(adpcm_nibbles[-1] << 4)

                # --- HEADER: Seq(4) + Pred(2) + Idx(1) ---
                # Big Endian (>), Unsigned Int (I), Short (h), Unsigned Char (B)
                header = struct.pack(">IhB", seq, current_pred, current_idx)
                pkt = header + payload

                try:
                    sock.sendto(bytes(pkt), (args.host, args.port))
                except Exception as e:
                    print(f"Send error: {e}")

                seq = (seq + 1) & 0xFFFFFFFF
                idx += FRAME_SAMPLES

                # --- PRECISION TIMING ---
                frames_sent += 1
                # Calculate exactly when the NEXT packet should be sent
                target_time = start_time + (frames_sent * (FRAME_MS * 0.85) / 1000.0)
                sleep_duration = target_time - time.time()

                if sleep_duration > 0:
                    time.sleep(sleep_duration)
                # ------------------------

        except Exception as e:
            print(f"Error loading audio file: {e}")
            return

    else:
        # --- MICROPHONE MODE ---
        audio = pyaudio.PyAudio()
        if args.device is None:
            default_device = audio.get_default_input_device_info()
            device_index = default_device.get("index")
            print(f"Using default device: {default_device.get('name')}")
        else:
            device_index = args.device

        try:
            stream = audio.open(
                format=audio.get_format_from_width(SAMPLE_WIDTH),
                channels=CHANNELS,
                rate=SAMPLE_RATE,
                input=True,
                input_device_index=device_index,
                frames_per_buffer=CHUNK_SIZE,
            )
        except Exception as e:
            print(f"Error opening audio stream: {e}")
            return

        print("\nStreaming from microphone... Press Ctrl+C to stop")

        try:
            seq = 0
            while True:
                # Block until we have enough samples (hardware clocked)
                samples_collected = []
                samples_needed = FRAME_SAMPLES
                while samples_needed > 0:
                    to_read = min(samples_needed, CHUNK_SIZE)
                    data = stream.read(to_read, exception_on_overflow=False)
                    for k in range(0, len(data), SAMPLE_WIDTH):
                        sample = int.from_bytes(
                            data[k : k + SAMPLE_WIDTH], byteorder="little", signed=True
                        )
                        samples_collected.append(sample)
                    samples_needed = FRAME_SAMPLES - len(samples_collected)

                # --- CAPTURE STATE ---
                current_pred = encoder.predictor
                current_idx = encoder.step_index
                # ---------------------

                adpcm_nibbles = bytearray()
                for sample in samples_collected:
                    nibble = encoder.encode_sample(sample)
                    adpcm_nibbles.append(nibble)

                payload = bytearray()
                for j in range(0, len(adpcm_nibbles) - 1, 2):
                    payload.append((adpcm_nibbles[j] << 4) | adpcm_nibbles[j + 1])
                if len(adpcm_nibbles) % 2 == 1:
                    payload.append(adpcm_nibbles[-1] << 4)

                # --- HEADER ---
                header = struct.pack(">IhB", seq, current_pred, current_idx)
                pkt = header + payload

                try:
                    sock.sendto(bytes(pkt), (args.host, args.port))
                except Exception as e:
                    print(f"Send error: {e}")

                seq = (seq + 1) & 0xFFFFFFFF
                # Note: No manual sleep needed for Mic mode because stream.read()
                # is blocked by the audio hardware sample rate.

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
