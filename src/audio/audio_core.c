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
#include "ethervox/error.h"

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
ethervox_result_t ethervox_audio_init(ethervox_audio_runtime_t* runtime, const ethervox_audio_config_t* config) {
  ETHERVOX_CHECK_PTR(runtime);
  ETHERVOX_CHECK_PTR(config);

  memset(runtime, 0, sizeof(ethervox_audio_runtime_t));
  runtime->config = *config;

  // Register platform-specific driver
  ethervox_result_t result = ethervox_audio_register_platform_driver(runtime);
  if (ethervox_is_error(result)) {
    ETHERVOX_RETURN_ERROR(result, "Failed to register platform audio driver");
  }

  // Initialize platform-specific audio subsystem
  if (runtime->driver.init) {
    result = runtime->driver.init(runtime, config);
    if (ethervox_is_success(result)) {
      runtime->is_initialized = true;
  snprintf(runtime->current_language, sizeof(runtime->current_language), "%s", "en");  // Default language
      runtime->language_confidence = 1.0f;
    } else {
      ETHERVOX_RETURN_ERROR(result, "Platform audio driver initialization failed");
    }
  }

  return ETHERVOX_SUCCESS;
}

// Start audio processing
ethervox_result_t ethervox_audio_start(ethervox_audio_runtime_t* runtime) {
  ETHERVOX_CHECK_PTR(runtime);
  if (!runtime->is_initialized) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Audio runtime not initialized");
  }

  ethervox_result_t result = ETHERVOX_SUCCESS;

  // Start audio capture
  if (runtime->driver.start_capture) {
    result = runtime->driver.start_capture(runtime);
    if (ethervox_is_success(result)) {
      runtime->is_capturing = true;
    }
  }

  // Start audio playback
  if (ethervox_is_success(result) && runtime->driver.start_playback) {
    result = runtime->driver.start_playback(runtime);
    if (ethervox_is_success(result)) {
      runtime->is_playing = true;
    }
  }

  return result;
}

ethervox_result_t ethervox_audio_start_capture(ethervox_audio_runtime_t* runtime) {
  ETHERVOX_CHECK_PTR(runtime);
  if (!runtime->is_initialized) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Audio runtime not initialized");
  }

  if (runtime->is_capturing) {
    return ETHERVOX_SUCCESS;
  }

  if (!runtime->driver.start_capture) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_SUPPORTED, "Audio driver does not support start_capture");
  }

  ethervox_result_t result = runtime->driver.start_capture(runtime);
  if (ethervox_is_success(result)) {
    runtime->is_capturing = true;
  }
  return result;
}

ethervox_result_t ethervox_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
  ETHERVOX_CHECK_PTR(runtime);
  if (!runtime->is_initialized) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Audio runtime not initialized");
  }

  if (!runtime->is_capturing) {
    return ETHERVOX_SUCCESS;
  }

  if (!runtime->driver.stop_capture) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_SUPPORTED, "Audio driver does not support stop_capture");
  }

  ethervox_result_t result = runtime->driver.stop_capture(runtime);
  if (ethervox_is_success(result)) {
    runtime->is_capturing = false;
  }
  return result;
}

ethervox_result_t ethervox_audio_read(ethervox_audio_runtime_t* runtime, ethervox_audio_buffer_t* buffer) {
  ETHERVOX_CHECK_PTR(runtime);
  ETHERVOX_CHECK_PTR(buffer);
  if (!runtime->is_initialized) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Audio runtime not initialized");
  }

  if (!runtime->driver.read_audio) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_SUPPORTED, "Audio driver does not support read_audio");
  }

  return runtime->driver.read_audio(runtime, buffer);
}

// Stop audio processing
ethervox_result_t ethervox_audio_stop(ethervox_audio_runtime_t* runtime) {
  ETHERVOX_CHECK_PTR(runtime);

  ethervox_result_t result = ETHERVOX_SUCCESS;

  // Stop audio capture
  if (runtime->is_capturing && runtime->driver.stop_capture) {
    result = runtime->driver.stop_capture(runtime);
    runtime->is_capturing = false;
  }

  // Stop audio playback
  if (runtime->is_playing && runtime->driver.stop_playback) {
    ethervox_result_t playback_result = runtime->driver.stop_playback(runtime);
    if (ethervox_is_success(result)) {
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
ethervox_result_t ethervox_language_detect(const ethervox_audio_buffer_t* buffer,
                             ethervox_language_detect_t* result) {
  ETHERVOX_CHECK_PTR(buffer);
  ETHERVOX_CHECK_PTR(result);

  // Placeholder: Simple heuristic based on audio characteristics
  // In a real implementation, this would use ML models
  snprintf(result->language_code, sizeof(result->language_code), "%s", "en");
  result->confidence = kEthervoxAudioLanguageConfidenceDefault;
  result->is_ambient = true;

  return ETHERVOX_SUCCESS;
}

// TTS synthesis (placeholder implementation)
ethervox_result_t ethervox_tts_synthesize(ethervox_audio_runtime_t* runtime,
                            const ethervox_tts_request_t* request,
                            ethervox_audio_buffer_t* output) {
  ETHERVOX_CHECK_PTR(runtime);
  ETHERVOX_CHECK_PTR(request);
  ETHERVOX_CHECK_PTR(output);

#ifdef __APPLE__
  // Use macOS `say` command for TTS
  // Build command with proper escaping
  char command[2048];
  
  // Escape single quotes in text for shell
  char escaped_text[1024];
  const char* src = request->text;
  char* dst = escaped_text;
  while (*src && (dst - escaped_text) < (int)sizeof(escaped_text) - 10) {
    if (*src == '\'') {
      *dst++ = '\'';
      *dst++ = '\\';
      *dst++ = '\'';
      *dst++ = '\'';
    } else {
      *dst++ = *src;
    }
    src++;
  }
  *dst = '\0';
  
  snprintf(command, sizeof(command), "say '%s' &", escaped_text);
  
  // Execute TTS in background
  int result = system(command);
  
  // Return empty buffer (audio plays directly via system)
  output->data = NULL;
  output->size = 0;
  output->channels = 1;
  output->timestamp_us = 0;
  
  if (result != 0) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_TTS_SYNTHESIS_FAILED, "TTS system command failed");
  }
  return ETHERVOX_SUCCESS;
  
#else
  // Fallback: Generate simple tone as audio output
  uint32_t samples = runtime->config.sample_rate * kEthervoxAudioTtsDurationSeconds;
  output->data = (float*)malloc(samples * sizeof(float));
  if (!output->data) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate TTS output buffer");
  }
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

  return ETHERVOX_SUCCESS;
#endif
}

// Free audio buffer
void ethervox_audio_buffer_free(ethervox_audio_buffer_t* buffer) {
  if (buffer && buffer->data) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
  }
}