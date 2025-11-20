#!/usr/bin/env python3
"""
Quick MQTT Audio Test
Send quick commands to test audio playback
"""

import paho.mqtt.client as mqtt
import paho.mqtt.publish as publish
import sys
import time

BROKER = "broker.hivemq.com"
PORT = 1883

# Global variables for file listing
file_list_received = None
status_received = None

def on_message(client, userdata, msg):
    """Handle incoming MQTT messages"""
    global file_list_received, status_received
    
    payload = msg.payload.decode()
    
    if msg.topic == "smartalarm/files":
        file_list_received = payload
        print(f"\n📁 Files received: {payload}")
        
        if payload.strip():
            files = payload.split(',')
            print(f"\n🎵 Available audio files ({len(files)}):")
            for i, filename in enumerate(files, 1):
                print(f"  {i}. {filename}")
        else:
            print("📁 No audio files found on SD card")
            
    elif msg.topic == "smartalarm/status":
        status_received = payload
        if payload == "files_listed":
            print("✓ File list request acknowledged")
        elif payload == "no_files":
            print("📁 No audio files available")
        else:
            print(f"Status: {payload}")

def send(topic, message):
    """Send a single MQTT message"""
    try:
        publish.single(topic, message, hostname=BROKER, port=PORT)
        print(f"✓ Sent: [{topic}] {message}")
    except Exception as e:
        print(f"✗ Error: {e}")

def list_files():
    """Send list command and wait for response"""
    global file_list_received, status_received
    
    # Reset globals
    file_list_received = None
    status_received = None
    
    # Create MQTT client for receiving
    client = mqtt.Client()
    client.on_message = on_message
    
    try:
        client.connect(BROKER, PORT, 60)
        client.subscribe("smartalarm/files")
        client.subscribe("smartalarm/status")
        client.loop_start()
        
        # Send the list command
        send("smartalarm/commands", "list_files")
        
        # Wait for response
        timeout = 5.0
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            if file_list_received is not None or (status_received and status_received == "no_files"):
                break
            time.sleep(0.1)
        
        client.loop_stop()
        client.disconnect()
        
        if file_list_received is None and status_received != "no_files":
            print("⏰ Timeout waiting for file list response")
            
    except Exception as e:
        print(f"✗ Error connecting to MQTT: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python3 mqtt_send.py play <filename>   - Play audio")
        print("  python3 mqtt_send.py stop               - Stop audio")
        print("  python3 mqtt_send.py volume <0.0-1.0>   - Set volume")
        print("  python3 mqtt_send.py list               - List files")
        print("\nExamples:")
        print("  python3 mqtt_send.py play /alarm1.mp3")
        print("  python3 mqtt_send.py stop")
        print("  python3 mqtt_send.py volume 0.5")
        sys.exit(1)
    
    cmd = sys.argv[1].lower()
    
    if cmd == "play" and len(sys.argv) >= 3:
        filename = sys.argv[2]
        if not filename.startswith('/'):
            filename = '/' + filename
        send("smartalarm/play_audio", filename)
        
    elif cmd == "stop":
        send("smartalarm/commands", "stop_audio")
        
    elif cmd == "volume" and len(sys.argv) >= 3:
        vol = sys.argv[2]
        send("smartalarm/commands", f"volume={vol}")
        
    elif cmd == "list":
        list_files()
        print("Check Serial Monitor for output")
        
    else:
        print(f"Unknown command: {cmd}")
