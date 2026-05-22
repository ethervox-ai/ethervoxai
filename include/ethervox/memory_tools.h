/**
 * @file memory_tools.h
 * @brief Conversation memory tools for Governor LLM context management
 *
 * Provides efficient storage, retrieval, and export of conversation history
 * allowing the LLM to maintain long-term context across sessions.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_MEMORY_TOOLS_H
#define ETHERVOX_MEMORY_TOOLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ETHERVOX_MEMORY_MAX_TEXT_LEN 8192
#define ETHERVOX_MEMORY_MAX_TAGS 16
#define ETHERVOX_MEMORY_TAG_LEN 64
#define ETHERVOX_MEMORY_MAX_ENTRIES 10000
#define ETHERVOX_MEMORY_INDEX_BUCKETS 256
#define ETHERVOX_MEMORY_HASH_BUCKETS 128
#define ETHERVOX_MEMORY_MAX_BUCKET_SIZE 64

/**
 * Hash table bucket for fast lookups
 */
typedef struct {
    uint32_t entry_indices[ETHERVOX_MEMORY_MAX_BUCKET_SIZE];  // Indices into entries array
    uint32_t count;                                           // Number of entries in bucket
} ethervox_memory_hash_bucket_t;

/**
 * Memory entry representing a single conversational fact/event
 */
typedef struct {
    uint64_t memory_id;                      // Unique identifier
    uint64_t turn_id;                        // Conversation turn number
    time_t timestamp;                        // When stored
    
    char text[ETHERVOX_MEMORY_MAX_TEXT_LEN]; // Content
    char tags[ETHERVOX_MEMORY_MAX_TAGS][ETHERVOX_MEMORY_TAG_LEN]; // Categories
    uint32_t tag_count;                      // Number of tags
    
    float importance;                        // 0.0-1.0 relevance score
    bool is_user_message;                    // vs assistant message
    
    uint32_t tools_called_count;             // Tools invoked in this turn
    char tools_called[16][64];               // Tool names
} ethervox_memory_entry_t;

/**
 * Search result with relevance score
 */
typedef struct {
    ethervox_memory_entry_t entry;
    float relevance;                         // Similarity/match score
} ethervox_memory_search_result_t;

/**
 * Memory index for fast tag-based lookup
 */
typedef struct {
    char tag[ETHERVOX_MEMORY_TAG_LEN];
    uint64_t memory_ids[1024];               // IDs with this tag
    uint32_t count;
} ethervox_memory_tag_index_t;

/**
 * Main memory store structure
 */
typedef struct {
    char session_id[64];                     // UUID for this session
    time_t session_started;
    uint64_t current_turn_id;
    
    ethervox_memory_entry_t* entries;        // Dynamic array of memories
    uint32_t entry_count;
    uint32_t entry_capacity;
    
    // Tag-based index for fast retrieval (legacy)
    ethervox_memory_tag_index_t* tag_index;
    uint32_t tag_index_count;
    uint32_t tag_index_capacity;
    
    // Hash table indices for O(1) lookups
    ethervox_memory_hash_bucket_t* tag_hash;      // Tag -> entry indices
    ethervox_memory_hash_bucket_t* id_hash;       // ID -> entry index
    
    // Statistics
    uint64_t total_memories_stored;
    uint64_t total_searches;
    uint64_t total_exports;
    
    // File persistence
    char storage_filepath[512];              // Current session file
    FILE* append_log;                        // Open file for appends
    
    bool is_initialized;
} ethervox_memory_store_t;

/**
 * Initialize memory store
 * 
 * @param store Memory store to initialize
 * @param session_id Optional session ID (NULL = generate new)
 * @param storage_dir Directory for persistent storage (NULL = memory-only mode)
 *                    Platform-specific paths:
 *                    - Desktop: "/tmp" or "~/.ethervoxai/memory"
 *                    - Android: Use app's internal storage (context.getFilesDir())
 *                    - ESP32: "/spiffs/memory" or NULL to disable persistence
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_init(
    ethervox_memory_store_t* store,
    const char* session_id,
    const char* storage_dir
);

/**
 * Cleanup and close memory store
 * 
 * @param store Memory store to cleanup
 */
void ethervox_memory_cleanup(ethervox_memory_store_t* store);

/**
 * TOOL: memory_store - Save a fact/event to memory
 * 
 * @param store Memory store
 * @param text Content to remember
 * @param tags Array of category tags
 * @param tag_count Number of tags
 * @param importance Relevance score 0.0-1.0
 * @param is_user_message True if from user, false if from assistant
 * @param memory_id_out Output: assigned memory ID
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_store_add(
    ethervox_memory_store_t* store,
    const char* text,
    const char* tags[],
    uint32_t tag_count,
    float importance,
    bool is_user_message,
    uint64_t* memory_id_out
);

/**
 * Store a new conversational memory with tool tracking
 * 
 * @param store Memory store
 * @param text Content to store
 * @param tags Array of category tags
 * @param tag_count Number of tags
 * @param importance Relevance score 0.0-1.0
 * @param is_user_message True if from user, false if from assistant
 * @param tools_called Array of tool names that were invoked (can be NULL)
 * @param tools_count Number of tools called (max 16)
 * @param memory_id_out Output: assigned memory ID
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_store_add_with_tools(
    ethervox_memory_store_t* store,
    const char* text,
    const char* tags[],
    uint32_t tag_count,
    float importance,
    bool is_user_message,
    const char* tools_called[],
    uint32_t tools_count,
    uint64_t* memory_id_out
);

/**
 * TOOL: memory_search - Query memories by tags and text similarity
 * 
 * @param store Memory store
 * @param query Search query text (can be NULL for tag-only search)
 * @param tag_filter Array of required tags (can be NULL)
 * @param tag_filter_count Number of required tags
 * @param limit Maximum results to return
 * @param results Output: array of search results (caller must free)
 * @param result_count Output: number of results
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_search(
    ethervox_memory_store_t* store,
    const char* query,
    const char* tag_filter[],
    uint32_t tag_filter_count,
    uint32_t limit,
    ethervox_memory_search_result_t** results,
    uint32_t* result_count
);

/**
 * Update tags for an existing memory entry
 * 
 * @param store Memory store
 * @param memory_id ID of memory to update
 * @param tags Array of new tags (replaces all existing tags)
 * @param tag_count Number of tags
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_update_tags(
    ethervox_memory_store_t* store,
    uint64_t memory_id,
    const char* tags[],
    uint32_t tag_count
);

/**
 * Update text content for an existing memory entry
 * 
 * @param store Memory store
 * @param memory_id ID of memory to update
 * @param new_text New text content
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_update_text(
    ethervox_memory_store_t* store,
    uint64_t memory_id,
    const char* new_text
);

/**
 * TOOL: memory_summarize - Generate summary of recent conversation
 * 
 * @param store Memory store
 * @param window_size Number of recent turns to summarize
 * @param focus_topic Optional: focus on specific topic (can be NULL)
 * @param summary_out Output: summary text (caller must free)
 * @param key_points_out Output: array of key points (caller must free)
 * @param key_points_count Output: number of key points
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_summarize(
    ethervox_memory_store_t* store,
    uint32_t window_size,
    const char* focus_topic,
    char** summary_out,
    char*** key_points_out,
    uint32_t* key_points_count
);

/**
 * TOOL: memory_export - Save session to file
 * 
 * @param store Memory store
 * @param filepath Output file path
 * @param format "json" or "markdown"
 * @param bytes_written Output: number of bytes written
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_export(
    ethervox_memory_store_t* store,
    const char* filepath,
    const char* format,
    uint64_t* bytes_written
);

/**
 * Import memories from a file (JSONL format)
 * Handles all operations: ADD, DELETE, UPDATE, UPDATE_TEXT
 * Automatically appends "imported" tag to all imported memories
 * Preserves original memory IDs, timestamps, and all tags
 * 
 * @param store Memory store
 * @param filepath Input file path
 * @param turns_loaded Output: number of turns loaded
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_import(
    ethervox_memory_store_t* store,
    const char* filepath,
    uint32_t* turns_loaded
);

/**
 * Platform-agnostic function to load the most recent previous session
 * Automatically finds and imports the latest .jsonl file from the storage directory
 * (excluding the current session file)
 * 
 * This ensures memory continuity across app restarts - important memories like
 * user names, preferences, and high-importance items are preserved.
 * 
 * @param store Memory store (must be initialized with storage directory)
 * @param turns_loaded Output: number of turns loaded (can be NULL)
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_load_previous_session(
    ethervox_memory_store_t* store,
    uint32_t* turns_loaded
);

/**
 * Archive all previous session files
 * 
 * Moves all .jsonl session files (except current session) to an archive subdirectory.
 * Useful for cleanup while preserving history. The archive directory is created
 * at <storage_dir>/archive/ if it doesn't exist.
 * 
 * Platform support:
 * - Desktop (macOS/Linux/Windows): Full support
 * - Android: Supported in app internal storage
 * - ESP32: May fail if filesystem doesn't support subdirectories
 * 
 * @param store Memory store (must be initialized with storage directory)
 * @param files_archived Output: number of files archived (can be NULL)
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_archive_sessions(
    ethervox_memory_store_t* store,
    uint32_t* files_archived
);

/**
 * TOOL: memory_forget - Prune old/irrelevant memories
 * 
 * @param store Memory store
 * @param older_than_seconds Remove entries older than this (0 = ignore)
 * @param importance_threshold Remove entries below this importance (0.0 = ignore)
 * @param items_pruned Output: number of items removed
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_forget(
    ethervox_memory_store_t* store,
    uint64_t older_than_seconds,
    float importance_threshold,
    uint32_t* items_pruned
);

/**
 * TOOL: memory_delete - Delete specific memories by ID
 * 
 * @param store Memory store
 * @param memory_ids Array of memory IDs to delete
 * @param id_count Number of IDs in the array
 * @param items_deleted Output: number of items deleted
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_delete_by_ids(
    ethervox_memory_store_t* store,
    const uint64_t* memory_ids,
    uint32_t id_count,
    uint32_t* items_deleted
);

/**
 * Get memory entry by ID
 * 
 * @param store Memory store
 * @param memory_id Memory ID to retrieve
 * @param entry_out Output: memory entry (not owned by caller)
 * @return ETHERVOX_SUCCESS on success, error code if not found
 */
ethervox_result_t ethervox_memory_get_by_id(
    ethervox_memory_store_t* store,
    uint64_t memory_id,
    const ethervox_memory_entry_t** entry_out
);

/**
 * TOOL: memory_store_correction - Store user corrections as high-priority learning
 * 
 * Corrections are tagged with ["correction", "high_priority"] and importance=0.99
 * These are prioritized in adaptive system prompts to "teach" the model.
 * 
 * @param store Memory store
 * @param correction_text What the user corrected
 * @param context Optional context about what was wrong (can be NULL)
 * @param memory_id_out Output: assigned memory ID
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_store_correction(
    ethervox_memory_store_t* store,
    const char* correction_text,
    const char* context,
    uint64_t* memory_id_out
);

/**
 * TOOL: memory_store_pattern - Store successful interaction pattern
 * 
 * Patterns are tagged with ["pattern", "success"] and importance=0.90
 * These are used to reinforce successful behaviors in adaptive system prompts.
 * 
 * @param store Memory store
 * @param pattern_description What worked well
 * @param memory_id_out Output: assigned memory ID
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_store_pattern(
    ethervox_memory_store_t* store,
    const char* pattern_description,
    uint64_t* memory_id_out
);

/**
 * Get user corrections for adaptive system prompt
 * 
 * @param store Memory store
 * @param results Output: array of correction results (caller must free)
 * @param result_count Output: number of results
 * @param limit Maximum results to return
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_get_corrections(
    ethervox_memory_store_t* store,
    ethervox_memory_search_result_t** results,
    uint32_t* result_count,
    uint32_t limit
);

/**
 * Get successful patterns for adaptive system prompt
 * 
 * @param store Memory store
 * @param results Output: array of pattern results (caller must free)
 * @param result_count Output: number of results
 * @param limit Maximum results to return
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_get_patterns(
    ethervox_memory_store_t* store,
    ethervox_memory_search_result_t** results,
    uint32_t* result_count,
    uint32_t limit
);

/**
 * Register memory tools with Governor tool registry
 * 
 * @param registry Governor tool registry
 * @param store Memory store instance to use
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_memory_tools_register(
    void* registry,
    ethervox_memory_store_t* store
);

/**
 * Set privacy mode for memory logging (secret mode)
 * When enabled, memory_store and related tools will skip actual storage
 * but return success to avoid disrupting the LLM flow.
 * 
 * @param disable_logging true to enable secret mode (no logging), false for normal operation
 */
void ethervox_memory_set_privacy_mode(bool disable_logging);

/**
 * Get current privacy mode state
 * 
 * @return true if privacy mode is enabled (secret mode), false if normal operation
 */
bool ethervox_memory_get_privacy_mode(void);


/**
 * Get current privacy mode state
 * 
 * @return true if privacy mode is enabled (secret mode), false if normal operation
 */
bool ethervox_memory_get_privacy_mode(void);


#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_MEMORY_TOOLS_H
