/**
 * @file stt.h
 * @brief Speech-to-Text engine for EthervoxAI
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
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
 */
typedef enum {
  ETHERVOX_STT_BACKEND_VOSK,     // Vosk (lightweight, offline)
  ETHERVOX_STT_BACKEND_WHISPER,  // Whisper.cpp (higher accuracy)
  ETHERVOX_STT_BACKEND_CUSTOM,   // Custom backend
} ethervox_stt_backend_t;

/**
 * STT configuration
 */
typedef struct {
  ethervox_stt_backend_t backend;
  const char* model_path;       // Path to STT model
  const char* language;         // Language code (e.g., "en-US", "es-ES", "zh-CN")
  uint32_t sample_rate;         // Audio sample rate (16000 Hz)
  bool enable_partial_results;  // Stream partial transcriptions
  bool enable_punctuation;      // Add punctuation to results
  float vad_threshold;          // Voice activity detection threshold
  bool translate_to_english;    // Translate non-English speech to English (Whisper only)
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

// Backend-specific functions (internal)
ethervox_result_t ethervox_stt_whisper_init(ethervox_stt_runtime_t* runtime);
ethervox_result_t ethervox_stt_whisper_start(ethervox_stt_runtime_t* runtime);
ethervox_result_t ethervox_stt_whisper_process(ethervox_stt_runtime_t* runtime,
                                  const ethervox_audio_buffer_t* audio_buffer,
                                  ethervox_stt_result_t* result);
ethervox_result_t ethervox_stt_whisper_finalize(ethervox_stt_runtime_t* runtime, ethervox_stt_result_t* result);
void ethervox_stt_whisper_stop(ethervox_stt_runtime_t* runtime);
void ethervox_stt_whisper_cleanup(ethervox_stt_runtime_t* runtime);

// Vosk backend functions
ethervox_result_t ethervox_stt_vosk_init(ethervox_stt_runtime_t* runtime);
ethervox_result_t ethervox_stt_vosk_start(ethervox_stt_runtime_t* runtime);
ethervox_result_t ethervox_stt_vosk_process(ethervox_stt_runtime_t* runtime,
                               const ethervox_audio_buffer_t* audio_buffer,
                               ethervox_stt_result_t* result);
ethervox_result_t ethervox_stt_vosk_finalize(ethervox_stt_runtime_t* runtime, ethervox_stt_result_t* result);
void ethervox_stt_vosk_stop(ethervox_stt_runtime_t* runtime);
void ethervox_stt_vosk_cleanup(ethervox_stt_runtime_t* runtime);

// Testing utilities
/**
 * Test Whisper with a WAV file
 * 
 * @param runtime STT runtime (must be initialized)
 * @param wav_file Path to 16kHz mono WAV file
 * @param result Output transcription result
 * @return 0 on success, negative on error
 */
int ethervox_whisper_test_wav(
    ethervox_stt_runtime_t* runtime,
    const char* wav_file,
    ethervox_stt_result_t* result
);

/**
 * Quick test using the JFK sample (expected to work out of box)
 * 
 * @param runtime STT runtime (must be initialized)
 * @return 0 on success, negative on error
 */
int ethervox_whisper_test_jfk(ethervox_stt_runtime_t* runtime);

#ifdef __cplusplus
}
#endif

#endif  // ETHERVOX_STT_H