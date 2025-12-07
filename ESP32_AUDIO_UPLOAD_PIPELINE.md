# ESP32 Audio Upload Pipeline

This document describes the complete pipeline for uploading audio files from the frontend to the ESP32 device.

## Architecture Overview

```
Frontend (React) → Backend API → MinIO S3 → ESP32 (HTTP Download)
                         ↓
                    MQTT Broker (HiveMQ)
                         ↓
                       ESP32
```

## Pipeline Steps

### 1. Frontend Upload Initiation
**Location**: `fe-alarm/src/components/audio/MqttAudioUploadDialog.tsx`

- User selects an MP3 file through the dialog
- File is sent via `FormData` to backend endpoint
- Progress polling starts (500ms intervals)

**API Endpoint**: `POST /api/esp32/upload-audio`

### 2. Backend Processing
**Location**: `fe-alarm-backend/src/app/api/esp32/upload-audio/route.ts`

#### Step 2.1: File Reception & Validation
- Receives multipart/form-data
- Generates unique soundId (e.g., `EbBW3i3L`)
- Progress: **10%**

#### Step 2.2: Upload to MinIO S3
- Converts file to Buffer
- Uploads to MinIO using AWS S3 SDK
  - **Bucket**: `audio`
  - **Key**: `esp32-audio/sound_{soundId}.mp3`
  - **Example**: `esp32-audio/sound_EbBW3i3L.mp3`
- Progress: **30%**

**MinIO Configuration**:
```
Host: 172.20.10.2
S3 API Port: 9000 (or 9011 depending on Docker mapping)
Console Port: 9001
Credentials: minioadmin/minioadmin
```

#### Step 2.3: Generate Download URL
- Constructs public URL for ESP32 to download
- **Format**: `http://172.20.10.2:9000/audio/esp32-audio/sound_{soundId}.mp3`
- **Example**: `http://172.20.10.2:9000/audio/esp32-audio/sound_EbBW3i3L.mp3`
- Progress: **50%**

#### Step 2.4: Subscribe to ESP32 Response
- Subscribes to MQTT topic: `esp32/audio/status`
- Sets up message handler to listen for ESP32 confirmation
- Progress: **60%**

#### Step 2.5: Publish Download Command
- Publishes to MQTT topic: `esp32/audio_download_cmd`
- **Message Format**: `{url}|{soundId}`
- **Example**: `http://172.20.10.2:9000/audio/esp32-audio/sound_EbBW3i3L.mp3|EbBW3i3L`
- QoS: 1 (at least once delivery)
- Progress: **70%**

#### Step 2.6: Wait for ESP32 Response
- Listens on `esp32/audio/status` topic
- **Expected Response Format**:
  - Success: `download_success|{soundId}`
  - Failure: `download_failed|{soundId}`
- Timeout: 30 seconds
- Progress: **100%** (on success)

### 3. ESP32 Download Process
**Required ESP32 Implementation**:

#### Step 3.1: MQTT Connection
- Connect to MQTT broker: `broker.hivemq.com:1883`
- Subscribe to topic: `esp32/audio_download_cmd`

#### Step 3.2: Receive Download Command
- Parse message: Split by `|` character
  - `parts[0]`: Download URL
  - `parts[1]`: Sound ID

**Example C++ Code**:
```cpp
void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, "esp32/audio_download_cmd") == 0) {
    String message = "";
    for (int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    
    int separatorIndex = message.indexOf('|');
    String url = message.substring(0, separatorIndex);
    String soundId = message.substring(separatorIndex + 1);
    
    downloadAudio(url, soundId);
  }
}
```

#### Step 3.3: HTTP Download
- Make HTTP GET request to the provided URL
- Save MP3 file to ESP32 storage (SPIFFS/SD card)
- Filename: `sound_{soundId}.mp3`

**Example C++ Code**:
```cpp
void downloadAudio(String url, String soundId) {
  HTTPClient http;
  http.begin(url);
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    File file = SPIFFS.open("/sound_" + soundId + ".mp3", "w");
    http.writeToStream(&file);
    file.close();
    
    // Publish success
    mqttClient.publish("esp32/audio/status", 
      ("download_success|" + soundId).c_str());
  } else {
    // Publish failure
    mqttClient.publish("esp32/audio/status", 
      ("download_failed|" + soundId).c_str());
  }
  http.end();
}
```

#### Step 3.4: Publish Status Response
- Publish to topic: `esp32/audio/status`
- **Success Message**: `download_success|{soundId}`
- **Failure Message**: `download_failed|{soundId}`

### 4. Backend Response Handling
- Receives ESP32 status message
- Validates soundId matches
- Updates progress to 100%
- Returns success/failure response to frontend
- Cleans up MQTT listeners and progress tracking

### 5. Frontend Completion
- Stops progress polling
- Shows success/failure message
- Enables preview/stop buttons (on success)

## MQTT Topics Summary

| Topic | Direction | Publisher | Subscriber | Message Format |
|-------|-----------|-----------|------------|----------------|
| `esp32/audio_download_cmd` | Backend → ESP32 | Backend API | ESP32 | `{url}\|{soundId}` |
| `esp32/audio/status` | ESP32 → Backend | ESP32 | Backend API | `download_success\|{soundId}` or `download_failed\|{soundId}` |
| `smartalarm/play_audio` | Backend → ESP32 | Backend API | ESP32 | `{filename}` or `stop` |

## Progress Tracking

| Stage | Progress % | Description |
|-------|-----------|-------------|
| Upload Start | 10% | File received by backend |
| S3 Upload Complete | 30% | File uploaded to MinIO |
| URL Generated | 50% | Download URL constructed |
| MQTT Subscribed | 60% | Listening for ESP32 response |
| MQTT Published | 70% | Download command sent to ESP32 |
| ESP32 Confirmed | 100% | ESP32 downloaded successfully |

## Error Handling

### Backend Errors
- **S3 Upload Failed**: Returns 500 with S3 error details
- **MQTT Publish Failed**: Returns 500 with MQTT error
- **ESP32 Timeout**: Returns 408 after 30 seconds
- **ESP32 Download Failed**: Returns 500 with ESP32 error message

### ESP32 Errors
- **Network Unreachable**: Cannot reach MinIO URL (172.20.10.2)
- **HTTP Error**: MinIO returns non-200 status
- **Storage Full**: ESP32 filesystem full
- **MQTT Disconnected**: Not connected to broker

## Network Requirements

### Backend Server
- Must run locally (not Vercel) to access MinIO
- Must reach MinIO at `172.20.10.2:9000`
- Must reach MQTT broker at `broker.hivemq.com:1883`

### ESP32 Device
- Must be on same local network as MinIO
- Must reach `172.20.10.2:9000` for HTTP downloads
- Must reach `broker.hivemq.com:1883` for MQTT
- Requires WiFi connection

### Frontend
- MQTT WebSocket: `ws://broker.hivemq.com:8000/mqtt`
- Backend API: Set via `NEXT_PUBLIC_API_BASE_URL`

## Testing Checklist

- [ ] MinIO accessible at `http://172.20.10.2:9001/browser/audio`
- [ ] Backend can upload files to MinIO bucket `audio`
- [ ] Backend can publish to `esp32/audio_download_cmd`
- [ ] ESP32 connected to WiFi
- [ ] ESP32 connected to MQTT broker
- [ ] ESP32 subscribed to `esp32/audio_download_cmd`
- [ ] ESP32 can reach `http://172.20.10.2:9000`
- [ ] ESP32 can download files via HTTP
- [ ] ESP32 publishes to `esp32/audio/status`
- [ ] Backend receives ESP32 status messages
- [ ] Frontend shows progress updates
- [ ] Preview/Stop buttons work after upload

## Debugging Tips

### Monitor MQTT Traffic
Use MQTT Explorer to monitor:
- Topic: `esp32/audio_download_cmd` (commands from backend)
- Topic: `esp32/audio/status` (responses from ESP32)

### Check MinIO
Access MinIO console: `http://172.20.10.2:9001`
- Username: `minioadmin`
- Password: `minioadmin`
- Navigate to `audio` bucket → `esp32-audio/` folder

### Backend Logs
Look for these log messages:
```
[S3 Upload Success] Bucket: audio, Key: esp32-audio/sound_xxx.mp3
[MQTT Publish Success] Topic: esp32/audio_download_cmd, Command: {url}|{soundId}
[ESP32 Response] download_success|{soundId}
```

### ESP32 Serial Monitor
- Check WiFi connection status
- Check MQTT connection status
- Log received MQTT messages
- Log HTTP download progress
- Log file write operations

## File Locations

- Frontend Dialog: `fe-alarm/src/components/audio/MqttAudioUploadDialog.tsx`
- Backend API: `fe-alarm-backend/src/app/api/esp32/upload-audio/route.ts`
- Progress Endpoint: `fe-alarm-backend/src/app/api/esp32/upload-progress/[soundId]/route.ts`
- S3 Client Config: `fe-alarm-backend/src/lib/s3-client.ts`
- MQTT Client: `fe-alarm-backend/src/lib/mqttClient.ts`
