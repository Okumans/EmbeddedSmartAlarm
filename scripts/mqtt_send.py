#!/usr/bin/env python3
"""
Quick MQTT Audio Test
Send quick commands to test audio playback
"""

import paho.mqtt.publish as publish
import sys

BROKER = "broker.hivemq.com"
PORT = 1883

def send(topic, message):
    """Send a single MQTT message"""
    try:
        publish.single(topic, message, hostname=BROKER, port=PORT)
        print(f"✓ Sent: [{topic}] {message}")
    except Exception as e:
        print(f"✗ Error: {e}")

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
        send("smartalarm/commands", "list_files")
        print("Check Serial Monitor for output")
        
    else:
        print(f"Unknown command: {cmd}")
