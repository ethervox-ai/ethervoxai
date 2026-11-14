/**
#include "ethervox/error.h"
 * @file context_manage.c
 * @brief Context management tool registration and wrapper
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/context_tools.h"
#include "ethervox/config.h"
#include "ethervox/tool_catalogue.h"
#include "context_tools_catalogue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// For JSON parsing (simple approach)
#include <ctype.h>

#define CTX_LOG(...) ETHERVOX_LOGI(__VA_ARGS__)
#define CTX_ERROR(...) ETHERVOX_LOGE(__VA_ARGS__)

// Global memory store reference for tool execution
static ethervox_memory_store_t* g_memory_store = NULL;
static ethervox_governor_t* g_governor_ref = NULL;

/**
 * Simple JSON value extractor
 * Extracts value for a given key from JSON string
 */
static char* extract_json_string(const char* json, const char* key) {
    if (!json || !key) return NULL;
    
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* key_pos = strstr(json, search);
    if (!key_pos) return NULL;
    
    // Find the value after the key
    const char* colon = strchr(key_pos, ':');
    if (!colon) return NULL;
    
    // Skip whitespace
    colon++;
    while (*colon && isspace(*colon)) colon++;
    
    // Check if value is a string (starts with ")
    if (*colon == '"') {
        colon++;  // Skip opening quote
        const char* end = strchr(colon, '"');
        if (!end) return NULL;
        
        size_t len = end - colon;
        char* value = malloc(len + 1);
        if (!value) return NULL;
        
        strncpy(value, colon, len);
        value[len] = '\0';
        return value;
    }
    
    return NULL;
}

static int extract_json_int(const char* json, const char* key, int default_value) {
    if (!json || !key) return default_value;
    
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* key_pos = strstr(json, search);
    if (!key_pos) return default_value;
    
    const char* colon = strchr(key_pos, ':');
    if (!colon) return default_value;
    
    colon++;
    while (*colon && isspace(*colon)) colon++;
    
    if (isdigit(*colon) || *colon == '-') {
        return atoi(colon);
    }
    
    return default_value;
}

/**
 * context_manage tool execution function
 */
static int context_manage_execute(
    const char* args_json,
    char** result,
    char** error
) {
    if (!args_json || !result || !error) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    CTX_LOG("[Context] context_manage called with args: %s", args_json);
    
    // Extract parameters
    char* action_str = extract_json_string(args_json, "action");
    if (!action_str) {
        *error = strdup("Missing required parameter: action");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int keep_last_n = extract_json_int(args_json, "keep_last_n_turns", 10);
    char* detail = extract_json_string(args_json, "summary_detail");
    const char* detail_level = detail ? detail : "moderate";
    
    // Validate action
    context_action_t action;
    if (strcmp(action_str, "summarize_old") == 0) {
        action = CONTEXT_ACTION_SUMMARIZE_OLD;
    } else if (strcmp(action_str, "shift_window") == 0) {
        action = CONTEXT_ACTION_SHIFT_WINDOW;
    } else if (strcmp(action_str, "prune_unimportant") == 0) {
        action = CONTEXT_ACTION_PRUNE_UNIMPORTANT;
    } else {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Unknown action: %s", action_str);
        *error = strdup(err_msg);
        free(action_str);
        if (detail) free(detail);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Execute action
    context_action_result_t ctx_result;
    int ret = -1;
    
    switch (action) {
        case CONTEXT_ACTION_SUMMARIZE_OLD:
            ret = context_action_summarize_old(
                g_governor_ref, g_memory_store, keep_last_n, detail_level, &ctx_result);
            break;
            
        case CONTEXT_ACTION_SHIFT_WINDOW:
            ret = context_action_shift_window(
                g_governor_ref, keep_last_n, &ctx_result);
            break;
            
        case CONTEXT_ACTION_PRUNE_UNIMPORTANT:
            ret = context_action_prune_unimportant(
                g_governor_ref, 0.5f, &ctx_result);
            break;
    }
    
    free(action_str);
    if (detail) free(detail);
    
    // Build result JSON
    if (ethervox_is_success(ret) && ctx_result.success) {
        char result_json[512];
        snprintf(result_json, sizeof(result_json),
                 "{\"success\": true, \"tokens_freed\": %d, "
                 "\"turns_removed\": %u, \"summary_stored\": %s, "
                 "\"memory_id\": %llu}",
                 ctx_result.tokens_freed,
                 ctx_result.turns_removed,
                 ctx_result.summary_memory_id > 0 ? "true" : "false",
                 (unsigned long long)ctx_result.summary_memory_id);
        
        *result = strdup(result_json);
        return ETHERVOX_SUCCESS;
    } else {
        char err_msg[512];
        snprintf(err_msg, sizeof(err_msg),
                 "Context management failed: %s",
                 ctx_result.error_msg[0] ? ctx_result.error_msg : "unknown error");
        *error = strdup(err_msg);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
}

/**
 * Register context_manage tool with governor
 */
ethervox_result_t register_context_manage_tool(
    ethervox_tool_registry_t* registry,
    ethervox_memory_store_t* memory_store
) {
    if (!registry) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    // Store global references (needed for tool execution)
    g_memory_store = memory_store;
    
    // Note: We also need the governor reference, but that's circular
    // This will be set externally or we need a different approach
    
    ethervox_tool_t tool = {
        .test_scenario = "Manage the conversation context",
        .execute = context_manage_execute,
        .is_deterministic = false,
        .requires_confirmation = false,
        .is_stateful = true,
        .estimated_latency_ms = 500.0f
    };
    ethervox_tool_catalogue_load(ETHERVOX_CATALOGUE_CONTEXT_TOOLS_JSON, "context_manage",
        ethervox_tool_catalogue_build_profile(), &tool);
    
    return ethervox_tool_registry_add(registry, &tool);
}

/**
 * Set governor reference for context management
 * This should be called after governor init
 */
void context_tools_set_governor(ethervox_governor_t* governor) {
    g_governor_ref = governor;
}
