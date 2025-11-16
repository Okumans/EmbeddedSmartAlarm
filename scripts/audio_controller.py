#!/usr/bin/env python3
"""
Smart Alarm Clock - MQTT Audio Controller
Control audio playback on ESP32 via MQTT
"""

import paho.mqtt.client as mqtt
import sys
import time

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "SmartAlarmClock_Controller"


def on_connect(client, userdata, flags, rc):
    """Callback when connected to MQTT broker"""
    if rc == 0:
        print(f"✓ Connected to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
        # Subscribe to all smartalarm topics to see responses
        client.subscribe("smartalarm/#")
    else:
        print(f"✗ Connection failed with code {rc}")


def on_message(client, userdata, msg):
    """Callback when a message is received"""
    print(f"[{msg.topic}] {msg.payload.decode('utf-8')}")


def play_audio(client, filename):
    """Play an audio file from SPIFFS"""
    if not filename.startswith("/"):
        filename = "/" + filename

    print(f"\n🎵 Playing audio: {filename}")
    client.publish("smartalarm/play_audio", filename)


def send_command(client, command):
    """Send a command to the ESP32"""
    print(f"\n📤 Sending command: {command}")
    client.publish("smartalarm/commands", command)


def set_volume(client, volume):
    """Set audio volume (0.0 to 1.0)"""
    volume = max(0.0, min(1.0, float(volume)))
    print(f"\n🔊 Setting volume to: {volume}")
    client.publish("smartalarm/commands", f"volume={volume}")


def print_menu():
    """Print the control menu"""
    print("\n" + "=" * 60)
    print("  Smart Alarm Clock - Audio Controller")
    print("=" * 60)
    print("\nCommands:")
    print("  play <filename>    - Play audio file (e.g., play alarm1.mp3)")
    print("  stop               - Stop audio playback")
    print("  list               - List available audio files")
    print("  volume <0.0-1.0>   - Set volume (e.g., volume 0.5)")
    print("  test               - Test audio system")
    print("  help               - Show this help")
    print("  quit / exit        - Exit controller")
    print("\nExamples:")
    print("  play /alarm1.mp3")
    print("  play wake_up.wav")
    print("  volume 0.7")
    print("  stop")
    print("=" * 60)


def interactive_mode(client):
    """Run interactive command mode"""
    print_menu()

    while True:
        try:
            command = input("\n> ").strip()

            if not command:
                continue

            parts = command.split(maxsplit=1)
            cmd = parts[0].lower()
            arg = parts[1] if len(parts) > 1 else None

            if cmd in ["quit", "exit", "q"]:
                print("\n✓ Exiting...")
                break

            elif cmd == "help" or cmd == "?":
                print_menu()

            elif cmd == "play":
                if arg:
                    play_audio(client, arg)
                else:
                    print("❌ Usage: play <filename>")

            elif cmd == "stop":
                send_command(client, "stop_audio")

            elif cmd == "list":
                send_command(client, "list_files")
                print("📝 Check Serial Monitor for file list")

            elif cmd == "volume" or cmd == "vol":
                if arg:
                    try:
                        set_volume(client, arg)
                    except ValueError:
                        print("❌ Invalid volume. Use a number between 0.0 and 1.0")
                else:
                    print("❌ Usage: volume <0.0-1.0>")

            elif cmd == "test":
                print("\n🧪 Testing audio system...")
                send_command(client, "list_files")

            else:
                print(f"❌ Unknown command: {cmd}")
                print("   Type 'help' for available commands")

        except KeyboardInterrupt:
            print("\n\n✓ Exiting...")
            break
        except Exception as e:
            print(f"❌ Error: {e}")


def command_mode(client, args):
    """Run single command mode"""
    if len(args) < 1:
        print("❌ No command provided")
        print_menu()
        return

    cmd = args[0].lower()

    if cmd == "play" and len(args) >= 2:
        play_audio(client, args[1])
        time.sleep(2)  # Wait for command to be sent

    elif cmd == "stop":
        send_command(client, "stop_audio")
        time.sleep(1)

    elif cmd == "list":
        send_command(client, "list_files")
        print("📝 Check Serial Monitor for file list")
        time.sleep(1)

    elif cmd == "volume" and len(args) >= 2:
        set_volume(client, args[1])
        time.sleep(1)

    else:
        print(f"❌ Invalid command: {' '.join(args)}")
        print_menu()


def main():
    """Main function"""
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, MQTT_CLIENT_ID)
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        # Connect to broker
        print(f"Connecting to {MQTT_BROKER}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()

        # Wait for connection
        time.sleep(1)

        # Check if running with command line arguments
        if len(sys.argv) > 1:
            command_mode(client, sys.argv[1:])
        else:
            interactive_mode(client)

        client.loop_stop()
        client.disconnect()

    except KeyboardInterrupt:
        print("\n\n✓ Shutting down...")
    except Exception as e:
        print(f"\n✗ Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
