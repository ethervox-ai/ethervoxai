/**
 * @file audio.h
 * @brief Audio processing interface definitions for EthervoxAI
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
#ifndef ETHERVOX_AUDIO_H
#define ETHERVOX_AUDIO_H

#include "ethervox/config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio configuration
typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t buffer_size;
    bool enable_noise_suppression;
    bool enable_echo_cancellation;
} ethervox_audio_config_t;

// Audio buffer structure
typedef struct {
    float* data;
    uint32_t size;
    uint32_t channels;
    uint64_t timestamp_us;
} ethervox_audio_buffer_t;

// Language identification result
typedef struct {
    char language_code[8];  // ISO 639-1 code
    float confidence;
    bool is_ambient;        // True if detected without wake word
} ethervox_language_detect_t;

// Speech-to-text result
typedef struct {
    char* text;
    char language_code[8];
    float confidence;
    bool is_final;
    uint64_t start_time_us;
    uint64_t end_time_us;
} ethervox_stt_result_t;

// Text-to-speech request
typedef struct {
    const char* text;
    const char* language_code;
    float speech_rate;
    float pitch;
    const char* voice_id;
} ethervox_tts_request_t;

// Audio runtime interface
typedef struct ethervox_audio_runtime ethervox_audio_runtime_t;

// Function pointers for platform-specific implementations
typedef struct {
    int (*init)(ethervox_audio_runtime_t* runtime, const ethervox_audio_config_t* config);
    int (*start_capture)(ethervox_audio_runtime_t* runtime);
    int (*stop_capture)(ethervox_audio_runtime_t* runtime);
    int (*start_playback)(ethervox_audio_runtime_t* runtime);
    int (*stop_playback)(ethervox_audio_runtime_t* runtime);
    int (*read_audio)(ethervox_audio_runtime_t* runtime, ethervox_audio_buffer_t* buffer);
    int (*write_audio)(ethervox_audio_runtime_t* runtime, const ethervox_audio_buffer_t* buffer);
    void (*cleanup)(ethervox_audio_runtime_t* runtime);
} ethervox_audio_driver_t;

// Main audio runtime structure
struct ethervox_audio_runtime {
    ethervox_audio_config_t config;
    ethervox_audio_driver_t driver;
    void* platform_data;
    bool is_initialized;
    bool is_capturing;
    bool is_playing;
    
    // Language detection state
    char current_language[8];
    float language_confidence;
    
    // Callbacks
    void (*on_audio_data)(const ethervox_audio_buffer_t* buffer, void* user_data);
    void (*on_language_detected)(const ethervox_language_detect_t* result, void* user_data);
    void (*on_stt_result)(const ethervox_stt_result_t* result, void* user_data);
    void* user_data;
};

// Public API functions
int ethervox_audio_init(ethervox_audio_runtime_t* runtime, const ethervox_audio_config_t* config);
int ethervox_audio_start(ethervox_audio_runtime_t* runtime);
int ethervox_audio_stop(ethervox_audio_runtime_t* runtime);
void ethervox_audio_cleanup(ethervox_audio_runtime_t* runtime);

// Speech processing functions
int ethervox_stt_process(ethervox_audio_runtime_t* runtime, const ethervox_audio_buffer_t* buffer, ethervox_stt_result_t* result);
int ethervox_tts_synthesize(ethervox_audio_runtime_t* runtime, const ethervox_tts_request_t* request, ethervox_audio_buffer_t* output);
int ethervox_language_detect(const ethervox_audio_buffer_t* buffer, ethervox_language_detect_t* result);

// Utility functions
ethervox_audio_config_t ethervox_audio_get_default_config(void);
void ethervox_audio_buffer_free(ethervox_audio_buffer_t* buffer);
void ethervox_stt_result_free(ethervox_stt_result_t* result);

// Platform-specific driver registration
int ethervox_audio_register_platform_driver(ethervox_audio_runtime_t* runtime);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_AUDIO_H