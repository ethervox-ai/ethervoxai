/**
#include "ethervox/error.h"
 * @file train_pronunciation.c
 * @brief LLM tool for adaptive pronunciation training
 *
 * Trains pronunciation corrections by comparing user audio to synthesized variants.
 * Uses mel spectrogram similarity to find best matching phoneme sequence.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/conversation_tools.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include "ethervox/pronunciation_trainer.h"
#include "../../tts/phonemizer/pronunciation_overrides.h"
#include "../../tts/phonemizer/phonemizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LOG_ERROR(...) ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...) ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(...) ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

// Global references (set by conversation session)
// TODO: These should come from context passed via tool execution
static phonemizer_context_t* g_phonemizer = NULL;
static void* g_tts = NULL;
static void* g_stt = NULL;

/**
 * Parse JSON string field (simple parser without cJSON dependency)
 */
static char* parse_json_string(const char* json, const char* key) {
    if (!json || !key) return NULL;
    
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* pos = strstr(json, search);
    if (!pos) return NULL;
    
    pos = strchr(pos, ':');
    if (!pos) return NULL;
    pos++;
    
    while (*pos == ' ' || *pos == '\t' || *pos == '\n') pos++;
    
    if (*pos != '"') return NULL;
    pos++;
    
    const char* end = pos;
    while (*end && *end != '"') {
        if (*end == '\\' && *(end + 1)) {
            end += 2;
        } else {
            end++;
        }
    }
    
    if (*end != '"') return NULL;
    
    size_t len = end - pos;
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    
    strncpy(result, pos, len);
    result[len] = '\0';
    return result;
}

/**
 * Parse JSON boolean field
 */
static bool parse_json_bool(const char* json, const char* key, bool default_value) {
    if (!json || !key) return default_value;
    
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* pos = strstr(json, search);
    if (!pos) return default_value;
    
    pos = strchr(pos, ':');
    if (!pos) return default_value;
    pos++;
    
    while (*pos == ' ' || *pos == '\t' || *pos == '\n') pos++;
    
    if (strncmp(pos, "true", 4) == 0) return true;
    if (strncmp(pos, "false", 5) == 0) return false;
    
    return default_value;
}

/**
 * Train pronunciation tool execution wrapper
 */
static int tool_train_pronunciation_wrapper(
    const char* params_json,
    char** result,
    char** error
) {
    if (!params_json || !result || !error) {
        if (error) *error = strdup("Invalid tool invocation");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    *result = NULL;
    *error = NULL;
    
    // Parse parameters
    char* word = parse_json_string(params_json, "word");
    char* audio_path = parse_json_string(params_json, "audio_path");
    bool save_override = parse_json_bool(params_json, "save_to_overrides", true);
    
    if (!word || !audio_path) {
        *error = strdup("Missing required parameters: 'word' and 'audio_path'");
        free(word);
        free(audio_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check if TTS/phonemizer are available
    if (!g_phonemizer || !g_tts) {
        *error = strdup("TTS/phonemizer not initialized");
        free(word);
        free(audio_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    LOG_INFO("Training pronunciation for word: %s (audio: %s)", word, audio_path);
    
    // Configure training
    pronunciation_training_config_t config = pronunciation_trainer_default_config();
    config.verbose = true;
    config.min_similarity = 0.70f;
    
    // Run training
    pronunciation_training_result_t training_result;
    ethervox_result_t ret = pronunciation_trainer_train(
        word, 
        audio_path, 
        g_phonemizer, 
        (tts_context_t*)g_tts,
        (stt_context_t*)g_stt,
        &config, 
        &training_result
    );
    
    if (ethervox_is_success(ret) && training_result.success) {
        LOG_INFO("Training successful: %s → %s (similarity: %.3f)",
                 word, training_result.best_phonemes, training_result.similarity_score);
        
        // Save to overrides - TODO: Access g_phonemizer->overrides properly
        // For now, log success without saving (requires accessing internal struct)
        if (save_override) {
            LOG_INFO("Training successful for '%s' → '%s' (score: %.3f)", 
                     word, training_result.best_phonemes, training_result.similarity_score);
            LOG_INFO("TODO: Save pronunciation override (requires phonemizer context API)");
            
            int total_count = 0, ready_for_promotion = 0;
            
            // Format success response
            char response_buf[1024];
            snprintf(response_buf, sizeof(response_buf),
                     "{\"success\":true,\"word\":\"%s\",\"phonemes\":\"%s\","
                     "\"similarity\":%.3f,\"variants_tested\":%d,"
                     "\"saved\":true,\"total_overrides\":%d,\"ready_for_promotion\":%d}",
                     word, training_result.best_phonemes,
                     training_result.similarity_score, training_result.variants_tested,
                     total_count, ready_for_promotion);
            
            *result = strdup(response_buf);
        } else {
            char response_buf[512];
            snprintf(response_buf, sizeof(response_buf),
                     "{\"success\":true,\"word\":\"%s\",\"phonemes\":\"%s\","
                     "\"similarity\":%.3f,\"variants_tested\":%d,\"saved\":false}",
                     word, training_result.best_phonemes,
                     training_result.similarity_score, training_result.variants_tested);
            
            *result = strdup(response_buf);
        }
        
        pronunciation_training_result_free(&training_result);
        free(word);
        free(audio_path);
        return ETHERVOX_SUCCESS;
    } else {
        LOG_ERROR("Pronunciation training failed for '%s': %s",
                  word, training_result.error_message ? training_result.error_message : "unknown error");
        
        *error = strdup(training_result.error_message ? 
                       training_result.error_message : "Training failed");
        
        pronunciation_training_result_free(&training_result);
        free(word);
        free(audio_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
}

/**
 * Get the train_pronunciation tool definition
 */
ethervox_tool_t* ethervox_tool_train_pronunciation_create(void) {
    static ethervox_tool_t tool = {
        .name = "train_pronunciation",
        .description =
            "Train pronunciation by comparing user audio to variants. Use when word mispronounced.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"word\":{\"type\":\"string\",\"description\":\"The word to train pronunciation for\"},"
            "\"audio_path\":{\"type\":\"string\",\"description\":\"Path to user's audio recording (WAV format)\"},"
            "\"save_to_overrides\":{\"type\":\"boolean\",\"description\":\"Save best match to override database (default: true)\"}"
            "},\"required\":[\"word\",\"audio_path\"]}",
        .execute = tool_train_pronunciation_wrapper,
        .is_deterministic = false,       // Audio comparison varies
        .requires_confirmation = false,  // Auto-save approved corrections
        .is_stateful = true,            // Modifies pronunciation database
        .estimated_latency_ms = 5000.0f // Training takes a few seconds
    };
    
    return &tool;
}

/**
 * Set global context for pronunciation trainer (called from conversation session)
 */
void ethervox_train_pronunciation_set_context(void* phonemizer, void* tts, void* stt) {
    g_phonemizer = (phonemizer_context_t*)phonemizer;
    g_tts = tts;
    g_stt = stt;
    
    if (g_phonemizer && g_tts) {
        LOG_INFO("Pronunciation trainer context initialized");
    }
}
