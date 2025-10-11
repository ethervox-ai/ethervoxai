/**
 * @file wake_word.h
 * @brief Wake word detection for EthervoxAI
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 */

#ifndef ETHERVOX_WAKE_WORD_H
#define ETHERVOX_WAKE_WORD_H

#include <stdint.h>
#include <stdbool.h>
#include "ethervox/audio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Wake word detection methods
 */
typedef enum {
    ETHERVOX_WAKE_METHOD_KEYWORD_SPOTTING,  // Simple pattern matching
    ETHERVOX_WAKE_METHOD_PORCUPINE,         // Picovoice Porcupine (if available)
    ETHERVOX_WAKE_METHOD_CUSTOM_NN,         // Custom neural network
} ethervox_wake_method_t;

/**
 * Wake word detection configuration
 */
typedef struct {
    ethervox_wake_method_t method;
    const char* wake_word;              // e.g., "hey ethervox"
    float sensitivity;                  // 0.0 - 1.0 (higher = more sensitive)
    uint32_t sample_rate;              // Audio sample rate (16000 Hz recommended)
    uint32_t frame_length;             // Samples per frame (512 recommended)
    const char* model_path;            // Path to wake word model (if using NN)
    bool continuous_listening;         // Keep listening after wake word
    uint32_t timeout_ms;               // Timeout after wake word detected
} ethervox_wake_config_t;

/**
 * Wake word detection result
 */
typedef struct {
    bool detected;                     // Wake word detected
    float confidence;                  // Detection confidence (0.0 - 1.0)
    uint64_t timestamp_us;            // Detection timestamp (microseconds)
    uint32_t start_index;             // Audio buffer start index
    uint32_t end_index;               // Audio buffer end index
    const char* wake_word;            // Detected wake word
} ethervox_wake_result_t;

/**
 * Wake word detection runtime
 */
typedef struct {
    ethervox_wake_config_t config;
    void* detector_context;           // Internal detector state
    bool is_initialized;
    
    // Audio processing
    float* audio_buffer;              // Circular buffer for audio
    uint32_t buffer_size;
    uint32_t write_index;
    
    // Detection state
    bool wake_detected;
    uint64_t last_detection_time;
    
    // Platform-specific data
    void* platform_data;
} ethervox_wake_runtime_t;

/**
 * Get default wake word configuration
 */
ethervox_wake_config_t ethervox_wake_get_default_config(void);

/**
 * Initialize wake word detection
 * 
 * @param runtime Wake word runtime structure
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int ethervox_wake_init(ethervox_wake_runtime_t* runtime, const ethervox_wake_config_t* config);

/**
 * Process audio for wake word detection
 * 
 * @param runtime Wake word runtime
 * @param audio_buffer Audio buffer from ethervox_audio_read()
 * @param result Detection result (output)
 * @return 0 on success, -1 on error
 */
int ethervox_wake_process(ethervox_wake_runtime_t* runtime, 
                         const ethervox_audio_buffer_t* audio_buffer,
                         ethervox_wake_result_t* result);

/**
 * Reset wake word detector state
 */
void ethervox_wake_reset(ethervox_wake_runtime_t* runtime);

/**
 * Cleanup wake word detection
 */
void ethervox_wake_cleanup(ethervox_wake_runtime_t* runtime);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_WAKE_WORD_H