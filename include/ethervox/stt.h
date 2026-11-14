/**
 * @file stt.h
 * @brief Speech-to-Text engine for EthervoxAI
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Proprietary and confidential. See LICENSE.
 */

#ifndef ETHERVOX_STT_H
#define ETHERVOX_STT_H

#include <stdbool.h>
#include <stdint.h>

#include "ethervox/audio.h"
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * STT backend types
 *
 * Vosk and Whisper have been replaced (not extended) by two Granite Speech
 * variants for the voice-conversation (Mode 1/4) and transcription (Mode 2)
 * pipelines - there is no backward-compat path.
 *
 *   ETHERVOX_STT_BACKEND_GRANITE_SPEECH        - granite-speech-4.1-2b (BASE)
 *       Punctuated, capitalized ASR + AST. Used by Mode 1 (voice conversation)
 *       and Mode 4 (voice-to-text). Never given tool-calling/conversational
 *       prompts - IBM's model card confirms unfamiliar prompts just fall back
 *       to plain transcription, so prompting is ASR-only by construction.
 *   ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS   - granite-speech-4.1-2b-plus
 *       Adds built-in Speaker-Attributed ASR (SAA): prompted with
 *       "Speaker attribution: ... adding [Speaker 1]: and [Speaker 2]: tags"
 *       to get native speaker-tagged output - replaces Whisper AND any
 *       separate diarization heuristic in one step. Used by Mode 2
 *       (transcription) exclusively. Supports `prefix_text`-based incremental
 *       decoding for long recordings (see ethervox_stt_config_t below).
 *
 * Both variants load via llama.cpp's mtmd (multimodal) library - see
 * cmake/FetchDependencies.cmake for the required llama.cpp version pin
 * (b9045 / commit a00e47e422bc4e48b8d2cdcfb16b7e55748237c2 or later - the
 * first release with the Granite Speech Conformer+QFormer support in
 * tools/mtmd/models/granite-speech.cpp).
 */
typedef enum {
  ETHERVOX_STT_BACKEND_GRANITE_SPEECH,       // granite-speech-4.1-2b (BASE): ASR+AST, Modes 1 & 4
  ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS,  // granite-speech-4.1-2b-plus: SAA transcription, Mode 2
  ETHERVOX_STT_BACKEND_CUSTOM,   // Custom backend
} ethervox_stt_backend_t;

/**
 * STT configuration
 */
typedef struct {
  ethervox_stt_backend_t backend;
  const char* model_path;       // Path to the main Granite Speech GGUF (LLM + audio encoder weights)
  const char* mmproj_path;      // Path to the companion mmproj GGUF (audio projector) - required,
                                 // Granite Speech always ships as a (model, mmproj) pair, never one file
  const char* language;         // Language code (e.g., "en-US", "es-ES", "zh-CN")
  uint32_t sample_rate;         // Audio sample rate - must be 16000 Hz (Granite Speech's fixed encoder rate)
  bool enable_partial_results;  // Stream partial transcriptions
  bool enable_punctuation;      // Add punctuation to results
  float vad_threshold;          // Voice activity detection threshold
  bool translate_to_english;    // Translate non-English speech to English (AST prompt)
  const char* prefix_text;      // Granite Speech Plus incremental decoding: pass the previously
                                 // decoded transcript segment back in so a long Mode 2 recording can
                                 // be chunked (~30-60s segments) without re-decoding earlier audio and
                                 // without losing consistent [Speaker N]: numbering across chunk
                                 // boundaries. Leave NULL for Mode 1/4 (BASE variant, single utterance)
                                 // and for the first chunk of a Mode 2 session.
  uint32_t max_transcript_tokens;  // Cap on generated transcript tokens (0 = backend default:
                                    // 256 for BASE single-utterance calls, 1024 for PLUS chunks)
  int n_gpu_layers;              // GPU offload layers for the Granite Speech LLM decoder (0 = CPU only)
  struct ethervox_model_pool* pool;  // N6.3: Model pool for budget enforcement (NULL = direct loading for compatibility)
} ethervox_stt_config_t;

/**
 * STT result
 */
typedef struct ethervox_stt_result {
  char* text;              // Transcribed text
  float confidence;        // Overall confidence (0.0 - 1.0)
  bool is_partial;         // Partial result (not final)
  bool is_final;           // Final result
  uint64_t start_time_us;  // Start timestamp
  uint64_t end_time_us;    // End timestamp
  const char* language;    // Detected language
} ethervox_stt_result_t;

/**
 * STT runtime
 */
typedef struct {
  ethervox_stt_config_t config;
  void* backend_context;  // Backend-specific context (Vosk/Whisper)
  bool is_initialized;
  bool is_processing;

  // Audio buffering for streaming
  float* audio_accumulator;
  uint32_t accumulator_size;
  uint32_t accumulator_write_pos;

  // Platform-specific
  void* platform_data;
} ethervox_stt_runtime_t;

/**
 * Get default STT configuration
 */
ethervox_stt_config_t ethervox_stt_get_default_config(void);

/**
 * Initialize STT engine
 *
 * @param runtime STT runtime structure
 * @param config Configuration (NULL for defaults)
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_stt_init(ethervox_stt_runtime_t* runtime, const ethervox_stt_config_t* config);

/**
 * Start STT processing session
 *
 * @param runtime STT runtime
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_stt_start(ethervox_stt_runtime_t* runtime);

/**
 * Process audio for speech recognition
 *
 * @param runtime STT runtime
 * @param audio_buffer Audio buffer from ethervox_audio_read()
 * @param result Recognition result (output, can be partial)
 * @return ETHERVOX_SUCCESS on success, error code otherwise, 1 if no result yet
 */
ethervox_result_t ethervox_stt_process(ethervox_stt_runtime_t* runtime,
                         const ethervox_audio_buffer_t* audio_buffer,
                         ethervox_stt_result_t* result);

/**
 * Finalize STT processing and get final result
 *
 * @param runtime STT runtime
 * @param result Final recognition result (output)
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_stt_finalize(ethervox_stt_runtime_t* runtime, ethervox_stt_result_t* result);

/**
 * Stop STT processing session
 */
void ethervox_stt_stop(ethervox_stt_runtime_t* runtime);

/**
 * Free STT result
 */
void ethervox_stt_result_free(ethervox_stt_result_t* result);

/**
 * Set language for transcription (hot-switch without re-init)
 * 
 * @param runtime STT runtime
 * @param language Language code ("en", "es", "zh", etc.) or "auto" for detection
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_stt_set_language(ethervox_stt_runtime_t* runtime, const char* language);

/**
 * Cleanup STT engine
 */
void ethervox_stt_cleanup(ethervox_stt_runtime_t* runtime);

// Granite Speech backend functions (internal) - shared implementation for both
// the BASE (ETHERVOX_STT_BACKEND_GRANITE_SPEECH) and PLUS
// (ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS) variants; the active variant is
// read from runtime->config.backend at init time and selects the ASR vs SAA
// prompt - see src/stt/granite_speech_backend.c.
ethervox_result_t ethervox_stt_granite_speech_init(ethervox_stt_runtime_t* runtime);
ethervox_result_t ethervox_stt_granite_speech_start(ethervox_stt_runtime_t* runtime);
ethervox_result_t ethervox_stt_granite_speech_process(ethervox_stt_runtime_t* runtime,
                                  const ethervox_audio_buffer_t* audio_buffer,
                                  ethervox_stt_result_t* result);
ethervox_result_t ethervox_stt_granite_speech_finalize(ethervox_stt_runtime_t* runtime, ethervox_stt_result_t* result);
void ethervox_stt_granite_speech_stop(ethervox_stt_runtime_t* runtime);
void ethervox_stt_granite_speech_cleanup(ethervox_stt_runtime_t* runtime);

/**
 * Release the ~1.1 GB audio projector while keeping the underlying model resident (C3.6b) -
 * for a host that wants to free memory between utterances/sessions without paying the full
 * model-reload cost of ethervox_stt_granite_speech_cleanup()+init() next time.
 *
 * Only meaningful when this runtime was loaded through a model pool (runtime->config.pool set) -
 * that's the only path where the projector is a separately-freeable allocation the pool tracks;
 * the legacy direct-loading path (no pool) returns ETHERVOX_ERROR_NOT_SUPPORTED, since there's
 * nothing pool-level to detach there. A no-op, successful call if already released.
 *
 * @param runtime STT runtime, previously initialized via ethervox_stt_granite_speech_init()
 * @return ETHERVOX_SUCCESS on success (including already-released), error code otherwise
 */
ethervox_result_t ethervox_stt_granite_speech_release_projector(ethervox_stt_runtime_t* runtime);

/**
 * Re-attaches the audio projector previously released via
 * ethervox_stt_granite_speech_release_projector(), without reloading the model. A no-op,
 * successful call if already attached.
 *
 * @param runtime STT runtime
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_stt_granite_speech_reattach_projector(ethervox_stt_runtime_t* runtime);

#ifdef __cplusplus
}
#endif

#endif  // ETHERVOX_STT_H