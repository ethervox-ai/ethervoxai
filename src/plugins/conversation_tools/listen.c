/**
#include "ethervox/error.h"
 * @file listen.c
 * @brief LLM tool for capturing user speech input with timeout
 *
 * Enables the LLM to explicitly request user input via microphone.
 * Useful for clarification questions or multi-turn conversations where
 * the LLM needs more information.
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

// External callback getter (defined in speak.c)
extern void* ethervox_conversation_tools_get_callbacks(void);

/**
 * Parse JSON integer field
 */
static int parse_json_int(const char* json, const char* key, int default_value) {
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
    
    // Parse integer
    int value = atoi(pos);
    return value > 0 ? value : default_value;
}

/**
 * Parse JSON string field (simple version for prompt_hint)
 */
static char* parse_json_string_simple(const char* json, const char* key) {
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
    
    // Find closing quote
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
    
    return result;
}

/**
 * Tool wrapper: listen
 * 
 * JSON Parameters:
 * {
 *   "timeout_ms": 5000 (optional, default: 5000ms),
 *   "prompt_hint": "waiting for color choice..." (optional)
 * }
 * 
 * Returns:
 * {
 *   "success": true,
 *   "user_input": "transcribed text or null if timeout",
 *   "timeout_reached": false
 * }
 */
static int tool_listen_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    if (!args_json || !result || !error) {
        if (error) *error = strdup("Invalid parameters to listen tool");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Get callbacks
    ethervox_conversation_callbacks_t* callbacks = 
        (ethervox_conversation_callbacks_t*)ethervox_conversation_tools_get_callbacks();
    
    if (!callbacks || !callbacks->on_listen) {
        // Not in voice mode
        LOG_DEBUG("listen tool called but no microphone callback set (CLI mode?)");
        *result = strdup("{\"success\":false,\"error\":\"Microphone not available in CLI mode\"}");
        return ETHERVOX_SUCCESS; // Not a failure, just not applicable
    }
    
    // Parse optional parameters
    int timeout_ms = parse_json_int(args_json, "timeout_ms", 5000);
    char* prompt_hint = parse_json_string_simple(args_json, "prompt_hint");
    
    // Clamp timeout to reasonable range
    if (timeout_ms < 1000) timeout_ms = 1000;
    if (timeout_ms > 30000) timeout_ms = 30000;
    
    LOG_INFO("listen tool: timeout=%dms, prompt_hint='%s'",
             timeout_ms, prompt_hint ? prompt_hint : "(none)");
    
    // Call the callback (typically implemented in voice_conversation.c)
    char* user_input = NULL;
    int ret = callbacks->on_listen(&user_input, timeout_ms, prompt_hint, callbacks->user_data);
    
    if (prompt_hint) free(prompt_hint);
    
    if (ret != 0) {
        *error = strdup("Microphone capture failed");
        if (user_input) free(user_input);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Format response
    char response_buf[8192]; // Large buffer for transcribed text
    if (user_input && strlen(user_input) > 0) {
        // Escape quotes in user input
        char* escaped_input = (char*)malloc(strlen(user_input) * 2 + 1);
        if (!escaped_input) {
            *error = strdup("Memory allocation failed");
            free(user_input);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
        
        char* src = user_input;
        char* dst = escaped_input;
        while (*src) {
            if (*src == '"' || *src == '\\') {
                *dst++ = '\\';
            }
            *dst++ = *src++;
        }
        *dst = '\0';
        
        snprintf(response_buf, sizeof(response_buf),
                 "{\"success\":true,\"user_input\":\"%s\",\"timeout_reached\":false}",
                 escaped_input);
        
        free(escaped_input);
        free(user_input);
    } else {
        // Timeout reached, no input
        snprintf(response_buf, sizeof(response_buf),
                 "{\"success\":true,\"user_input\":null,\"timeout_reached\":true}");
        if (user_input) free(user_input);
    }
    
    *result = strdup(response_buf);
    return ETHERVOX_SUCCESS;
}

/**
 * Get the listen tool definition
 */
ethervox_tool_t* ethervox_tool_listen_create(void) {
    static ethervox_tool_t tool = {
        .name = "listen",
        .description = 
            "Capture speech input (English/Chinese/German). Use for clarifications/multi-turn. "
            "Waits timeout_ms for user speech. Returns transcript or null on timeout. "
            "Not available in CLI mode.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"timeout_ms\":{\"type\":\"integer\",\"description\":\"Maximum time to wait for user input in milliseconds (default: 5000, range: 1000-30000)\"},"
            "\"prompt_hint\":{\"type\":\"string\",\"description\":\"Optional hint about what you're waiting for (e.g., 'waiting for your answer...')\"}"
            "},\"required\":[]}",
        .execute = tool_listen_wrapper,
        .is_deterministic = false,      // User input varies
        .requires_confirmation = false,  // No confirmation needed
        .is_stateful = true,            // Changes conversation state (turn management)
        .estimated_latency_ms = 2000.0f // Depends on user response time
    };
    
    return &tool;
}
