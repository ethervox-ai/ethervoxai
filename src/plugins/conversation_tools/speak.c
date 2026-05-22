/**
#include "ethervox/error.h"
 * @file speak.c
 * @brief LLM tool for text-to-speech output with conversational control
 *
 * Enables the LLM to speak responses aloud via TTS, with control over:
 * - Turn-taking (wait_for_response: auto-open mic after speaking)
 * - Interruption (allow_interrupt: user can interrupt by speaking)
 * - Voice style (future: enthusiastic, calm, professional)
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/conversation_tools.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LOG_ERROR(...) ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...) ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(...) ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...) ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)

// Global callbacks (set by conversation session)
static ethervox_conversation_callbacks_t* g_callbacks = NULL;

/**
 * Parse JSON boolean field (handles true/false as strings or booleans)
 */
static bool parse_json_bool(const char* json, const char* key, bool default_value) {
    if (!json || !key) return default_value;
    
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* pos = strstr(json, search);
    if (!pos) return default_value;
    
    // Skip past key and find colon
    pos = strchr(pos, ':');
    if (!pos) return default_value;
    pos++;
    
    // Skip whitespace
    while (*pos == ' ' || *pos == '\t' || *pos == '\n') pos++;
    
    // Check for true/false
    if (strncmp(pos, "true", 4) == 0) return true;
    if (strncmp(pos, "false", 5) == 0) return false;
    
    // Check for quoted "true"/"false"
    if (*pos == '"') {
        pos++;
        if (strncmp(pos, "true", 4) == 0) return true;
        if (strncmp(pos, "false", 5) == 0) return false;
    }
    
    return default_value;
}

/**
 * Parse JSON string field
 */
static char* parse_json_string(const char* json, const char* key) {
    if (!json || !key) return NULL;
    
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* pos = strstr(json, search);
    if (!pos) return NULL;
    
    // Find opening quote of value
    pos = strchr(pos, ':');
    if (!pos) return NULL;
    pos++;
    
    // Skip whitespace
    while (*pos == ' ' || *pos == '\t' || *pos == '\n') pos++;
    
    if (*pos != '"') return NULL;
    pos++; // Skip opening quote
    
    // Find closing quote (handle escaped quotes)
    const char* end = pos;
    while (*end && *end != '"') {
        if (*end == '\\' && *(end + 1)) {
            end += 2; // Skip escaped character
        } else {
            end++;
        }
    }
    
    if (*end != '"') return NULL;
    
    // Extract string
    size_t len = end - pos;
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    
    strncpy(result, pos, len);
    result[len] = '\0';
    
    // Unescape common sequences
    // TODO: More robust unescaping if needed
    char* src = result;
    char* dst = result;
    while (*src) {
        if (*src == '\\' && *(src + 1)) {
            src++;
            switch (*src) {
                case 'n': *dst++ = '\n'; break;
                case 't': *dst++ = '\t'; break;
                case 'r': *dst++ = '\r'; break;
                case '"': *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                default: *dst++ = *src; break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    
    return result;
}

/**
 * Tool wrapper: speak
 * 
 * JSON Parameters:
 * {
 *   "text": "Text to speak" (required),
 *   "language": "en|de|es|zh" (optional, default: auto-detect from text),
 *   "wait_for_response": true|false (optional, default: false),
 *   "allow_interrupt": true|false (optional, default: true),
 *   "voice_style": "normal"|"enthusiastic"|"calm" (optional, default: "normal")
 * }
 */
static int tool_speak_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    if (!args_json || !result || !error) {
        if (error) *error = strdup("Invalid parameters to speak tool");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check if callbacks are set
    if (!g_callbacks || !g_callbacks->on_speak) {
        // Not an error - just means we're in CLI mode, not voice mode
        LOG_DEBUG("speak tool called but no TTS callback set (CLI mode?)");
        *result = strdup("{\"success\":true,\"mode\":\"text_only\",\"message\":\"TTS not available, printed to console\"}");
        return ETHERVOX_SUCCESS;
    }
    
    // Parse text parameter (required)
    char* text = parse_json_string(args_json, "text");
    if (!text || strlen(text) == 0) {
        if (text) free(text);
        *error = strdup("Missing or empty 'text' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Parse optional parameters
    bool wait_for_response = parse_json_bool(args_json, "wait_for_response", false);
    bool allow_interrupt = parse_json_bool(args_json, "allow_interrupt", true);
    
    // Parse language parameter (optional, NULL means auto-detect)
    char* language = parse_json_string(args_json, "language");
    if (language && strlen(language) == 0) {
        free(language);
        language = NULL;  // Empty string → auto-detect
    }
    
    // Validate language code if provided
    if (language) {
        if (strcmp(language, "en") != 0 && strcmp(language, "de") != 0 &&
            strcmp(language, "es") != 0 && strcmp(language, "zh") != 0) {
            // Return error so Governor can retry with valid code
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg),
                    "Invalid language code '%s'. Supported languages: en (English), de (German), es (Spanish), zh (Chinese). "
                    "Omit language parameter for auto-detection.", language);
            free(language);
            free(text);
            *error = strdup(error_msg);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    // Parse emotion parameter (maps to Piper speaker_id for emotional models)
    char* emotion = parse_json_string(args_json, "emotion");
    if (!emotion) {
        emotion = strdup("neutral");
    }
    
    // Map emotion to speaker_id for LibriTTS-R emotional models
    // These are example mappings - actual IDs should be calibrated per model
    int speaker_id = 0;  // Default: neutral
    if (strcmp(emotion, "happy") == 0 || strcmp(emotion, "enthusiastic") == 0) {
        speaker_id = 45;  // Brighter, energetic speaker
    } else if (strcmp(emotion, "sad") == 0 || strcmp(emotion, "somber") == 0) {
        speaker_id = 78;  // Lower, slower speaker
    } else if (strcmp(emotion, "calm") == 0 || strcmp(emotion, "soothing") == 0) {
        speaker_id = 23;  // Gentle, measured speaker
    } else if (strcmp(emotion, "excited") == 0 || strcmp(emotion, "energetic") == 0) {
        speaker_id = 101; // Fast, dynamic speaker
    } else if (strcmp(emotion, "professional") == 0 || strcmp(emotion, "formal") == 0) {
        speaker_id = 12;  // Clear, authoritative speaker
    } else if (strcmp(emotion, "friendly") == 0 || strcmp(emotion, "warm") == 0) {
        speaker_id = 67;  // Approachable, conversational speaker
    } else if (strcmp(emotion, "serious") == 0 || strcmp(emotion, "grave") == 0) {
        speaker_id = 34;  // Deep, deliberate speaker
    }
    // Note: speaker_id mapping is currently not passed to TTS backend
    // TODO: Add speaker_id parameter to on_speak callback signature
    
    LOG_INFO("speak tool: text='%s', language='%s', emotion='%s' (speaker_id=%d), wait_for_response=%d, allow_interrupt=%d",
             text, language ? language : "auto", emotion, speaker_id, wait_for_response, allow_interrupt);
    
    free(emotion);
    
    // Call the callback (typically implemented in voice_conversation.c)
    int ret = g_callbacks->on_speak(text, language, wait_for_response, allow_interrupt, g_callbacks->user_data);
    
    free(text);
    if (language) free(language);
    
    if (ret != 0) {
        *error = strdup("TTS playback failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Format success response
    char response_buf[256];
    snprintf(response_buf, sizeof(response_buf),
             "{\"success\":true,\"wait_for_response\":%s,\"allow_interrupt\":%s}",
             wait_for_response ? "true" : "false",
             allow_interrupt ? "true" : "false");
    
    *result = strdup(response_buf);
    return ETHERVOX_SUCCESS;
}

/**
 * Get the speak tool definition
 */
ethervox_tool_t* ethervox_tool_speak_create(void) {
    static ethervox_tool_t tool = {
        .name = "speak",
        .description = 
            "TTS audio output with language and emotion control. Use for voice responses. "
            "CRITICAL: Call this tool sentence-by-sentence. Split responses into individual sentences for natural speech flow. "
            "Never send multiple sentences in one call - break them up for better prosody and interruption support. "
            "ALWAYS specify language parameter (en/de/es/zh) to match the user's language - this is critical for proper voice selection. "
            "Languages: en (English), de (German), es (Spanish), zh (Chinese). "
            "Emotions: neutral, happy, sad, calm, excited. "
            "wait_for_response=true auto-opens mic after speaking. "
            "allow_interrupt=true lets user interrupt. Degrades to text in CLI mode.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"text\":{\"type\":\"string\",\"description\":\"The text to speak aloud\"},"
            "\"language\":{\"type\":\"string\",\"enum\":[\"en\",\"de\",\"es\",\"zh\"],\"description\":\"Language code for TTS voice (en=English, de=German, es=Spanish, zh=Chinese). If omitted, auto-detects from text.\"},"
            "\"emotion\":{\"type\":\"string\",\"enum\":[\"neutral\",\"happy\",\"sad\",\"calm\",\"excited\",\"professional\",\"friendly\",\"serious\",\"enthusiastic\",\"somber\",\"soothing\",\"energetic\",\"formal\",\"warm\",\"grave\"],\"description\":\"Voice emotion/style to use (default: neutral). For system prompts use: neutral, happy, sad, calm, excited.\"},"
            "\"wait_for_response\":{\"type\":\"boolean\",\"description\":\"If true, automatically open microphone after speaking to listen for user's response (turn-taking)\"},"
            "\"allow_interrupt\":{\"type\":\"boolean\",\"description\":\"If true, user can interrupt by speaking during playback (default: true)\"}"
            "},\"required\":[\"text\"]}",
        .execute = tool_speak_wrapper,
        .is_deterministic = false,      // TTS playback state varies
        .requires_confirmation = false,  // No confirmation needed for speaking
        .is_stateful = true,            // Changes conversation state
        .estimated_latency_ms = 500.0f  // TTS synthesis + playback startup
    };
    
    return &tool;
}

void ethervox_conversation_tools_set_callbacks(void* callbacks) {
    g_callbacks = (ethervox_conversation_callbacks_t*)callbacks;
    if (g_callbacks) {
        LOG_INFO("Conversation tool callbacks registered");
    }
}

void* ethervox_conversation_tools_get_callbacks(void) {
    return g_callbacks;
}
