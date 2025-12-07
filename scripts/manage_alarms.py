#!/usr/bin/env python3

"""
Manual Alarm Manager
Manually add, remove, or modify alarm times in alarm_data.json
Useful for testing without needing MQTT messages
"""

import json
from pathlib import Path
from datetime import datetime

ALARM_DATA_FILE = Path(__file__).parent / "alarm_data.json"


def load_alarm_data():
    """Load alarm data from JSON file"""
    try:
        if ALARM_DATA_FILE.exists():
            with open(ALARM_DATA_FILE, 'r') as f:
                return json.load(f)
        else:
            return {
                "alarms": [],
                "last_update": None,
                "last_payload": None
            }
    except Exception as e:
        print(f"✗ Error loading alarm data: {e}")
        return None


def save_alarm_data(data):
    """Save alarm data to JSON file"""
    try:
        with open(ALARM_DATA_FILE, 'w') as f:
            json.dump(data, f, indent=2)
        print(f"✓ Saved alarm data to {ALARM_DATA_FILE}")
        return True
    except Exception as e:
        print(f"✗ Error saving alarm data: {e}")
        return False


def validate_time(time_str):
    """Validate HH:MM format"""
    if len(time_str) != 5 or time_str[2] != ':':
        return False
    try:
        hours, minutes = time_str.split(':')
        return 0 <= int(hours) < 24 and 0 <= int(minutes) < 60
    except ValueError:
        return False


def add_alarm(time_str):
    """Add a new alarm"""
    if not validate_time(time_str):
        print(f"✗ Invalid time format: {time_str}. Use HH:MM format (e.g., 07:30)")
        return False
    
    data = load_alarm_data()
    if data is None:
        return False
    
    if time_str in data["alarms"]:
        print(f"⚠ Alarm {time_str} already exists")
        return False
    
    data["alarms"].append(time_str)
    data["alarms"].sort()  # Keep alarms sorted
    data["last_update"] = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    data["last_payload"] = ",".join(data["alarms"])
    
    if save_alarm_data(data):
        print(f"✓ Added alarm: {time_str}")
        print(f"  Total alarms: {len(data['alarms'])}")
        return True
    return False


def remove_alarm(time_str):
    """Remove an alarm"""
    data = load_alarm_data()
    if data is None:
        return False
    
    if time_str not in data["alarms"]:
        print(f"⚠ Alarm {time_str} not found")
        return False
    
    data["alarms"].remove(time_str)
    data["last_update"] = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    data["last_payload"] = ",".join(data["alarms"])
    
    if save_alarm_data(data):
        print(f"✓ Removed alarm: {time_str}")
        print(f"  Total alarms: {len(data['alarms'])}")
        return True
    return False


def list_alarms():
    """List all alarms"""
    data = load_alarm_data()
    if data is None:
        return
    
    print("\n" + "=" * 60)
    print("  Current Alarms")
    print("=" * 60)
    
    if data["alarms"]:
        for i, alarm in enumerate(data["alarms"], 1):
            print(f"  {i}. {alarm}")
        print()
        print(f"Total: {len(data['alarms'])} alarm(s)")
        if data["last_update"]:
            print(f"Last updated: {data['last_update']}")
    else:
        print("  No alarms configured")
    
    print("=" * 60 + "\n")


def clear_all():
    """Clear all alarms"""
    data = {
        "alarms": [],
        "last_update": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "last_payload": ""
    }
    
    if save_alarm_data(data):
        print("✓ All alarms cleared")
        return True
    return False


def main():
    """Main function"""
    import sys
    
    if len(sys.argv) < 2:
        print("\nUsage:")
        print("  python manage_alarms.py list              - List all alarms")
        print("  python manage_alarms.py add HH:MM         - Add an alarm")
        print("  python manage_alarms.py remove HH:MM      - Remove an alarm")
        print("  python manage_alarms.py clear             - Clear all alarms")
        print("\nExamples:")
        print("  python manage_alarms.py add 07:30")
        print("  python manage_alarms.py remove 14:00")
        print("  python manage_alarms.py list")
        print()
        list_alarms()
        return
    
    command = sys.argv[1].lower()
    
    if command == "list":
        list_alarms()
    elif command == "add" and len(sys.argv) >= 3:
        add_alarm(sys.argv[2])
    elif command == "remove" and len(sys.argv) >= 3:
        remove_alarm(sys.argv[2])
    elif command == "clear":
        response = input("Are you sure you want to clear all alarms? (yes/no): ")
        if response.lower() == "yes":
            clear_all()
        else:
            print("Cancelled")
    else:
        print("✗ Invalid command or missing arguments")
        print("Run without arguments to see usage")


if __name__ == "__main__":
    main()
