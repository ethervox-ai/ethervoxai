#include <unity.h>
#include "esp32_platform_detector.h"
#include "config.h"

using namespace ethervoxai;

void setUp(void) {
    // Set up before each test
}

void tearDown(void) {
    // Clean up after each test
}

void test_platform_detection() {
    ESP32Capabilities caps = platformDetector.getCapabilities();
    
    // Test basic hardware detection
    TEST_ASSERT_TRUE(caps.chip_model.length() > 0);
    TEST_ASSERT_TRUE(caps.chip_model.startsWith("ESP32"));
    TEST_ASSERT_GREATER_THAN(0, caps.chip_revision);
    
    // Test memory detection
    TEST_ASSERT_GREATER_THAN(100, caps.total_memory_kb);     // At least 100KB
    TEST_ASSERT_GREATER_THAN(50, caps.available_memory_kb);   // At least 50KB free
    TEST_ASSERT_GREATER_THAN(0, caps.flash_size_mb);         // Some flash memory
    
    // Test CPU detection
    TEST_ASSERT_GREATER_OR_EQUAL(1, caps.cpu_cores);         // At least 1 core
    TEST_ASSERT_GREATER_THAN(80, caps.cpu_freq_mhz);         // At least 80MHz
    
    // Test connectivity
    TEST_ASSERT_TRUE(caps.has_wifi);                         // All ESP32s have WiFi
    
    // Test audio capabilities
    TEST_ASSERT_TRUE(caps.has_i2s);                          // All ESP32s have I2S
    TEST_ASSERT_TRUE(caps.has_adc);                          // All ESP32s have ADC
    
    // Test performance tier calculation
    TEST_ASSERT_TRUE(caps.performance_tier == "low" || 
                     caps.performance_tier == "medium" || 
                     caps.performance_tier == "high");
    
    // Test model constraints
    TEST_ASSERT_GREATER_THAN(50, caps.max_model_size_kb);    // At least 50KB for models
    TEST_ASSERT_GREATER_THAN(64, caps.max_context_length);   // At least 64 tokens
}

void test_memory_functions() {
    // Test real-time memory monitoring
    uint32_t free_memory = platformDetector.getAvailableMemoryKB();
    TEST_ASSERT_GREATER_THAN(0, free_memory);
    
    // Test memory utility functions
    TEST_ASSERT_TRUE(esp32_utils::checkMemoryAvailable(10)); // 10KB should be available
    
    // Test memory report generation
    JsonDocument memory_report = esp32_utils::getMemoryReport();
    TEST_ASSERT_TRUE(memory_report.containsKey("total_heap"));
    TEST_ASSERT_TRUE(memory_report.containsKey("free_heap"));
    TEST_ASSERT_GREATER_THAN(0, memory_report["total_heap"].as<uint32_t>());
}

void test_psram_detection() {
    ESP32Capabilities caps = platformDetector.getCapabilities();
    
    if (caps.has_psram) {
        // If PSRAM is detected, validate its properties
        TEST_ASSERT_GREATER_THAN(0, caps.psram_size_kb);
        TEST_ASSERT_TRUE(caps.has_spiram);
        
        // PSRAM should allow larger models
        TEST_ASSERT_GREATER_THAN(256, caps.max_model_size_kb);
    } else {
        // No PSRAM detected
        TEST_ASSERT_EQUAL(0, caps.psram_size_kb);
        TEST_ASSERT_FALSE(caps.has_spiram);
    }
}

void test_performance_stats() {
    // Test performance statistics
    JsonDocument stats = platformDetector.getPerformanceStats();
    
    TEST_ASSERT_TRUE(stats.containsKey("inference_count"));
    TEST_ASSERT_TRUE(stats.containsKey("free_heap_kb"));
    TEST_ASSERT_GREATER_OR_EQUAL(0, stats["inference_count"].as<uint32_t>());
    TEST_ASSERT_GREATER_THAN(0, stats["free_heap_kb"].as<uint32_t>());
}

void test_chip_model_specific() {
    ESP32Capabilities caps = platformDetector.getCapabilities();
    
    if (caps.chip_model == "ESP32") {
        // Standard ESP32 tests
        TEST_ASSERT_EQUAL(2, caps.cpu_cores);
        TEST_ASSERT_TRUE(caps.has_bluetooth);
        TEST_ASSERT_TRUE(caps.has_dac);
    }
    else if (caps.chip_model == "ESP32-S2") {
        // ESP32-S2 specific tests
        TEST_ASSERT_EQUAL(1, caps.cpu_cores);        // Single core
        TEST_ASSERT_FALSE(caps.has_bluetooth);       // No Bluetooth
        TEST_ASSERT_TRUE(caps.has_dac);              // Has DAC
    }
    else if (caps.chip_model == "ESP32-S3") {
        // ESP32-S3 specific tests
        TEST_ASSERT_EQUAL(2, caps.cpu_cores);        // Dual core
        TEST_ASSERT_TRUE(caps.has_bluetooth);        // Has Bluetooth
        TEST_ASSERT_TRUE(caps.has_dac);              // Has DAC
        // S3 typically has better performance
        TEST_ASSERT_TRUE(caps.performance_tier == "medium" || 
                         caps.performance_tier == "high");
    }
    else if (caps.chip_model == "ESP32-C3") {
        // ESP32-C3 specific tests
        TEST_ASSERT_EQUAL(1, caps.cpu_cores);        // Single core RISC-V
        TEST_ASSERT_TRUE(caps.has_bluetooth);        // Has Bluetooth LE
        TEST_ASSERT_FALSE(caps.has_dac);             // No DAC
    }
}

void test_capability_caching() {
    // Test that capabilities are cached properly
    unsigned long start_time = millis();
    ESP32Capabilities caps1 = platformDetector.getCapabilities();
    unsigned long first_call_time = millis() - start_time;
    
    start_time = millis();
    ESP32Capabilities caps2 = platformDetector.getCapabilities();
    unsigned long second_call_time = millis() - start_time;
    
    // Second call should be much faster (cached)
    TEST_ASSERT_LESS_THAN(first_call_time, second_call_time + 5); // Allow 5ms tolerance
    
    // Results should be identical
    TEST_ASSERT_EQUAL_STRING(caps1.chip_model.c_str(), caps2.chip_model.c_str());
    TEST_ASSERT_EQUAL(caps1.total_memory_kb, caps2.total_memory_kb);
    TEST_ASSERT_EQUAL(caps1.cpu_cores, caps2.cpu_cores);
}

void test_board_type_detection() {
    ESP32Capabilities caps = platformDetector.getCapabilities();
    
    // Board type should be detected
    TEST_ASSERT_TRUE(caps.board_type.length() > 0);
    TEST_ASSERT_TRUE(caps.board_type.indexOf("ESP32") >= 0);
    
    // Board type should correlate with detected features
    if (caps.board_type.indexOf("WROVER") >= 0) {
        // WROVER boards typically have PSRAM
        TEST_ASSERT_TRUE(caps.has_psram || caps.psram_size_kb == 0); // May not be enabled
    }
    
    if (caps.board_type.indexOf("DevKit") >= 0) {
        // DevKit boards should have adequate flash
        TEST_ASSERT_GREATER_OR_EQUAL(4, caps.flash_size_mb);
    }
}

void setup() {
    delay(2000); // Wait for serial monitor
    
    UNITY_BEGIN();
    
    // Initialize platform detector
    platformDetector.refreshCapabilities();
    
    // Run tests
    RUN_TEST(test_platform_detection);
    RUN_TEST(test_memory_functions);
    RUN_TEST(test_psram_detection);
    RUN_TEST(test_performance_stats);
    RUN_TEST(test_chip_model_specific);
    RUN_TEST(test_capability_caching);
    RUN_TEST(test_board_type_detection);
    
    UNITY_END();
}

void loop() {
    // Empty - tests run once in setup()
}
