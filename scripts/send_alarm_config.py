#!/usr/bin/env python3

"""
Send Alarm Configuration to ESP32
Sends time and alarm list to the Smart Alarm Clock via MQTT
"""

import paho.mqtt.client as mqtt
from datetime import datetime
import sys
import time

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "AlarmConfig_Sender"

# MQTT Topics
TOPIC_TIME = "smartalarm/time"
TOPIC_ALARM_LIST = "smartalarm/alarmlist"


def on_connect(client, userdata, flags, rc):
    """Callback when connected to MQTT broker"""
    if rc == 0:
        print(f"✓ Connected to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
    else:
        print(f"✗ Connection failed with code {rc}")
        sys.exit(1)


def send_current_time(client):
    """Send current system time to ESP32"""
    current_time = datetime.now().strftime("%H:%M:%S")
    
    result = client.publish(TOPIC_TIME, current_time, qos=1)
    if result.rc == mqtt.MQTT_ERR_SUCCESS:
        print(f"✓ Sent current time: {current_time}")
        return True
    else:
        print(f"✗ Failed to send time")
        return False


def send_alarm_list(client, alarms):
    """
    Send alarm list to ESP32
    
    Args:
        client: MQTT client
        alarms: List of alarm times in HH:MM format (e.g., ["07:30", "08:00"])
    """
    if not alarms:
        print("⚠ No alarms to send")
        return False
    
    # Join alarms into CSV format
    alarm_csv = ",".join(alarms)
    
    result = client.publish(TOPIC_ALARM_LIST, alarm_csv, qos=1)
    if result.rc == mqtt.MQTT_ERR_SUCCESS:
        print(f"✓ Sent alarm list: {alarm_csv}")
        print(f"  Total alarms: {len(alarms)}")
        return True
    else:
        print(f"✗ Failed to send alarm list")
        return False


def main():
    """Main function"""
    print("\n" + "=" * 70)
    print("  Smart Alarm Clock - Alarm Configuration Sender")
    print("=" * 70)
    print(f"Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print()

    # Parse command line arguments
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python send_alarm_config.py time                    - Send current time")
        print("  python send_alarm_config.py alarms HH:MM [HH:MM...] - Send alarm list")
        print("  python send_alarm_config.py both HH:MM [HH:MM...]   - Send both time and alarms")
        print("\nExamples:")
        print("  python send_alarm_config.py time")
        print("  python send_alarm_config.py alarms 07:30 08:00 14:30")
        print("  python send_alarm_config.py both 07:30 08:00")
        print()
        return

    command = sys.argv[1].lower()

    # Create MQTT client
    client = mqtt.Client(client_id=MQTT_CLIENT_ID)
    client.on_connect = on_connect

    try:
        # Connect to broker
        print(f"Connecting to {MQTT_BROKER}:{MQTT_PORT}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        time.sleep(1)  # Wait for connection

        # Process command
        if command == "time":
            send_current_time(client)
        
        elif command == "alarms":
            if len(sys.argv) < 3:
                print("✗ Error: Please provide at least one alarm time (HH:MM)")
                return
            
            alarms = sys.argv[2:]
            
            # Validate alarm format
            valid_alarms = []
            for alarm in alarms:
                if len(alarm) == 5 and alarm[2] == ':':
                    try:
                        hours, minutes = alarm.split(':')
                        if 0 <= int(hours) < 24 and 0 <= int(minutes) < 60:
                            valid_alarms.append(alarm)
                        else:
                            print(f"⚠ Invalid alarm time (out of range): {alarm}")
                    except ValueError:
                        print(f"⚠ Invalid alarm format: {alarm}")
                else:
                    print(f"⚠ Invalid alarm format: {alarm}")
            
            if valid_alarms:
                send_alarm_list(client, valid_alarms)
            else:
                print("✗ No valid alarms to send")
        
        elif command == "both":
            if len(sys.argv) < 3:
                print("✗ Error: Please provide at least one alarm time (HH:MM)")
                return
            
            # Send time first
            send_current_time(client)
            time.sleep(0.5)
            
            # Then send alarms
            alarms = sys.argv[2:]
            valid_alarms = []
            for alarm in alarms:
                if len(alarm) == 5 and alarm[2] == ':':
                    try:
                        hours, minutes = alarm.split(':')
                        if 0 <= int(hours) < 24 and 0 <= int(minutes) < 60:
                            valid_alarms.append(alarm)
                    except ValueError:
                        pass
            
            if valid_alarms:
                send_alarm_list(client, valid_alarms)
        
        else:
            print(f"✗ Unknown command: {command}")
            print("   Use: time, alarms, or both")

        # Wait for messages to be sent
        time.sleep(1)

        print("\n✓ Done!")
        print("=" * 70 + "\n")

    except KeyboardInterrupt:
        print("\n\n✓ Cancelled by user")
    except Exception as e:
        print(f"\n✗ Error: {e}")
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
