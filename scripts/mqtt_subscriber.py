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

# Topics to subscribe to
TOPICS = [
    # Gateway (local) sensor topics
    "smartalarm/gateway/temperature",
    "smartalarm/gateway/humidity",
    "smartalarm/gateway/pressure",
    "smartalarm/gateway/status",
    # Remote sensor topics (via ESP-NOW)
    "smartalarm/sensor/temperature",
    "smartalarm/sensor/humidity",
    "smartalarm/sensor/pressure",
    "smartalarm/sensor/uvindex",
    "smartalarm/sensor/battery",
    "smartalarm/sensor/status",
]

# Store latest values
gateway_data = {
    "temperature": None,
    "humidity": None,
    "pressure": None,
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
    payload = msg.payload.decode("utf-8")

    # Determine if it's gateway or remote sensor data
    is_gateway = "gateway" in topic
    is_remote = "sensor" in topic and "gateway" not in topic
    
    data_dict = gateway_data if is_gateway else remote_data
    source = "Gateway" if is_gateway else "Remote"

    # Update sensor data based on topic
    if "temperature" in topic:
        data_dict["temperature"] = payload
        data_dict["last_update"] = timestamp
        print(f"🌡️  [{timestamp}] {source} Temperature: {payload}°C")

    elif "humidity" in topic:
        data_dict["humidity"] = payload
        data_dict["last_update"] = timestamp
        print(f"💧 [{timestamp}] {source} Humidity: {payload}%")

    elif "pressure" in topic:
        data_dict["pressure"] = payload
        data_dict["last_update"] = timestamp
        print(f"🌍 [{timestamp}] {source} Pressure: {payload} hPa")
    
    elif "uvindex" in topic:
        remote_data["uvindex"] = payload
        remote_data["last_update"] = timestamp
        print(f"☀️  [{timestamp}] Remote UV Index: {payload}")
    
    elif "battery" in topic:
        remote_data["battery"] = payload
        remote_data["last_update"] = timestamp
        print(f"🔋 [{timestamp}] Remote Battery: {payload}%")

    elif "status" in topic:
        data_dict["status"] = payload
        print(f"📡 [{timestamp}] {source} Status: {payload}")

    else:
        print(f"📨 [{timestamp}] {topic}: {payload}")

    # Print summary every time we get data
    print_summary()


def print_summary():
    """Print a summary of all current sensor values"""
    print("─" * 70)
    print("Current Sensor Values:")
    print()
    print("  📍 Gateway (Local Sensors):")
    print(f"    Temperature: {gateway_data['temperature'] or 'N/A'}°C")
    print(f"    Humidity:    {gateway_data['humidity'] or 'N/A'}%")
    print(f"    Pressure:    {gateway_data['pressure'] or 'N/A'} hPa")
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
