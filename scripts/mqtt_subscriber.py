#!/usr/bin/env python3

"""
MQTT Subscriber for Smart Alarm Clock
Subscribes to sensor data topics and displays the values in real-time
Supports both Gateway (local) sensors and Remote sensor node via ESP-NOW
"""

import paho.mqtt.client as mqtt
from datetime import datetime
import sys

# MQTT Configuration (matching the ESP32 settings)
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "SmartAlarmClock_Subscriber"

# Topics to subscribe to (using wildcards)
TOPICS = [
    "smartalarm/#",
    "esp32/#",
]

# Store latest values
gateway_data = {
    "temperature": None,
    "humidity": None,
    "status": None,
    "last_update": None,
}

remote_data = {
    "temperature": None,
    "humidity": None,
    "pressure": None,
    "uvindex": None,
    "battery": None,
    "status": None,
    "last_update": None,
}

audio_data = {
    "status": None,
    "stream_status": None,
    "last_command": None,
    "last_update": None,
}


def on_connect(client, userdata, flags, rc):
    """Callback when connected to MQTT broker"""
    if rc == 0:
        print(f"✓ Connected to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
        print("=" * 70)

        # Subscribe to all topics
        for topic in TOPICS:
            client.subscribe(topic)
            print(f"  Subscribed to: {topic}")

        print("=" * 70)
        print("\nWaiting for messages... (Press Ctrl+C to exit)\n")
    else:
        print(f"✗ Connection failed with code {rc}")
        sys.exit(1)


def on_disconnect(client, userdata, rc):
    """Callback when disconnected from MQTT broker"""
    if rc != 0:
        print(f"\n✗ Unexpected disconnection (code: {rc}). Reconnecting...")


def on_message(client, userdata, msg):
    """Callback when a message is received"""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    topic = msg.topic
    try:
        payload = msg.payload.decode("utf-8")
    except UnicodeDecodeError:
        payload = f"[Binary data of length {len(msg.payload)}]"


    # Determine if it's gateway or remote sensor data
    is_gateway = "gateway" in topic
    is_remote = "sensor" in topic
    is_audio = "audio" in topic or "stream" in topic or "play" in topic or "esp32" in topic
    
    source = "Gateway" if is_gateway else "Remote" if is_remote else "System"

    # Update data dictionaries based on topic
    if is_gateway:
        data_dict = gateway_data
        if "temperature" in topic:
            data_dict["temperature"] = payload
            print(f"🌡️  [{timestamp}] {source} Temperature: {payload}°C")
        elif "humidity" in topic:
            data_dict["humidity"] = payload
            print(f"💧 [{timestamp}] {source} Humidity: {payload}%")
        elif "status" in topic:
            data_dict["status"] = payload
            print(f"📡 [{timestamp}] {source} Status: {payload}")
        else:
            print(f"📨 [{timestamp}] {topic}: {payload}")
        data_dict["last_update"] = timestamp

    elif is_remote:
        data_dict = remote_data
        if "temperature" in topic:
            data_dict["temperature"] = payload
            print(f"🌡️  [{timestamp}] {source} Temperature: {payload}°C")
        elif "humidity" in topic:
            data_dict["humidity"] = payload
            print(f"💧 [{timestamp}] {source} Humidity: {payload}%")
        elif "pressure" in topic:
            data_dict["pressure"] = payload
            print(f"🌍 [{timestamp}] {source} Pressure: {payload} hPa")
        elif "uvindex" in topic:
            data_dict["uvindex"] = payload
            print(f"☀️  [{timestamp}] {source} UV Index: {payload}")
        elif "battery" in topic:
            data_dict["battery"] = payload
            print(f"🔋 [{timestamp}] {source} Battery: {payload}%")
        elif "status" in topic:
            data_dict["status"] = payload
            print(f"📡 [{timestamp}] {source} Status: {payload}")
        else:
            print(f"📨 [{timestamp}] {topic}: {payload}")
        data_dict["last_update"] = timestamp

    elif is_audio:
        audio_data["last_update"] = timestamp
        if "status" in topic:
            audio_data["status"] = payload
            print(f"🎵 [{timestamp}] Audio Status: {payload}")
        elif "stream" in topic:
            audio_data["stream_status"] = payload
            print(f"🎤 [{timestamp}] Stream Status: {payload}")
        else:
            audio_data["last_command"] = f"{topic}: {payload}"
            print(f"🎧 [{timestamp}] {topic}: {payload}")

    else:
        print(f"📨 [{timestamp}] {topic}: {payload}")

    # Print summary every time we get data
    print_summary()


def print_summary():
    """Print a summary of all current sensor values"""
    print("─" * 70)
    print("Current System State:")
    print()
    print("  📍 Gateway (Local Sensors):")
    print(f"    Temperature: {gateway_data['temperature'] or 'N/A'}°C")
    print(f"    Humidity:    {gateway_data['humidity'] or 'N/A'}%")
    print(f"    Status:      {gateway_data['status'] or 'N/A'}")
    if gateway_data["last_update"]:
        print(f"    Last Update: {gateway_data['last_update']}")
    
    print()
    print("  📡 Remote Sensor (via ESP-NOW):")
    print(f"    Temperature: {remote_data['temperature'] or 'N/A'}°C")
    print(f"    Humidity:    {remote_data['humidity'] or 'N/A'}%")
    print(f"    Pressure:    {remote_data['pressure'] or 'N/A'} hPa")
    print(f"    UV Index:    {remote_data['uvindex'] or 'N/A'}")
    print(f"    Battery:     {remote_data['battery'] or 'N/A'}%")
    print(f"    Status:      {remote_data['status'] or 'N/A'}")
    if remote_data["last_update"]:
        print(f"    Last Update: {remote_data['last_update']}")

    print()
    print("  🔊 Audio System:")
    print(f"    Audio Status:  {audio_data['status'] or 'N/A'}")
    print(f"    Stream Status: {audio_data['stream_status'] or 'N/A'}")
    print(f"    Last Command:  {audio_data['last_command'] or 'N/A'}")
    if audio_data["last_update"]:
        print(f"    Last Update:   {audio_data['last_update']}")
    print("─" * 70)
    print()


def main():
    """Main function to run the MQTT subscriber"""
    print("\n" + "=" * 70)
    print("  Smart Alarm Clock - MQTT Subscriber")
    print("  Monitoring: Gateway + Remote Sensor (ESP-NOW)")
    print("=" * 70)
    print(f"Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"Client ID: {MQTT_CLIENT_ID}")
    print()

    # Create MQTT client
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, MQTT_CLIENT_ID)

    # Set callbacks
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    try:
        # Connect to broker
        print(f"Connecting to {MQTT_BROKER}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)

        # Start the loop
        client.loop_forever()

    except KeyboardInterrupt:
        print("\n\n✓ Shutting down gracefully...")
        client.disconnect()
        print("✓ Disconnected from MQTT broker")
        print("\nFinal sensor readings:")
        print_summary()

    except Exception as e:
        print(f"\n✗ Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
