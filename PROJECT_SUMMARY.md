# Smart Alarm Clock - Comprehensive Project Summary

**Repository:** EmbeddedSmartAlarm (Owner: Okumans)  
**Branch:** main  
**Last Updated:** November 18, 2025

---

## 📋 Table of Contents
1. [Project Overview](#project-overview)
2. [System Architecture](#system-architecture)
3. [Hardware Components](#hardware-components)
4. [Software Structure](#software-structure)
5. [Communication Protocols](#communication-protocols)
6. [Configuration Details](#configuration-details)
7. [Code Structure & File Organization](#code-structure--file-organization)
8. [Key Features](#key-features)
9. [Data Flow](#data-flow)
10. [Development Environment](#development-environment)
11. [MQTT Topics & Messages](#mqtt-topics--messages)
12. [Python Control Scripts](#python-control-scripts)

---

## 🎯 Project Overview

### Purpose
The **Smart Alarm Clock** is an IoT-enabled embedded system that combines environmental monitoring with audio alarm functionality. The project demonstrates a multi-node sensor network architecture using ESP32 and ESP8266 microcontrollers communicating via ESP-NOW protocol, with cloud connectivity through MQTT.

### Core Functionality
- **Environmental Monitoring**: Real-time temperature, humidity, pressure, and UV index tracking
- **Audio Playback**: MP3/WAV alarm sounds via I2S audio output
- **Dual-Node Architecture**: Gateway (ESP32) + Remote Sensor Node (NodeMCU ESP8266)
- **Cloud Connectivity**: MQTT integration for remote monitoring and control
- **Visual Display**: OLED display showing sensor data and system status
- **Remote Control**: Python scripts for MQTT-based audio and system control

---

## 🏗️ System Architecture

### High-Level Architecture

The system is centered around a dual-node design, with the ESP32 Gateway acting as the central hub. The gateway's software has been refactored to a dual-core FreeRTOS architecture, enabling parallel processing of network communication and real-time tasks like audio playback.

```
                                Cloud Layer (MQTT)
                                broker.hivemq.com
                                       │
                                       │ MQTT
                                       │
 C ┌───────────────────────────────────┴───────────────────────────────────┐
 O │                      Gateway Node (ESP32)                             │
 R │ ┌─────────────────────────────────┬─────────────────────────────────┐ │
 E │ │ Core 0 (Network)                │ Core 1 (Real-Time)              │ │
   │ │                                 │                                 │ │
 0 │ │ ┌─────────────┐ ┌───────────┐ │ ┌─────────────┐ ┌───────────┐ │
   │ │ │  WiFi Stack │ │ MQTT Task │ │ │ Audio Decode│ │Sensor Task│ │
   │ │ └─────────────┘ └─────┬─────┘ │ │ Task (HP)   │ └─────┬─────┘ │
   │ │                       │         │ └─────┬─────┘       │         │ │
   │ └───────────────────────┼─────────┴───────┼─────────────┼─────────┘ │
   │                         │                 │             │           │
   └─────────────────────────┼─────────────────┼─────────────┼───────────┘
                             │                 │             │
         ┌───────────────────┘                 │             └──────────┐
         │                                     │                        │
         │ ESP-NOW                             │ I2S Audio              │ Local
         │                                     │                        │ Sensors
         ▼                                     ▼                        ▼
┌──────────────────┐                  ┌─────────────┐            ┌───────────┐
│  Sensor Node     │                  │   I2S DAC/  │            │   DHT22   │
│  (ESP8266)       │                  │  Amplifier  │            │  Sensor   │
└──────────────────┘                  └─────────────┘            └───────────┘
```

### Node Roles

#### Gateway Node (ESP32)
- **Primary Role**: System coordinator, MQTT bridge, audio controller
- **Board**: ESP32 DOIT DevKit V1
- **Architecture**: Dual-Core FreeRTOS for parallel processing.
- **Responsibilities**:
  - **RTOS-based Task Management**: Runs independent tasks for MQTT, audio, sensors, and display.
  - Receive sensor data from remote nodes via ESP-NOW.
  - Forward all data to MQTT broker via a dedicated `MQTT Task`.
  - Read local sensors using a dedicated `Sensor Task`.
  - Control audio playback in a high-priority `Audio Decode Task`.
  - Display system status on OLED via a dedicated `Display Task`.
  - Accept remote commands via a modular, handler-based MQTT system.

#### Sensor Node (NodeMCU ESP8266)
- **Primary Role**: Environmental data collection and transmission
- **Board**: NodeMCU V2 (ESP8266)
- **Responsibilities**:
  - Read multiple environmental sensors
  - Transmit sensor data via ESP-NOW every 5 seconds
  - Simulate battery monitoring
  - Low-power operation for potential battery use

---

## 🔌 Hardware Components

### Gateway Node (ESP32) Components

| Component | Model/Type | Interface | Pin Configuration | Purpose |
|-----------|-----------|-----------|-------------------|---------|
| **Microcontroller** | ESP32 DOIT DevKit V1 | - | - | Main controller |
| **Temperature/Humidity** | DHT22 | Digital | GPIO 4 (DHTPIN) | Local climate monitoring |
| **I2C Multiplexer** | TCA9548A | I2C | SDA=21, SCL=22 | Multiple I2C device management |
| **OLED Display** | SSD1306 128x64 | I2C via TCA | Channel 1, 0x3C | System status display |
| **Audio Output** | I2S DAC/Amplifier | I2S | BCLK=26, LRC=25, DOUT=27 | Alarm audio playback |
| **WiFi** | Built-in ESP32 | - | - | MQTT + ESP-NOW |

### Sensor Node (ESP8266) Components

| Component | Model/Type | Interface | Pin Configuration | Purpose |
|-----------|-----------|-----------|-------------------|---------|
| **Microcontroller** | NodeMCU V2 (ESP8266) | - | - | Sensor node controller |
| **Temperature/Humidity** | DHT22 | Digital | D4 (GPIO2) | Outside climate monitoring |
| **Pressure Sensor** | BMP180/BMP085 | I2C | D2=SDA, D1=SCL | Atmospheric pressure |
| **UV Sensor** | GUVA-S12SD | Analog | A0 | UV index measurement |
| **WiFi** | Built-in ESP8266 | - | - | ESP-NOW communication |

---

## 💻 Software Structure

### Project Directory Structure

The project has been refactored into a modular, multi-file structure to support the new FreeRTOS architecture.

```
SmartAlarmClock/
├── platformio.ini              # PlatformIO configuration (multi-env)
├── PROJECT_SUMMARY.md          # This document
│
├── include/                    # Shared header files
│   ├── audio_data.h           # Alarm melody definitions
│   ├── audio_manager.h        # Audio system interface
│   ├── config.h               # Centralized configuration
│   ├── mqtt_handler.h         # MQTT handler definitions
│   ├── mqtt_manager.h         # MQTT manager interface
│   ├── rtos_tasks.h           # FreeRTOS task definitions
│   ├── sensor_data.h          # ESP-NOW data structure
│   └── sensor_manager.h       # Local sensor manager interface
│
├── src/                        # Source code (multi-target)
│   ├── gateway_esp32/         # Gateway ESP32 firmware
│   │   ├── main.cpp          # Main entry point, setup, RTOS startup
│   │   ├── audio_manager.cpp # Audio playback implementation
│   │   ├── mqtt_manager.cpp  # MQTT connection & handler logic
│   │   ├── rtos_tasks.cpp    # FreeRTOS task implementations
│   │   └── sensor_manager.cpp# Local sensor reading logic
│   │
│   └── sensor_nodemcu/        # Sensor node firmware
│       └── main.cpp          # Sensor node logic
│
├── data/                       # SPIFFS/LittleFS filesystem data
│   └── aimaihwaelw2.mp3      # Audio file for alarm
│
├── scripts/                    # Python control utilities
│   ├── ... (see Python section)
│
└── ...
```

### FreeRTOS Real-Time Architecture

The gateway firmware has been migrated to a **preemptive, dual-core FreeRTOS architecture**. This provides true parallelism, improves real-time performance for audio processing, and prepares the system for future features like WebSocket audio streaming. The `loop()` function is now only used for minimal WiFi maintenance.

#### Task Distribution

Tasks are pinned to specific cores to optimize performance and prevent resource contention.

**Core 0 (Network & Communication)**
- **WiFi Stack** (System Managed)
- **MQTT Task** (`Priority 2`): Manages all MQTT communication, including publishing sensor data and processing incoming commands. Runs on the same core as the WiFi stack for efficiency.

**Core 1 (Real-Time Processing)**
- **Audio Decode Task** (`Priority 3` - *Highest*): Handles audio playback (MP3/WAV) and is reserved for future real-time audio stream decoding. Its high priority ensures smooth, uninterrupted audio.
- **Sensor Task** (`Priority 1`): Reads local sensors and triggers MQTT publishing at fixed intervals.
- **Display Task** (`Priority 1`): Updates the OLED display with the latest data.
- **Audio Encode Task** (`Priority 3`, *Suspended*): Reserved for future microphone input and audio encoding.

This multi-tasking architecture ensures that network operations on Core 0 do not block critical real-time audio processing on Core 1, and high-priority tasks can preempt lower-priority ones.

---

## 🔧 Configuration Details

### PlatformIO Configuration (`platformio.ini`)

#### Gateway Environment (`env_gateway_esp32`)
```ini
[env:env_gateway_esp32]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
monitor_speed = 115200
board_build.partitions = min_spiffs.csv
build_flags = 
	-D CONFIG_BROWNOUT_DET=0
build_src_filter = +<gateway_esp32/>

lib_deps = 
    Adafruit GFX Library
    Adafruit SSD1306
    Adafruit Unified Sensor
    DHT sensor library
    TCA9548A
    PubSubClient
    earlephilhower/ESP8266Audio @ ^1.9.7
    adafruit/Adafruit BMP085 Library@^1.2.4
    regenbogencode/ESPNowW@^1.0.2
    bblanchon/ArduinoJson@^7.4.2
```

#### Sensor Node Environment (`env_sensor_nodemcu`)
```ini
[env:env_sensor_nodemcu]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
upload_speed = 115200
build_src_filter = +<sensor_nodemcu/>

lib_deps = 
    DHT sensor library
    Adafruit Unified Sensor
    PubSubClient
    adafruit/Adafruit BMP085 Library@^1.2.4
    me-no-dev/ESPAsyncTCP@^1.2.2
    regenbogencode/ESPNowW@^1.0.2
    bblanchon/ArduinoJson@^7.4.2
```

### Network Configuration

#### WiFi Settings (Gateway)
```cpp
// WiFi network credentials (defined in include/config.h)
static const char* WIFI_SSID = "Pleaseconnecttome";
static const char* WIFI_PASSWORD = "n1234567!";

// Soft Access Point for ESP-NOW
static const char* SOFT_AP_SSID = "SmartAlarm-Gateway";
static const char* SOFT_AP_PASSWORD = "12345678";
#define WIFI_CHANNEL 6  // Fixed channel for ESP-NOW
```

#### MQTT Broker Configuration
```cpp
static const char* MQTT_SERVER = "broker.hivemq.com";
static const int MQTT_PORT = 1883;
static const char* MQTT_CLIENT_ID = "SmartAlarmClock";

// Audio upload topics (gateway <-> uploader)
static const char* MQTT_TOPIC_AUDIO_REQUEST = "esp32/audio_request";
static const char* MQTT_TOPIC_AUDIO_CHUNK = "esp32/audio_chunk";
static const char* MQTT_TOPIC_AUDIO_RESPONSE = "esp32/audio_response";
static const char* MQTT_TOPIC_AUDIO_ACK = "esp32/audio_ack";
```

#### ESP-NOW Configuration
```cpp
// Sensor Node -> Gateway MAC Address
// CRITICAL: Use ESP32's Soft AP MAC address
uint8_t gatewayAddress[] = { 0x28, 0x56, 0x2F, 0x4A, 0x15, 0x0D };
```

### Timing Parameters

| Parameter | Gateway | Sensor Node | Purpose |
|-----------|---------|-------------|---------|
| **Sensor Read Interval** | 2000 ms | 5000 ms | How often to read sensors |
| **MQTT Publish Interval** | 10000 ms | N/A | How often to publish to MQTT |
| **Remote Data Timeout** | 30000 ms | N/A | Max age for remote sensor data |

---

## 📡 Communication Protocols

### 1. ESP-NOW Protocol

**Purpose**: Low-latency, peer-to-peer communication between ESP8266 sensor node and ESP32 gateway.

**Data Structure** (`sensor_data.h`):
```cpp
#pragma pack(push, 1)
typedef struct {
    uint32_t timestamp;       // 4 bytes - Timestamp in milliseconds
    float temperature;        // 4 bytes - Temperature in Celsius
    float humidity;           // 4 bytes - Humidity percentage
    float pressure;           // 4 bytes - Air pressure in hPa
    float uvIndex;            // 4 bytes - UV index (0-11+)
    uint8_t batteryLevel;     // 1 byte  - Battery level (0-100%)
    uint8_t sensorId;         // 1 byte  - Sensor node identifier
    char deviceName[16];      // 16 bytes - Device name
} SensorData;                 // Total: 38 bytes
#pragma pack(pop)
```

**Key Features**:
- Fixed 38-byte packet size
- Packed structure for ESP8266/ESP32 compatibility
- Transmission every 5 seconds from sensor node
- Callback-based reception on gateway

**Critical Setup Notes**:
- Both devices must be on the **same WiFi channel** (Channel 6)
- Gateway operates in **AP+STA mode** (Access Point + Station)
- Sensor node connects to gateway's Soft AP
- Must use **Gateway's AP MAC address** (not Station MAC)

### 2. MQTT Protocol

**Purpose**: Cloud connectivity for remote monitoring and control.

**Broker**: `broker.hivemq.com:1883` (Public HiveMQ broker)

**Architecture**:
- Gateway acts as MQTT client
- Publishes sensor data to topic hierarchy
- Subscribes to command topics
- Python scripts can publish/subscribe

---

## 📨 MQTT Topics & Messages

### MQTT Handler Architecture

The gateway uses a modular **`MQTTManager`** to handle incoming messages. Instead of a single callback function, the system registers multiple, independent handler functions for different topics. Each handler has a name and a priority, allowing critical commands (like audio control) to be processed before less important ones.

This architecture provides:
- **Decoupling**: Logic is separated into clean, self-contained handlers.
- **Prioritization**: Ensures timely processing of important commands.
- **Testability**: Individual handlers can be tested in isolation.
- **Dynamic Responses**: Handlers can directly publish responses, acknowledgments, or new data.

### Sensor Data Topics (Published by Gateway)

#### Local Gateway Sensors
| Topic | Data Type | Example | Unit | Update Rate |
|-------|-----------|---------|------|-------------|
| `smartalarm/gateway/temperature/inside` | Float | "24.5" | °C | 10s |
| `smartalarm/gateway/humidity/inside` | Float | "65.2" | % | 10s |
| `smartalarm/gateway/status` | String | "online" | - | On connect |

#### Remote Sensor Node Data (via ESP-NOW)
| Topic | Data Type | Example | Unit | Update Rate |
|-------|-----------|---------|------|-------------|
| `smartalarm/sensor/temperature/outside` | Float | "28.35" | °C | 5s |
| `smartalarm/sensor/humidity/outside` | Float | "72.15" | % | 5s |
| `smartalarm/sensor/pressure/outside` | Float | "1013.25" | hPa | 5s |
| `smartalarm/sensor/uvindex/outside` | Float | "5.20" | Index | 5s |
| `smartalarm/sensor/battery/outside` | Integer | "87" | % | 5s |
| `smartalarm/sensor/status` | String | "SensorNode01 online" | - | 5s |

#### Audio Upload Topics (Used by `mqtt_audiochunkupload.py`)

| Topic | Direction | Purpose | Message Format |
|-------|-----------|---------|----------------|
| `esp32/audio_request` | Python → ESP32 | Request storage info | `REQUEST_FREE_SPACE` |
| `esp32/audio_response` | ESP32 → Python | Report storage status | `FREE:<bytes>:<currentAudioSize>` |
| `esp32/audio_chunk` | Python → ESP32 | Upload audio data | `START:<size>`, `CHUNK:<idx>:<total>:<data>`, `END` |
| `esp32/audio_ack` | ESP32 → Python | Acknowledge chunk | `ACK:<chunkIndex>` |

**Audio Upload Protocol**:
1. Python requests free space: `REQUEST_FREE_SPACE`
2. ESP32 responds: `FREE:122880:14881` (free bytes : current audio size)
3. Python sends: `START:50000` (file size in bytes)
4. ESP32 deletes old file, opens new file for writing
5. For each 4096-byte chunk:
   - Python sends: `CHUNK:0:15:<4096 bytes of data>`
   - ESP32 writes to LittleFS and responds: `ACK:0`
   - Python waits for ACK before sending next chunk
6. Python sends: `END`
7. ESP32 closes file, upload complete

**Key Features**:
- Chunk size: 4096 bytes
- ACK-based flow control (prevents buffer overflow)
- Timeout: 5 seconds per chunk
- MQTT buffer size: 4200 bytes (set in `setupMQTT()`)
- Validates file size before upload
- Accounts for space freed by deleting old audio file

### Command Topics (Processed by MQTT Handlers)

Commands are processed by handlers registered to specific topics.

#### Audio Control (`smartalarm/play_audio`)
- **Handler**: `AudioPlayback` (Priority 150)
- **Action**: Receives a filename (e.g., `/sound.mp3`), validates the path, and calls `audio.playFile()`. It then publishes a status update to `smartalarm/audio/status`.

#### System Commands (`smartalarm/commands`)
- **Handler**: `SystemCommands` (Priority 100)
- **Action**: This handler processes multiple string-based commands:
  - `stop_audio`: Stops current audio playback.
  - `list_files`: Lists audio files in the console.
  - `volume=<0.0-1.0>`: Sets the audio volume.
  - `status`: Publishes a detailed status message.
- Each command publishes its own confirmation or status update.

#### Audio Upload Control (Internal Topics)
- **Handlers**: The `AudioManager` registers its own set of handlers for the audio upload process (`esp32/audio_request`, `esp32/audio_chunk`, etc.). These handlers manage the file transfer protocol, responding to requests and acknowledging data chunks directly.

---

## 🐍 Python Control Scripts

### 1. `mqtt_send.py` - Quick Command Sender

**Purpose**: Send single MQTT commands quickly from command line

**Usage Examples**:
```bash
# Play audio file
python3 mqtt_send.py play alarm1.mp3
python3 mqtt_send.py play /aimaihwaelw1.mp3

# Stop playback
python3 mqtt_send.py stop

# Set volume (0.0 to 1.0)
python3 mqtt_send.py volume 0.5

# List audio files
python3 mqtt_send.py list
```

**Features**:
- Simple command-line interface
- Auto-adds leading "/" to filenames
- Connects to broker.hivemq.com
- Single-shot publish (no persistent connection)

### 2. `mqtt_subscriber.py` - Real-time Monitor

**Purpose**: Subscribe to all sensor topics and display data in real-time

**Features**:
- Monitors all gateway and remote sensor topics
- Displays formatted output with timestamps
- Shows connection status
- Tracks last update times
- Emoji indicators for different sensor types (🌡️ 💧 🔆 etc.)

**Usage**:
```bash
python3 mqtt_subscriber.py
# Press Ctrl+C to exit
```

**Output Format**:
```
✓ Connected to MQTT broker: broker.hivemq.com:1883
========================================
  Subscribed to: smartalarm/gateway/temperature
  Subscribed to: smartalarm/sensor/temperature
  ...
========================================

Waiting for messages... (Press Ctrl+C to exit)

🌡️  [2025-11-17 14:32:45] Remote Temperature: 28.35°C
💧 [2025-11-17 14:32:45] Remote Humidity: 72.15%
🌀 [2025-11-17 14:32:45] Remote Pressure: 1013.25hPa
```

### 3. `audio_controller.py` - Interactive Audio Control

**Purpose**: Interactive shell for audio control with persistent MQTT connection

**Features**:
- Interactive command prompt
- Persistent MQTT connection
- Real-time feedback from gateway
- Multiple audio formats (MP3, WAV)
- Volume control
- File listing

**Usage**:
```bash
python3 audio_controller.py

# Interactive commands:
> play alarm1.mp3
> play /wake_up.wav
> stop
> volume 0.8
> list
> help
> quit
```

**Dependencies** (`pyproject.toml`):
```toml
[project]
name = "scripts"
version = "0.1.0"
requires-python = ">=3.13"
dependencies = [
    "paho-mqtt>=2.1.0",
    "pydub>=0.25.1",
]
```

### 4. `mqtt_audiochunkupload.py` - Remote Audio File Uploader

**Purpose**: Upload MP3 audio files to ESP32's LittleFS filesystem via MQTT

**Features**:
- Upload any MP3 file to ESP32 remotely
- Optional MP3 compression (configurable bitrate)
- Smart space calculation (accounts for existing audio file)
- Chunk-based transfer with ACK confirmation
- Progress tracking during upload
- Automatic validation of available space
- File size: 4096-byte chunks for reliability

**Usage Examples**:
```bash
# Upload with default compression (32k bitrate)
python mqtt_audiochunkupload.py myaudio.mp3

# Upload with custom compression
python mqtt_audiochunkupload.py myaudio.mp3 --bitrate 64k

# Upload WITHOUT compression (use original file)
python mqtt_audiochunkupload.py myaudio.mp3 --bitrate 0

# Specify custom output filename
python mqtt_audiochunkupload.py input.mp3 --bitrate 128k --output temp.mp3

# Show help
python mqtt_audiochunkupload.py --help
```

**Command-Line Arguments**:
- `input_file` - Required: Input MP3 file path
- `-b, --bitrate` - Compression bitrate (32k, 64k, 128k, etc.) or 0 for no compression (default: 32k)
- `-o, --output` - Output filename for compressed file (default: compressed.mp3)

**Output Example**:
```
[INFO] Input file: alarm.mp3
[INFO] Original size: 245823 bytes
[+] Compressing MP3 to 32k bitrate...
[✓] Compression complete.
[+] Requesting free storage from ESP32...
[✓] ESP32 reports:
    - Free space: 122880 bytes
    - Current audio file: 14881 bytes
[INFO] Upload file size: 48234 bytes
[INFO] Available space (including current audio): 137761 bytes
[INFO] Note: Current audio (14881 bytes) will be replaced
[✓] Enough space. Uploading file...
[+] Sending 12 chunks...
  - Sent chunk 1/12, waiting for ACK...
[✓] Received ACK for chunk 0
  - ACK received for chunk 0
  ...
[✓] Done sending all chunks.
[✓] Upload complete!
```

**Technical Details**:
- **Protocol**: Custom chunk-based MQTT transfer protocol
- **Chunk size**: 4096 bytes
- **Timeout**: 5 seconds per ACK
- **Flow control**: Waits for ACK before sending next chunk
- **Space validation**: Calculates available space = free + existing audio
- **Compression**: Uses pydub library with configurable bitrates
- **Error handling**: Aborts on timeout or insufficient space

**Dependencies**:
- `paho-mqtt` - MQTT client
- `pydub` - Audio processing and compression

---

## 🎵 Audio System

### Audio Manager Class

**Header**: `include/audio_manager.h`  
**Implementation**: `src/gateway_esp32/audio_manager.cpp` (592 lines)

**Capabilities**:
- MP3 and WAV playback
- I2S audio output
- LittleFS filesystem integration (ESP32 only)
- Volume control (0.0 to 1.0)
- Non-blocking playback via `loop()`
- Remote audio file upload via MQTT
- Storage space management

**Key Methods**:
```cpp
bool begin();                               // Initialize audio system and filesystem
bool playFile(const char* filename);        // Auto-detect and play file
void stop();                                // Stop playback
void loop();                                // Update playback (called by RTOS task)
void setVolume(float volume);               // Set volume (0.0-1.0)
bool playing();                             // Check if audio is playing
void listFiles();                           // List filesystem audio files
void registerMQTTHandlers(MQTTManager& mqtt); // Register audio upload handlers
```

**Audio Upload System**:
```cpp
// File upload variables
File fsFile;
String recvFilename = "/sound.mp3";
bool receivingFile = false;
size_t expectedSize = 0;
size_t receivedSize = 0;
unsigned long lastChunkTime = 0;  // Upload timeout tracking
```

**MQTT Upload Handler**:
The `AudioManager` uses the `registerMQTTHandlers` method to register its own high-priority handlers with the `MQTTManager`. This encapsulates the audio upload logic within the `AudioManager` class itself. These handlers are responsible for:
- Responding to `REQUEST_FREE_SPACE` with filesystem stats.
- Receiving file chunks on the `esp32/audio_chunk` topic.
- Sending acknowledgments for each chunk on the `esp32/audio_ack` topic.
- Validating and writing chunk data to LittleFS.
- Tracking upload progress and handling timeouts or errors.

**Filesystem Management**:
- **ESP32**: Uses LittleFS (partition: min_spiffs.csv)
- Auto-formats on first use if needed
- Reports total, used, and free space
- Deletes old audio before uploading new file
- File path: `/sound.mp3` (default audio file)

---

## 🎵 Audio System (continued)

**I2S Pin Configuration**:
```cpp
#define I2S_BCLK 26   // Bit clock
#define I2S_LRC  25   // Left/Right clock
#define I2S_DOUT 27   // Data out
```

**Audio Storage**:
- Files stored in LittleFS filesystem (ESP32 only)
- Can be uploaded via `mqtt_audiochunkupload.py` script
- Default file: `/sound.mp3`
- Partition: `min_spiffs.csv` for ESP32
- Typical capacity: ~180 KB for audio storage

### Predefined Melodies

**Header**: `include/audio_data.h`

Contains PROGMEM melody arrays with frequency/duration pairs:
- `alarm_beep` - Simple beep pattern
- `morning_melody` - Gentle wake-up
- `classic_alarm` - Traditional alarm
- `happy_birthday` - Birthday melody
- `do_re_mi` - Musical scale
- `frere_jacques` - Brother John song
- `imperial_march` - Star Wars theme
- `nokia_tone` - Classic Nokia ringtone
- `success_sound` - Notification sound
- `error_sound` - Warning sound

---

## 🔄 Data Flow

With the FreeRTOS architecture, data flows between independent tasks, primarily communicating through shared objects and function calls.

### 1. Sensor Data Collection Flow (Local & Remote)

Data from local and remote sensors is handled by different tasks but funneled through the `MQTT Task` for cloud communication.

```
CORE 1                                     CORE 0
======                                     ======
┌───────────────────┐
│ ESP-NOW Callback  │
│ (Interrupt-driven)│
└─────────┬─────────┘
          │
          ▼
┌───────────────────┐                      ┌───────────────────┐
│ Stores Remote Data│                      │    Sensor Task    │
│ (Global variable) ├──┐                   │ (Every 2 seconds) │
└───────────────────┘  │                   └─────────┬─────────┘
                       │                             │
                       │                             ▼
                       │                   ┌───────────────────┐
                       │                   │ Reads Local DHT22 │
                       │                   └─────────┬─────────┘
                       │                             │
                       └──────────┐  ┌───────────────┘
                                  │  │
                                  ▼  ▼
                          ┌───────────────────┐
                          │   Publish Data    │
                          │ (Every 10 seconds)│
                          └─────────┬─────────┘
                                    │ (mqtt.publish)
                                    │
                                    ▼
                               ┌─────────────┐
                               │  MQTT Task  │
                               └──────┬──────┘
                                      │
                                      ▼ MQTT
                               ┌─────────────┐
                               │ MQTT Broker │
                               └─────────────┘
```

### 2. Audio Control Flow

Audio commands are received by the `MQTT Task` on Core 0 and trigger playback managed by the high-priority `Audio Decode Task` on Core 1.

```
CORE 0                                     CORE 1
======                                     ======
┌───────────────────┐
│   MQTT Client     │
└─────────┬─────────┘
          │ (MQTT Subscribe)
          ▼
┌───────────────────┐
│     MQTT Task     │
│ Receives Command  │
└─────────┬─────────┘
          │ (dispatch to handler)
          ▼
┌───────────────────┐
│ Command Handler   │
│ (in main.cpp)     │
└─────────┬─────────┘
          │ (audio.playFile)
          │
          └─────────────────────────┐
                                    │
                                    ▼
                          ┌───────────────────┐
                          │ Audio Decode Task │
                          │  (High Priority)  │
                          └─────────┬─────────┘
                                    │ (audio.loop)
                                    ▼
                          ┌───────────────────┐
                          │ Reads from LittleFS│
                          └─────────┬─────────┘
                                    │
                                    ▼
                          ┌───────────────────┐
                          │ I2S Audio Output  │
                          └───────────────────┘
```

### 3. Display Update Flow

The `Display Task` runs independently on Core 1, periodically fetching the latest sensor data from shared variables to update the OLED screen.

```
CORE 1
======
┌───────────────────┐
│    Display Task   │
│  (Every 200ms)    │
└─────────┬─────────┘
          │
          ▼
┌───────────────────┐
│  updateDisplay()  │
└─────────┬─────────┘
          │
          ├─► Reads Local Sensor Data
          │   (from SensorManager object)
          │
          ├─► Reads Remote Sensor Data
          │   (from global variable)
          │
          └─► Reads WiFi/MQTT Status
              (from client objects)
              │
              ▼
┌───────────────────┐
│   Update OLED     │
│   via I2C Mux     │
└───────────────────┘
```

---

## 🔍 Key Features Details

### 1. Multi-Mode WiFi Operation (Gateway)

The gateway operates in **AP+STA mode** - critical for ESP-NOW:
- **Station (STA)**: Connects to home WiFi for internet/MQTT
- **Access Point (AP)**: Creates "SmartAlarm-Gateway" network for ESP-NOW

**Channel Synchronization**:
```cpp
// Gateway detects WiFi channel automatically
esp_wifi_get_channel(&primary, &secondary);
// Then creates Soft AP on SAME channel
WiFi.softAP(SOFT_AP_SSID, SOFT_AP_PASSWORD, wifiChannel, 0);
```

**Important**: Sensor node MUST use same channel (Channel 6).

### 2. I2C Multiplexing

Uses TCA9548A I2C multiplexer to manage multiple I2C devices:
- **Channel 1**: OLED Display (0x3C)
- Future expansion possible on other channels

**Usage Pattern**:
```cpp
tca.openChannel(TCA_CHANNEL_OLED);
// ... perform I2C operations ...
tca.closeChannel(TCA_CHANNEL_OLED);
```

### 3. High-Priority Audio Task

Audio playback is no longer handled by a simple `loop()` call. It now runs in a dedicated **high-priority FreeRTOS task (`AudioDecode`) pinned to Core 1**.

This robust, preemptive multitasking approach provides significant benefits:
- **Resource Isolation**: Audio processing is completely isolated from network activity on Core 0.
- **No Starvation**: As the highest priority task on its core, audio decoding will always get the processing time it needs, preempting sensor reads or display updates.
- **Smooth Playback**: Guarantees smooth, uninterrupted audio even when the system is under heavy load from WiFi or MQTT.

### 4. Sensor Data Validation

Gateway validates ESP-NOW packets:
```cpp
if (data_len == sizeof(SensorData)) {
    memcpy(&remoteSensorData, data, sizeof(SensorData));
    // ... process valid data ...
} else {
    Serial.printf("Invalid data size! Expected %d, got %d\n",
                  sizeof(SensorData), data_len);
}
```

### 5. Battery Simulation

Sensor node simulates battery level:
```cpp
sensorData.batteryLevel = 100 - (transmissionCount % 100);
```

This demonstrates battery monitoring capability for future battery-powered deployment.

---

## 🚀 Development Environment

### Build System: PlatformIO

**Build Commands**:
```bash
# Build gateway firmware
pio run -e env_gateway_esp32

# Build sensor node firmware
pio run -e env_sensor_nodemcu

# Upload to gateway
pio run -e env_gateway_esp32 -t upload

# Upload to sensor node
pio run -e env_sensor_nodemcu -t upload

# Serial monitor
pio device monitor -e env_gateway_esp32
pio device monitor -e env_sensor_nodemcu
```

**Upload SPIFFS Data**:
```bash
# Upload audio files to ESP32
pio run -e env_gateway_esp32 -t uploadfs
```

### Serial Monitor Output

**Gateway Output Example**:
```
========================================
Smart Alarm Clock - Starting
========================================
[System] Brownout detector disabled

[System] TCA9548A initialized
[Display] Initializing OLED...
[Display] OLED initialized
[AudioManager] Initializing LittleFS...
[AudioManager] ✓ LittleFS mounted.
[AudioManager] ✓ Audio system ready.

[WiFi] Configuring WiFi...
[WiFi] ✓ Connected to WiFi!
[WiFi] IP Address: 192.168.1.100
[MQTTManager] Initialized
[MQTT] Registering message handlers...
[MQTT] Handler registration complete

[ESP-NOW] ✓ Initialized successfully
[System] Setup complete!

[RTOS] Initializing task queues...
[RTOS] ✓ Queues created successfully

[RTOS] Starting tasks...
[RTOS] ✓ All tasks created successfully

========================================
Task Assignment:
========================================
Core 0 (Network):
  - WiFi Stack (system)
  - MQTT Task (priority 2)
  - WebSocket Task (priority 2, suspended)

Core 1 (Audio/Display):
  - Audio Decode (priority 3)
  - Audio Encode (priority 3, suspended)
  - Sensor Task (priority 1)
  - Display Task (priority 1)
========================================

[System] FreeRTOS tasks running!
[System] Arduino loop() will be used for WiFi maintenance only

[RTOS] Display Task started on Core 1
[RTOS] Sensor Task started on Core 1
[RTOS] Audio Encode Task started on Core 1
[RTOS] Audio Decode Task started on Core 1
[RTOS] WebSocket Task started on Core 0
[RTOS] MQTT Task started on Core 0
```

**Sensor Node Output Example**:
```
=== Smart Alarm - Sensor Node ===

[BMP] ✓ BMP initialized
[WiFi] ✓ Connected!
[ESP-NOW] ✓ Initialized
[ESP-NOW] ✓ Peer added

╔════════ REAL SENSOR DATA ═══════════════╗
 ║ Temp:       28.35 °C                  ║
 ║ Humidity:   72.15 %                   ║
 ║ Pressure:  1013.25 hPa                ║
 ║ UV Index:    5.20                     ║
 ║ Battery:     87 %                     ║
╚═════════════════════════════════════════╝
[ESP-NOW] Packet sent to: 28:56:2F:4A:15:0D | Status: ✓ Success
```

---

## 📊 Performance Metrics

### Memory Usage
- **SensorData struct**: 38 bytes (packed)
- **ESP-NOW max payload**: 250 bytes (38 bytes used)
- **Gateway RAM**: ~60% used with all features
- **Sensor Node RAM**: ~45% used

### Timing Characteristics
- **ESP-NOW latency**: <10ms typical
- **MQTT publish interval**: 10 seconds (gateway sensors)
- **Sensor reading**: 5 seconds (remote node)
- **Display update**: 2 seconds
- **Audio processing**: Non-blocking, runs in background

### Network Bandwidth
- **ESP-NOW**: 38 bytes every 5 seconds = ~7.6 bytes/sec
- **MQTT**: ~100 bytes every 10 seconds = ~10 bytes/sec
- **Total**: Minimal bandwidth usage

---

## 🔒 Security Considerations

### Current Implementation
- **WiFi**: WPA2 password protection
- **MQTT**: Unencrypted connection to public broker
- **ESP-NOW**: Encrypted with pre-shared key option (not implemented)

### Recommendations for Production
1. Use private MQTT broker with authentication
2. Enable TLS/SSL for MQTT (port 8883)
3. Implement ESP-NOW encryption
4. Use strong WiFi passwords
5. Implement OTA update security
6. Add authentication for audio commands

---

## 🐛 Troubleshooting Guide

### ESP-NOW Connection Issues

**Problem**: Sensor node can't send to gateway
**Solutions**:
1. Verify both devices on same WiFi channel (6)
2. Use gateway's **AP MAC address** (not Station MAC)
3. Check gateway is in AP+STA mode
4. Verify MAC address in sensor node code

### MQTT Connection Issues

**Problem**: Gateway can't connect to MQTT broker
**Solutions**:
1. Check internet connectivity
2. Verify broker address and port
3. Try alternative broker (test.mosquitto.org)
4. Check firewall settings

### Audio Playback Issues

**Problem**: Audio won't play
**Solutions**:
1. Verify file exists in SPIFFS (`list_files` command)
2. Check file format (MP3/WAV only)
3. Verify I2S wiring (BCLK, LRC, DOUT)
4. Check volume level (try `volume=0.8`)
5. Ensure audio file is properly formatted

### Display Issues

**Problem**: OLED display blank or corrupted
**Solutions**:
1. Check I2C connections (SDA=21, SCL=22)
2. Verify TCA9548A multiplexer working
3. Check I2C address (0x3C)
4. Ensure proper channel selection

---

## 📈 Future Enhancement Ideas

### Hardware Enhancements
- [ ] Add RTC (Real-Time Clock) for alarm scheduling
- [ ] Implement actual battery monitoring for sensor node
- [ ] Add temperature/humidity sensor to gateway (currently configured but not actively used)
- [ ] Add light sensor for automatic display brightness
- [ ] Include physical buttons for local control
- [ ] Add SD card for audio storage expansion

### Software Enhancements
- [ ] Web interface for configuration and control
- [ ] Alarm scheduling system
- [ ] Data logging to cloud database
- [ ] Over-The-Air (OTA) firmware updates
- [ ] Deep sleep mode for sensor node
- [ ] Multiple sensor node support
- [ ] Audio playlist functionality
- [ ] Weather forecast integration

---

## 📚 Dependencies Summary

### ESP32 Gateway Libraries
- **Adafruit GFX Library** - Graphics primitives
- **Adafruit SSD1306** - OLED display driver
- **Adafruit Unified Sensor** - Sensor abstraction
- **DHT sensor library** - DHT22 temperature/humidity
- **TCA9548A** - I2C multiplexer
- **PubSubClient** - MQTT client (buffer size set to 4200 bytes)
- **ESP8266Audio** - MP3/WAV audio playback
- **ESPNowW** - ESP-NOW wrapper
- **ArduinoJson** - JSON parsing

### ESP8266 Sensor Node Libraries
- **DHT sensor library** - DHT22 sensor
- **Adafruit Unified Sensor** - Sensor abstraction
- **PubSubClient** - MQTT (not actively used)
- **Adafruit BMP085 Library** - BMP180 pressure sensor
- **ESPAsyncTCP** - Async TCP library
- **ESPNowW** - ESP-NOW wrapper
- **ArduinoJson** - JSON support

### Python Scripts Dependencies
- **paho-mqtt** >= 2.1.0 - MQTT client library
- **pydub** >= 0.25.1 - Audio processing and compression (for mqtt_audiochunkupload.py)

---

## 📞 Contact & Support

**Repository**: EmbeddedSmartAlarm  
**Owner**: Okumans  
**Branch**: main

For questions, issues, or contributions, please refer to the repository on GitHub.

---

## 📝 Notes for Agent Context

### Critical Implementation Details

1. **ESP-NOW MAC Address**: The sensor node MUST use the gateway's **Soft AP MAC address**, not the Station MAC. This is printed in the gateway serial output.

2. **WiFi Channel Synchronization**: Both nodes must be on the same WiFi channel. The gateway auto-detects its channel and prints it. Update sensor node if different.

3. **MQTT Topic Structure**: Topics explicitly specify "inside" vs "outside" for clarity:
   - Gateway sensors: `.../inside`
   - Remote sensors: `.../outside`

4. **Non-Blocking Design**: Audio playback is non-blocking via `audio.loop()` calls in main loop.

5. **Data Structure Packing**: The `SensorData` struct uses `#pragma pack(1)` to ensure identical memory layout on ESP8266 and ESP32.

6. **LittleFS vs SPIFFS**: ESP32 uses LittleFS (more reliable), ESP8266 uses SPIFFS. Both are transparent in AudioManager code.

7. **MQTT Buffer Size**: Set to 4200 bytes in `setupMQTT()` to accommodate 4096-byte audio chunks plus headers.

8. **Audio Upload Protocol**: Uses custom ACK-based protocol for reliable file transfer over MQTT. Each chunk requires acknowledgment.

9. **I2C Multiplexing**: The TCA9548A allows multiple I2C devices with same address. Channels must be explicitly opened/closed.

10. **Python Scripts**: Require Python 3.13+, paho-mqtt, and pydub. Install with `pip install paho-mqtt pydub`.

### Common Use Cases

**Monitor Sensor Data**:
```bash
python3 scripts/mqtt_subscriber.py
```

**Play Alarm Sound**:
```bash
python3 scripts/mqtt_send.py play sound.mp3
```

**Upload New Audio File**:
```bash
# With compression
python3 scripts/mqtt_audiochunkupload.py myaudio.mp3 --bitrate 32k

# Without compression
python3 scripts/mqtt_audiochunkupload.py myaudio.mp3 --bitrate 0
```

**Interactive Control**:
```bash
python3 scripts/audio_controller.py
```

**Build and Upload**:
```bash
pio run -e env_gateway_esp32 -t upload
pio run -e env_sensor_nodemcu -t upload
```

---

**End of Project Summary**
