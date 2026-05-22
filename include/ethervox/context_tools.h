/**
 * @file context_tools.h
 * @brief Context management tools for automatic overflow handling
 *
 * Provides LLM-accessible tools for managing context window when it fills up,
 * including summarization, window shifting, and selective pruning.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_CONTEXT_TOOLS_H
#define ETHERVOX_CONTEXT_TOOLS_H

#include "ethervox/governor.h"
#include "ethervox/memory_tools.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Context management action types
 */
typedef enum {
    CONTEXT_ACTION_SUMMARIZE_OLD,    // Summarize old turns, store in memory
    CONTEXT_ACTION_SHIFT_WINDOW,     // Drop oldest turns, no summary
    CONTEXT_ACTION_PRUNE_UNIMPORTANT // Selectively remove low-importance turns
} context_action_t;

/**
 * Context action result
 */
typedef struct {
    int tokens_freed;           // Number of tokens freed
    uint32_t turns_removed;     // Number of conversation turns removed
    uint64_t summary_memory_id; // Memory ID of stored summary (0 if not stored)
    bool success;               // Whether action succeeded
    char error_msg[256];        // Error message if failed
} context_action_result_t;

// ============================================================================
// Context Actions
// ============================================================================

/**
 * Summarize old conversation turns and store in memory
 * 
 * @param governor Governor instance with conversation history
 * @param memory_store Memory store for saving summaries
 * @param keep_last_n_turns Number of recent turns to keep verbatim
 * @param detail_level "brief", "moderate", or "detailed"
 * @param result Output result structure
 * @return 0 on success, negative on error
 */
int context_action_summarize_old(
    ethervox_governor_t* governor,
    ethervox_memory_store_t* memory_store,
    uint32_t keep_last_n_turns,
    const char* detail_level,
    context_action_result_t* result
);

/**
 * Shift context window by dropping oldest turns
 * 
 * @param governor Governor instance
 * @param keep_last_n_turns Number of recent turns to keep
 * @param result Output result structure
 * @return 0 on success, negative on error
 */
int context_action_shift_window(
    ethervox_governor_t* governor,
    uint32_t keep_last_n_turns,
    context_action_result_t* result
);

/**
 * Prune unimportant conversation turns
 * 
 * @param governor Governor instance
 * @param importance_threshold Minimum importance to keep (0.0-1.0)
 * @param result Output result structure
 * @return 0 on success, negative on error
 */
int context_action_prune_unimportant(
    ethervox_governor_t* governor,
    float importance_threshold,
    context_action_result_t* result
);

// ============================================================================
// Tool Registration
// ============================================================================

/**
 * Register context_manage tool with governor
 * 
 * @param registry Tool registry to add to
 * @param memory_store Memory store for summaries (optional, can be NULL)
 * @return 0 on success, negative on error
 */
int register_context_manage_tool(
    ethervox_tool_registry_t* registry,
    ethervox_memory_store_t* memory_store
);

/**
 * Set governor reference for context management
 * Must be called after registering the tool and before use
 * 
 * @param governor Governor instance
 */
void context_tools_set_governor(ethervox_governor_t* governor);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_CONTEXT_TOOLS_H
