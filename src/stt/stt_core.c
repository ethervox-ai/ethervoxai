/**
 * @file stt_core.c
 * @brief Core Speech-to-Text implementation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ethervox/stt.h"

// Placeholder for Vosk integration (will be implemented with actual Vosk lib)
typedef struct {
  void* model;
  void* recognizer;
  char* last_result;
} vosk_context_t;

// Default configuration
ethervox_stt_config_t ethervox_stt_get_default_config(void) {
  ethervox_stt_config_t config = {.backend = ETHERVOX_STT_BACKEND_VOSK,
                                  .model_path = "./models/vosk-model-small-en-us-0.15",
                                  .language = "en-US",
                                  .sample_rate = 16000,
                                  .enable_partial_results = true,
                                  .enable_punctuation = true,
                                  .vad_threshold = 0.5f};
  return config;
}

// Initialize STT engine
int ethervox_stt_init(ethervox_stt_runtime_t* runtime, const ethervox_stt_config_t* config) {
  if (!runtime)
    return -1;

  memset(runtime, 0, sizeof(ethervox_stt_runtime_t));

  // Copy configuration
  if (config) {
    runtime->config = *config;
    if (config->model_path) {
      runtime->config.model_path = strdup(config->model_path);
    }
    if (config->language) {
      runtime->config.language = strdup(config->language);
    }
  } else {
    runtime->config = ethervox_stt_get_default_config();
  }

  // Allocate audio accumulator (5 seconds)
  runtime->accumulator_size = runtime->config.sample_rate * 5;
  runtime->audio_accumulator = (float*)calloc(runtime->accumulator_size, sizeof(float));
  if (!runtime->audio_accumulator) {
    return -1;
  }

  // Initialize backend
  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_VOSK: {
      // TODO: Initialize actual Vosk library
      // For now, placeholder
      vosk_context_t* ctx = (vosk_context_t*)calloc(1, sizeof(vosk_context_t));
      if (!ctx) {
        free(runtime->audio_accumulator);
        return -1;
      }

      printf("STT: Vosk backend initialized (placeholder - requires vosk library)\n");
      printf("STT: Model path: %s\n", runtime->config.model_path);
      printf("STT: Language: %s\n", runtime->config.language);

      runtime->backend_context = ctx;
      break;
    }

    case ETHERVOX_STT_BACKEND_WHISPER:
      printf("Whisper.cpp backend not yet implemented\n");
      free(runtime->audio_accumulator);
      return -1;

    default:
      free(runtime->audio_accumulator);
      return -1;
  }

  runtime->is_initialized = true;
  return 0;
}

// Start STT session
int ethervox_stt_start(ethervox_stt_runtime_t* runtime) {
  if (!runtime || !runtime->is_initialized)
    return -1;

  runtime->is_processing = true;
  runtime->accumulator_write_pos = 0;

  printf("STT: Processing started\n");
  return 0;
}

// Process audio
int ethervox_stt_process(ethervox_stt_runtime_t* runtime,
                         const ethervox_audio_buffer_t* audio_buffer,
                         ethervox_stt_result_t* result) {
  if (!runtime || !runtime->is_initialized || !runtime->is_processing || !audio_buffer || !result) {
    return -1;
  }

  memset(result, 0, sizeof(ethervox_stt_result_t));

  // Convert and accumulate audio
  const int16_t* samples = (const int16_t*)audio_buffer->data;
  uint32_t sample_count = audio_buffer->size / sizeof(int16_t);

  for (uint32_t i = 0;
       i < sample_count && runtime->accumulator_write_pos < runtime->accumulator_size; i++) {
    runtime->audio_accumulator[runtime->accumulator_write_pos++] = samples[i] / 32768.0f;
  }

  // For demo: simple word detection based on audio patterns
  // In production, this would call Vosk/Whisper

  // Placeholder logic: detect if we have enough audio
  if (runtime->accumulator_write_pos > runtime->config.sample_rate * 2) {
    // Simulate recognition result
    result->text = strdup("[Placeholder STT] Detected speech");
    result->confidence = 0.85f;
    result->is_partial = false;
    result->is_final = true;
    result->start_time_us = audio_buffer->timestamp_us;
    result->end_time_us = audio_buffer->timestamp_us;
    result->language = runtime->config.language;

    return 0;  // Have result
  }

  return 1;  // No result yet
}

// Finalize and get final result
int ethervox_stt_finalize(ethervox_stt_runtime_t* runtime, ethervox_stt_result_t* result) {
  if (!runtime || !result)
    return -1;

  memset(result, 0, sizeof(ethervox_stt_result_t));

  // TODO: Get final result from Vosk/Whisper
  result->text = strdup("[Placeholder STT] Final transcription");
  result->confidence = 0.90f;
  result->is_partial = false;
  result->is_final = true;
  result->language = runtime->config.language;

  printf("STT: Final result: %s (confidence: %.2f)\n", result->text, result->confidence);

  return 0;
}

// Stop STT session
void ethervox_stt_stop(ethervox_stt_runtime_t* runtime) {
  if (!runtime)
    return;

  runtime->is_processing = false;
  runtime->accumulator_write_pos = 0;

  printf("STT: Processing stopped\n");
}

// Free result
void ethervox_stt_result_free(ethervox_stt_result_t* result) {
  if (!result)
    return;

  if (result->text) {
    free(result->text);
    result->text = NULL;
  }
}

// Cleanup
void ethervox_stt_cleanup(ethervox_stt_runtime_t* runtime) {
  if (!runtime)
    return;

  if (runtime->audio_accumulator) {
    free(runtime->audio_accumulator);
  }

  if (runtime->config.model_path) {
    free((void*)runtime->config.model_path);
  }

  if (runtime->config.language) {
    free((void*)runtime->config.language);
  }

  if (runtime->backend_context) {
    if (runtime->config.backend == ETHERVOX_STT_BACKEND_VOSK) {
      vosk_context_t* ctx = (vosk_context_t*)runtime->backend_context;
      if (ctx->last_result)
        free(ctx->last_result);
    }
    free(runtime->backend_context);
  }

  runtime->is_initialized = false;
  printf("STT engine cleaned up\n");
}