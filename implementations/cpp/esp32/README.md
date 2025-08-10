# 🤖 EthervoxAI ESP32 C++ Implementation

High-performance C++ implementation for ESP32 microcontrollers using PlatformIO.

## 🎯 Features

- **🧠 Local AI Inference**: TensorFlow Lite Micro integration
- **🔒 Privacy-First**: All processing on-device
- **⚡ Optimized Performance**: Hardware-specific optimizations
- **🌐 Multi-Language**: Voice processing for multiple languages
- **📡 Connectivity**: WiFi and Bluetooth support
- **🔋 Power Management**: Advanced power optimization modes

## 🛠️ Development Setup

### Prerequisites

1. **PlatformIO Core** or **PlatformIO IDE**:
   ```bash
   # Option 1: VS Code Extension (Recommended)
   # Install "PlatformIO IDE" extension in VS Code
   
   # Option 2: CLI Installation
   pip install platformio
   ```

2. **ESP32 Board**: Any ESP32 development board (ESP32-DevKitC, NodeMCU-32S, etc.)

### Quick Start

1. **Clone and Navigate**:
   ```bash
   cd implementations/cpp/esp32
   ```

2. **Build Project**:
   ```bash
   pio run
   ```

3. **Upload to ESP32**:
   ```bash
   pio run --target upload
   ```

4. **Monitor Serial**:
   ```bash
   pio device monitor
   ```

## 📁 Project Structure

```
esp32/
├── platformio.ini          # PlatformIO configuration
├── src/
│   ├── main.cpp            # Main application entry
│   ├── platform_detector.cpp
│   ├── inference_engine.cpp
│   ├── audio_manager.cpp
│   └── wifi_manager.cpp
├── include/
│   ├── platform_detector.h
│   ├── inference_engine.h
│   ├── audio_manager.h
│   └── config.h
├── lib/
│   └── EthervoxAI/         # Core library
├── test/
│   └── test_platform.cpp
└── data/
    └── models/             # AI models (ONNX/TFLite)
```

## ⚙️ Configuration

### PlatformIO Configuration (`platformio.ini`)

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino, espidf
monitor_speed = 115200

# Build flags for optimization
build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -DETHERVOX_ESP32
    -DTENSORFLOW_LITE_MICRO
    -Os  # Optimize for size

# Libraries
lib_deps = 
    tanakamasayuki/ESP32 BLE Keyboard@^0.3.2
    bblanchon/ArduinoJson@^6.21.3
    knolleary/PubSubClient@^2.8
    # TensorFlow Lite Micro (when available)

# Upload settings
upload_speed = 921600
upload_port = /dev/ttyUSB0  # Adjust for your system

# Monitor settings
monitor_filters = esp32_exception_decoder
```

### Hardware Configuration

```cpp
// config.h
#define WIFI_SSID_DEFAULT "EthervoxAI-Setup"
#define WIFI_PASSWORD_DEFAULT "ethervox2025"

// Audio pins
#define I2S_WS_PIN 25
#define I2S_SCK_PIN 26
#define I2S_SD_PIN 22

// LED indicators
#define STATUS_LED_PIN 2
#define PRIVACY_LED_PIN 4

// Memory allocation
#define MAX_MODEL_SIZE_KB 512
#define AUDIO_BUFFER_SIZE 1024
#define MAX_INFERENCE_TIME_MS 500
```

## 🧠 AI Model Integration

### TensorFlow Lite Micro Setup

```cpp
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

class InferenceEngine {
private:
    tflite::MicroErrorReporter error_reporter;
    tflite::AllOpsResolver resolver;
    const tflite::Model* model;
    tflite::MicroInterpreter* interpreter;
    uint8_t tensor_arena[8 * 1024]; // 8KB tensor arena
    
public:
    bool loadModel(const uint8_t* model_data, size_t model_size);
    bool runInference(float* input, float* output);
    void optimizeForESP32();
};
```

## 🔧 Performance Optimizations

### Memory Management

```cpp
// Optimized memory allocation for ESP32
class MemoryManager {
public:
    static void* allocateAligned(size_t size, size_t alignment = 4);
    static void deallocate(void* ptr);
    static size_t getAvailableHeap();
    static void defragmentHeap();
    
    // PSRAM support for larger models
    static bool initPSRAM();
    static void* allocatePSRAM(size_t size);
};
```

### Power Management

```cpp
class PowerManager {
public:
    enum PowerMode {
        HIGH_PERFORMANCE,   // 240MHz, all peripherals active
        BALANCED,          // 160MHz, optimized for efficiency  
        LOW_POWER,         // 80MHz, minimal peripherals
        DEEP_SLEEP         // Sleep between wake words
    };
    
    void setPowerMode(PowerMode mode);
    void enableWakeOnVoice();
    void manageCPUFrequency();
};
```

## 📡 Connectivity Features

### WiFi Manager

```cpp
class WiFiManager {
public:
    bool connect(const char* ssid, const char* password);
    void startAccessPoint();
    void enableOTA(); // Over-the-air updates
    bool syncTimeNTP();
    
    // Privacy features
    void enableAPMode(); // For setup without internet
    void disableCloudFeatures();
};
```

### Bluetooth Audio

```cpp
class BluetoothAudio {
public:
    bool initA2DP(); // Audio streaming
    bool initBLE();  // Low-energy communication
    void enableHandsFree();
    void streamAudioToDevice(const char* device_name);
};
```

## 🧪 Testing

### Unit Tests

```cpp
// test/test_platform.cpp
#include <unity.h>
#include "platform_detector.h"

void test_memory_detection() {
    auto caps = PlatformDetector::getCapabilities();
    TEST_ASSERT_GREATER_THAN(100000, caps.available_memory); // >100KB
}

void test_model_compatibility() {
    auto compat = PlatformDetector::checkModelCompatibility("tiny-llama", 150);
    TEST_ASSERT_TRUE(compat.is_compatible);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_memory_detection);
    RUN_TEST(test_model_compatibility);
    UNITY_END();
}
```

### Hardware-in-the-Loop Testing

```bash
# Run tests on actual hardware
pio test --environment esp32

# Upload and monitor test results
pio test --environment esp32 --verbose
```

## 🔒 Security Features

### Hardware Security

```cpp
class SecurityManager {
public:
    bool initSecureBoot();
    bool enableFlashEncryption();
    void generateSecureRandom(uint8_t* buffer, size_t length);
    
    // Privacy enforcement
    void erasePersonalData();
    bool validateModelSignature(const uint8_t* model_data);
};
```

## 📊 Monitoring & Debugging

### Real-time Monitoring

```cpp
class SystemMonitor {
public:
    struct SystemStats {
        uint32_t free_heap;
        uint32_t cpu_usage;
        float temperature;
        uint32_t inference_count;
        uint32_t average_inference_time;
    };
    
    SystemStats getStats();
    void logPerformanceMetrics();
    void sendTelemetryIfEnabled();
};
```

### Serial Debugging

```cpp
// Enable detailed logging
#define ETHERVOX_DEBUG 1

// Use ESP_LOG macros for different levels
ESP_LOGI("ETHERVOX", "Platform detection completed");
ESP_LOGD("ETHERVOX", "Available memory: %d KB", free_memory_kb);
ESP_LOGE("ETHERVOX", "Model loading failed: %s", error_msg);
```

## 🚀 Deployment

### OTA Updates

```cpp
class OTAManager {
public:
    bool checkForUpdates();
    bool downloadUpdate(const char* url);
    bool verifyAndInstall();
    void rollbackOnFailure();
    
    // Privacy-conscious updates
    bool updateFromLocalServer(IPAddress server_ip);
};
```

### Production Build

```bash
# Optimized production build
pio run -e esp32 --build-flags="-DPRODUCTION_BUILD -Os -DNDEBUG"

# Generate firmware binary
pio run -e esp32 --target buildfs  # Build filesystem
pio run -e esp32 --target buildprog # Build program
```

## 📚 Model Catalog

### Supported Models

| Model | Size | Use Case | Performance |
|-------|------|----------|-------------|
| `keyword-detector` | 20KB | Wake word detection | Excellent |
| `intent-classifier` | 80KB | Command classification | Very Good |
| `tiny-llama-q2` | 150KB | Basic conversation | Good |
| `multilingual-asr` | 300KB | Speech recognition | Good |

### Model Loading

```cpp
// Load model from SPIFFS/LittleFS
bool InferenceEngine::loadModelFromStorage(const char* model_path) {
    File model_file = SPIFFS.open(model_path, "r");
    if (!model_file) return false;
    
    size_t model_size = model_file.size();
    uint8_t* model_data = (uint8_t*)malloc(model_size);
    model_file.readBytes((char*)model_data, model_size);
    
    return loadModel(model_data, model_size);
}
```

## 🌐 Integration with Other Implementations

### Cross-Language Compatibility

```cpp
// Compatible with Python/TypeScript implementations
struct EthervoxMessage {
    uint32_t message_type;
    uint32_t payload_length;
    uint8_t payload[];
};

// WebSocket communication with main application
void sendCapabilitiesToHost() {
    auto caps = PlatformDetector::getCapabilities();
    JsonDocument doc;
    doc["platform"] = "esp32";
    doc["available_memory"] = caps.available_memory;
    doc["performance_tier"] = caps.performance_tier;
    websocket.sendJSON(doc);
}
```

## 🔧 Advanced Configuration

### Custom Board Support

```ini
# platformio.ini for custom ESP32 board
[env:custom_esp32]
platform = espressif32
board = esp32dev
framework = arduino, espidf

# Custom partition table
board_build.partitions = partitions_custom.csv

# Custom bootloader
board_build.bootloader = bootloader_custom.bin

# Hardware-specific flags
build_flags = 
    -DCUSTOM_BOARD_V2
    -DHAS_EXTERNAL_PSRAM
    -DAUDIO_CODEC_WM8960
```

### Performance Profiling

```cpp
// Built-in profiling
#include "esp_timer.h"

class Profiler {
public:
    void startTimer(const char* name);
    void endTimer(const char* name);
    void printReport();
    
private:
    std::map<std::string, uint64_t> timers;
};

// Usage
Profiler profiler;
profiler.startTimer("inference");
engine.runInference(input, output);
profiler.endTimer("inference");
```

---

## 🤝 Contributing

See the main [CONTRIBUTING.md](../../../CONTRIBUTING.md) for general guidelines.

### ESP32-Specific Guidelines

1. **Test on Real Hardware**: Always test on actual ESP32 devices
2. **Memory Constraints**: Keep memory usage under 300KB for models
3. **Power Efficiency**: Optimize for battery-powered use cases
4. **Documentation**: Document pin assignments and hardware requirements

## 📞 Support

- **Hardware Issues**: Check wiring and power supply first
- **Model Performance**: Try smaller models or adjust tensor arena size
- **Memory Issues**: Enable PSRAM or reduce buffer sizes
- **WiFi Problems**: Check antenna and signal strength

---

**🚀 Ready to build privacy-first AI on ESP32!**

*Last updated: August 2025*
