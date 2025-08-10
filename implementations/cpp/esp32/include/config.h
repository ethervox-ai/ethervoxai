/**
 * 🔧 EthervoxAI ESP32 Configuration
 * 
 * Hardware and software configuration constants
 * Adjust these values based on your ESP32 board and requirements
 */

#pragma once

// ================ Hardware Configuration ================

// WiFi Access Point Settings (for device setup)
#define WIFI_AP_SSID "EthervoxAI-ESP32"
#define WIFI_AP_PASSWORD "ethervox2025"
#define ENABLE_WIFI_SETUP true

// Audio Configuration (I2S pins)
#define I2S_WS_PIN 25        // Word Select (LRC)
#define I2S_SCK_PIN 26       // Serial Clock (BCLK)
#define I2S_SD_PIN 22        // Serial Data (DIN)
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_BITS_PER_SAMPLE 16

// LED Indicators
#define STATUS_LED_PIN 2     // Built-in LED
#define PRIVACY_LED_PIN 4    // Privacy indicator LED
#define ERROR_LED_PIN 5      // Error indicator LED

// Button Inputs
#define WAKE_BUTTON_PIN 0    // Boot button for wake word
#define MODE_BUTTON_PIN 12   // Mode selection button

// ================ Memory Configuration ================

// Memory Allocation Limits
#define MAX_MODEL_SIZE_KB 512           // Maximum AI model size
#define TENSOR_ARENA_SIZE_KB 8          // TensorFlow Lite tensor arena
#define AUDIO_BUFFER_SIZE_SAMPLES 1024  // Audio processing buffer
#define MIN_MEMORY_THRESHOLD_KB 50      // Low memory warning threshold

// PSRAM Configuration (if available)
#define PREFER_PSRAM_FOR_MODELS true
#define PSRAM_CACHE_SIZE_KB 64

// ================ AI Model Configuration ================

// Tensor Dimensions (adjust based on your models)
#define INPUT_TENSOR_SIZE 80        // Input feature vector size
#define OUTPUT_TENSOR_SIZE 10       // Number of output classes
#define MAX_SEQUENCE_LENGTH 128     // Maximum input sequence length

// Model Performance Settings
#define MAX_INFERENCE_TIME_MS 500   // Maximum allowed inference time
#define INFERENCE_TIMEOUT_MS 1000   // Inference timeout
#define MODEL_CACHE_SIZE_KB 64      // Model caching

// ================ Performance Configuration ================

// CPU Frequency Settings (MHz)
#define CPU_FREQ_HIGH_PERFORMANCE 240
#define CPU_FREQ_BALANCED 160
#define CPU_FREQ_LOW_POWER 80

// Task Priorities (FreeRTOS)
#define AI_TASK_PRIORITY 5
#define AUDIO_TASK_PRIORITY 6
#define NETWORK_TASK_PRIORITY 3
#define MONITORING_TASK_PRIORITY 1

// Timing Configuration
#define STATUS_UPDATE_INTERVAL_MS 5000    // Status update frequency
#define INFERENCE_INTERVAL_MS 2000        // Demo inference frequency
#define MEMORY_CHECK_INTERVAL_MS 10000    // Memory monitoring frequency

// ================ Safety Configuration ================

// Temperature Limits
#define MAX_CPU_TEMP_C 80.0        // CPU temperature warning threshold
#define CRITICAL_CPU_TEMP_C 90.0   // Emergency shutdown temperature

// Watchdog Configuration
#define WATCHDOG_TIMEOUT_MS 10000  // Watchdog timeout
#define AI_WATCHDOG_TIMEOUT_MS 2000 // AI inference watchdog

// Memory Safety
#define EMERGENCY_MEMORY_THRESHOLD_KB 20  // Emergency cleanup threshold
#define STACK_SIZE_AI_TASK 8192          // AI task stack size
#define STACK_SIZE_AUDIO_TASK 4096       // Audio task stack size

// ================ Privacy Configuration ================

// Privacy Features
#define ENABLE_PRIVACY_INDICATORS true
#define ENABLE_DATA_ENCRYPTION true
#define ENABLE_SECURE_BOOT false        // Set to true for production
#define ENABLE_FLASH_ENCRYPTION false   // Set to true for production

// Data Retention
#define MAX_AUDIO_HISTORY_SAMPLES 0     // Don't store audio by default
#define CLEAR_INFERENCE_CACHE_ON_BOOT true
#define ENABLE_SECURE_ERASE true

// ================ Debug Configuration ================

// Logging Levels
#ifdef DEBUG_BUILD
    #define LOG_LEVEL_PLATFORM LOG_LEVEL_DEBUG
    #define LOG_LEVEL_AI LOG_LEVEL_DEBUG
    #define LOG_LEVEL_AUDIO LOG_LEVEL_DEBUG
#else
    #define LOG_LEVEL_PLATFORM LOG_LEVEL_INFO
    #define LOG_LEVEL_AI LOG_LEVEL_INFO
    #define LOG_LEVEL_AUDIO LOG_LEVEL_WARN
#endif

// Debug Features
#define ENABLE_SERIAL_COMMANDS true
#define ENABLE_PERFORMANCE_MONITORING true
#define ENABLE_MEMORY_DEBUGGING false    // Only for development

// Serial Configuration
#define SERIAL_BAUD_RATE 115200
#define SERIAL_TIMEOUT_MS 100

// ================ Model Catalog Configuration ================

// Default Models (stored in SPIFFS)
#define DEFAULT_KEYWORD_MODEL "keyword_detector_v1.tflite"
#define DEFAULT_INTENT_MODEL "intent_classifier_v1.tflite"
#define DEFAULT_VOICE_MODEL "voice_activity_detector_v1.tflite"

// Model File Paths
#define MODELS_PATH "/models/"
#define CONFIG_PATH "/config/"
#define CACHE_PATH "/cache/"

// Model Metadata
#define MODEL_VERSION_FILE "/models/version.json"
#define MODEL_MANIFEST_FILE "/models/manifest.json"

// ================ Network Configuration ================

// Access Point Settings
#define AP_MAX_CONNECTIONS 4
#define AP_CHANNEL 1
#define AP_HIDDEN false

// Web Server Settings (for setup interface)
#define WEB_SERVER_PORT 80
#define WEBSOCKET_PORT 81
#define WEB_SERVER_TIMEOUT_MS 5000

// OTA Update Settings
#define ENABLE_OTA_UPDATES false        // Disable by default for security
#define OTA_SERVER_PORT 8080
#define OTA_USERNAME "admin"
#define OTA_PASSWORD "ethervox2025"

// ================ Audio Processing Configuration ================

// Voice Activity Detection
#define VAD_THRESHOLD 0.5               // Voice activity threshold
#define VAD_WINDOW_SIZE_MS 30           // VAD analysis window
#define SILENCE_TIMEOUT_MS 2000         // Stop recording after silence

// Audio Preprocessing
#define ENABLE_NOISE_REDUCTION true
#define ENABLE_AUTOMATIC_GAIN_CONTROL true
#define ENABLE_ECHO_CANCELLATION false

// Audio Format
#define AUDIO_CHANNELS 1                // Mono audio
#define AUDIO_FORMAT I2S_SLOT_MODE_MONO
#define DMA_BUFFER_COUNT 2
#define DMA_BUFFER_LEN 512

// ================ Build Configuration ================

// Version Information
#define ETHERVOX_VERSION_MAJOR 1
#define ETHERVOX_VERSION_MINOR 0
#define ETHERVOX_VERSION_PATCH 0
#define ETHERVOX_BUILD_DATE __DATE__
#define ETHERVOX_BUILD_TIME __TIME__

// Feature Flags
#define FEATURE_MULTILINGUAL_SUPPORT true
#define FEATURE_OFFLINE_MODE true
#define FEATURE_VOICE_COMMANDS true
#define FEATURE_INTENT_RECOGNITION true
#define FEATURE_KEYWORD_SPOTTING true

// Board-Specific Configuration
#ifdef ETHERVOX_ESP32S3
    #undef MAX_MODEL_SIZE_KB
    #define MAX_MODEL_SIZE_KB 1024      // More memory available
    #undef TENSOR_ARENA_SIZE_KB
    #define TENSOR_ARENA_SIZE_KB 16     // Larger tensor arena
#endif

#ifdef ETHERVOX_ESP32C3
    #undef MAX_MODEL_SIZE_KB
    #define MAX_MODEL_SIZE_KB 128       // Less memory available
    #undef TENSOR_ARENA_SIZE_KB
    #define TENSOR_ARENA_SIZE_KB 4      // Smaller tensor arena
#endif

// ================ Utility Macros ================

// Logging Macros
#define LOG_ETHERVOX(level, format, ...) \
    Serial.printf("[%s] " format "\n", level, ##__VA_ARGS__)

#define LOG_INFO(format, ...) LOG_ETHERVOX("INFO", format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) LOG_ETHERVOX("WARN", format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) LOG_ETHERVOX("ERROR", format, ##__VA_ARGS__)

#ifdef DEBUG_BUILD
    #define LOG_DEBUG(format, ...) LOG_ETHERVOX("DEBUG", format, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(format, ...)
#endif

// Memory Allocation Macros
#define MALLOC_CAP_SPIRAM_FIRST (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define MALLOC_CAP_INTERNAL_FIRST (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)

// Timing Utilities
#define BENCHMARK_START() unsigned long _benchmark_start = millis()
#define BENCHMARK_END(name) \
    LOG_DEBUG("%s took %lu ms", name, millis() - _benchmark_start)

// Error Handling
#define CHECK_ERROR(x, msg) \
    do { \
        if (!(x)) { \
            LOG_ERROR("Error: %s", msg); \
            return false; \
        } \
    } while(0)

#define CHECK_ERROR_RETURN(x, msg, ret) \
    do { \
        if (!(x)) { \
            LOG_ERROR("Error: %s", msg); \
            return ret; \
        } \
    } while(0)
