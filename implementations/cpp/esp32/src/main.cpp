/**
 * 🧠 EthervoxAI ESP32 Main Application
 * 
 * Demonstrates privacy-first AI processing on ESP32 microcontrollers
 * Integrates platform detection, model management, and inference engine
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "esp32_platform_detector.h"
#include "inference_engine.h"
#include "config.h"

using namespace ethervoxai;

// Global components
ESP32PlatformDetector detector;
InferenceEngine aiEngine;

// System state
bool systemInitialized = false;
unsigned long lastStatusUpdate = 0;
unsigned long lastInference = 0;

// Forward declarations
bool initializeSystem();
void displayCapabilities();
bool setupAIModel();
void updateSystemStatus();
void runSampleInference();
void handleSerialCommands();
void setupWiFiAP();
void printHelp();

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Welcome message
    Serial.println("🚀 EthervoxAI ESP32 - Privacy-First AI");
    Serial.println("=====================================");
    
    // Initialize components
    if (!initializeSystem()) {
        Serial.println("❌ System initialization failed!");
        return;
    }
    
    // Display system capabilities
    displayCapabilities();
    
    // Load and configure AI model
    if (!setupAIModel()) {
        Serial.println("⚠️  AI model setup failed - running in limited mode");
    }
    
    // Setup complete
    systemInitialized = true;
    Serial.println("✅ EthervoxAI ESP32 ready!");
    Serial.println("🔒 All processing happens locally on device");
    Serial.println();
}

void loop() {
    if (!systemInitialized) {
        delay(1000);
        return;
    }
    
    // Regular status updates
    if (millis() - lastStatusUpdate > STATUS_UPDATE_INTERVAL_MS) {
        updateSystemStatus();
        lastStatusUpdate = millis();
    }
    
    // Simulate AI inference (replace with real input)
    if (millis() - lastInference > INFERENCE_INTERVAL_MS) {
        runSampleInference();
        lastInference = millis();
    }
    
    // Handle any incoming commands
    handleSerialCommands();
    
    // Small delay to prevent watchdog issues
    delay(10);
}

bool initializeSystem() {
    Serial.println("🔧 Initializing system components...");
    
    // Initialize SPIFFS for model storage
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ SPIFFS initialization failed");
        return false;
    }
    Serial.println("✅ SPIFFS initialized");
    
    // Initialize platform detector
    detector.refreshCapabilities();
    Serial.println("✅ Platform detector initialized");
    
    // Configure memory for AI workloads
    esp32_utils::configureHeapForAI();
    Serial.println("✅ Memory optimized for AI");
    
    // Initialize WiFi in AP mode for setup (no internet required)
    if (ENABLE_WIFI_SETUP) {
        setupWiFiAP();
    }
    
    // Initialize AI inference engine
    if (!aiEngine.initialize()) {
        Serial.println("⚠️  AI engine initialization failed");
        return false;
    }
    Serial.println("✅ AI engine initialized");
    
    return true;
}

void displayCapabilities() {
    Serial.println("📊 ESP32 System Capabilities:");
    Serial.println("============================");
    
    auto caps = detector.getCapabilities();
    
    Serial.printf("🔧 Chip: %s (Rev %d)\n", caps.chip_model.c_str(), caps.chip_revision);
    Serial.printf("💾 Memory: %d KB total, %d KB available\n", 
                  caps.total_memory_kb, caps.available_memory_kb);
    
    if (caps.has_psram) {
        Serial.printf("🧠 PSRAM: %d KB available\n", caps.psram_size_kb);
    }
    
    Serial.printf("💽 Flash: %d MB\n", caps.flash_size_mb);
    Serial.printf("⚡ CPU: %d cores @ %d MHz\n", caps.cpu_cores, caps.cpu_freq_mhz);
    Serial.printf("📶 Performance: %s\n", caps.performance_tier.c_str());
    Serial.printf("🧠 Max Model: %d KB\n", caps.max_model_size_kb);
    Serial.printf("📝 Max Context: %d tokens\n", caps.max_context_length);
    
    // Connectivity
    Serial.printf("📡 WiFi: %s\n", caps.has_wifi ? "Available" : "Not available");
    Serial.printf("📶 Bluetooth: %s\n", caps.has_bluetooth ? "Available" : "Not available");
    
    // Audio capabilities
    if (caps.has_i2s) {
        Serial.printf("🎵 Audio: I2S @ %d Hz max\n", caps.max_sample_rate);
    }
    
    Serial.println();
}

bool setupAIModel() {
    Serial.println("🧠 Setting up AI model...");
    
    // Check available models
    auto recommendations = detector.getRecommendedModels();
    JsonArray models = recommendations["models"];
    
    if (models.size() == 0) {
        Serial.println("❌ No compatible models found");
        return false;
    }
    
    // Try to load the first recommended model
    for (JsonObject model : models) {
        String modelName = model["name"];
        Serial.printf("🔄 Attempting to load model: %s\n", modelName.c_str());
        
        if (aiEngine.loadModel(modelName)) {
            Serial.printf("✅ Model loaded successfully: %s\n", modelName.c_str());
            return true;
        }
    }
    
    Serial.println("❌ Failed to load any compatible model");
    return false;
}

void setupWiFiAP() {
    Serial.println("📡 Setting up WiFi Access Point...");
    
    // Create AP for device setup (no internet required)
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.printf("✅ WiFi AP started: %s\n", WIFI_AP_SSID);
    Serial.printf("🔗 Connect to: http://%s\n", IP.toString().c_str());
    Serial.println("🔒 No internet connection required");
}

void runSampleInference() {
    // Sample data for testing (replace with real audio/text input)
    float input_data[INPUT_TENSOR_SIZE];
    float output_data[OUTPUT_TENSOR_SIZE];
    
    // Fill with sample data
    for (int i = 0; i < INPUT_TENSOR_SIZE; i++) {
        input_data[i] = sin(i * 0.1) * 0.5; // Sample sine wave
    }
    
    // Run inference
    unsigned long start_time = millis();
    bool success = aiEngine.runInference(input_data, output_data);
    unsigned long inference_time = millis() - start_time;
    
    if (success) {
        Serial.printf("🧠 Inference completed in %lu ms\n", inference_time);
        
        // Display top result
        float max_confidence = 0.0;
        int max_index = 0;
        for (int i = 0; i < OUTPUT_TENSOR_SIZE; i++) {
            if (output_data[i] > max_confidence) {
                max_confidence = output_data[i];
                max_index = i;
            }
        }
        
        Serial.printf("📊 Top result: Class %d (%.2f confidence)\n", 
                      max_index, max_confidence);
    } else {
        Serial.println("❌ Inference failed");
    }
}

void updateSystemStatus() {
    // Get current system status
    uint32_t free_memory = detector.getAvailableMemoryKB();
    float cpu_temp = detector.getCPUTemperature();
    auto perf_stats = detector.getPerformanceStats();
    
    Serial.printf("📊 Status - Memory: %d KB free", free_memory);
    
    if (cpu_temp > 0) {
        Serial.printf(", CPU: %.1f°C", cpu_temp);
    }
    
    Serial.println();
    
    // Check for memory pressure
    if (free_memory < MIN_MEMORY_THRESHOLD_KB) {
        Serial.println("⚠️  Low memory warning - running cleanup");
        esp32_utils::emergencyMemoryCleanup();
    }
    
    // Check for overheating
    if (cpu_temp > MAX_CPU_TEMP_C) {
        Serial.println("🌡️  High temperature - reducing CPU frequency");
        detector.setPowerMode("low_power");
    }
}

void handleSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command == "status") {
            displayCapabilities();
        }
        else if (command == "memory") {
            auto memory_report = esp32_utils::getMemoryReport();
            serializeJson(memory_report, Serial);
            Serial.println();
        }
        else if (command == "models") {
            auto models_doc = detector.getRecommendedModels();
            JsonArray models = models_doc["models"];
            for (JsonObject model : models) {
                Serial.printf("📦 %s - %s (%s)\n", 
                            model["name"].as<String>().c_str(),
                            model["size"].as<String>().c_str(),
                            model["reason"].as<String>().c_str());
            }
        }
        else if (command.startsWith("power ")) {
            String mode = command.substring(6);
            detector.setPowerMode(mode);
            Serial.printf("⚡ Power mode set to: %s\n", mode.c_str());
        }
        else if (command == "inference") {
            runSampleInference();
        }
        else if (command == "help") {
            printHelp();
        }
        else {
            Serial.printf("❓ Unknown command: %s (type 'help' for commands)\n", 
                         command.c_str());
        }
    }
}

void printHelp() {
    Serial.println("🔧 Available Commands:");
    Serial.println("=====================");
    Serial.println("status     - Show system capabilities");
    Serial.println("memory     - Show detailed memory report");
    Serial.println("models     - List recommended AI models");
    Serial.println("power <mode> - Set power mode (high_performance/balanced/low_power)");
    Serial.println("inference  - Run sample AI inference");
    Serial.println("help       - Show this help message");
    Serial.println();
}

#if defined(ESP_PLATFORM) && defined(ARDUINO_ARCH_ESP32)
// When using ESP-IDF with Arduino framework, provide app_main that delegates to Arduino
extern "C" void app_main() {
    // Arduino framework will handle setup() and loop()
    initArduino();
    setup();
    while(true) {
        loop();
        vTaskDelay(1);
    }
}
#endif
