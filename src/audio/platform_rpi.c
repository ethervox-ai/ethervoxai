/**
 * @file platform_rpi.c
 * @brief Raspberry Pi-specific audio platform implementation for EthervoxAI
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
#include <stdlib.h>
#include <string.h>

#include "ethervox/audio.h"
// #include <alsa/asoundlib.h>
#include <bcm2835.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

// Raspberry Pi audio data structure
typedef struct {
  int i2s_fd;  // I2S device file descriptor
  uint32_t buffer_frames;
  uint8_t* capture_buffer;
  uint8_t* playback_buffer;
  bool is_capturing;
  bool is_playing;
  int mic_array_enable_pin;
  int mic_array_sel_pins[3];
  char* audio_buffer;
} rpi_audio_data_t;

static int rpi_gpio_init(rpi_audio_data_t* audio_data) {
  if (!bcm2835_init()) {
    printf("Failed to initialize BCM2835 library\n");
    return -1;
  }

  // Configure GPIO pins for mic array
  audio_data->mic_array_enable_pin = RPI_GPIO_P1_18;
  audio_data->mic_array_sel_pins[0] = RPI_GPIO_P1_22;
  audio_data->mic_array_sel_pins[1] = RPI_GPIO_P1_24;
  audio_data->mic_array_sel_pins[2] = RPI_GPIO_P1_26;

  bcm2835_gpio_fsel(audio_data->mic_array_enable_pin, BCM2835_GPIO_FSEL_OUTP);
  for (int i = 0; i < 3; i++) {
    bcm2835_gpio_fsel(audio_data->mic_array_sel_pins[i], BCM2835_GPIO_FSEL_OUTP);
  }

  // Enable mic array
  bcm2835_gpio_write(audio_data->mic_array_enable_pin, HIGH);

  printf("Raspberry Pi GPIO initialized for mic array\n");
  return 0;
}

static int rpi_audio_init(ethervox_audio_runtime_t* runtime,
                          const ethervox_audio_config_t* config) {
  rpi_audio_data_t* audio_data = (rpi_audio_data_t*)malloc(sizeof(rpi_audio_data_t));
  if (!audio_data) {
    return -1;
  }

  memset(audio_data, 0, sizeof(rpi_audio_data_t));
  runtime->platform_data = audio_data;

  audio_data->buffer_frames = config->buffer_size;
  audio_data->audio_buffer =
      (char*)malloc(audio_data->buffer_frames * config->channels * sizeof(int16_t));

  // Initialize GPIO for mic array control
  if (rpi_gpio_init(audio_data) != 0) {
    printf("Warning: GPIO initialization failed, continuing without mic array control\n");
  }

  printf("Raspberry Pi audio driver initialized\n");
  return 0;
}

static int rpi_select_microphone(rpi_audio_data_t* audio_data, int mic_index) {
  if (mic_index < 0 || mic_index > 7) {
    return -1;
  }

  // Set selection pins based on mic index (3-bit binary)
  for (int i = 0; i < 3; i++) {
    bcm2835_gpio_write(audio_data->mic_array_sel_pins[i], (mic_index & (1 << i)) ? HIGH : LOW);
  }

  // Small delay for switching
  bcm2835_delay(1);

  return 0;
}

static int rpi_audio_start_capture(ethervox_audio_runtime_t* runtime) {
  if (!runtime || !runtime->platform_data)
    return -1;

  rpi_audio_data_t* audio_data = (rpi_audio_data_t*)runtime->platform_data;

// For cross-compilation stub - actual I2S initialization would go here
#ifdef ETHERVOX_RPI_HARDWARE
  // Open I2S device for audio capture
  audio_data->i2s_fd = open("/dev/i2s", O_RDWR);
  if (audio_data->i2s_fd < 0) {
    printf("Cannot open I2S device for capture\n");
    return -1;
  }

// Configure I2S parameters using ioctl
// This would involve setting sample rate, channels, bit depth, etc.
#else
  // Stub for cross-compilation
  printf("RPI audio capture started (stub)\n");
#endif

  audio_data->is_capturing = true;
  return 0;
}

static int rpi_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
  if (!runtime || !runtime->platform_data)
    return -1;

  rpi_audio_data_t* audio_data = (rpi_audio_data_t*)runtime->platform_data;

#ifdef ETHERVOX_RPI_HARDWARE
  if (audio_data->i2s_fd >= 0) {
    close(audio_data->i2s_fd);
    audio_data->i2s_fd = -1;
  }
#endif

  audio_data->is_capturing = false;
  return 0;
}

static int rpi_audio_start_playback(ethervox_audio_runtime_t* runtime) {
  if (!runtime || !runtime->platform_data)
    return -1;

  rpi_audio_data_t* audio_data = (rpi_audio_data_t*)runtime->platform_data;

#ifdef ETHERVOX_RPI_HARDWARE
// I2S playback initialization would go here
#else
  // Stub for cross-compilation
  printf("RPI audio playback started (stub)\n");
#endif

  audio_data->is_playing = true;
  return 0;
}

static int rpi_audio_stop_playback(ethervox_audio_runtime_t* runtime) {
  if (!runtime || !runtime->platform_data)
    return -1;

  rpi_audio_data_t* audio_data = (rpi_audio_data_t*)runtime->platform_data;

#ifdef ETHERVOX_RPI_HARDWARE
  // Close I2S device if open
  if (audio_data->i2s_fd >= 0) {
    close(audio_data->i2s_fd);
    audio_data->i2s_fd = -1;
  }
#endif

  audio_data->is_playing = false;
  printf("Raspberry Pi audio playback stopped\n");
  return 0;
}

static void rpi_audio_cleanup(ethervox_audio_runtime_t* runtime) {
  rpi_audio_data_t* audio_data = (rpi_audio_data_t*)runtime->platform_data;

  if (audio_data) {
    // Cleanup GPIO
    bcm2835_close();

    if (audio_data->audio_buffer) {
      free(audio_data->audio_buffer);
    }
    free(audio_data);
    runtime->platform_data = NULL;
  }
  printf("Raspberry Pi audio driver cleaned up\n");
}

int ethervox_audio_register_platform_driver(ethervox_audio_runtime_t* runtime) {
  runtime->driver.init = rpi_audio_init;
  runtime->driver.start_capture = rpi_audio_start_capture;
  runtime->driver.stop_capture = rpi_audio_stop_capture;
  runtime->driver.start_playback = rpi_audio_start_playback;
  runtime->driver.stop_playback = rpi_audio_stop_playback;
  runtime->driver.cleanup = rpi_audio_cleanup;

  return 0;
}

//#endif // ETHERVOX_PLATFORM_RPI