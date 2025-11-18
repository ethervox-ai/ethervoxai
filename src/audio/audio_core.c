/**
 * @file audio_core.c
 * @brief Core audio processing functionality for EthervoxAI
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ethervox/audio.h"

static const float kEthervoxAudioLanguageConfidenceDefault = 0.85f;
static const float kEthervoxAudioFinalConfidenceDefault = 0.90f;
static const uint32_t kEthervoxAudioTtsDurationSeconds = 2U;
static const float kEthervoxAudioToneAmplitude = 0.5f;
static const float kEthervoxAudioToneFrequencyHz = 440.0f;
static const float kEthervoxAudioTwoPi = 6.283185307f;

// Default configuration
ethervox_audio_config_t ethervox_audio_get_default_config(void) {
  ethervox_audio_config_t config = {.sample_rate = ETHERVOX_AUDIO_SAMPLE_RATE,
                                    .channels = ETHERVOX_AUDIO_CHANNELS_DEFAULT,
                                    .bits_per_sample = ETHERVOX_AUDIO_BITS_PER_SAMPLE,
                                    .buffer_size = ETHERVOX_AUDIO_BUFFER_SIZE,
                                    .enable_noise_suppression = true,
                                    .enable_echo_cancellation = true};
  return config;
}

// Initialize audio runtime
int ethervox_audio_init(ethervox_audio_runtime_t* runtime, const ethervox_audio_config_t* config) {
  if (!runtime || !config) {
    return -1;
  }

  memset(runtime, 0, sizeof(ethervox_audio_runtime_t));
  runtime->config = *config;

  // Register platform-specific driver
  int result = ethervox_audio_register_platform_driver(runtime);
  if (result != 0) {
    fprintf(stderr, "Failed to register platform audio driver (err=%d)\n", result);
    return result;
  }

  // Initialize platform-specific audio subsystem
  if (runtime->driver.init) {
    result = runtime->driver.init(runtime, config);
    if (result == 0) {
      runtime->is_initialized = true;
  snprintf(runtime->current_language, sizeof(runtime->current_language), "%s", "en");  // Default language
      runtime->language_confidence = 1.0f;
    } else {
      fprintf(stderr, "Platform audio driver initialization failed (err=%d)\n", result);
    }
  }

  return result;
}

// Start audio processing
int ethervox_audio_start(ethervox_audio_runtime_t* runtime) {
  if (!runtime || !runtime->is_initialized) {
    return -1;
  }

  int result = 0;

  // Start audio capture
  if (runtime->driver.start_capture) {
    result = runtime->driver.start_capture(runtime);
    if (result == 0) {
      runtime->is_capturing = true;
    }
  }

  // Start audio playback
  if (result == 0 && runtime->driver.start_playback) {
    result = runtime->driver.start_playback(runtime);
    if (result == 0) {
      runtime->is_playing = true;
    }
  }

  return result;
}

int ethervox_audio_start_capture(ethervox_audio_runtime_t* runtime) {
  if (!runtime || !runtime->is_initialized) {
    return -1;
  }

  if (runtime->is_capturing) {
    return 0;
  }

  if (!runtime->driver.start_capture) {
    fprintf(stderr, "Audio driver does not support start_capture\n");
    return -1;
  }

  int result = runtime->driver.start_capture(runtime);
  if (result == 0) {
    runtime->is_capturing = true;
  }
  return result;
}

int ethervox_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
  if (!runtime || !runtime->is_initialized) {
    return -1;
  }

  if (!runtime->is_capturing) {
    return 0;
  }

  if (!runtime->driver.stop_capture) {
    fprintf(stderr, "Audio driver does not support stop_capture\n");
    return -1;
  }

  int result = runtime->driver.stop_capture(runtime);
  if (result == 0) {
    runtime->is_capturing = false;
  }
  return result;
}

int ethervox_audio_read(ethervox_audio_runtime_t* runtime, ethervox_audio_buffer_t* buffer) {
  if (!runtime || !runtime->is_initialized || !buffer) {
    return -1;
  }

  if (!runtime->driver.read_audio) {
    fprintf(stderr, "Audio driver does not support read_audio\n");
    return -1;
  }

  return runtime->driver.read_audio(runtime, buffer);
}

// Stop audio processing
int ethervox_audio_stop(ethervox_audio_runtime_t* runtime) {
  if (!runtime) {
    return -1;
  }

  int result = 0;

  // Stop audio capture
  if (runtime->is_capturing && runtime->driver.stop_capture) {
    result = runtime->driver.stop_capture(runtime);
    runtime->is_capturing = false;
  }

  // Stop audio playback
  if (runtime->is_playing && runtime->driver.stop_playback) {
    int playback_result = runtime->driver.stop_playback(runtime);
    if (result == 0) {
      result = playback_result;
    }
    runtime->is_playing = false;
  }

  return result;
}

// Cleanup audio runtime
void ethervox_audio_cleanup(ethervox_audio_runtime_t* runtime) {
  if (!runtime) {
    return;
  }

  ethervox_audio_stop(runtime);

  if (runtime->driver.cleanup) {
    runtime->driver.cleanup(runtime);
  }

  runtime->is_initialized = false;
}

// Language detection (placeholder implementation)
int ethervox_language_detect(const ethervox_audio_buffer_t* buffer,
                             ethervox_language_detect_t* result) {
  if (!buffer || !result) {
    return -1;
  }

  // Placeholder: Simple heuristic based on audio characteristics
  // In a real implementation, this would use ML models
  snprintf(result->language_code, sizeof(result->language_code), "%s", "en");
  result->confidence = kEthervoxAudioLanguageConfidenceDefault;
  result->is_ambient = true;

  return 0;
}

// TTS synthesis (placeholder implementation)
int ethervox_tts_synthesize(ethervox_audio_runtime_t* runtime,
                            const ethervox_tts_request_t* request,
                            ethervox_audio_buffer_t* output) {
  if (!runtime || !request || !output) {
    return -1;
  }

  // Placeholder: Generate simple tone as audio output
  uint32_t samples = runtime->config.sample_rate * kEthervoxAudioTtsDurationSeconds;
  output->data = (float*)malloc(samples * sizeof(float));
  output->size = samples;
  output->channels = 1;
  output->timestamp_us = 0;

  // Generate a simple sine wave (440 Hz)
  const float sample_rate_f = (float)runtime->config.sample_rate;

  for (uint32_t i = 0; i < samples; i++) {
    const float time_s = (float)i / sample_rate_f;
    output->data[i] = kEthervoxAudioToneAmplitude *
                      sinf(kEthervoxAudioTwoPi * kEthervoxAudioToneFrequencyHz * time_s);
  }

  return 0;
}

// Free audio buffer
void ethervox_audio_buffer_free(ethervox_audio_buffer_t* buffer) {
  if (buffer && buffer->data) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
  }
}