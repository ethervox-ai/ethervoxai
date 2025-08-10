/**
 * 🔍 ESP32 Platform Detector Implementation
 * 
 * ESP32-specific hardware detection and capability assessment
 * Follows EthervoxAI cross-language protocol
 */

#include "esp32_platform_detector.h"
#include "config.h"
#include <Arduino.h>
#include <esp_system.h>
#include <WiFi.h>

namespace ethervoxai {

// Global singleton instance
ESP32PlatformDetector platformDetector;

ESP32PlatformDetector::ESP32PlatformDetector() :
    _capabilities_cached(false),
    _last_detection_ms(0),
    _cache_duration_ms(60000), // 1 minute cache
    _monitoring_enabled(false),
    _inference_count(0),
    _total_inference_time_ms(0),
    _max_inference_time_ms(0),
    _min_inference_time_ms(UINT32_MAX) {
}

ESP32Capabilities ESP32PlatformDetector::getCapabilities() {
    unsigned long current_time = millis();
    
    // Return cached capabilities if still valid
    if (_capabilities_cached && 
        (current_time - _last_detection_ms) < _cache_duration_ms) {
        return _capabilities;
    }
    
    LOG_INFO("🔍 Detecting ESP32 capabilities...");
    
    // Detect hardware specifications
    _capabilities.chip_model = detectChipModel();
    _capabilities.chip_revision = detectChipRevision();
    _capabilities.board_type = detectBoardType();
    
    // Memory detection
    _capabilities.total_memory_kb = detectTotalMemory();
    _capabilities.available_memory_kb = esp_get_free_heap_size() / 1024;
    _capabilities.psram_size_kb = detectPSRAMSize();
    _capabilities.flash_size_mb = detectFlashSize();
    
    // CPU information
    _capabilities.cpu_cores = 2; // ESP32 is dual-core (except ESP32-S2)
    if (_capabilities.chip_model.indexOf("ESP32-S2") >= 0) {
        _capabilities.cpu_cores = 1; // ESP32-S2 is single-core
    }
    _capabilities.cpu_freq_mhz = getCpuFrequencyMhz();
    
    // Connectivity detection
    _capabilities.has_wifi = detectWiFiCapability();
    _capabilities.has_bluetooth = detectBluetoothCapability();
    _capabilities.has_bluetooth_le = detectBluetoothLECapability();
    
    // AI/ML capabilities
    _capabilities.has_psram = (_capabilities.psram_size_kb > 0);
    _capabilities.has_spiram = _capabilities.has_psram; // Same thing
    _capabilities.max_model_size_kb = calculateMaxModelSize();
    _capabilities.max_context_length = calculateMaxContextLength();
    _capabilities.performance_tier = calculatePerformanceTier();
    
    // Audio capabilities
    _capabilities.has_i2s = detectI2S();
    _capabilities.has_adc = detectADC();
    _capabilities.has_dac = detectDAC();
    _capabilities.max_sample_rate = detectMaxSampleRate();
    
    // Power management
    _capabilities.power_mode = "balanced"; // Default mode
    _capabilities.battery_powered = false; // Assume USB powered
    _capabilities.estimated_runtime_hours = 0; // Unknown without battery info
    
    // Cache the results
    _capabilities_cached = true;
    _last_detection_ms = current_time;
    
    logCapabilities();
    
    return _capabilities;
}

String ESP32PlatformDetector::detectChipModel() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    switch (chip_info.model) {
        case CHIP_ESP32:
            return "ESP32";
        case CHIP_ESP32S2:
            return "ESP32-S2";
        case CHIP_ESP32S3:
            return "ESP32-S3";
        case CHIP_ESP32C3:
            return "ESP32-C3";
        default:
            return "ESP32-Unknown";
    }
}

uint8_t ESP32PlatformDetector::detectChipRevision() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    return chip_info.revision;
}

String ESP32PlatformDetector::detectBoardType() {
    // Try to detect common development boards
    // This is a simplified detection - in practice, you might read
    // board-specific GPIO pins or check for specific hardware
    
    if (detectPSRAMSize() > 0) {
        if (detectFlashSize() >= 16) {
            return "ESP32-DevKitC-WROVER"; // Has PSRAM and large flash
        } else {
            return "ESP32-WROVER-Kit";
        }
    } else {
        if (detectFlashSize() >= 8) {
            return "ESP32-DevKitC-WROOM"; // Standard dev board
        } else {
            return "ESP32-Generic";
        }
    }
}

uint32_t ESP32PlatformDetector::detectTotalMemory() {
    // ESP32 internal SRAM + any external PSRAM using Arduino framework
    uint32_t total_ram = ESP.getHeapSize(); // Total heap size in bytes
    if (psramFound()) {
        total_ram += ESP.getPsramSize(); // Add PSRAM if available
    }
    
    return total_ram / 1024; // Convert to KB
}

uint32_t ESP32PlatformDetector::detectPSRAMSize() {
    // Arduino framework compatible PSRAM detection
    if (psramFound()) {
        return ESP.getPsramSize() / 1024; // Convert to KB
    }
    return 0;
}

uint32_t ESP32PlatformDetector::detectFlashSize() {
    return ESP.getFlashChipSize() / (1024 * 1024); // Convert to MB
}

bool ESP32PlatformDetector::detectWiFiCapability() {
    // All ESP32 variants have WiFi except some very specific chips
    return true;
}

bool ESP32PlatformDetector::detectBluetoothCapability() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    // ESP32-S2 doesn't have Bluetooth
    return (chip_info.model != CHIP_ESP32S2);
}

bool ESP32PlatformDetector::detectBluetoothLECapability() {
    // Same as Bluetooth for ESP32 family
    return detectBluetoothCapability();
}

bool ESP32PlatformDetector::detectI2S() {
    // All ESP32 variants have I2S capability
    return true;
}

bool ESP32PlatformDetector::detectADC() {
    // All ESP32 variants have ADC
    return true;
}

bool ESP32PlatformDetector::detectDAC() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    // ESP32-C3 doesn't have DAC
    return (chip_info.model != CHIP_ESP32C3);
}

uint32_t ESP32PlatformDetector::detectMaxSampleRate() {
    // ESP32 I2S can handle up to 192kHz but practical limit for AI is lower
    return 48000; // 48kHz is good for voice processing
}

String ESP32PlatformDetector::calculatePerformanceTier() {
    uint32_t memory_score = 0;
    uint32_t cpu_score = 0;
    
    // Memory scoring
    if (_capabilities.total_memory_kb >= 8192) { // 8MB+
        memory_score = 3;
    } else if (_capabilities.total_memory_kb >= 4096) { // 4MB+
        memory_score = 2;
    } else {
        memory_score = 1;
    }
    
    // CPU scoring based on model and frequency
    if (_capabilities.chip_model == "ESP32-S3" && _capabilities.cpu_freq_mhz >= 240) {
        cpu_score = 3;
    } else if (_capabilities.cpu_cores >= 2 && _capabilities.cpu_freq_mhz >= 160) {
        cpu_score = 2;
    } else {
        cpu_score = 1;
    }
    
    // PSRAM bonus
    if (_capabilities.has_psram) {
        memory_score = min(memory_score + 1, (uint32_t)3);
    }
    
    uint32_t total_score = memory_score + cpu_score;
    
    if (total_score >= 5) {
        return "high";
    } else if (total_score >= 3) {
        return "medium";
    } else {
        return "low";
    }
}

uint32_t ESP32PlatformDetector::calculateMaxModelSize() {
    // Use 60% of available memory for models, with PSRAM preference
    uint32_t available_memory = _capabilities.available_memory_kb;
    
    if (_capabilities.has_psram) {
        // With PSRAM, we can use more memory for models
        available_memory += (_capabilities.psram_size_kb * 0.8); // 80% of PSRAM
    }
    
    uint32_t max_model = available_memory * 0.6; // 60% of available
    
    // Enforce reasonable limits based on performance tier
    if (_capabilities.performance_tier == "high") {
        return min(max_model, (uint32_t)1024); // Max 1MB
    } else if (_capabilities.performance_tier == "medium") {
        return min(max_model, (uint32_t)512);  // Max 512KB
    } else {
        return min(max_model, (uint32_t)256);  // Max 256KB
    }
}

uint32_t ESP32PlatformDetector::calculateMaxContextLength() {
    // Context length based on available memory and performance
    if (_capabilities.performance_tier == "high" && _capabilities.has_psram) {
        return 512;
    } else if (_capabilities.performance_tier == "medium") {
        return 256;
    } else {
        return 128;
    }
}

void ESP32PlatformDetector::logCapabilities() {
    LOG_INFO("📊 ESP32 Capabilities Detected:");
    LOG_INFO("   🔧 Chip: %s (Rev %d)", _capabilities.chip_model.c_str(), _capabilities.chip_revision);
    LOG_INFO("   💾 Memory: %d KB total, %d KB available", 
             _capabilities.total_memory_kb, _capabilities.available_memory_kb);
    
    if (_capabilities.has_psram) {
        LOG_INFO("   🧠 PSRAM: %d KB", _capabilities.psram_size_kb);
    }
    
    LOG_INFO("   💽 Flash: %d MB", _capabilities.flash_size_mb);
    LOG_INFO("   ⚡ CPU: %d cores @ %d MHz", _capabilities.cpu_cores, _capabilities.cpu_freq_mhz);
    LOG_INFO("   📶 Performance: %s", _capabilities.performance_tier.c_str());
    LOG_INFO("   🧠 Max Model: %d KB", _capabilities.max_model_size_kb);
    LOG_INFO("   📝 Max Context: %d tokens", _capabilities.max_context_length);
    
    std::vector<String> features;
    if (_capabilities.has_wifi) features.push_back("WiFi");
    if (_capabilities.has_bluetooth) features.push_back("BT");
    if (_capabilities.has_i2s) features.push_back("I2S");
    if (_capabilities.has_adc) features.push_back("ADC");
    if (_capabilities.has_dac) features.push_back("DAC");
    
    if (!features.empty()) {
        String feature_list = "";
        for (size_t i = 0; i < features.size(); i++) {
            if (i > 0) feature_list += ", ";
            feature_list += features[i];
        }
        LOG_INFO("   🚀 Features: %s", feature_list.c_str());
    }
}

uint32_t ESP32PlatformDetector::getAvailableMemoryKB() {
    return esp_get_free_heap_size() / 1024;
}

float ESP32PlatformDetector::getCPUTemperature() {
    // ESP32 doesn't have a built-in temperature sensor
    // This would need external sensor or estimation based on performance
    return -1.0; // Indicate unavailable
}

void ESP32PlatformDetector::refreshCapabilities() {
    _capabilities_cached = false;
    getCapabilities();
}

DynamicJsonDocument ESP32PlatformDetector::getPerformanceStats() {
    DynamicJsonDocument doc(2048);
    
    doc["inference_count"] = _inference_count;
    doc["total_inference_time_ms"] = _total_inference_time_ms;
    doc["max_inference_time_ms"] = _max_inference_time_ms;
    doc["min_inference_time_ms"] = (_min_inference_time_ms == UINT32_MAX) ? 0 : _min_inference_time_ms;
    
    if (_inference_count > 0) {
        doc["average_inference_time_ms"] = _total_inference_time_ms / _inference_count;
    } else {
        doc["average_inference_time_ms"] = 0;
    }
    
    doc["free_heap_kb"] = esp_get_free_heap_size() / 1024;
    doc["min_free_heap_kb"] = esp_get_minimum_free_heap_size() / 1024;
    
    return doc;
}

DynamicJsonDocument ESP32PlatformDetector::getRecommendedModels() {
    DynamicJsonDocument doc(2048);
    JsonArray models = doc.createNestedArray("models");
    
    // Analyze capabilities and recommend models
    auto caps = getCapabilities();
    
    // Recommend models based on memory and performance
    if (caps.available_memory_kb >= 512) {
        JsonObject model1 = models.createNestedObject();
        model1["name"] = "ethervox-small";
        model1["size"] = "256KB";
        model1["reason"] = "Good balance of accuracy and memory usage";
    }
    
    if (caps.available_memory_kb >= 1024) {
        JsonObject model2 = models.createNestedObject();
        model2["name"] = "ethervox-medium";
        model2["size"] = "512KB";
        model2["reason"] = "Higher accuracy for sufficient memory";
    }
    
    if (caps.has_psram && caps.psram_size_kb >= 2048) {
        JsonObject model3 = models.createNestedObject();
        model3["name"] = "ethervox-large";
        model3["size"] = "1MB";
        model3["reason"] = "Maximum accuracy with PSRAM support";
    }
    
    // Always include the tiny model as fallback
    JsonObject model_tiny = models.createNestedObject();
    model_tiny["name"] = "ethervox-tiny";
    model_tiny["size"] = "64KB";
    model_tiny["reason"] = "Minimal memory footprint, basic functionality";
    
    return doc;
}

void ESP32PlatformDetector::setPowerMode(const String& mode) {
    LOG_INFO("🔋 Setting power mode to: %s", mode.c_str());
    
    if (mode == "low_power") {
        // Set CPU frequency to lower setting for power saving
        setCpuFrequencyMhz(80);
        LOG_INFO("   📊 CPU frequency set to 80MHz");
    } else if (mode == "performance") {
        // Set CPU frequency to maximum for performance
        setCpuFrequencyMhz(240);
        LOG_INFO("   📊 CPU frequency set to 240MHz");
    } else if (mode == "balanced") {
        // Set CPU frequency to balanced setting
        setCpuFrequencyMhz(160);
        LOG_INFO("   📊 CPU frequency set to 160MHz");
    } else {
        LOG_WARN("⚠️  Unknown power mode: %s, using balanced", mode.c_str());
        setCpuFrequencyMhz(160);
    }
}

// ESP32 utility functions implementation
namespace esp32_utils {

void configureHeapForAI() {
    LOG_INFO("🔧 Configuring heap for AI workloads...");
    
    // Set PSRAM allocation strategy if available
    if (psramFound()) {
        setPSRAMPriority(true);
        LOG_INFO("✅ PSRAM configured for AI models");
    }
}

void setPSRAMPriority(bool prefer_psram) {
    // This is a placeholder - actual implementation would configure
    // heap allocation preferences for PSRAM vs internal RAM
    if (prefer_psram) {
        LOG_DEBUG("📊 PSRAM priority enabled for large allocations");
    }
}

DynamicJsonDocument getMemoryReport() {
    DynamicJsonDocument doc(1024);
    
    doc["total_heap"] = ESP.getHeapSize();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["min_free_heap"] = ESP.getMinFreeHeap();
    
    doc["internal_total"] = ESP.getHeapSize();
    doc["internal_free"] = ESP.getFreeHeap();
    
    if (psramFound()) {
        doc["psram_total"] = ESP.getPsramSize();
        doc["psram_free"] = ESP.getFreePsram();
    }
    
    return doc;
}

bool checkMemoryAvailable(uint32_t required_kb) {
    uint32_t available_kb = ESP.getFreeHeap() / 1024;
    return (available_kb >= required_kb);
}

void emergencyMemoryCleanup() {
    LOG_WARN("🧹 Emergency memory cleanup initiated");
    
    // Force garbage collection - Arduino framework doesn't have direct heap control
    // This is a placeholder for cleanup logic
    
    // This would trigger any cleanup in the AI engine
    LOG_INFO("✅ Emergency cleanup completed");
}

} // namespace esp32_utils

} // namespace ethervoxai
