/**
 * @file conversation_summary.h
 * @brief Simple LLM-based conversation summarization (MVP)
 * 
 * Provides manual conversation summarization with KV cache persistence.
 * MVP: Simple, blocking, always regenerate from scratch.
 * 
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_CONVERSATION_SUMMARY_H
#define ETHERVOX_CONVERSATION_SUMMARY_H

#include "ethervox/error.h"
#include "ethervox/governor.h"
#include "ethervox/memory_tools.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate LLM-based conversation summary (MVP: Simple & Blocking)
 * 
 * Creates a summary of recent conversation turns, excluding action items.
 * This is a synchronous/blocking operation (3-5 seconds typical).
 * 
 * @param governor Governor instance
 * @param memory_store Memory store containing conversation history
 * @param summary_out Output buffer for summary text
 * @param summary_size Size of output buffer
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_generate_conversation_summary(
    ethervox_governor_t* governor,
    ethervox_memory_store_t* memory_store,
    char* summary_out,
    size_t summary_size
);

/**
 * Save conversation summary to KV cache file
 * MVP: Simple save, no metadata
 * 
 * @param governor Governor instance
 * @param summary_text Summary text to save
 * @param cache_dir Cache directory path
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_save_conversation_summary(
    struct ethervox_governor* governor,
    const char* summary_text,
    const char* cache_dir
);

/**
 * Load conversation summary from KV cache file
 * MVP: Simple load, no metadata validation
 * 
 * @param governor Governor instance
 * @param cache_dir Cache directory path
 * @return ETHERVOX_SUCCESS or error code (not finding file is OK)
 */
ethervox_result_t ethervox_load_conversation_summary(
    struct ethervox_governor* governor,
    const char* cache_dir
);

/**
 * Get the last generated conversation summary text for UI display
 * @return Summary text or message if no summary exists
 */
const char* ethervox_get_last_summary(void);

/**
 * Restore conversation context from memory after loading summary
 * 
 * Loads recent conversations (after summary checkpoint), active tasks,
 * and important context items into KV cache for continuity.
 * 
 * @param governor Governor instance
 * @param memory_store Memory store containing conversation history
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_restore_context_from_memory(
    struct ethervox_governor* governor,
    ethervox_memory_store_t* memory_store
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_CONVERSATION_SUMMARY_H
