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
#include <stdio.h>
#include <string.h>

#include "ethervox/audio.h"
#include "ethervox/error.h"

#ifdef ETHERVOX_PLATFORM_ESP32
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "ESP32_AUDIO";

// I2S Configuration for ESP32
#define I2S_SAMPLE_RATE 16000
#define I2S_BITS_PER_SAMPLE 16
#define I2S_CHANNEL_NUM 1

// GPIO pins for I2S (adjust based on your hardware)
#define I2S_BCK_IO 26
#define I2S_WS_IO 25
#define I2S_DATA_IN_IO 33

// Platform-specific audio data
typedef struct {
  i2s_chan_handle_t rx_handle;
  uint8_t buffer[1024];
  size_t buffer_size;
} esp32_audio_data_t;

// Initialize audio runtime
ethervox_result_t ethervox_audio_init(ethervox_audio_runtime_t* runtime, const ethervox_audio_config_t* config) {
  ETHERVOX_CHECK_PTR(runtime);

  memset(runtime, 0, sizeof(ethervox_audio_runtime_t));
  runtime->is_initialized = true;

  // Use config if needed (for now just acknowledge it exists)
  (void)config;  // Suppress unused parameter warning

  ESP_LOGI(TAG, "Audio runtime initialized for ESP32");
  return ETHERVOX_SUCCESS;
}
// Start audio capture
ethervox_result_t ethervox_audio_start_capture(ethervox_audio_runtime_t* runtime) {
  ETHERVOX_CHECK_PTR(runtime);
  if (!runtime->is_initialized) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Runtime not initialized");
  }

  ESP_LOGI(TAG, "Initializing I2S for audio capture...");

  // Allocate platform-specific data
  esp32_audio_data_t* audio_data = (esp32_audio_data_t*)malloc(sizeof(esp32_audio_data_t));
  if (!audio_data) {
    ESP_LOGE(TAG, "Failed to allocate audio data");
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate audio data");
  }

  audio_data->buffer_size = sizeof(audio_data->buffer);

  // Configure I2S channel
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

  esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &audio_data->rx_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
    free(audio_data);
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "I2S channel creation failed");
  }

  // Configure I2S standard mode
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = I2S_BCK_IO,
              .ws = I2S_WS_IO,
              .dout = I2S_GPIO_UNUSED,
              .din = I2S_DATA_IN_IO,
              .invert_flags =
                  {
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
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "I2S std mode init failed");
  }

  ret = i2s_channel_enable(audio_data->rx_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
    i2s_del_channel(audio_data->rx_handle);
    free(audio_data);
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "I2S channel enable failed");
  }

  runtime->platform_data = audio_data;
  runtime->is_capturing = true;

  ESP_LOGI(TAG, "I2S audio capture started successfully");
  return ETHERVOX_SUCCESS;
}

// Stop audio capture
ethervox_result_t ethervox_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
  ETHERVOX_CHECK_PTR(runtime);
  if (!runtime->is_capturing) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Capture not active");
  }

  esp32_audio_data_t* audio_data = (esp32_audio_data_t*)runtime->platform_data;
  if (audio_data) {
    i2s_channel_disable(audio_data->rx_handle);
    i2s_del_channel(audio_data->rx_handle);
    free(audio_data);
    runtime->platform_data = NULL;
  }

  runtime->is_capturing = false;
  ESP_LOGI(TAG, "I2S audio capture stopped");
  return ETHERVOX_SUCCESS;
}

// Read audio data
ethervox_result_t ethervox_audio_read(ethervox_audio_runtime_t* runtime, ethervox_audio_buffer_t* buffer) {
  ETHERVOX_CHECK_PTR(runtime);
  ETHERVOX_CHECK_PTR(buffer);
  if (!runtime->is_capturing) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Capture not active");
  }

  esp32_audio_data_t* audio_data = (esp32_audio_data_t*)runtime->platform_data;
  if (!audio_data) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Audio data not initialized");
  }

  size_t bytes_read = 0;
  esp_err_t ret = i2s_channel_read(audio_data->rx_handle, audio_data->buffer,
                                   audio_data->buffer_size, &bytes_read, portMAX_DELAY);

  if (ret != ESP_OK || bytes_read == 0) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "I2S read failed");
  }

  // Cast to float* as expected by the header
  buffer->data = (float*)audio_data->buffer;
  buffer->size = bytes_read;
  buffer->timestamp_us = esp_timer_get_time();
  buffer->channels = I2S_CHANNEL_NUM;

  return ETHERVOX_SUCCESS;
}

// Cleanup audio runtime
void ethervox_audio_cleanup(ethervox_audio_runtime_t* runtime) {
  if (!runtime)
    return;

  if (runtime->is_capturing) {
    ethervox_audio_stop_capture(runtime);
  }

  runtime->is_initialized = false;
  ESP_LOGI(TAG, "Audio runtime cleaned up");
}

// Platform driver registration (called by audio_core during initialization)
ethervox_result_t ethervox_audio_register_platform_driver(ethervox_audio_runtime_t* runtime) {
  ETHERVOX_CHECK_PTR(runtime);

  // For ESP32, we don't need to register function pointers
  // because we directly implement the ethervox_audio_* functions
  // The linker will resolve them automatically

  ESP_LOGI(TAG, "ESP32 audio platform driver registered");
  return ETHERVOX_SUCCESS;
}

#endif  // ETHERVOX_PLATFORM_ESP32