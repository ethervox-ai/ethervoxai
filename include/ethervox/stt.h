/**
 * @file stt.h
 * @brief Speech-to-Text engine for EthervoxAI
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 */

#ifndef ETHERVOX_STT_H
#define ETHERVOX_STT_H

#include <stdint.h>
#include <stdbool.h>
#include "ethervox/audio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * STT backend types
 */
typedef enum {
    ETHERVOX_STT_BACKEND_VOSK,      // Vosk (lightweight, offline)
    ETHERVOX_STT_BACKEND_WHISPER,   // Whisper.cpp (higher accuracy)
    ETHERVOX_STT_BACKEND_CUSTOM,    // Custom backend
} ethervox_stt_backend_t;

/**
 * STT configuration
 */
typedef struct {
    ethervox_stt_backend_t backend;
    const char* model_path;         // Path to STT model
    const char* language;           // Language code (e.g., "en-US", "es-ES", "zh-CN")
    uint32_t sample_rate;          // Audio sample rate (16000 Hz)
    bool enable_partial_results;   // Stream partial transcriptions
    bool enable_punctuation;       // Add punctuation to results
    float vad_threshold;           // Voice activity detection threshold
} ethervox_stt_config_t;

/**
 * STT result
 */
typedef struct ethervox_stt_result {
    char* text;                    // Transcribed text
    float confidence;              // Overall confidence (0.0 - 1.0)
    bool is_partial;               // Partial result (not final)
    bool is_final;                 // Final result
    uint64_t start_time_us;       // Start timestamp
    uint64_t end_time_us;         // End timestamp
    const char* language;         // Detected language
} ethervox_stt_result_t;

/**
 * STT runtime
 */
typedef struct {
    ethervox_stt_config_t config;
    void* backend_context;        // Backend-specific context (Vosk/Whisper)
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
 * @return 0 on success, -1 on error
 */
int ethervox_stt_init(ethervox_stt_runtime_t* runtime, const ethervox_stt_config_t* config);

/**
 * Start STT processing session
 * 
 * @param runtime STT runtime
 * @return 0 on success, -1 on error
 */
int ethervox_stt_start(ethervox_stt_runtime_t* runtime);

/**
 * Process audio for speech recognition
 * 
 * @param runtime STT runtime
 * @param audio_buffer Audio buffer from ethervox_audio_read()
 * @param result Recognition result (output, can be partial)
 * @return 0 on success, -1 on error, 1 if no result yet
 */
int ethervox_stt_process(ethervox_stt_runtime_t* runtime,
                        const ethervox_audio_buffer_t* audio_buffer,
                        ethervox_stt_result_t* result);

/**
 * Finalize STT processing and get final result
 * 
 * @param runtime STT runtime
 * @param result Final recognition result (output)
 * @return 0 on success, -1 on error
 */
int ethervox_stt_finalize(ethervox_stt_runtime_t* runtime, ethervox_stt_result_t* result);

/**
 * Stop STT processing session
 */
void ethervox_stt_stop(ethervox_stt_runtime_t* runtime);

/**
 * Free STT result
 */
void ethervox_stt_result_free(ethervox_stt_result_t* result);

/**
 * Cleanup STT engine
 */
void ethervox_stt_cleanup(ethervox_stt_runtime_t* runtime);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_STT_H