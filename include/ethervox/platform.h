/**
 * @file platform.h
 * @brief Platform abstraction layer interface for EthervoxAI
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * 
 * This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - NonCommercial: Not for commercial use
 * - ShareAlike: Distribute under same license
 * 
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */
#ifndef ETHERVOX_PLATFORM_H
#define ETHERVOX_PLATFORM_H

#include "ethervox/config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform capabilities
typedef struct {
    bool has_audio_input;
    bool has_audio_output;
    bool has_microphone_array;
    bool has_gpio;
    bool has_spi;
    bool has_i2c;
    bool has_uart;
    bool has_wifi;
    bool has_bluetooth;
    bool has_ethernet;
    bool has_display;
    bool has_camera;
    uint32_t max_audio_channels;
    uint32_t max_sample_rate;
    uint32_t gpio_pin_count;
    uint32_t ram_size_mb;
    uint32_t flash_size_mb;
} ethervox_platform_capabilities_t;

// GPIO configuration
typedef enum {
    ETHERVOX_GPIO_INPUT = 0,
    ETHERVOX_GPIO_OUTPUT,
    ETHERVOX_GPIO_INPUT_PULLUP,
    ETHERVOX_GPIO_INPUT_PULLDOWN,
    ETHERVOX_GPIO_ANALOG_INPUT,
    ETHERVOX_GPIO_PWM_OUTPUT
} ethervox_gpio_mode_t;

typedef struct {
    uint32_t pin;
    ethervox_gpio_mode_t mode;
    bool initial_state;
} ethervox_gpio_config_t;

// sleepmode enum
typedef enum {
    ETHERVOX_SLEEP_LIGHT,
    ETHERVOX_SLEEP_DEEP
} ethervox_sleep_mode_t;

// Platform-specific data structures
typedef struct {
    char platform_name[32];
    char hardware_revision[16];
    char cpu_model[64];
    uint32_t cpu_frequency_mhz;
    uint32_t core_count;
    ethervox_platform_capabilities_t capabilities;
    void* platform_specific_data;
} ethervox_platform_info_t;

// Hardware abstraction layer interface
typedef struct {
    // Platform initialization
    int (*init)(ethervox_platform_info_t* platform);
    void (*cleanup)(ethervox_platform_info_t* platform);

    // GPIO operations
    int (*gpio_configure)(uint32_t pin, ethervox_gpio_mode_t mode);
    int (*gpio_write)(uint32_t pin, bool state);
    bool (*gpio_read)(uint32_t pin);
    int (*gpio_set_pwm)(uint32_t pin, uint32_t duty_cycle);
    
    // I2C operations
    int (*i2c_init)(uint32_t bus, uint32_t sda_pin, uint32_t scl_pin);
    int (*i2c_write)(uint32_t bus, uint8_t device_addr, const uint8_t* data, uint32_t len);
    int (*i2c_read)(uint32_t bus, uint8_t device_addr, uint8_t* data, uint32_t len);
    void (*i2c_cleanup)(uint32_t bus);
    
    // SPI operations
    int (*spi_init)(uint32_t bus, uint32_t mosi_pin, uint32_t miso_pin, uint32_t clk_pin, uint32_t cs_pin);
    int (*spi_transfer)(uint32_t bus, const uint8_t* tx_data, uint8_t* rx_data, uint32_t len);
    void (*spi_cleanup)(uint32_t bus);
    
    // System operations
    void (*system_reset)(void);
    void (*system_sleep)(ethervox_sleep_mode_t mode);
    uint64_t (*get_timestamp_us)(void);
    uint32_t (*get_free_memory)(void);
    float (*get_cpu_temperature)(void);
    uint32_t (*get_free_heap_size)(void);

    // Power management
    int (*set_cpu_frequency)(uint32_t frequency_mhz);
    int (*enable_power_saving)(bool enable);
    float (*get_battery_voltage)(void);
    uint32_t (*delay_us)(uint32_t us);
    uint32_t (*delay_ms)(uint32_t ms);

} ethervox_platform_hal_t;

// Main platform structure
typedef struct {
    ethervox_platform_info_t info;
    ethervox_platform_hal_t hal;
    bool is_initialized;
    uint64_t boot_time;
    uint32_t error_count;
    char last_error[256];
} ethervox_platform_t;

// Public API functions
int ethervox_platform_init(ethervox_platform_t* platform);
void ethervox_platform_cleanup(ethervox_platform_t* platform);

// Platform detection
const char* ethervox_platform_get_name(void);
ethervox_platform_capabilities_t ethervox_platform_get_capabilities(void);
bool ethervox_platform_has_capability(const char* capability);

// GPIO utilities
int ethervox_gpio_configure_pin(ethervox_platform_t* platform, const ethervox_gpio_config_t* config);
int ethervox_gpio_write_pin(ethervox_platform_t* platform, uint32_t pin, bool state);
bool ethervox_gpio_read_pin(ethervox_platform_t* platform, uint32_t pin);

// System utilities
uint64_t ethervox_platform_get_uptime_ms(ethervox_platform_t* platform);
uint32_t ethervox_platform_get_memory_usage(ethervox_platform_t* platform);
float ethervox_platform_get_cpu_usage(ethervox_platform_t* platform);

// Device-specific profiles
int ethervox_platform_load_device_profile(ethervox_platform_t* platform, const char* profile_name);

// // Platform-specific HAL registration
int ethervox_platform_register_hal(ethervox_platform_t* platform);

// #ifdef ETHERVOX_PLATFORM_ESP32
// int esp32_hal_register(ethervox_platform_t* platform);
// #endif

// #ifdef ETHERVOX_PLATFORM_RPI
// int rpi_hal_register(ethervox_platform_t* platform);
// #endif

// #if defined(ETHERVOX_PLATFORM_WINDOWS) || defined(ETHERVOX_PLATFORM_LINUX)
// int desktop_hal_register(ethervox_platform_t* platform);
// #endif

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_PLATFORM_H