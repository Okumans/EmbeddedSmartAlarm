#!/usr/bin/env python3
"""
MQTT Audio Download Monitor
Subscribes to esp32/audio_download_cmd and esp32/audio/status
to monitor audio file download commands and responses.
"""

import paho.mqtt.client as mqtt
import time
from datetime import datetime

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "audio_download_monitor"

# Topics
TOPIC_DOWNLOAD_CMD = "esp32/audio_download_cmd"
TOPIC_AUDIO_STATUS = "esp32/audio/status"

def on_connect(client, userdata, flags, rc):
    """Callback when connected to MQTT broker"""
    if rc == 0:
        print(f"[{get_timestamp()}] Connected to MQTT broker: {MQTT_BROKER}")
        
        # Subscribe to both topics
        client.subscribe(TOPIC_DOWNLOAD_CMD)
        print(f"[{get_timestamp()}] Subscribed to: {TOPIC_DOWNLOAD_CMD}")
        
        client.subscribe(TOPIC_AUDIO_STATUS)
        print(f"[{get_timestamp()}] Subscribed to: {TOPIC_AUDIO_STATUS}")
        
        print("\n" + "="*80)
        print("Monitoring audio download commands and status...")
        print("="*80 + "\n")
    else:
        print(f"[{get_timestamp()}] Connection failed with code: {rc}")

def on_disconnect(client, userdata, rc):
    """Callback when disconnected from MQTT broker"""
    if rc != 0:
        print(f"\n[{get_timestamp()}] Unexpected disconnection. Reconnecting...")

def on_message(client, userdata, msg):
    """Callback when a message is received"""
    timestamp = get_timestamp()
    topic = msg.topic
    payload = msg.payload.decode('utf-8', errors='replace')
    
    print(f"\n[{timestamp}] Topic: {topic}")
    print(f"{'─'*80}")
    
    if topic == TOPIC_DOWNLOAD_CMD:
        # Parse download command
        if '|' in payload:
            parts = payload.split('|')
            url = parts[0]
            sound_id = parts[1] if len(parts) > 1 else "Unknown"
            
            print(f"📥 DOWNLOAD COMMAND RECEIVED")
            print(f"   Sound ID: {sound_id}")
            print(f"   URL: {url}")
            
            # Check if presigned URL
            if '?' in url and 'X-Amz' in url:
                print(f"   Type: Presigned URL (MinIO S3)")
            else:
                print(f"   Type: Direct URL")
        else:
            print(f"📥 DOWNLOAD COMMAND (RAW): {payload}")
    
    elif topic == TOPIC_AUDIO_STATUS:
        # Parse status message
        if '|' in payload:
            parts = payload.split('|')
            status = parts[0]
            sound_id = parts[1] if len(parts) > 1 else "Unknown"
            
            if status == "download_success":
                print(f"✅ DOWNLOAD SUCCESS")
                print(f"   Sound ID: {sound_id}")
                print(f"   File saved as: /sound_{sound_id}.mp3")
            elif status == "download_failed":
                print(f"❌ DOWNLOAD FAILED")
                print(f"   Sound ID: {sound_id}")
            else:
                print(f"📢 STATUS: {status}")
                print(f"   Sound ID: {sound_id}")
        else:
            print(f"📢 STATUS: {payload}")
    
    print(f"{'─'*80}")

def get_timestamp():
    """Get formatted timestamp"""
    return datetime.now().strftime("%H:%M:%S")

def main():
    """Main function"""
    print("\n" + "="*80)
    print("ESP32 Audio Download Monitor")
    print("="*80)
    print(f"Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"Client ID: {MQTT_CLIENT_ID}")
    print("="*80 + "\n")
    
    # Create MQTT client (use callback_api_version for paho-mqtt 2.0+)
    client = mqtt.Client(client_id=MQTT_CLIENT_ID, callback_api_version=mqtt.CallbackAPIVersion.VERSION1)
    
    # Set callbacks
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    
    # Connect to broker
    print(f"[{get_timestamp()}] Connecting to MQTT broker...")
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
    except Exception as e:
        print(f"[{get_timestamp()}] Error connecting to broker: {e}")
        return
    
    # Start loop
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print(f"\n[{get_timestamp()}] Stopping monitor...")
        client.disconnect()
        print(f"[{get_timestamp()}] Disconnected. Goodbye!")

if __name__ == "__main__":
    main()
