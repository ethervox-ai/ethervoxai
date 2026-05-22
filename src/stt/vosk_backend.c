/**
 * @file vosk_backend.c
 * @brief Vosk STT backend for real-time conversation
 *
 * Provides lightweight, real-time speech recognition for voice conversations.
 * Vosk is faster than Whisper (~0.3x realtime) and uses less memory (~50MB).
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "ethervox/stt.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"
#include "ethervox/config.h"

// Conditional compilation based on Vosk availability
#ifdef VOSK_AVAILABLE
#include "vosk_api.h"

#define LOG_ERROR(...) ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...)  ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...)  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(...) ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Vosk backend context for streaming recognition
 */
typedef struct {
    VoskModel* model;
    VoskRecognizer* recognizer;
    
    // Streaming state
    char* partial_result;
    char* final_result;
    bool has_final_result;
    
    // Statistics
    uint64_t audio_frames_processed;
    uint64_t recognition_count;
} vosk_backend_context_t;

/**
 * @brief Parse JSON result from Vosk
 * 
 * Vosk returns JSON like: {"text": "hello world"}
 * We extract the text field.
 */
static char* parse_vosk_json_text(const char* json) {
    if (!json) return NULL;
    
    const char* text_key = "\"text\"";
    const char* text_start = strstr(json, text_key);
    if (!text_start) return NULL;
    
    // Skip past "text": "
    text_start = strchr(text_start, ':');
    if (!text_start) return NULL;
    text_start++;
    
    // Skip whitespace
    while (*text_start == ' ' || *text_start == '\t') text_start++;
    
    // Skip opening quote
    if (*text_start != '"') return NULL;
    text_start++;
    
    // Find closing quote
    const char* text_end = strchr(text_start, '"');
    if (!text_end) return NULL;
    
    // Extract text
    size_t len = text_end - text_start;
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    
    memcpy(result, text_start, len);
    result[len] = '\0';
    
    return result;
}

/**
 * @brief Initialize Vosk backend
 */
ethervox_result_t ethervox_stt_vosk_init(ethervox_stt_runtime_t* runtime) {
    ETHERVOX_CHECK_PTR(runtime);
    
    vosk_backend_context_t* ctx = (vosk_backend_context_t*)calloc(1, sizeof(vosk_backend_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate Vosk context");
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    // Set Vosk log level (0 = errors only, -1 = silent)
    vosk_set_log_level(-1);  // Silent unless debug enabled
    
    // Load model
    const char* model_path = runtime->config.model_path;
    if (!model_path) {
        // Try default location
        const char* home = getenv("HOME");
        if (home) {
            static char default_path[512];
            snprintf(default_path, sizeof(default_path), "%s/.ethervox/models/vosk/vosk-model-small-en-us-0.15", home);
            model_path = default_path;
        } else {
            LOG_ERROR("No model path specified and HOME not set");
            free(ctx);
            return ETHERVOX_ERROR_FILE_NOT_FOUND;
        }
    }
    
    LOG_INFO("Loading Vosk model from: %s", model_path);
    ctx->model = vosk_model_new(model_path);
    if (!ctx->model) {
        LOG_ERROR("Failed to load Vosk model from: %s", model_path);
        LOG_ERROR("Download model: https://alphacephei.com/vosk/models");
        LOG_ERROR("Suggested: vosk-model-small-en-us-0.15 (~40MB)");
        free(ctx);
        return ETHERVOX_ERROR_STT_INIT;
    }
    
    // Create recognizer
    float sample_rate = runtime->config.sample_rate;
    ctx->recognizer = vosk_recognizer_new(ctx->model, sample_rate);
    if (!ctx->recognizer) {
        LOG_ERROR("Failed to create Vosk recognizer");
        vosk_model_free(ctx->model);
        free(ctx);
        return ETHERVOX_ERROR_STT_INIT;
    }
    
    // Enable partial results for streaming
    if (runtime->config.enable_partial_results) {
        vosk_recognizer_set_max_alternatives(ctx->recognizer, 0);
        vosk_recognizer_set_words(ctx->recognizer, 1);
    }
    
    runtime->backend_context = ctx;
    
    LOG_INFO("Vosk backend initialized (sample_rate=%.0f Hz)", sample_rate);
    
    return ETHERVOX_SUCCESS;
}

/**
 * @brief Start Vosk recognition session
 */
ethervox_result_t ethervox_stt_vosk_start(ethervox_stt_runtime_t* runtime) {
    ETHERVOX_CHECK_PTR(runtime);
    if (!runtime->backend_context) {
        LOG_ERROR("Invalid runtime or context");
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    
    vosk_backend_context_t* ctx = (vosk_backend_context_t*)runtime->backend_context;
    
    // Reset state
    ctx->has_final_result = false;
    if (ctx->partial_result) {
        free(ctx->partial_result);
        ctx->partial_result = NULL;
    }
    if (ctx->final_result) {
        free(ctx->final_result);
        ctx->final_result = NULL;
    }
    
    LOG_DEBUG("Vosk recognition started");
    
    return ETHERVOX_SUCCESS;
}

/**
 * @brief Process audio frame with Vosk
 */
ethervox_result_t ethervox_stt_vosk_process(ethervox_stt_runtime_t* runtime,
                               const ethervox_audio_buffer_t* audio_buffer,
                               ethervox_stt_result_t* result) {
    ETHERVOX_CHECK_PTR(runtime);
    ETHERVOX_CHECK_PTR(runtime->backend_context);
    ETHERVOX_CHECK_PTR(audio_buffer);
    ETHERVOX_CHECK_PTR(result);
    
    vosk_backend_context_t* ctx = (vosk_backend_context_t*)runtime->backend_context;
    
    // Convert float samples to int16
    size_t sample_count = audio_buffer->frame_count;
    int16_t* pcm_data = (int16_t*)malloc(sample_count * sizeof(int16_t));
    if (!pcm_data) {
        LOG_ERROR("Failed to allocate PCM buffer");
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    for (size_t i = 0; i < sample_count; i++) {
        float sample = audio_buffer->data[i];
        // Clamp to [-1.0, 1.0]
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        pcm_data[i] = (int16_t)(sample * 32767.0f);
    }
    
    // Feed audio to Vosk
    int accept_result = vosk_recognizer_accept_waveform(ctx->recognizer, (const char*)pcm_data, sample_count * sizeof(int16_t));
    
    free(pcm_data);
    
    ctx->audio_frames_processed++;
    
    if (accept_result) {
        // Final result available
        const char* json_result = vosk_recognizer_result(ctx->recognizer);
        
        char* text = parse_vosk_json_text(json_result);
        if (text && strlen(text) > 0) {
            if (ctx->final_result) free(ctx->final_result);
            ctx->final_result = text;
            ctx->has_final_result = true;
            ctx->recognition_count++;
            
            // Populate result
            result->text = strdup(text);
            result->confidence = 0.9f;  // Vosk doesn't provide per-word confidence easily
            result->is_final = true;
            result->is_partial = false;
            
            LOG_DEBUG("Vosk final result: %s", text);
            
            return ETHERVOX_SUCCESS;  // Success with final result
        } else {
            if (text) free(text);
        }
    } else {
        // Partial result
        if (runtime->config.enable_partial_results) {
            const char* json_partial = vosk_recognizer_partial_result(ctx->recognizer);
            
            char* text = parse_vosk_json_text(json_partial);
            if (text && strlen(text) > 0) {
                if (ctx->partial_result) free(ctx->partial_result);
                ctx->partial_result = text;
                
                // Populate result
                result->text = strdup(text);
                result->confidence = 0.5f;  // Lower confidence for partial
                result->is_final = false;
                result->is_partial = true;
                
                LOG_DEBUG("Vosk partial result: %s", text);
                
                return ETHERVOX_SUCCESS;  // Success with partial result
            } else {
                if (text) free(text);
            }
        }
    }
    
    return 1;  // No result yet
}

/**
 * @brief Finalize Vosk recognition
 */
ethervox_result_t ethervox_stt_vosk_finalize(ethervox_stt_runtime_t* runtime, ethervox_stt_result_t* result) {
    ETHERVOX_CHECK_PTR(runtime);
    ETHERVOX_CHECK_PTR(runtime->backend_context);
    ETHERVOX_CHECK_PTR(result);
    
    vosk_backend_context_t* ctx = (vosk_backend_context_t*)runtime->backend_context;
    
    // Get any remaining buffered result
    const char* json_final = vosk_recognizer_final_result(ctx->recognizer);
    
    char* text = parse_vosk_json_text(json_final);
    if (text && strlen(text) > 0) {
        result->text = text;
        result->confidence = 0.9f;
        result->is_final = true;
        result->is_partial = false;
        
        LOG_INFO("Vosk finalized: %s", text);
        
        return ETHERVOX_SUCCESS;
    }
    
    // No final result, use last final if available
    if (ctx->final_result) {
        result->text = strdup(ctx->final_result);
        result->confidence = 0.9f;
        result->is_final = true;
        result->is_partial = false;
        
        return ETHERVOX_SUCCESS;
    }
    
    if (text) free(text);
    
    LOG_WARN("Vosk finalize: no result available");
    return ETHERVOX_ERROR_STT_NO_RESULT;
}

/**
 * @brief Stop Vosk recognition
 */
void ethervox_stt_vosk_stop(ethervox_stt_runtime_t* runtime) {
    if (!runtime || !runtime->backend_context) {
        return;
    }
    
    vosk_backend_context_t* ctx = (vosk_backend_context_t*)runtime->backend_context;
    
    LOG_INFO("Vosk stopped (frames=%llu, recognitions=%llu)",
             (unsigned long long)ctx->audio_frames_processed,
             (unsigned long long)ctx->recognition_count);
}

/**
 * @brief Cleanup Vosk backend
 */
void ethervox_stt_vosk_cleanup(ethervox_stt_runtime_t* runtime) {
    if (!runtime || !runtime->backend_context) {
        return;
    }
    
    vosk_backend_context_t* ctx = (vosk_backend_context_t*)runtime->backend_context;
    
    if (ctx->recognizer) {
        vosk_recognizer_free(ctx->recognizer);
    }
    
    if (ctx->model) {
        vosk_model_free(ctx->model);
    }
    
    if (ctx->partial_result) {
        free(ctx->partial_result);
    }
    
    if (ctx->final_result) {
        free(ctx->final_result);
    }
    
    free(ctx);
    runtime->backend_context = NULL;
    
    LOG_INFO("Vosk backend cleaned up");
}

#else
// Vosk not available - provide stubs

ethervox_result_t ethervox_stt_vosk_init(ethervox_stt_runtime_t* runtime) {
    (void)runtime;
    ETHERVOX_LOG_ERROR("Vosk backend not available - recompile with VOSK_AVAILABLE=1");
    ETHERVOX_LOG_ERROR("Download Vosk: https://github.com/alphacep/vosk-api");
    return ETHERVOX_ERROR_NOT_SUPPORTED;
}

ethervox_result_t ethervox_stt_vosk_start(ethervox_stt_runtime_t* runtime) {
    (void)runtime;
    return ETHERVOX_ERROR_NOT_SUPPORTED;
}

ethervox_result_t ethervox_stt_vosk_process(ethervox_stt_runtime_t* runtime,
                               const ethervox_audio_buffer_t* audio_buffer,
                               ethervox_stt_result_t* result) {
    (void)runtime;
    (void)audio_buffer;
    (void)result;
    return ETHERVOX_ERROR_NOT_SUPPORTED;
}

ethervox_result_t ethervox_stt_vosk_finalize(ethervox_stt_runtime_t* runtime, ethervox_stt_result_t* result) {
    (void)runtime;
    (void)result;
    return ETHERVOX_ERROR_NOT_SUPPORTED;
}

void ethervox_stt_vosk_stop(ethervox_stt_runtime_t* runtime) {
    (void)runtime;
}

void ethervox_stt_vosk_cleanup(ethervox_stt_runtime_t* runtime) {
    (void)runtime;
}

#endif  // VOSK_AVAILABLE
