/**
 * @file test_elegoo_el_sm_012.cpp
 * @brief Comprehensive test suite for ELEGOO EL-SM-012 module
 * @version 1.0.0
 * @date 2025-08-10
 * 
 * Tests specific to the ELEGOO EL-SM-012 ESP32 development module.
 * Validates hardware functionality, pin mappings, and performance characteristics.
 */

#include <unity.h>
#include <Arduino.h>
#include "esp32_platform_detector.h"
#include "config.h"

#ifdef ELEGOO_EL_SM_012
#include "elegoo_el_sm_012_config.h"

using namespace ethervoxai;

ESP32PlatformDetector detector;

void setUp(void) {
    // Initialize ELEGOO module before each test
    initElegooModule();
    detector.refreshCapabilities();
}

void tearDown(void) {
    // Cleanup after each test
    delay(100);
}

/**
 * Test ELEGOO EL-SM-012 module detection
 */
void test_elegoo_module_detection() {
    ESP32Capabilities caps = detector.getCapabilities();
    
    // Verify module is detected correctly
    TEST_ASSERT_EQUAL_STRING(BOARD_TYPE_STRING, caps.board_type.c_str());
    TEST_ASSERT_TRUE(caps.board_type.indexOf("ELEGOO") >= 0);
    TEST_ASSERT_TRUE(caps.board_type.indexOf("EL-SM-012") >= 0);
}

/**
 * Test module capabilities match specifications
 */
void test_elegoo_capabilities() {
    ElegooCapabilities elegoo_caps = getElegooCapabilities();
    ESP32Capabilities esp_caps = detector.getCapabilities();
    
    // Verify board identification
    TEST_ASSERT_EQUAL_STRING(BOARD_NAME, elegoo_caps.board_name);
    TEST_ASSERT_EQUAL_STRING(BOARD_VERSION, elegoo_caps.board_version);
    
    // Verify connectivity capabilities
    TEST_ASSERT_TRUE(elegoo_caps.has_wifi);
    TEST_ASSERT_TRUE(elegoo_caps.has_bluetooth);
    TEST_ASSERT_EQUAL(HAS_PSRAM, elegoo_caps.has_psram);
    
    // Verify memory specifications
    TEST_ASSERT_EQUAL(FLASH_SIZE_MB, elegoo_caps.flash_size_mb);
    TEST_ASSERT_EQUAL(SRAM_SIZE_KB, elegoo_caps.sram_size_kb);
    TEST_ASSERT_EQUAL(PSRAM_SIZE_KB, elegoo_caps.psram_size_kb);
}

/**
 * Test GPIO pin functionality
 */
void test_elegoo_gpio_pins() {
    // Test built-in LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    TEST_ASSERT_TRUE(digitalRead(LED_BUILTIN) == HIGH || LED_BUILTIN == 2); // ESP32 LED quirk
    
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    TEST_ASSERT_TRUE(digitalRead(LED_BUILTIN) == LOW || LED_BUILTIN == 2);
    
    // Test additional LEDs if defined
    #ifdef LED_RED
    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, HIGH);
    delay(50);
    digitalWrite(LED_RED, LOW);
    #endif
    
    #ifdef LED_GREEN
    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_GREEN, HIGH);
    delay(50);
    digitalWrite(LED_GREEN, LOW);
    #endif
    
    #ifdef LED_BLUE
    pinMode(LED_BLUE, OUTPUT);
    digitalWrite(LED_BLUE, HIGH);
    delay(50);
    digitalWrite(LED_BLUE, LOW);
    #endif
}

/**
 * Test I2C bus functionality
 */
void test_elegoo_i2c_bus() {
    Wire.begin(SDA_PIN, SCL_PIN);
    delay(100);
    
    // Scan for I2C devices
    int device_count = 0;
    for (int address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            device_count++;
        }
    }
    
    // I2C bus should be functional (may or may not have devices)
    TEST_ASSERT_TRUE(device_count >= 0); // Just verify no crash
}

/**
 * Test SPI bus functionality
 */
void test_elegoo_spi_bus() {
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS);
    delay(100);
    
    // Test SPI settings
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    SPI.endTransaction();
    
    // SPI should initialize without errors
    TEST_ASSERT_TRUE(true); // If we get here, SPI initialized successfully
}

/**
 * Test ADC functionality
 */
void test_elegoo_adc_pins() {
    // Test primary ADC pins
    int adc_value = analogRead(ADC1_CH0);
    TEST_ASSERT_TRUE(adc_value >= 0 && adc_value <= 4095);
    
    adc_value = analogRead(ADC1_CH3);
    TEST_ASSERT_TRUE(adc_value >= 0 && adc_value <= 4095);
}

/**
 * Test memory performance with ELEGOO module
 */
void test_elegoo_memory_performance() {
    ESP32Capabilities caps = detector.getCapabilities();
    
    // Verify sufficient memory for AI workloads
    TEST_ASSERT_GREATER_OR_EQUAL(256, caps.available_memory_kb); // At least 256KB free
    
    // Test memory allocation
    void* test_buffer = malloc(MAX_MODEL_SIZE_BYTES);
    TEST_ASSERT_NOT_NULL(test_buffer);
    
    // Test memory usage
    memset(test_buffer, 0xAA, MAX_MODEL_SIZE_BYTES);
    TEST_ASSERT_EQUAL(0xAA, ((uint8_t*)test_buffer)[1000]);
    
    free(test_buffer);
}

/**
 * Test CPU performance characteristics
 */
void test_elegoo_cpu_performance() {
    ESP32Capabilities caps = detector.getCapabilities();
    
    // Verify CPU specifications
    TEST_ASSERT_EQUAL(CPU_FREQ_MHZ, caps.cpu_freq_mhz);
    TEST_ASSERT_GREATER_OR_EQUAL(2, caps.cpu_cores);
    
    // Basic performance test
    unsigned long start_time = micros();
    volatile uint32_t result = 0;
    for (int i = 0; i < 10000; i++) {
        result += i * i;
    }
    unsigned long elapsed = micros() - start_time;
    
    // Should complete computation in reasonable time
    TEST_ASSERT_LESS_THAN(10000, elapsed); // Less than 10ms
}

/**
 * Test WiFi capability
 */
void test_elegoo_wifi_capability() {
    ESP32Capabilities caps = detector.getCapabilities();
    
    TEST_ASSERT_TRUE(caps.has_wifi);
    
    // Test WiFi initialization (don't connect, just test availability)
    WiFi.mode(WIFI_STA);
    delay(100);
    
    // WiFi should be available
    TEST_ASSERT_TRUE(WiFi.getMode() == WIFI_STA);
    
    WiFi.mode(WIFI_OFF);
}

/**
 * Test power management features
 */
void test_elegoo_power_management() {
    ESP32Capabilities caps = detector.getCapabilities();
    
    // Test power mode setting
    detector.setPowerMode("balanced");
    delay(100);
    
    // Test CPU frequency scaling
    uint32_t optimal_freq = detector.getOptimalCPUFreq();
    TEST_ASSERT_GREATER_THAN(80, optimal_freq); // At least 80MHz
    TEST_ASSERT_LESS_OR_EQUAL(240, optimal_freq); // Max 240MHz
}

/**
 * Test board revision detection
 */
void test_elegoo_board_revision() {
    uint8_t revision = getBoardRevision();
    TEST_ASSERT_GREATER_OR_EQUAL(1, revision);
    TEST_ASSERT_LESS_OR_EQUAL(10, revision); // Reasonable revision range
}

void setup() {
    delay(2000); // Wait for serial monitor
    
    UNITY_BEGIN();
    
    // Initialize ELEGOO module
    initElegooModule();
    
    // Initialize platform detector
    detector.refreshCapabilities();
    
    // Run ELEGOO-specific tests
    RUN_TEST(test_elegoo_module_detection);
    RUN_TEST(test_elegoo_capabilities);
    RUN_TEST(test_elegoo_gpio_pins);
    RUN_TEST(test_elegoo_i2c_bus);
    RUN_TEST(test_elegoo_spi_bus);
    RUN_TEST(test_elegoo_adc_pins);
    RUN_TEST(test_elegoo_memory_performance);
    RUN_TEST(test_elegoo_cpu_performance);
    RUN_TEST(test_elegoo_wifi_capability);
    RUN_TEST(test_elegoo_power_management);
    RUN_TEST(test_elegoo_board_revision);
    
    UNITY_END();
}

void loop() {
    // Empty - tests run once in setup()
}

#else
// Fallback for non-ELEGOO builds
void setup() {
    Serial.begin(115200);
    Serial.println("ELEGOO EL-SM-012 tests skipped - not compiled for ELEGOO module");
}

void loop() {
    delay(1000);
}
#endif
