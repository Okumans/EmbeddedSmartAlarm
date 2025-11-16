#!/usr/bin/env python3

"""
MQTT Subscriber for Smart Alarm Clock
Subscribes to sensor data topics and displays the values in real-time
"""

import paho.mqtt.client as mqtt
from datetime import datetime
import sys

# MQTT Configuration (matching the ESP32 settings)
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "SmartAlarmClock_Subscriber"

# Topics to subscribe to
TOPICS = [
    "smartalarm/temperature",
    "smartalarm/humidity",
    "smartalarm/pressure",
    "smartalarm/status",
    "smartalarm/#",  # Subscribe to all smartalarm topics
]

# Store latest values
sensor_data = {
    "temperature": None,
    "humidity": None,
    "pressure": None,
    "status": None,
    "last_update": None,
}


def on_connect(client, userdata, flags, rc):
    """Callback when connected to MQTT broker"""
    if rc == 0:
        print(f"✓ Connected to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
        print("=" * 60)

        # Subscribe to all topics
        for topic in TOPICS[:-1]:  # Skip the wildcard topic
            client.subscribe(topic)
            print(f"  Subscribed to: {topic}")

        print("=" * 60)
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
    payload = msg.payload.decode("utf-8")

    # Update sensor data
    if "temperature" in topic:
        sensor_data["temperature"] = payload
        sensor_data["last_update"] = timestamp
        print(f"🌡️  [{timestamp}] Temperature: {payload}°C")

    elif "humidity" in topic:
        sensor_data["humidity"] = payload
        sensor_data["last_update"] = timestamp
        print(f"💧 [{timestamp}] Humidity: {payload}%")

    elif "pressure" in topic:
        sensor_data["pressure"] = payload
        sensor_data["last_update"] = timestamp
        print(f"🌍 [{timestamp}] Pressure: {payload} hPa")

    elif "status" in topic:
        sensor_data["status"] = payload
        print(f"📡 [{timestamp}] Status: {payload}")

    else:
        print(f"📨 [{timestamp}] {topic}: {payload}")

    # Print summary every time we get data
    print_summary()


def print_summary():
    """Print a summary of all current sensor values"""
    print("─" * 60)
    print("Current Sensor Values:")
    print(f"  Temperature: {sensor_data['temperature'] or 'N/A'}°C")
    print(f"  Humidity:    {sensor_data['humidity'] or 'N/A'}%")
    print(f"  Pressure:    {sensor_data['pressure'] or 'N/A'} hPa")
    print(f"  Status:      {sensor_data['status'] or 'N/A'}")
    if sensor_data["last_update"]:
        print(f"  Last Update: {sensor_data['last_update']}")
    print("─" * 60)
    print()


def main():
    """Main function to run the MQTT subscriber"""
    print("\n" + "=" * 60)
    print("  Smart Alarm Clock - MQTT Subscriber")
    print("=" * 60)
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
