#!/usr/bin/env python3

"""
MQTT Subscriber for Smart Alarm Clock
Subscribes to sensor data topics and displays the values in real-time
Supports both Gateway (local) sensors and Remote sensor node via ESP-NOW
"""

import paho.mqtt.client as mqtt
from datetime import datetime
import sys
import time
import json
from pathlib import Path

# MQTT Configuration (matching the ESP32 settings)
# Use TCP broker for desktop scripts so gateway (PubSubClient) can also
# connect to the same broker over TCP (1883).
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "SmartAlarmClock"

# Only subscribe to alarm list
TOPICS = ["smartalarm/alarmlist", "smartalarm/#", "esp32/#"]

# Alarm storage file path
ALARM_DATA_FILE = Path(__file__).parent / "alarm_data.json"

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

# Alarm data storage - in-memory array with persistence
alarm_data = {
    "alarms": [],  # List of alarm times in HH:MM format
    "last_update": None,
    "last_payload": None,
}

def load_alarm_data():
    """Load alarm data from JSON file if it exists"""
    try:
        if ALARM_DATA_FILE.exists():
            with open(ALARM_DATA_FILE, 'r') as f:
                data = json.load(f)
                alarm_data["alarms"] = data.get("alarms", [])
                alarm_data["last_update"] = data.get("last_update")
                alarm_data["last_payload"] = data.get("last_payload")
                print(f"✓ Loaded {len(alarm_data['alarms'])} alarm(s) from {ALARM_DATA_FILE}")
    except Exception as e:
        print(f"⚠ Could not load alarm data: {e}")


def save_alarm_data():
    """Save alarm data to JSON file"""
    try:
        with open(ALARM_DATA_FILE, 'w') as f:
            json.dump(alarm_data, f, indent=2)
        print(f"✓ Saved {len(alarm_data['alarms'])} alarm(s) to {ALARM_DATA_FILE}")
    except Exception as e:
        print(f"✗ Could not save alarm data: {e}")


def parse_alarm_payload(payload):
    """
    Parse alarm payload in CSV format (e.g., "07:30,08:00,14:30")
    Returns list of alarm times in HH:MM format
    """
    if not payload or not payload.strip():
        return []
    
    alarms = []
    for alarm_time in payload.split(','):
        alarm_time = alarm_time.strip()
        # Basic validation: expect HH:MM format
        if len(alarm_time) == 5 and alarm_time[2] == ':':
            try:
                # Validate it's actually a valid time
                hours, minutes = alarm_time.split(':')
                if 0 <= int(hours) < 24 and 0 <= int(minutes) < 60:
                    alarms.append(alarm_time)
            except ValueError:
                pass  # Skip invalid entries
    
    return alarms


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
    is_alarm = "alarm" in topic

    source = "Gateway" if is_gateway else "Remote" if is_remote else "System"

    # Special handling for alarm triggered notifications
    if topic == "smartalarm/alarm/triggered" or topic.endswith("/alarm/triggered"):
        print("\n" + "🔔" * 30)
        print(f"⏰ ALARM TRIGGERED at {timestamp}")
        print(f"Message: {payload}")
        print("🔔" * 30 + "\n")
        return

    # Special handling for alarm errors
    if topic == "smartalarm/alarm/error" or topic.endswith("/alarm/error"):
        print("\n" + "⚠️" * 30)
        print(f"❌ ALARM ERROR at {timestamp}")
        print(f"Message: {payload}")
        print("⚠️" * 30 + "\n")
        return

    # Update data dictionaries based on topic
    if is_gateway:
        data_dict = gateway_data
        if "temperature" in topic:
            data_dict["temperature"] = payload
        elif "humidity" in topic:
            data_dict["humidity"] = payload
        elif "status" in topic:
            data_dict["status"] = payload
        else:
            # Generic gateway topic
            pass
        data_dict["last_update"] = timestamp

    elif is_remote:
        data_dict = remote_data
        if "temperature" in topic:
            data_dict["temperature"] = payload
        elif "humidity" in topic:
            data_dict["humidity"] = payload
        elif "pressure" in topic:
            data_dict["pressure"] = payload
        elif "uvindex" in topic:
            data_dict["uvindex"] = payload
        elif "battery" in topic:
            data_dict["battery"] = payload
        elif "status" in topic:
            data_dict["status"] = payload
        else:
            pass
        data_dict["last_update"] = timestamp

    elif is_audio:
        audio_data["last_update"] = timestamp
        if "status" in topic:
            audio_data["status"] = payload
        elif "stream" in topic:
            audio_data["stream_status"] = payload
        else:
            audio_data["last_command"] = f"{topic}: {payload}"

    # Special handling for alarm list messages (print full details immediately)
    if topic == "smartalarm/alarmlist" or topic.endswith("/alarmlist"):
        print("\n" + "=" * 60)
        print(f"📨 Alarm list received at {timestamp}")
        print(f"Topic: {topic}")
        print(f"Payload: {payload}")
        
        # Parse and store alarm data
        parsed_alarms = parse_alarm_payload(payload)
        alarm_data["alarms"] = parsed_alarms
        alarm_data["last_update"] = timestamp
        alarm_data["last_payload"] = payload
        
        if parsed_alarms:
            print(f"Alarm Count: {len(parsed_alarms)}")
            print(f"Alarm Times: {', '.join(parsed_alarms)}")
            # Save to file for persistence
            save_alarm_data()
        else:
            print("No active alarms")
        print("" + "=" * 60 + "\n")

    # Rate-limit summary printing to avoid flooding the console
    global _last_summary_print
    try:
        last = _last_summary_print
    except NameError:
        last = 0
    SUMMARY_INTERVAL = 5.0  # seconds
    if time.time() - last >= SUMMARY_INTERVAL:
        print_summary()
        _last_summary_print = time.time()


def print_summary():
    """Print a summary of all current sensor values"""
    print("─" * 70)
    print("Current System State:")
    print()
    
    # Alarm information
    print("  ⏰ Alarm System:")
    if alarm_data["alarms"]:
        print(f"    Active Alarms: {len(alarm_data['alarms'])}")
        print(f"    Times:         {', '.join(alarm_data['alarms'])}")
        if alarm_data["last_update"]:
            print(f"    Last Update:   {alarm_data['last_update']}")
    else:
        print("    No active alarms")
    
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

    # Load previously saved alarm data
    load_alarm_data()
    print()

    # Create MQTT client (use client_id keyword for clarity)
    client = mqtt.Client(client_id=MQTT_CLIENT_ID)

    # Set callbacks
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    try:
        # Attach additional callbacks used by the alarm listener
        client.on_subscribe = lambda c, u, mid, granted_qos: print(f"✓ Subscription confirmed (QoS: {granted_qos[0]})")

        # Connect to broker (TCP)
        print(f"Connecting to {MQTT_BROKER}:{MQTT_PORT}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)

        # Start the loop (blocking)
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
