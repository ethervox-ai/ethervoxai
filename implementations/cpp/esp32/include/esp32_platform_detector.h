/**
 * 🔍 EthervoxAI Platform Detector - ESP32 C++ Implementation
 * 
 * ESP32-optimized implementation following EthervoxAI protocol.
 * Provides hardware detection and capability assessment for AI model selection.
 */

#pragma once

#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <soc/soc.h>

namespace ethervoxai {

/**
 * ESP32 System Capabilities Structure
 * Following EthervoxAI cross-language protocol
 */
struct ESP32Capabilities {
    // Hardware specs
    uint32_t total_memory_kb;
    uint32_t available_memory_kb;
    uint32_t psram_size_kb;
    uint32_t flash_size_mb;
    uint32_t cpu_cores;
    uint32_t cpu_freq_mhz;
    
    // ESP32 variant info
    String chip_model;           // ESP32, ESP32-S2, ESP32-S3, ESP32-C3
    uint8_t chip_revision;
    String board_type;
    
    // Connectivity
    bool has_wifi;
    bool has_bluetooth;
    bool has_bluetooth_le;
    
    // AI/ML capabilities
    bool has_psram;
    bool has_spiram;
    uint32_t max_model_size_kb;
    uint32_t max_context_length;
    String performance_tier;     // low, medium, high
    
    // Power management
    String power_mode;           // high_performance, balanced, low_power
    bool battery_powered;
    uint32_t estimated_runtime_hours;
    
    // Audio capabilities
    bool has_i2s;
    bool has_adc;
    bool has_dac;
    uint32_t max_sample_rate;
};

/**
 * Model Compatibility Assessment for ESP32
 */
struct ESP32ModelCompatibility {
    String model_name;
    bool is_compatible;
    uint32_t required_memory_kb;
    String expected_performance;  // poor, fair, good, excellent
    uint32_t estimated_inference_time_ms;
    float estimated_accuracy;
    std::vector<String> optimization_flags;
    std::vector<String> warnings;
    bool requires_psram;
    bool supports_quantization;
};

/**
 * ESP32 Platform Detector Class
 * 
 * Optimized for:
 * - ESP32 hardware variants
 * - Memory-constrained environments  
 * - Real-time inference requirements
 * - Power-efficient operation
 */
class ESP32PlatformDetector {
public:
    ESP32PlatformDetector();
    
    /**
     * Get ESP32 system capabilities (cached)
     */
    ESP32Capabilities getCapabilities();
    
    /**
     * Check model compatibility with ESP32 constraints
     */
    ESP32ModelCompatibility checkModelCompatibility(
        const String& model_name,
        uint32_t model_size_kb,
        uint32_t min_memory_kb = 0
    );
    
    /**
     * Get recommended models for current ESP32
     */
    DynamicJsonDocument getRecommendedModels();
    
    /**
     * Set power management mode
     */
    void setPowerMode(const String& mode);
    
    /**
     * Optimize memory allocation for AI inference
     */
    void optimizeMemoryForAI();
    
    /**
     * Get real-time memory status
     */
    uint32_t getAvailableMemoryKB();
    
    /**
     * Get current CPU temperature (if sensor available)
     */
    float getCPUTemperature();
    
    /**
     * Check if model can fit in available memory
     */
    bool canLoadModel(uint32_t model_size_kb);
    
    /**
     * Get optimal CPU frequency for power mode
     */
    uint32_t getOptimalCPUFreq();
    
    /**
     * Force refresh of cached capabilities
     */
    void refreshCapabilities();
    
    /**
     * Enable/disable performance monitoring
     */
    void setMonitoringEnabled(bool enabled);
    
    /**
     * Get performance statistics
     */
    DynamicJsonDocument getPerformanceStats();

private:
    ESP32Capabilities _capabilities;
    bool _capabilities_cached;
    unsigned long _last_detection_ms;
    unsigned long _cache_duration_ms;
    bool _monitoring_enabled;
    
    // Performance tracking
    uint32_t _inference_count;
    uint32_t _total_inference_time_ms;
    uint32_t _max_inference_time_ms;
    uint32_t _min_inference_time_ms;
    
    // ESP32 hardware detection methods
    String detectChipModel();
    uint8_t detectChipRevision();
    String detectBoardType();
    uint32_t detectTotalMemory();
    uint32_t detectPSRAMSize();
    uint32_t detectFlashSize();
    
    // Connectivity detection
    bool detectWiFiCapability();
    bool detectBluetoothCapability();
    bool detectBluetoothLECapability();
    
    // Audio hardware detection
    bool detectI2S();
    bool detectADC();
    bool detectDAC();
    uint32_t detectMaxSampleRate();
    
    // Performance calculation
    String calculatePerformanceTier();
    uint32_t calculateMaxModelSize();
    uint32_t calculateMaxContextLength();
    
    // Power management
    void applyCPUFrequency(uint32_t freq_mhz);
    void configurePeripheralPower(const String& mode);
    
    // Memory optimization
    void defragmentHeap();
    void configurePSRAM();
    bool allocateOptimalTensorArena();
    
    // Logging and monitoring
    void logCapabilities();
    void updatePerformanceStats(uint32_t inference_time_ms);
    
    // ESP32-specific utility methods
    bool isPSRAMAvailable();
    uint32_t getESP32Variant();
    String getResetReason();
    uint32_t getStackHighWaterMark();
};

/**
 * Singleton instance for global access
 */
extern ESP32PlatformDetector platformDetector;

/**
 * Utility functions for ESP32 optimization
 */
namespace esp32_utils {
    /**
     * Configure heap allocation for AI workloads
     */
    void configureHeapForAI();
    
    /**
     * Set up memory allocation priorities
     */
    void setPSRAMPriority(bool prefer_psram);
    
    /**
     * Get detailed memory information
     */
    DynamicJsonDocument getMemoryReport();
    
    /**
     * Check if enough memory for operation
     */
    bool checkMemoryAvailable(uint32_t required_kb);
    
    /**
     * Emergency memory cleanup
     */
    void emergencyMemoryCleanup();
    
    /**
     * Setup watchdog for AI inference
     */
    void configureWatchdogForAI(uint32_t timeout_ms);
    
    /**
     * Configure real-time priority for AI tasks
     */
    void setAITaskPriority(uint8_t priority);
}

} // namespace ethervoxai
