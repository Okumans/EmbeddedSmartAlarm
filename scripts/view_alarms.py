#!/usr/bin/env python3

"""
Alarm Data Viewer
View and manage stored alarm data from mqtt_subscriber.py
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
            return None
    except Exception as e:
        print(f"✗ Error loading alarm data: {e}")
        return None


def display_alarms():
    """Display current alarm data"""
    data = load_alarm_data()
    
    print("\n" + "=" * 60)
    print("  Smart Alarm Clock - Alarm Data Viewer")
    print("=" * 60)
    print(f"Data file: {ALARM_DATA_FILE}")
    print()
    
    if data is None:
        print("⚠ No alarm data file found.")
        print("   Run mqtt_subscriber.py to receive and store alarm data.")
        print("=" * 60 + "\n")
        return
    
    alarms = data.get("alarms", [])
    last_update = data.get("last_update", "Unknown")
    last_payload = data.get("last_payload", "N/A")
    
    print(f"📋 Alarm Information:")
    print(f"   Total Alarms: {len(alarms)}")
    print(f"   Last Update:  {last_update}")
    print(f"   Raw Payload:  {last_payload}")
    print()
    
    if alarms:
        print("⏰ Active Alarm Times:")
        for i, alarm_time in enumerate(alarms, 1):
            print(f"   {i}. {alarm_time}")
    else:
        print("   No active alarms configured.")
    
    print("=" * 60 + "\n")


def clear_alarms():
    """Clear all alarm data"""
    try:
        if ALARM_DATA_FILE.exists():
            ALARM_DATA_FILE.unlink()
            print("✓ Alarm data file deleted successfully.")
        else:
            print("⚠ No alarm data file to delete.")
    except Exception as e:
        print(f"✗ Error deleting alarm data: {e}")


def main():
    """Main function"""
    import sys
    
    if len(sys.argv) > 1 and sys.argv[1] == "clear":
        clear_alarms()
    else:
        display_alarms()
        print("Usage:")
        print("  python view_alarms.py       - View current alarms")
        print("  python view_alarms.py clear - Clear all alarm data")
        print()


if __name__ == "__main__":
    main()
