#pragma once

/**
 * @file elegoo_el_sm_012_config.h
 * @brief Configuration file for ELEGOO EL-SM-012 module
 * @version 1.0.0
 * @date 2025-08-10
 * 
 * Hardware-specific configuration for the ELEGOO EL-SM-012 ESP32 development module.
 * This file contains pin mappings, hardware capabilities, and optimization settings
 * specific to the ELEGOO EL-SM-012 module.
 */

#ifdef ELEGOO_EL_SM_012

// Board identification
#define BOARD_NAME "ELEGOO EL-SM-012"
#define BOARD_VERSION "1.0"
#define BOARD_VENDOR "ELEGOO"

// Hardware capabilities
#define HAS_WIFI true
#define HAS_BLUETOOTH true
#define HAS_PSRAM false          // Adjust based on actual module specs
#define HAS_EXTERNAL_ANTENNA false

// Pin mappings for ELEGOO EL-SM-012
#define LED_BUILTIN 2
#define LED_RED     25           // Adjust based on actual module pinout
#define LED_GREEN   26           // Adjust based on actual module pinout  
#define LED_BLUE    27           // Adjust based on actual module pinout

// Button pins
#define BUTTON_BOOT 0
#define BUTTON_USER 35           // Adjust based on actual module pinout

// I2S Audio pins (if module has audio capabilities)
#define I2S_WS      25           // Word Select / LRCLK
#define I2S_SD      26           // Serial Data
#define I2S_SCK     27           // Serial Clock

// I2C pins
#define SDA_PIN     21
#define SCL_PIN     22

// SPI pins
#define SPI_MOSI    23
#define SPI_MISO    19
#define SPI_SCK     18
#define SPI_CS      5

// UART pins
#define UART1_TX    17
#define UART1_RX    16
#define UART2_TX    4
#define UART2_RX    2

// ADC pins
#define ADC1_CH0    36           // VP
#define ADC1_CH3    39           // VN
#define ADC1_CH4    32
#define ADC1_CH5    33
#define ADC1_CH6    34
#define ADC1_CH7    35

// PWM/DAC pins
#define DAC1        25
#define DAC2        26

// Memory configuration
#define FLASH_SIZE_MB 4          // Typical for ESP32 modules
#define SRAM_SIZE_KB  520        // ESP32 internal SRAM
#define PSRAM_SIZE_KB 0          // No external PSRAM by default

// Performance settings
#define CPU_FREQ_MHZ 240
#define WIFI_TASK_STACK_SIZE 4096
#define BLUETOOTH_TASK_STACK_SIZE 4096

// AI model constraints for this module
#define MAX_MODEL_SIZE_BYTES (512 * 1024)      // 512KB
#define TENSOR_ARENA_SIZE_BYTES (8 * 1024)     // 8KB
#define AUDIO_BUFFER_SIZE_SAMPLES 1024
#define AUDIO_SAMPLE_RATE 16000

// Power management
#define ENABLE_LIGHT_SLEEP true
#define ENABLE_DEEP_SLEEP true
#define POWER_SAVE_MODE false    // Disable for performance testing

// Communication settings
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define MQTT_KEEPALIVE_SEC 60
#define HTTP_TIMEOUT_MS 5000

// Debug settings
#define ENABLE_SERIAL_DEBUG true
#define SERIAL_BAUD_RATE 115200
#define LOG_LEVEL 3              // INFO level

// Module-specific optimizations
#define OPTIMIZE_FOR_SPEED true
#define ENABLE_WATCHDOG true
#define STACK_GUARD_SIZE 512

// Board type detection string
#define BOARD_TYPE_STRING "ELEGOO_EL_SM_012"

// Hardware revision detection
inline uint8_t getBoardRevision() {
    // Read hardware revision pins or return default
    return 1; // Version 1.0
}

// Module initialization function
inline void initElegooModule() {
    // Initialize module-specific hardware
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    // Initialize any module-specific peripherals
    Serial.begin(SERIAL_BAUD_RATE);
    while (!Serial && millis() < 5000) {
        delay(10);
    }
    
    Serial.println("ELEGOO EL-SM-012 Module Initialized");
    Serial.printf("Board: %s v%s\n", BOARD_NAME, BOARD_VERSION);
    Serial.printf("CPU Frequency: %d MHz\n", CPU_FREQ_MHZ);
    Serial.printf("Flash Size: %d MB\n", FLASH_SIZE_MB);
}

// Board capabilities structure
struct ElegooCapabilities {
    bool has_wifi = HAS_WIFI;
    bool has_bluetooth = HAS_BLUETOOTH;
    bool has_psram = HAS_PSRAM;
    bool has_external_antenna = HAS_EXTERNAL_ANTENNA;
    uint8_t flash_size_mb = FLASH_SIZE_MB;
    uint16_t sram_size_kb = SRAM_SIZE_KB;
    uint16_t psram_size_kb = PSRAM_SIZE_KB;
    const char* board_name = BOARD_NAME;
    const char* board_version = BOARD_VERSION;
    uint8_t revision = 1;
};

inline ElegooCapabilities getElegooCapabilities() {
    ElegooCapabilities caps;
    caps.revision = getBoardRevision();
    return caps;
}

#endif // ELEGOO_EL_SM_012
