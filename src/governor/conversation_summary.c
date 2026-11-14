/**
 * @file conversation_summary.c
 * @brief Simple LLM-based conversation summarization (MVP)
 * 
 * MVP: Manual trigger, blocking operation, always regenerate from scratch
 * 
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/conversation_summary.h"
#include "ethervox/config.h"
#include "ethervox/error.h"
#include "ethervox/memory_tools.h"
#include "ethervox/kv_cache_persistence.h"
#include "ethervox/paths.h"
#include <llama.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Store last generated summary for UI display
static char g_last_summary[4096] = {0};
static bool g_has_summary = false;

static const char* get_model_basename(const char* model_path);

/**
 * Generate a conversation summary and move it into the live KV cache (MVP: blocking).
 *
 * Delegates to ethervox_governor_summarize_conversation_to_cache(), which builds the
 * summary from the governor's own conversation_history (not memory.json - that would
 * miss anything not yet flushed), clears the conversation region of the KV cache, and
 * decodes the summary back in so the model is immediately warm. It also persists the
 * summary to memory_store here for us, tagged "context_summary".
 */
ethervox_result_t ethervox_generate_conversation_summary(
    struct ethervox_governor* governor,
    ethervox_memory_store_t* memory_store,
    char* summary_out,
    size_t summary_size
) {
    if (!governor || !summary_out || summary_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    ETHERVOX_LOGI("Starting conversation summarization");

    ethervox_result_t result = ethervox_governor_summarize_conversation_to_cache(
        governor, memory_store, summary_out, summary_size);

    if (ethervox_is_success(result)) {
        if (summary_out[0] == '\0') {
            ETHERVOX_LOGI("No conversation to summarize");
        } else {
            ETHERVOX_LOGI("Summary generated and moved into KV cache: %zu chars", strlen(summary_out));
            strncpy(g_last_summary, summary_out, sizeof(g_last_summary) - 1);
            g_last_summary[sizeof(g_last_summary) - 1] = '\0';
            g_has_summary = true;
        }
    } else {
        ETHERVOX_LOGE("Failed to generate summary");
    }

    return result;
}

/**
 * Save conversation summary to KV cache file
 */
ethervox_result_t ethervox_save_conversation_summary(
    struct ethervox_governor* governor,
    const char* summary_text,
    const char* cache_dir
) {
    if (!governor || !summary_text || !cache_dir) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build paths struct — kv_cache API requires ethervox_paths_t, not raw strings
    ethervox_paths_t paths = {0};
    paths.cache_dir = cache_dir;

    ethervox_result_t result = ethervox_kv_cache_save(&paths, governor);

    if (ethervox_is_success(result)) {
        ETHERVOX_LOGI("Conversation summary saved to cache_dir: %s", cache_dir);
    } else {
        ETHERVOX_LOGE("Failed to save conversation summary");
    }

    return result;
}

/**
 * Load conversation summary from KV cache file
 */
ethervox_result_t ethervox_load_conversation_summary(
    struct ethervox_governor* governor,
    const char* cache_dir
) {
    if (!governor || !cache_dir) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build paths struct — kv_cache API requires ethervox_paths_t, not raw strings
    ethervox_paths_t paths = {0};
    paths.cache_dir = cache_dir;

    if (!ethervox_kv_cache_exists(&paths, governor)) {
        ETHERVOX_LOGI("No conversation summary found (first session)");
        return ETHERVOX_SUCCESS;
    }

    ethervox_result_t result = ethervox_kv_cache_load(&paths, governor);

    if (ethervox_is_success(result)) {
        ETHERVOX_LOGI("Conversation summary loaded from cache_dir: %s", cache_dir);
    } else {
        ETHERVOX_LOGW("Failed to load conversation summary (will generate new)");
    }

    return result;
}

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * Extract model basename from full path
 */
static const char* get_model_basename(const char* model_path) {
    if (!model_path) {
        return "unknown";
    }
    
    const char* basename = strrchr(model_path, '/');
    if (basename) {
        basename++;  // Skip the '/'
    } else {
        basename = model_path;
    }
    
    return basename;
}

/**
 * Get the last generated conversation summary text
 * Returns the summary text for UI display
 */
const char* ethervox_get_last_summary(void) {
    if (!g_has_summary) {
        return "No summary generated yet. Use the 'Summarize Conversation' button to create one.";
    }
    return g_last_summary;
}

/**
 * Prepare context restoration by finding most recent summary
 * 
 * NOTE: Actual restoration happens at generation time in governor.c
 * This function runs at startup to find and store the summary for UI display.
 * 
 * The generation-time restoration (in process_dialogue) will:
 * 1. Detect if KV cache needs restoration (max_pos <= system_prompt_token_count)
 * 2. Load summary from memory (tagged with context_summary) 
 * 3. Load conversation turns from conversation_history
 * 4. Tokenize and add to KV cache right before generation
 * 
 * @param governor Governor instance
 * @param memory_store Memory store (for finding summary)
 * @return ETHERVOX_SUCCESS
 */
ethervox_result_t ethervox_restore_context_from_memory(
    struct ethervox_governor* governor,
    ethervox_memory_store_t* memory_store
) {
    if (!governor || !memory_store || !memory_store->is_initialized) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ETHERVOX_LOGI("Preparing context restoration (actual restore happens on first message)");
    
    // Find the last summary checkpoint for UI display
    uint64_t summary_timestamp = 0;
    int summaries_found = 0;
    
    for (int i = memory_store->entry_count - 1; i >= 0; i--) {
        ethervox_memory_entry_t* entry = &memory_store->entries[i];
        
        // Check for context_summary tag
        for (uint32_t j = 0; j < entry->tag_count; j++) {
            if (strcmp(entry->tags[j], "context_summary") == 0) {
                summary_timestamp = entry->timestamp;
                summaries_found++;
                
                // Store the summary text for UI display
                strncpy(g_last_summary, entry->text, sizeof(g_last_summary) - 1);
                g_last_summary[sizeof(g_last_summary) - 1] = '\0';
                g_has_summary = true;
                
                ETHERVOX_LOGI("Found summary checkpoint at timestamp: %llu", summary_timestamp);
                goto done_searching;
            }
        }
    }
    
done_searching:
    
    if (summaries_found > 0) {
        ETHERVOX_LOGI("Context restoration ready: found summary from %llu", summary_timestamp);
    } else {
        ETHERVOX_LOGI("Context restoration ready: no summary found, will restore from full history");
    }
    
    // Context restoration happens automatically in governor.c process_dialogue()
    // when condition (max_pos <= system_prompt_token_count && turn_count > 0) is met.
    // This ensures conversation_history is fully loaded when restoration runs.
    
    return ETHERVOX_SUCCESS;
}
