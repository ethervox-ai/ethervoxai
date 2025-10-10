/**
 * @file platform_esp32.c
 * @brief ESP32-specific audio platform implementation for EthervoxAI
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
#include "ethervox/audio.h"
#include <stdio.h>
#include <string.h>

#ifdef ETHERVOX_PLATFORM_ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "ESP32_AUDIO";

// I2S Configuration for ESP32
#define I2S_SAMPLE_RATE     16000
#define I2S_BITS_PER_SAMPLE 16
#define I2S_CHANNEL_NUM     1

// GPIO pins for I2S (adjust based on your hardware)
#define I2S_BCK_IO          26
#define I2S_WS_IO           25
#define I2S_DATA_IN_IO      33

// Platform-specific audio data
typedef struct {
    i2s_chan_handle_t rx_handle;
    uint8_t buffer[1024];
    size_t buffer_size;
} esp32_audio_data_t;

// Initialize audio runtime
int ethervox_audio_init(ethervox_audio_runtime_t* runtime, const ethervox_audio_config_t* config) {
    if (!runtime) return -1;
    
    memset(runtime, 0, sizeof(ethervox_audio_runtime_t));
    runtime->is_initialized = true;
    
    // Use config if needed (for now just acknowledge it exists)
    (void)config;  // Suppress unused parameter warning
    
    ESP_LOGI(TAG, "Audio runtime initialized for ESP32");
    return 0;
}
// Start audio capture
int ethervox_audio_start_capture(ethervox_audio_runtime_t* runtime) {
    if (!runtime || !runtime->is_initialized) return -1;
    
    ESP_LOGI(TAG, "Initializing I2S for audio capture...");
    
    // Allocate platform-specific data
    esp32_audio_data_t* audio_data = (esp32_audio_data_t*)malloc(sizeof(esp32_audio_data_t));
    if (!audio_data) {
        ESP_LOGE(TAG, "Failed to allocate audio data");
        return -1;
    }
    
    audio_data->buffer_size = sizeof(audio_data->buffer);
    
    // Configure I2S channel
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    
    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &audio_data->rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        free(audio_data);
        return -1;
    }
    
    // Configure I2S standard mode
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DATA_IN_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    ret = i2s_channel_init_std_mode(audio_data->rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S standard mode: %s", esp_err_to_name(ret));
        i2s_del_channel(audio_data->rx_handle);
        free(audio_data);
        return -1;
    }
    
    ret = i2s_channel_enable(audio_data->rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        i2s_del_channel(audio_data->rx_handle);
        free(audio_data);
        return -1;
    }
    
    runtime->platform_data = audio_data;
    runtime->is_capturing = true;
    
    ESP_LOGI(TAG, "I2S audio capture started successfully");
    return 0;
}

// Stop audio capture
int ethervox_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
    if (!runtime || !runtime->is_capturing) return -1;
    
    esp32_audio_data_t* audio_data = (esp32_audio_data_t*)runtime->platform_data;
    if (audio_data) {
        i2s_channel_disable(audio_data->rx_handle);
        i2s_del_channel(audio_data->rx_handle);
        free(audio_data);
        runtime->platform_data = NULL;
    }
    
    runtime->is_capturing = false;
    ESP_LOGI(TAG, "I2S audio capture stopped");
    return 0;
}

// Read audio data
int ethervox_audio_read(ethervox_audio_runtime_t* runtime, ethervox_audio_buffer_t* buffer) {
    if (!runtime || !buffer || !runtime->is_capturing) return -1;
    
    esp32_audio_data_t* audio_data = (esp32_audio_data_t*)runtime->platform_data;
    if (!audio_data) return -1;
    
    size_t bytes_read = 0;
    esp_err_t ret = i2s_channel_read(audio_data->rx_handle, audio_data->buffer, 
                                     audio_data->buffer_size, &bytes_read, 
                                     portMAX_DELAY);
    
    if (ret != ESP_OK || bytes_read == 0) {
        return -1;
    }
    
    // Cast to float* as expected by the header
    buffer->data = (float*)audio_data->buffer;
    buffer->size = bytes_read;
    buffer->timestamp_us = esp_timer_get_time();
    buffer->channels = I2S_CHANNEL_NUM;
    
    return 0;
}

// Cleanup audio runtime
void ethervox_audio_cleanup(ethervox_audio_runtime_t* runtime) {
    if (!runtime) return;
    
    if (runtime->is_capturing) {
        ethervox_audio_stop_capture(runtime);
    }
    
    runtime->is_initialized = false;
    ESP_LOGI(TAG, "Audio runtime cleaned up");
}

// Platform driver registration (called by audio_core during initialization)
int ethervox_audio_register_platform_driver(ethervox_audio_runtime_t* runtime) {
    if (!runtime) return -1;
    
    // For ESP32, we don't need to register function pointers
    // because we directly implement the ethervox_audio_* functions
    // The linker will resolve them automatically
    
    ESP_LOGI(TAG, "ESP32 audio platform driver registered");
    return 0;
}

#endif // ETHERVOX_PLATFORM_ESP32