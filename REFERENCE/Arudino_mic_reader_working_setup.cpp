#include <Arduino.h>
#include <driver/i2s.h>
#include <SD.h>
#include <SPI.h>

// --- PIN DEFINITIONS ---
// SD Card Pins
#define SD_CS_PIN    5
#define SD_MOSI_PIN  23
#define SD_MISO_PIN  19
#define SD_CLK_PIN   18

// I2S Pins (INMP441)
#define I2S_WS       33
#define I2S_SD       34
#define I2S_SCK      32

// Audio Configuration
#define SAMPLE_RATE       16000
#define I2S_READ_LEN      1024  // Number of samples to read at once

File wavFile;

// Global scope
static int32_t i2s_buffer[I2S_READ_LEN];
static int16_t file_buffer[I2S_READ_LEN];

// --- WAV HEADER FUNCTION ---
// This writes the standard WAV header for 16-bit Mono audio
void writeWavHeader(File &file, int numSamples) {
    uint32_t sampleRate = SAMPLE_RATE;
    uint16_t bitsPerSample = 16;      // We are converting to 16-bit before writing
    uint16_t numChannels = 1;         // Mono
    uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
    uint32_t dataSize = numSamples * numChannels * bitsPerSample / 8;
    uint32_t chunkSize = 36 + dataSize;
    uint16_t audioFormat = 1;         // PCM
    uint16_t blockAlign = numChannels * bitsPerSample / 8;

    file.seek(0);
    file.write((const uint8_t *)"RIFF", 4);
    file.write((uint8_t*)&chunkSize, 4);
    file.write((const uint8_t *)"WAVE", 4);
    file.write((const uint8_t *)"fmt ", 4);
    
    uint32_t subchunk1Size = 16;
    file.write((uint8_t*)&subchunk1Size, 4);
    file.write((uint8_t*)&audioFormat, 2);
    file.write((uint8_t*)&numChannels, 2);
    file.write((uint8_t*)&sampleRate, 4);
    file.write((uint8_t*)&byteRate, 4);
    file.write((uint8_t*)&blockAlign, 2);
    file.write((uint8_t*)&bitsPerSample, 2);
    
    file.write((const uint8_t *)"data", 4);
    file.write((uint8_t*)&dataSize, 4);
}

bool initSD(int maxRetries = 5) { Serial.println("\n[SD] Initializing SD card..."); pinMode(SD_CS_PIN, OUTPUT); digitalWrite(SD_CS_PIN, HIGH); pinMode(SD_MISO_PIN, INPUT_PULLUP); delay(50); SPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN); for (int attempt = 1; attempt <= maxRetries; attempt++) { if (SD.begin(SD_CS_PIN, SPI, 1000000)) { Serial.println("[SD] OK @4MHz"); return true; } if (SD.begin(SD_CS_PIN, SPI, 1000000)) { Serial.println("[SD] OK @1MHz"); return true; } delay(500); } Serial.println("[SD] FAIL"); return false; }

void setup() {
    Serial.begin(115200);
    
    initSD(5);

    // --- I2S CONFIGURATION ---
    // IMPORTANT: We configure for 32-bit reading to handle the INMP441 correctly
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // FIXED: Read 32 bits
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // FIXED: Try LEFT first (L/R pin -> GND)
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S), // Standard I2S
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,   // Smaller DMA buffers often help reduce latency
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,
        .data_in_num = I2S_SD
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    // Discard the first few milliseconds of data to stabilize the microphone
    i2s_start(I2S_NUM_0);

    Serial.println("Ready! Type: filename duration");
    Serial.println("Example: test 3");
}

void loop() {
    if (Serial.available()) {
        String name = Serial.readStringUntil(' ');
        int duration = Serial.parseInt();
        
        // Clear buffer junk
        while(Serial.available()) Serial.read();

        if (duration <= 0) return;

        String filename = "/" + name + ".wav";
        Serial.printf("Recording %s for %d sec...\n", filename.c_str(), duration);
        
        // Remove existing file
        if (SD.exists(filename)) SD.remove(filename);

        wavFile = SD.open(filename, FILE_WRITE);
        if (!wavFile) {
            Serial.println("File open failed!");
            return;
        }

        // Placeholder for WAV header (44 bytes)
        uint8_t header[44] = {0};
        wavFile.write(header, 44);

        size_t bytesRead;
        unsigned long totalBytesWritten = 0;
        unsigned long startTime = millis();
        unsigned long endTime = startTime + (duration * 1000);

        while (millis() < endTime) {
            // 1. Read 32-bit samples
            // Note: sizeof(i2s_buffer) is number of BYTES, not number of samples
            i2s_read(I2S_NUM_0, (void*)i2s_buffer, sizeof(i2s_buffer), &bytesRead, portMAX_DELAY);
            
            int samplesRead = bytesRead / 4; // 4 bytes per 32-bit sample

            // 2. Process and Convert
            for (int i = 0; i < samplesRead; i++) {
                // The INMP441 is 24-bit. The data is in the top 24 bits of the 32-bit word.
                // We want 16-bit output.
                // Shifting right by 14 gives a good volume boost (gain) without too much clipping.
                // Mathematically correct is >> 16, but that is often too quiet.
                
                int32_t val = i2s_buffer[i];
                val = val >> 14; 

                // hard clipping to prevent overflow wrapping (static)
                if (val > 32767) val = 32767;
                if (val < -32768) val = -32768;

                file_buffer[i] = (int16_t)val;
            }

            // 3. Write 16-bit samples to SD
            wavFile.write((uint8_t*)file_buffer, samplesRead * sizeof(int16_t));
            totalBytesWritten += samplesRead * sizeof(int16_t);
        }

        // Finalize header
        // Total samples = total bytes / 2 (since 16-bit = 2 bytes)
        writeWavHeader(wavFile, totalBytesWritten / 2);
        wavFile.close();
        
        Serial.println("Done recording!");
    }
}