/**
 * @file stt_core.c
 * @brief Core Speech-to-Text dispatcher for EthervoxAI
 *
 * Dispatches to the Granite Speech backend (src/stt/granite_speech_backend.c)
 * for both supported variants:
 *   - ETHERVOX_STT_BACKEND_GRANITE_SPEECH       (BASE - Modes 1 & 4)
 *   - ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS  (PLUS - Mode 2, speaker-attributed)
 *
 * Both variants share one backend implementation - the variant only changes
 * which prompt is used (ASR vs SAA), selected at init time from
 * runtime->config.backend. Whisper and Vosk have been removed entirely, not
 * deprecated - no backward-compat path exists for them.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Proprietary and confidential. See LICENSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ethervox/stt.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"

// Default configuration - BASE variant (punctuated ASR, used by Modes 1 & 4).
// Callers that need Mode 2 (transcription) must explicitly request
// ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS and supply model_path/mmproj_path
// for the Plus GGUF pair.
ethervox_stt_config_t ethervox_stt_get_default_config(void) {
  ethervox_stt_config_t config = {
      .backend = ETHERVOX_STT_BACKEND_GRANITE_SPEECH,
      .model_path = NULL,
      .mmproj_path = NULL,
      .language = "en",
      .sample_rate = 16000,  // Fixed by Granite Speech's Conformer encoder
      .enable_partial_results = false,  // Granite Speech transcribes whole utterances, not streaming partials
      .enable_punctuation = true,       // BASE variant only - PLUS trades punctuation for SAA
      .vad_threshold = 0.5f,
      .translate_to_english = false,
      .prefix_text = NULL,
      .max_transcript_tokens = 0,  // 0 = backend picks a variant-appropriate default
      .n_gpu_layers = 0,
  };
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
    if (config->mmproj_path) {
      runtime->config.mmproj_path = strdup(config->mmproj_path);
    }
    if (config->language) {
      runtime->config.language = strdup(config->language);
    }
    if (config->prefix_text) {
      runtime->config.prefix_text = strdup(config->prefix_text);
    }
  } else {
    runtime->config = ethervox_stt_get_default_config();
  }

  // Allocate audio accumulator. Granite Speech transcribes whole utterances
  // (not a streaming/partial-result engine), so this buffer holds the entire
  // captured segment: 5s for Mode 1/4 (BASE, single conversational turn), or
  // up to the ~60s chunk size used for Mode 2 (PLUS, incremental decoding).
  // Size generously for the PLUS case; BASE callers simply use less of it.
  const uint32_t accumulator_seconds =
      (runtime->config.backend == ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS) ? 60 : 30;
  runtime->accumulator_size = runtime->config.sample_rate * accumulator_seconds;
  runtime->audio_accumulator = (float*)calloc(runtime->accumulator_size, sizeof(float));
  if (!runtime->audio_accumulator) {
    return ETHERVOX_ERROR_OUT_OF_MEMORY;
  }

  // Initialize backend
  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH:
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS: {
      ethervox_result_t result = ethervox_stt_granite_speech_init(runtime);
      if (ethervox_is_error(result)) {
        free(runtime->audio_accumulator);
        runtime->audio_accumulator = NULL;
        return result;
      }
      break;
    }

    default:
      ETHERVOX_LOG_ERROR("Unknown STT backend: %d", runtime->config.backend);
      free(runtime->audio_accumulator);
      runtime->audio_accumulator = NULL;
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

  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH:
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS:
      return ethervox_stt_granite_speech_start(runtime);

    default:
      ETHERVOX_LOG_ERROR("Unknown STT backend: %d", runtime->config.backend);
      return ETHERVOX_ERROR_NOT_SUPPORTED;
  }
}

// Process audio (accumulates; Granite Speech has no incremental/partial path)
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

  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH:
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS:
      return ethervox_stt_granite_speech_process(runtime, audio_buffer, result);

    default:
      ETHERVOX_LOG_ERROR("Unknown STT backend: %d", runtime->config.backend);
      return ETHERVOX_ERROR_NOT_SUPPORTED;
  }
}

// Finalize and get final result - this is where Granite Speech actually runs
// inference: the audio accumulated since ethervox_stt_start() is tokenized
// (via mtmd) together with the variant's prompt and decoded to text.
ethervox_result_t ethervox_stt_finalize(ethervox_stt_runtime_t* runtime, ethervox_stt_result_t* result) {
  ETHERVOX_CHECK_PTR(runtime);
  ETHERVOX_CHECK_PTR(result);

  memset(result, 0, sizeof(ethervox_stt_result_t));

  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH:
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS:
      return ethervox_stt_granite_speech_finalize(runtime, result);

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

  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH:
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS:
      ethervox_stt_granite_speech_stop(runtime);
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
    runtime->audio_accumulator = NULL;
  }

  switch (runtime->config.backend) {
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH:
    case ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS:
      ethervox_stt_granite_speech_cleanup(runtime);
      break;

    default:
      if (runtime->backend_context) {
        free(runtime->backend_context);
        runtime->backend_context = NULL;
      }
      break;
  }

  if (runtime->config.model_path) {
    free((void*)runtime->config.model_path);
  }
  if (runtime->config.mmproj_path) {
    free((void*)runtime->config.mmproj_path);
  }
  if (runtime->config.language) {
    free((void*)runtime->config.language);
  }
  if (runtime->config.prefix_text) {
    free((void*)runtime->config.prefix_text);
  }

  runtime->is_initialized = false;
  ETHERVOX_LOG_INFO("STT engine cleaned up");
}

/**
 * Set language (hot-switch without re-init).
 *
 * Granite Speech does not require per-language re-initialization the way
 * Whisper did (it is multilingual within one checkpoint), so this simply
 * updates the config's language hint used to construct the next prompt.
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

  if (runtime->config.language) {
    free((void*)runtime->config.language);
  }
  runtime->config.language = strdup(language);

  return ETHERVOX_SUCCESS;
}
