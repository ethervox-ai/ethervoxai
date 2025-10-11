/**
 * @file platform_core.c
 * @brief Cross-platform core functionality for EthervoxAI
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

#include "ethervox/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef ETHERVOX_PLATFORM_WINDOWS
#include <windows.h>
#endif

// Forward declarations ONLY (no function bodies)
#ifdef ETHERVOX_PLATFORM_RPI
#pragma message("-----Compiling with ETHERVOX_PLATFORM_RPI")
extern int rpi_hal_register(ethervox_platform_t* platform);
#endif
#ifdef ETHERVOX_PLATFORM_ESP32
#pragma message("----Compiling with ETHERVOX_PLATFORM_ESP32")
extern int esp32_hal_register(ethervox_platform_t* platform);
#endif
#if defined(ETHERVOX_PLATFORM_DESKTOP)
#pragma message("----Compiling with ETHERVOX_PLATFORM_DESKTOP")
extern int desktop_hal_register(ethervox_platform_t* platform);
#endif

// Register platform-specific HAL
int ethervox_platform_register_hal(ethervox_platform_t* platform) {
    if (!platform) return -1;
    
    #ifdef ETHERVOX_PLATFORM_ESP32
        #pragma message("Returning ESP32_HAL")
        return esp32_hal_register(platform);
    #elif defined(ETHERVOX_PLATFORM_RPI)
        #pragma message("Returning RPI_HAL")
        return rpi_hal_register(platform);
    #elif defined(ETHERVOX_PLATFORM_DESKTOP)
        #pragma message("Returning DESKTOP_HAL")
        return desktop_hal_register(platform);
    #else
        #pragma message("No HAL available for this platform")
        fprintf(stderr, "No HAL available for this platform\n");
        return -1;
    #endif
}

// Platform detection
const char* ethervox_platform_get_name(void) {
    #ifdef ETHERVOX_PLATFORM_ESP32
        return "ESP32";
    #elif defined(ETHERVOX_PLATFORM_RPI)
        return "Raspberry Pi";
    #elif defined(ETHERVOX_PLATFORM_WINDOWS)
        return "Windows";
    #elif defined(ETHERVOX_PLATFORM_LINUX)
        return "Linux";
    #elif defined(ETHERVOX_PLATFORM_MACOS)
        return "macOS";
    #else
        return "Unknown";
    #endif
}

// Get platform capabilities
ethervox_platform_capabilities_t ethervox_platform_get_capabilities(void) {
    ethervox_platform_capabilities_t caps = {0};
    
    #ifdef ETHERVOX_PLATFORM_ESP32
        caps.has_audio_input = true;
        caps.has_audio_output = true;
        caps.has_microphone_array = true;
        caps.has_gpio = true;
        caps.has_spi = true;
        caps.has_i2c = true;
        caps.has_uart = true;
        caps.has_wifi = true;
        caps.has_bluetooth = true;
        caps.has_ethernet = false;
        caps.has_display = true;
        caps.has_camera = false;
        caps.max_audio_channels = 2;
        caps.max_sample_rate = 48000;
        caps.gpio_pin_count = 34;
        caps.ram_size_mb = 8;
        caps.flash_size_mb = 16;
        
    #elif defined(ETHERVOX_PLATFORM_RPI)
        caps.has_audio_input = true;
        caps.has_audio_output = true;
        caps.has_microphone_array = true;
        caps.has_gpio = true;
        caps.has_spi = true;
        caps.has_i2c = true;
        caps.has_uart = true;
        caps.has_wifi = true;
        caps.has_bluetooth = true;
        caps.has_ethernet = true;
        caps.has_display = true;
        caps.has_camera = true;
        caps.max_audio_channels = 8;
        caps.max_sample_rate = 192000;
        caps.gpio_pin_count = 40;
        caps.ram_size_mb = 8192;  // 8GB for Pi 5
        caps.flash_size_mb = 0;   // SD card storage
        
    #elif defined(ETHERVOX_PLATFORM_DESKTOP)
        caps.has_audio_input = true;
        caps.has_audio_output = true;
        caps.has_microphone_array = false;
        caps.has_gpio = false;
        caps.has_spi = false;
        caps.has_i2c = false;
        caps.has_uart = false;
        caps.has_wifi = true;
        caps.has_bluetooth = true;
        caps.has_ethernet = true;
        caps.has_display = true;
        caps.has_camera = true;
        caps.max_audio_channels = 8;
        caps.max_sample_rate = 192000;
        caps.gpio_pin_count = 0;
        caps.ram_size_mb = 16384;  // 16GB typical
        caps.flash_size_mb = 0;    // SSD storage
    #else
        // Safe defaults for unknown platforms
        caps.has_audio_input = false;
        caps.has_audio_output = false;
        caps.has_microphone_array = false;
        caps.has_gpio = false;
        caps.has_spi = false;
        caps.has_i2c = false;
        caps.has_uart = false;
        caps.has_wifi = false;
        caps.has_bluetooth = false;
        caps.has_ethernet = false;
        caps.has_display = false;
        caps.has_camera = false;
        caps.max_audio_channels = 0;
        caps.max_sample_rate = 48000;
        caps.gpio_pin_count = 0;
        caps.ram_size_mb = 0;
        caps.flash_size_mb = 0;
    #endif
    
    return caps;
}

// Check if platform has specific capability
bool ethervox_platform_has_capability(const char* capability) {
    if (!capability) return false;
    
    ethervox_platform_capabilities_t caps = ethervox_platform_get_capabilities();
    
    if (strcmp(capability, "audio_input") == 0) return caps.has_audio_input;
    if (strcmp(capability, "audio_output") == 0) return caps.has_audio_output;
    if (strcmp(capability, "microphone_array") == 0) return caps.has_microphone_array;
    if (strcmp(capability, "gpio") == 0) return caps.has_gpio;
    if (strcmp(capability, "spi") == 0) return caps.has_spi;
    if (strcmp(capability, "i2c") == 0) return caps.has_i2c;
    if (strcmp(capability, "uart") == 0) return caps.has_uart;
    if (strcmp(capability, "wifi") == 0) return caps.has_wifi;
    if (strcmp(capability, "bluetooth") == 0) return caps.has_bluetooth;
    if (strcmp(capability, "ethernet") == 0) return caps.has_ethernet;
    if (strcmp(capability, "display") == 0) return caps.has_display;
    if (strcmp(capability, "camera") == 0) return caps.has_camera;
    
    return false;
}

// Get system timestamp in microseconds
static uint64_t get_system_timestamp_us(void) {
    #ifdef ETHERVOX_PLATFORM_WINDOWS
        LARGE_INTEGER frequency, counter;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        return (uint64_t)(counter.QuadPart * 1000000 / frequency.QuadPart);
    #else
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    #endif
}

// Initialize platform
int ethervox_platform_init(ethervox_platform_t* platform) {
    if (!platform) return -1;
    
    memset(platform, 0, sizeof(ethervox_platform_t));
    
    // Set platform information
    strncpy(platform->info.platform_name, ethervox_platform_get_name(), 
            sizeof(platform->info.platform_name) - 1);
    platform->info.capabilities = ethervox_platform_get_capabilities();
    platform->boot_time = get_system_timestamp_us();
    
    // Platform-specific initialization
    #ifdef ETHERVOX_PLATFORM_ESP32
        strcpy(platform->info.hardware_revision, "ESP32-S3");
        strcpy(platform->info.cpu_model, "Xtensa LX7");
        platform->info.cpu_frequency_mhz = 240;
        platform->info.core_count = 2;
        
    #elif defined(ETHERVOX_PLATFORM_RPI)
        strcpy(platform->info.hardware_revision, "4.0");
        strcpy(platform->info.cpu_model, "ARM Cortex-A76");
        platform->info.cpu_frequency_mhz = 2400;
        platform->info.core_count = 4;
        
    #elif defined(ETHERVOX_PLATFORM_DESKTOP)
        strcpy(platform->info.hardware_revision, "1.0");
        strcpy(platform->info.cpu_model, "x86_64");
        platform->info.cpu_frequency_mhz = 3000;
        platform->info.core_count = 8;
    #endif
    
    // Register platform-specific HAL functions
    int result = ethervox_platform_register_hal(platform);
    if (result != 0) {
        snprintf(platform->last_error, sizeof(platform->last_error),
                "Failed to register platform HAL");
        return result;
    }
    
    // Initialize platform-specific subsystems
    if (platform->hal.init) {
        result = platform->hal.init(&platform->info);
        if (result != 0) {
            snprintf(platform->last_error, sizeof(platform->last_error),
                    "Platform initialization failed");
            return result;
        }
    }
    
    platform->is_initialized = true;
    
    printf("Platform initialized: %s (%s)\n", 
           platform->info.platform_name, platform->info.hardware_revision);
    
    return 0;
}

// Cleanup platform
void ethervox_platform_cleanup(ethervox_platform_t* platform) {
    if (!platform || !platform->is_initialized) return;
    
    if (platform->hal.cleanup) {
        platform->hal.cleanup(&platform->info);
    }
    
    platform->is_initialized = false;
    printf("Platform cleaned up\n");
}

// Get uptime in milliseconds
uint64_t ethervox_platform_get_uptime_ms(ethervox_platform_t* platform) {
    if (!platform) return 0;
    
    uint64_t current_time = get_system_timestamp_us();
    return (current_time - platform->boot_time) / 1000;
}

// GPIO configuration helper
int ethervox_gpio_configure_pin(ethervox_platform_t* platform, const ethervox_gpio_config_t* config) {
    if (!platform || !config || !platform->hal.gpio_configure) {
        return -1;
    }
    
    int result = platform->hal.gpio_configure(config->pin, config->mode);
    if (result == 0 && config->mode == ETHERVOX_GPIO_OUTPUT) {
        // Set initial state for output pins
        platform->hal.gpio_write(config->pin, config->initial_state);
    }
    
    return result;
}

// GPIO write helper
int ethervox_gpio_write_pin(ethervox_platform_t* platform, uint32_t pin, bool state) {
    if (!platform || !platform->hal.gpio_write) {
        return -1;
    }
    
    return platform->hal.gpio_write(pin, state);
}

// GPIO read helper
bool ethervox_gpio_read_pin(ethervox_platform_t* platform, uint32_t pin) {
    if (!platform || !platform->hal.gpio_read) {
        return false;
    }
    
    return platform->hal.gpio_read(pin);
}

// Load device profile (placeholder implementation)
int ethervox_platform_load_device_profile(ethervox_platform_t* platform, const char* profile_name) {
    if (!platform || !profile_name) return -1;
    
    printf("Loading device profile: %s\n", profile_name);
    
    // In a real implementation, this would load configuration from a file
    // and configure hardware-specific settings like mic array geometry,
    // GPIO pin assignments, audio routing, etc.
    
    return 0;
}

// Register platform-specific HAL implementation
// This function is implemented in separate platform-specific files
// extern int ethervox_platform_register_hal(ethervox_platform_t* platform);
