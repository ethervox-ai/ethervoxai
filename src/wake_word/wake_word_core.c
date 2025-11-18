/**
 * @file wake_word_core.c
 * @brief Placeholder wake word implementation for EthervoxAI
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ethervox/wake_word.h"

#define DEFAULT_WAKE_WORD "hey ethervox"

ethervox_wake_config_t ethervox_wake_get_default_config(void) {
  ethervox_wake_config_t config = {.method = ETHERVOX_WAKE_METHOD_KEYWORD_SPOTTING,
                                   .wake_word = DEFAULT_WAKE_WORD,
                                   .sensitivity = 0.6f,
                                   .sample_rate = 16000,
                                   .frame_length = 512,
                                   .model_path = NULL,
                                   .continuous_listening = true,
                                   .timeout_ms = 5000};
  return config;
}

int ethervox_wake_init(ethervox_wake_runtime_t* runtime, const ethervox_wake_config_t* config) {
  if (!runtime) {
    return -1;
  }

  memset(runtime, 0, sizeof(*runtime));

  runtime->config = config ? *config : ethervox_wake_get_default_config();
  if (!runtime->config.wake_word) {
    runtime->config.wake_word = DEFAULT_WAKE_WORD;
  }

  runtime->buffer_size = runtime->config.sample_rate / 2;  // 500ms of audio at 16kHz
  runtime->audio_buffer = (float*)calloc(runtime->buffer_size, sizeof(float));
  if (!runtime->audio_buffer) {
    return -1;
  }

  runtime->is_initialized = true;
  runtime->write_index = 0;
  runtime->wake_detected = false;
  runtime->last_detection_time = 0;
  return 0;
}

int ethervox_wake_process(ethervox_wake_runtime_t* runtime,
                          const ethervox_audio_buffer_t* audio_buffer,
                          ethervox_wake_result_t* result) {
  if (!runtime || !runtime->is_initialized || !audio_buffer || !audio_buffer->data || !result) {
    return -1;
  }

  memset(result, 0, sizeof(*result));
  result->wake_word = runtime->config.wake_word;

  const int16_t* samples = (const int16_t*)audio_buffer->data;
  const uint32_t sample_count = audio_buffer->size / sizeof(int16_t);

  if (sample_count == 0) {
    return 1;
  }

  double energy_sum = 0.0;
  for (uint32_t i = 0; i < sample_count; ++i) {
    energy_sum += fabs((double)samples[i]) / 32768.0;
  }

  const double average_energy = energy_sum / sample_count;
  const double threshold = 0.15 + (0.5 * (1.0 - runtime->config.sensitivity));

  if (average_energy >= threshold && !runtime->wake_detected) {
    runtime->wake_detected = true;
    runtime->last_detection_time = audio_buffer->timestamp_us;

    result->detected = true;
    result->confidence = (float)fmin(1.0, average_energy * 2.0);
    result->timestamp_us = audio_buffer->timestamp_us;
    result->start_index = 0;
    result->end_index = sample_count;

    printf("Wake word detected (placeholder) with energy %.3f\n", average_energy);
    return 0;
  }

  if (!runtime->config.continuous_listening && runtime->wake_detected) {
    return 0;
  }

  return 1;
}

void ethervox_wake_reset(ethervox_wake_runtime_t* runtime) {
  if (!runtime) {
    return;
  }

  runtime->wake_detected = false;
  runtime->last_detection_time = 0;
}

void ethervox_wake_cleanup(ethervox_wake_runtime_t* runtime) {
  if (!runtime) {
    return;
  }

  if (runtime->audio_buffer) {
    free(runtime->audio_buffer);
    runtime->audio_buffer = NULL;
  }

  runtime->is_initialized = false;
}
