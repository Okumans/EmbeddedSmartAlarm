from pydub import AudioSegment
import paho.mqtt.client as mqtt
import os
import time
import math
import argparse
import sys

MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883

TOPIC_REQUEST = "esp32/audio_request"
TOPIC_RESPONSE = "esp32/audio_response"
TOPIC_CHUNK = "esp32/audio_chunk"
TOPIC_ACK = "esp32/audio_ack"

CHUNK_SIZE = 4096
free_space_reply = None
current_audio_size = None
last_ack = None


# -------------------------------------------------------------
# MQTT CALLBACK — receives ESP32 free space response
# -------------------------------------------------------------
def on_message(client, userdata, msg):
    global free_space_reply
    global current_audio_size
    global last_ack
    payload = msg.payload.decode()

    # Handle ACKs for chunks
    if msg.topic == TOPIC_ACK:
        # payload expected: "ACK:<chunk_index>"
        if payload.startswith("ACK:"):
            try:
                idx = int(payload.split(":", 1)[1])
                last_ack = idx
                # print acknowledgement
                print(f"[✓] Received ACK for chunk {idx}")
            except Exception:
                pass
        return

    # Handle free space response: "FREE:<freeSpace>:<currentAudioSize>"
    if payload.startswith("FREE:"):
        try:
            parts = payload[5:].split(":")
            free_space_reply = int(parts[0])
            current_audio_size = int(parts[1]) if len(parts) > 1 else 0
            print(f"[✓] ESP32 reports:")
            print(f"    - Free space: {free_space_reply} bytes")
            print(f"    - Current audio file: {current_audio_size} bytes")
        except Exception as e:
            print(f"[WARNING] Failed to parse FREE response: {e}")


# -------------------------------------------------------------
# REQUEST FREE SPACE FROM ESP32
# -------------------------------------------------------------
def request_free_space(client):
    global free_space_reply
    global current_audio_size
    free_space_reply = None
    current_audio_size = None

    print("[+] Requesting free storage from ESP32...")
    client.publish(TOPIC_REQUEST, "REQUEST_FREE_SPACE")

    # Wait for response
    for _ in range(190):  # wait max 3 seconds
        if free_space_reply is not None:
            return free_space_reply, current_audio_size
        time.sleep(0.1)

    print("[ERROR] ESP32 did not respond.")
    return None, None


# -------------------------------------------------------------
# COMPRESS MP3
# -------------------------------------------------------------
def compress_mp3(input_file, output_file, bitrate="32k"):
    """
    Compress MP3 file to specified bitrate.
    If bitrate is None or "0", skip compression and use original file.
    """
    if bitrate is None or bitrate == "0":
        print("[+] No compression - using original file")
        return input_file  # Return input file path (no compression)
    
    print(f"[+] Compressing MP3 to {bitrate} bitrate...")
    audio = AudioSegment.from_mp3(input_file)
    audio.export(output_file, format="mp3", bitrate=bitrate)
    print("[✓] Compression complete.")
    return output_file  # Return compressed file path


# -------------------------------------------------------------
# SEND CHUNKS
# -------------------------------------------------------------
def send_in_chunks(client, file_path):
    global last_ack
    with open(file_path, "rb") as f:
        data = f.read()

    file_size = len(data)
    total_chunks = math.ceil(file_size / CHUNK_SIZE)

    # Tell ESP32 file size
    client.publish(TOPIC_CHUNK, f"START:{file_size}")

    print(f"[+] Sending {total_chunks} chunks...")

    # Send each chunk and wait for ACK before sending next
    for i in range(total_chunks):
        start = i * CHUNK_SIZE
        end = start + CHUNK_SIZE
        chunk = data[start:end]

        header = f"CHUNK:{i}:{total_chunks}:".encode()
        client.publish(TOPIC_CHUNK, header + chunk)

        # wait for ACK for this chunk
        last_ack = None
        print(f"  - Sent chunk {i+1}/{total_chunks}, waiting for ACK...")
        wait_start = time.time()
        ack_timeout = 5.0  # seconds
        acknowledged = False
        while time.time() - wait_start < ack_timeout:
            if last_ack is not None and last_ack == i:
                acknowledged = True
                break
            time.sleep(0.01)

        if not acknowledged:
            print(f"[ERROR] No ACK for chunk {i} (timeout). Aborting upload.")
            return
        else:
            print(f"  - ACK received for chunk {i}")

    client.publish(TOPIC_CHUNK, "END")
    print("[✓] Done sending all chunks.")


# -------------------------------------------------------------
# MAIN
# -------------------------------------------------------------
if __name__ == "__main__":
    # Parse command line arguments
    parser = argparse.ArgumentParser(
        description="Upload MP3 audio file to ESP32 via MQTT",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Upload with default compression (32k bitrate)
  python mqtt_audiochunkupload.py input.mp3
  
  # Upload with custom compression
  python mqtt_audiochunkupload.py input.mp3 --bitrate 64k
  
  # Upload without compression (use original file)
  python mqtt_audiochunkupload.py input.mp3 --bitrate 0
        """
    )
    
    parser.add_argument("input_file", help="Input MP3 file to upload")
    parser.add_argument(
        "-b", "--bitrate",
        default="32k",
        help="Compression bitrate (e.g., 32k, 64k, 128k) or 0 for no compression (default: 32k)"
    )
    parser.add_argument(
        "-o", "--output",
        default="compressed.mp3",
        help="Output file name for compressed file (default: compressed.mp3)"
    )
    
    args = parser.parse_args()
    
    # Check if input file exists
    if not os.path.exists(args.input_file):
        print(f"[ERROR] Input file '{args.input_file}' not found!")
        sys.exit(1)
    
    INPUT = args.input_file
    OUTPUT = args.output
    BITRATE = args.bitrate if args.bitrate != "0" else None
    
    print(f"[INFO] Input file: {INPUT}")
    print(f"[INFO] Original size: {os.path.getsize(INPUT)} bytes")
    
    # Compress MP3 (or skip if bitrate is 0)
    file_to_upload = compress_mp3(INPUT, OUTPUT, BITRATE)

    # Connect to MQTT
    client = mqtt.Client()
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.subscribe(TOPIC_RESPONSE)
    client.subscribe(TOPIC_ACK)
    client.loop_start()

    # 1) Ask ESP32 free space and current audio file size
    free_space, current_audio = request_free_space(client)
    if free_space is None:
        print("[!] Aborting upload.")
        sys.exit(1)

    # 2) Compare with file size
    file_size = os.path.getsize(file_to_upload)

    print(f"\n[INFO] Upload file size: {file_size} bytes")
    
    # Calculate available space (free space + current audio that will be deleted)
    available_space = free_space + (current_audio if current_audio else 0)
    print(f"[INFO] Available space (including current audio): {available_space} bytes")

    if file_size > available_space:
        print("[❌] File too large for ESP32 storage. Upload canceled.")
        print(f"    Required: {file_size} bytes")
        print(f"    Available: {available_space} bytes")
        print(f"    (Free: {free_space} + Current audio: {current_audio})")
        sys.exit(1)
    
    if current_audio and current_audio > 0:
        print(f"[INFO] Note: Current audio ({current_audio} bytes) will be replaced")

    print("[✓] Enough space. Uploading file...")
    send_in_chunks(client, file_to_upload)

    client.loop_stop()
    client.disconnect()
    
    print("\n[✓] Upload complete!")
