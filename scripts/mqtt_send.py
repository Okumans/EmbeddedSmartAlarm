#!/usr/bin/env python3
"""
Quick MQTT Audio Test
Send quick commands to test audio playback
"""

import paho.mqtt.client as mqtt
import paho.mqtt.publish as publish
from urllib.parse import urlparse
import sys
import time

# Broker as WebSocket URL (use HiveMQ websockets endpoint)
# Note: public HiveMQ websockets listener is at ws://broker.hivemq.com:8000/mqtt
BROKER_URL = "broker.hivemq.com"
PORT = 1883               

# Parse broker URL into host, port and ws path
BROKER = BROKER_URL  # Direct hostname, no parsing needed
WS_PATH = "/mqtt"
# TRANSPORT = "websockets" if _parsed.scheme in ("ws", "wss") else "tcp"

# Global variables for file listing and alarm list
file_list_received = None
status_received = None
alarm_list_received = None
alarm_status_received = None

def on_message(client, userdata, msg):
    """Handle incoming MQTT messages"""
    global file_list_received, status_received, alarm_list_received, alarm_status_received
    
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
    
    elif msg.topic == "smartalarm/alarms":
        alarm_list_received = payload
        print(f"\n⏰ Current alarms: {payload}")
        
        if payload.strip():
            alarms = payload.split(',')
            print(f"\n📋 Configured alarms ({len(alarms)}):")
            for i, alarm_time in enumerate(alarms, 1):
                print(f"  {i}. {alarm_time}")
        else:
            print("⏰ No alarms configured")
    
    elif msg.topic == "smartalarm/alarmlist/parsed":
        alarm_list_received = payload
        alarm_status_received = payload
        if payload.strip():
            alarms = payload.split(',')
            print(f"✓ Alarm list updated: {len(alarms)} alarm(s) configured")
        else:
            print("✓ Alarm list cleared")
    
    elif msg.topic == "smartalarm/alarmlist/status":
        alarm_status_received = payload
        if payload == "ok":
            print("✓ Alarm list update acknowledged")
            
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
        # Create client with callback API version for paho-mqtt 2.0+
        client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION1)
        # If using websockets ensure websocket path is set before connect
        # if TRANSPORT == "websockets":
        #     client.ws_set_options(path=WS_PATH)

        client.connect(BROKER, PORT, 60)
        client.loop_start()
        
        # Publish with QoS 1 for reliable delivery
        result = client.publish(topic, message, qos=1)
        
        # Wait for the message to be sent
        result.wait_for_publish(timeout=2.0)
        
        client.loop_stop()
        client.disconnect()
        print(f"✓ Sent: [{topic}] {message}")
    except Exception as e:
        print(f"✗ Error sending to MQTT ({BROKER_URL}): {e}")

def list_files():
    """Send list command and wait for response"""
    global file_list_received, status_received
    
    # Reset globals
    file_list_received = None
    status_received = None
    
    # Create MQTT client for receiving with transport chosen from URL
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION1)
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
        print(f"✗ Error connecting to MQTT ({BROKER_URL}): {e}")

def set_alarms(alarm_times):
    """Set alarm list on ESP32"""
    alarm_csv = ",".join(alarm_times)
    send("smartalarm/alarmlist", alarm_csv)
    print(f"✓ Sent alarm list: {alarm_csv}")

def get_alarms():
    """Get current alarm list from ESP32"""
    global alarm_list_received, status_received
    
    # Reset globals
    alarm_list_received = None
    status_received = None
    
    # Create MQTT client for receiving
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION1)
    client.on_message = on_message
    
    try:
        client.connect(BROKER, PORT, 60)
        client.subscribe("smartalarm/alarms")
        client.subscribe("smartalarm/status")
        client.loop_start()
        
        # Request current alarms
        print("Requesting current alarm list...")
        send("smartalarm/commands", "list_alarms")
        
        # Wait for response
        timeout = 3.0
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            if alarm_list_received is not None or (status_received and status_received == "no_alarms"):
                break
            time.sleep(0.1)
        
        client.loop_stop()
        client.disconnect()
        
        if alarm_list_received is None and status_received != "no_alarms":
            print("⏰ Timeout waiting for alarm list response")
            
    except Exception as e:
        print(f"✗ Error connecting to MQTT ({BROKER_URL}): {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python3 mqtt_send.py play <filename>      - Play audio")
        print("  python3 mqtt_send.py stop                  - Stop audio")
        print("  python3 mqtt_send.py volume <0.0-1.0>      - Set volume")
        print("  python3 mqtt_send.py list                  - List files")
        print("  python3 mqtt_send.py alarms                - Get current alarms")
        print("  python3 mqtt_send.py setalarms HH:MM ...   - Set alarm times")
        print("\nExamples:")
        print("  python3 mqtt_send.py play /alarm1.mp3")
        print("  python3 mqtt_send.py stop")
        print("  python3 mqtt_send.py volume 0.5")
        print("  python3 mqtt_send.py list")
        print("  python3 mqtt_send.py alarms")
        print("  python3 mqtt_send.py setalarms 07:30 08:00 14:30")
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
        
    elif cmd == "alarms":
        get_alarms()
    
    elif cmd == "setalarms" and len(sys.argv) >= 3:
        alarm_times = sys.argv[2:]
        # Validate format
        valid_alarms = []
        for alarm in alarm_times:
            if len(alarm) == 5 and alarm[2] == ':':
                try:
                    h, m = alarm.split(':')
                    if 0 <= int(h) < 24 and 0 <= int(m) < 60:
                        valid_alarms.append(alarm)
                    else:
                        print(f"⚠ Invalid time (out of range): {alarm}")
                except ValueError:
                    print(f"⚠ Invalid format: {alarm}")
            else:
                print(f"⚠ Invalid format (use HH:MM): {alarm}")
        
        if valid_alarms:
            set_alarms(valid_alarms)
        else:
            print("✗ No valid alarm times provided")
        
    else:
        print(f"Unknown command: {cmd}")
