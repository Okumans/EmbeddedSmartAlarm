from pydub import AudioSegment
import paho.mqtt.client as mqtt
import os
import time
import math

MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883

TOPIC_REQUEST = "esp32/audio_request"
TOPIC_RESPONSE = "esp32/audio_response"
TOPIC_CHUNK = "esp32/audio_chunk"
TOPIC_ACK = "esp32/audio_ack"

CHUNK_SIZE = 4096
free_space_reply = None
last_ack = None


# -------------------------------------------------------------
# MQTT CALLBACK — receives ESP32 free space response
# -------------------------------------------------------------
def on_message(client, userdata, msg):
    global free_space_reply
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

    if payload.startswith("FREE:"):
        free_space_reply = int(payload[5:])
        print(f"[✓] ESP32 reports free space: {free_space_reply} bytes")


# -------------------------------------------------------------
# REQUEST FREE SPACE FROM ESP32
# -------------------------------------------------------------
def request_free_space(client):
    global free_space_reply
    free_space_reply = None

    print("[+] Requesting free storage from ESP32...")
    client.publish(TOPIC_REQUEST, "REQUEST_FREE_SPACE")

    # Wait for response
    for _ in range(30):  # wait max 3 seconds
        if free_space_reply is not None:
            return free_space_reply
        time.sleep(0.1)

    print("[ERROR] ESP32 did not respond.")
    return None


# -------------------------------------------------------------
# COMPRESS MP3
# -------------------------------------------------------------
def compress_mp3(input_file, output_file, bitrate="32k"):
    print("[+] Compressing MP3...")
    audio = AudioSegment.from_mp3(input_file)
    audio.export(output_file, format="mp3", bitrate=bitrate)
    print("[✓] Compression complete.")


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
    INPUT = "input.mp3"
    OUTPUT = "compressed.mp3"

    compress_mp3(INPUT, OUTPUT)

    # Connect to MQTT
    client = mqtt.Client()
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.subscribe(TOPIC_RESPONSE)
    client.subscribe(TOPIC_ACK)
    client.loop_start()

    # 1) Ask ESP32 free space
    free_space = request_free_space(client)
    if free_space is None:
        print("[!] Aborting upload.")
        exit()

    # 2) Compare with compressed file size
    file_size = os.path.getsize(OUTPUT)

    print(f"[INFO] File size: {file_size} bytes")

    if file_size > free_space:
        print("[❌] File too large for ESP32 storage. Upload canceled.")
        exit()

    print("[✓] Enough space. Uploading file...")
    send_in_chunks(client, OUTPUT)

    client.loop_stop()
    client.disconnect()
