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
#include "ethervox/error.h"
#include "ethervox/logging.h"

// Default configuration - use Whisper as default since it's implemented
ethervox_stt_config_t ethervox_stt_get_default_config(void) {
  ethervox_stt_config_t config = {.backend = ETHERVOX_STT_BACKEND_WHISPER,
                                  .model_path = NULL,
                                  .language = "auto",
                                  .sample_rate = 16000,
                                  .enable_partial_results = true,
                                  .enable_punctuation = true,
                                  .vad_threshold = 0.5f,
                                  .translate_to_english = false};  // Transcribe in original language by default
  return config;
}

// Initialize STT engine
ethervox_result_t ethervox_stt_init(ethervox_stt_runtime_t* runtime, const ethervox_stt_config_t* config) {
  ETHERVOX_CHECK_PTR(runtime); 

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
    return ETHERVOX_ERROR_OUT_OF_MEMORY;
  }

  // Initialize backend
  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_VOSK: {
      ethervox_result_t result = ethervox_stt_vosk_init(runtime);
      if (ethervox_is_error(result)) {
        free(runtime->audio_accumulator);
        return result;
      }
      break;
    }

    case ETHERVOX_STT_BACKEND_WHISPER:
      ethervox_result_t result = ethervox_stt_whisper_init(runtime);
      if (ethervox_is_error(result)) {
        free(runtime->audio_accumulator);
        return result;
      }
      break;

    default:
      free(runtime->audio_accumulator);
      return ETHERVOX_ERROR_NOT_SUPPORTED;
  }

  runtime->is_initialized = true;
  return ETHERVOX_SUCCESS;
}

// Start STT session
ethervox_result_t ethervox_stt_start(ethervox_stt_runtime_t* runtime) {
  ETHERVOX_CHECK_PTR(runtime);
  if (!runtime->is_initialized) {
    return ETHERVOX_ERROR_NOT_INITIALIZED;
  }

  runtime->is_processing = true;
  runtime->accumulator_write_pos = 0;

  // Delegate to backend-specific start
  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_WHISPER:
      return ethervox_stt_whisper_start(runtime);
    
    case ETHERVOX_STT_BACKEND_VOSK:
      return ethervox_stt_vosk_start(runtime);
    
    default:
      ETHERVOX_LOG_ERROR("Unknown STT backend: %d", runtime->config.backend);
      return ETHERVOX_ERROR_NOT_SUPPORTED;
  }
}

// Process audio
ethervox_result_t ethervox_stt_process(ethervox_stt_runtime_t* runtime,
                         const ethervox_audio_buffer_t* audio_buffer,
                         ethervox_stt_result_t* result) {
  ETHERVOX_CHECK_PTR(runtime);
  ETHERVOX_CHECK_PTR(audio_buffer);
  ETHERVOX_CHECK_PTR(result);
  if (!runtime->is_initialized || !runtime->is_processing) {
    return ETHERVOX_ERROR_NOT_INITIALIZED;
  }

  memset(result, 0, sizeof(ethervox_stt_result_t));

  // Delegate to backend-specific processing
  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_WHISPER:
      return ethervox_stt_whisper_process(runtime, audio_buffer, result);
    
    case ETHERVOX_STT_BACKEND_VOSK:
      return ethervox_stt_vosk_process(runtime, audio_buffer, result);
    
    default:
      ETHERVOX_LOG_ERROR("Unknown STT backend: %d", runtime->config.backend);
      return ETHERVOX_ERROR_NOT_SUPPORTED;
  }
}

// Finalize and get final result
ethervox_result_t ethervox_stt_finalize(ethervox_stt_runtime_t* runtime, ethervox_stt_result_t* result) {
  ETHERVOX_CHECK_PTR(runtime);
  ETHERVOX_CHECK_PTR(result);

  memset(result, 0, sizeof(ethervox_stt_result_t));

  // Delegate to backend-specific finalize
  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_WHISPER:
      return ethervox_stt_whisper_finalize(runtime, result);
    
    case ETHERVOX_STT_BACKEND_VOSK:
      return ethervox_stt_vosk_finalize(runtime, result);
    
    default:
      ETHERVOX_LOG_ERROR("Unknown STT backend: %d", runtime->config.backend);
      return ETHERVOX_ERROR_NOT_SUPPORTED;
  }
}

// Stop STT session
void ethervox_stt_stop(ethervox_stt_runtime_t* runtime) {
  if (!runtime) {
    return;
  }

  runtime->is_processing = false;
  runtime->accumulator_write_pos = 0;

  // Delegate to backend-specific stop
  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_WHISPER:
      ethervox_stt_whisper_stop(runtime);
      break;
    
    case ETHERVOX_STT_BACKEND_VOSK:
      ethervox_stt_vosk_stop(runtime);
      break;
    
    default:
      break;
  }

  ETHERVOX_LOG_INFO("STT: Processing stopped");
}

// Free result
void ethervox_stt_result_free(ethervox_stt_result_t* result) {
  if (!result) {
    return;
  }

  if (result->text) {
    free(result->text);
    result->text = NULL;
  }
}

// Cleanup
void ethervox_stt_cleanup(ethervox_stt_runtime_t* runtime) {
  if (!runtime) {
    return;
  }

  if (runtime->audio_accumulator) {
    free(runtime->audio_accumulator);
  }

  // Delegate to backend-specific cleanup
  if (runtime->config.backend == ETHERVOX_STT_BACKEND_WHISPER) {
    ethervox_stt_whisper_cleanup(runtime);
  } else if (runtime->config.backend == ETHERVOX_STT_BACKEND_VOSK) {
    ethervox_stt_vosk_cleanup(runtime);
  } else if (runtime->backend_context) {
    // Other backend cleanup
    free(runtime->backend_context);
    runtime->backend_context = NULL;
  }

  if (runtime->config.model_path) {
    free((void*)runtime->config.model_path);
  }

  if (runtime->config.language) {
    free((void*)runtime->config.language);
  }

  runtime->is_initialized = false;
  ETHERVOX_LOG_INFO("STT engine cleaned up");
}

/**
 * Set language (hot-switch without re-init)
 */
ethervox_result_t ethervox_stt_set_language(ethervox_stt_runtime_t* runtime, const char* language) {
  ETHERVOX_CHECK_PTR(runtime);
  if (!runtime->is_initialized) {
    ETHERVOX_LOG_ERROR("STT runtime not initialized");
    return ETHERVOX_ERROR_NOT_INITIALIZED;
  }
  
  if (!language) {
    ETHERVOX_LOG_ERROR("Language is NULL");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  // Update config
  runtime->config.language = language;
  
  // Delegate to backend-specific language switching
  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_WHISPER: {
      extern ethervox_result_t ethervox_stt_whisper_set_language(ethervox_stt_runtime_t* runtime, const char* language);
      return ethervox_stt_whisper_set_language(runtime, language);
    }
    
    case ETHERVOX_STT_BACKEND_VOSK:
      // TODO: Implement Vosk language switching
      ETHERVOX_LOG_ERROR("Language hot-switching not yet implemented for Vosk backend");
      return ETHERVOX_ERROR_NOT_IMPLEMENTED;
    
    default:
      ETHERVOX_LOG_ERROR("Unknown STT backend: %d", runtime->config.backend);
      return ETHERVOX_ERROR_NOT_SUPPORTED;
  }
}
