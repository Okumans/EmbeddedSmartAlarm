#!/usr/bin/env python3

import argparse
import http.server
import socketserver
import threading
import time
import paho.mqtt.client as mqtt
import socket
from uuid import uuid4
import os


class RobustHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        pass  # Silences the default access logs

    def handle_one_request(self):
        try:
            super().handle_one_request()
        except (ConnectionResetError, BrokenPipeError):
            print("[Server] Client disconnected early (Transfer interrupted)")


def get_local_ip():
    """Get the local IP address of this machine."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))  # Connect to Google DNS
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except Exception as e:
        print(f"Error getting local IP: {e}")
        return "127.0.0.1"


class HTTPAudioServer:
    def __init__(self, file_path, sound_id, port=8000):
        self.file_path = file_path
        self.sound_id = sound_id
        self.port = port
        self.server = None
        self.thread = None

    def start(self):
        """Start the HTTP server in a background thread."""
        # Change to the directory containing the file
        os.chdir(os.path.dirname(self.file_path))

        try:
            # 2. Update your server thread creation to use RobustHandler
            with socketserver.TCPServer(("", self.port), RobustHandler) as httpd:
                self.server = httpd
                print(f"Serving {self.file_path} at http://localhost:{self.port}")
                httpd.serve_forever()
        except Exception as e:
            print(f"HTTP Server error: {e}")

    def start_in_thread(self):
        """Start the server in a separate thread."""
        self.thread = threading.Thread(target=self.start, daemon=True)
        self.thread.start()

    def stop(self):
        """Stop the HTTP server."""
        if self.server:
            self.server.shutdown()
            self.server.server_close()

        if self.thread:
            self.thread.join()

def on_mqtt_connect(client, userdata, flags, rc):
    """MQTT connection callback."""
    if rc == 0:
        print("Connected to MQTT broker")
        # Subscribe to status topic
        client.subscribe("esp32/audio/status")
    else:
        print(f"Failed to connect to MQTT broker, return code {rc}")

def on_mqtt_message(client, userdata, msg):
    """MQTT message callback."""
    payload = msg.payload.decode()
    print(f"Received MQTT message: {msg.topic} -> {payload}")

    if msg.topic == "esp32/audio/status":
        if payload == "download_success":
            print("Download successful! Sending play_audio command...")
            time.sleep(2)  # Wait 2 seconds for MQTT receiver to restart
            # Send play_audio command
            play_command = f"/sound_{userdata['sound_id']}.mp3"
            client.publish("smartalarm/play_audio", play_command)
            print(f"Sent play command: {play_command}")
        elif payload == "download_failed":
            print("Download failed!")
            # Could add retry logic here

def main():
    parser = argparse.ArgumentParser(description="HTTP Audio Server for ESP32")
    parser.add_argument("--file", required=True, help="Path to the audio file to serve")
    parser.add_argument("--id", required=False, default=str(uuid4()), help="Sound ID for the audio file (string)")
    parser.add_argument("--port", type=int, default=8000, help="HTTP server port (default: 8000)")
    parser.add_argument("--mqtt-broker", default="broker.hivemq.com", help="MQTT broker IP")
    parser.add_argument("--mqtt-port", type=int, default=1883, help="MQTT broker port")

    args = parser.parse_args()
    
    # Start HTTP server
    server = HTTPAudioServer(args.file, args.id, args.port)
    server.start_in_thread()

    # Give server time to start
    time.sleep(1)

    # Get local IP
    local_ip = get_local_ip()
    filename = os.path.basename(args.file)
    url = f"http://{local_ip}:{args.port}/{filename}"

    print(f"Local IP: {local_ip}")
    print(f"Serving file at: {url}")

    # Setup MQTT
    client = mqtt.Client(userdata={"sound_id": args.id})
    client.on_connect = on_mqtt_connect
    client.on_message = on_mqtt_message

    try:
        client.connect(args.mqtt_broker, args.mqtt_port, 60)
    except Exception as e:
        print(f"Failed to connect to MQTT broker: {e}")
        return

    # Publish download command
    command = f"{url}|{args.id}"
    print(f"Publishing download command: {command}")
    client.publish("esp32/audio_download_cmd", command)

    # Start MQTT loop
    client.loop_forever()

if __name__ == "__main__":
    main()