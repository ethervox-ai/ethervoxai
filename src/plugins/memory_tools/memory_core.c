/**
#include "ethervox/error.h"
 * @file memory_core.c
 * @brief Core memory store implementation with efficient storage and indexing
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

// Generate a simple UUID-like session ID
static void generate_session_id(char* session_id, size_t len) {
    time_t now = time(NULL);
    snprintf(session_id, len, "session_%lu_%d", (unsigned long)now, rand() % 10000);
}

// Ensure directory exists
static int ensure_directory(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) != 0) {
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    return ETHERVOX_SUCCESS;
}

// Ensure storage directory and file handle are ready for writing
// This handles the case where files are deleted during a session
static int ensure_storage_ready(ethervox_memory_store_t* store) {
    if (!store || !store->is_initialized) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // If no storage path configured, nothing to do (memory-only mode)
    if (store->storage_filepath[0] == '\0') {
        return ETHERVOX_SUCCESS;
    }
    
    // If file handle is already open, we're good
    if (store->append_log != NULL) {
        return ETHERVOX_SUCCESS;
    }
    
    // File handle is NULL - need to (re)open it
    // First, ensure the directory exists
    char storage_dir[512];
    strncpy(storage_dir, store->storage_filepath, sizeof(storage_dir) - 1);
    storage_dir[sizeof(storage_dir) - 1] = '\0';
    
    char* last_slash = strrchr(storage_dir, '/');
    if (last_slash) {
        *last_slash = '\0';  // Truncate to get directory path
        
        // Recreate directory if it was deleted
        if (ensure_directory(storage_dir) != 0) {
            ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                        "Failed to create storage directory: %s", storage_dir);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    // (Re)open the file in append mode
    store->append_log = fopen(store->storage_filepath, "a");
    if (!store->append_log) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to open storage file: %s", store->storage_filepath);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Reopened memory storage file: %s", store->storage_filepath);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_memory_init(
    ethervox_memory_store_t* store,
    const char* session_id,
    const char* storage_dir
) {
    if (!store) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    memset(store, 0, sizeof(ethervox_memory_store_t));
    
    // Set or generate session ID
    if (session_id && session_id[0]) {
        snprintf(store->session_id, sizeof(store->session_id), "%s", session_id);
    } else {
        generate_session_id(store->session_id, sizeof(store->session_id));
    }
    
    store->session_started = time(NULL);
    store->current_turn_id = 0;
    
    // Initialize entries array
    store->entry_capacity = 256;  // Start with room for 256 entries
    store->entries = calloc(store->entry_capacity, sizeof(ethervox_memory_entry_t));
    if (!store->entries) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    store->entry_count = 0;
    
    // Initialize tag index
    store->tag_index_capacity = 64;  // Start with 64 tag buckets
    store->tag_index = calloc(store->tag_index_capacity, sizeof(ethervox_memory_tag_index_t));
    if (!store->tag_index) {
        free(store->entries);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    store->tag_index_count = 0;
    
    // Set up storage directory (platform-specific)
    if (storage_dir && storage_dir[0]) {
        if (ensure_directory(storage_dir) != 0) {
            ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, 
                        "Could not create storage directory: %s", storage_dir);
        }
        snprintf(store->storage_filepath, sizeof(store->storage_filepath),
                "%s/%s.jsonl", storage_dir, store->session_id);
    } else {
        // No storage dir = in-memory only mode
        // Caller must provide platform-appropriate path for persistence:
        // - Desktop/macOS: ~/.ethervox/memory (recommended)
        // - Android: context.getFilesDir() + "/memory"
        // - ESP32: /spiffs/memory or disabled
        store->storage_filepath[0] = '\0';
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "No storage directory provided - running in memory-only mode");
    }
    
    // Open append log (only if storage path is set)
    if (store->storage_filepath[0] != '\0') {
        store->append_log = fopen(store->storage_filepath, "a");
        if (!store->append_log) {
            ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                        "Could not open append log: %s", store->storage_filepath);
        }
    } else {
        store->append_log = NULL;  // Memory-only mode
    }
    
    store->is_initialized = true;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Initialized memory store: session=%s, storage=%s",
                store->session_id, store->storage_filepath);
    
    return ETHERVOX_SUCCESS;
}

void ethervox_memory_cleanup(ethervox_memory_store_t* store) {
    if (!store || !store->is_initialized) {
        return;
    }
    
    // Close append log
    if (store->append_log) {
        fclose(store->append_log);
        store->append_log = NULL;
    }
    
    // Free entries
    if (store->entries) {
        free(store->entries);
        store->entries = NULL;
    }
    
    // Free tag index
    if (store->tag_index) {
        free(store->tag_index);
        store->tag_index = NULL;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Cleaned up memory store: %llu memories stored, %llu searches",
                (unsigned long long)store->total_memories_stored, (unsigned long long)store->total_searches);
    
    store->is_initialized = false;
}

// Find or create tag index entry
static ethervox_memory_tag_index_t* get_tag_index(
    ethervox_memory_store_t* store,
    const char* tag
) {
    // Search for existing tag
    for (uint32_t i = 0; i < store->tag_index_count; i++) {
        if (strcmp(store->tag_index[i].tag, tag) == 0) {
            return &store->tag_index[i];
        }
    }
    
    // Grow index if needed
    if (store->tag_index_count >= store->tag_index_capacity) {
        uint32_t new_capacity = store->tag_index_capacity * 2;
        ethervox_memory_tag_index_t* new_index = realloc(
            store->tag_index,
            new_capacity * sizeof(ethervox_memory_tag_index_t)
        );
        if (!new_index) {
            return NULL;
        }
        store->tag_index = new_index;
        store->tag_index_capacity = new_capacity;
    }
    
    // Create new tag index
    ethervox_memory_tag_index_t* idx = &store->tag_index[store->tag_index_count];
    snprintf(idx->tag, sizeof(idx->tag), "%s", tag);
    idx->count = 0;
    store->tag_index_count++;
    
    return idx;
}

// Internal function for adding memories with explicit IDs (used by import)
ethervox_result_t memory_store_add_internal(
    ethervox_memory_store_t* store,
    const char* text,
    const char* tags[],
    uint32_t tag_count,
    float importance,
    bool is_user_message,
    uint64_t memory_id,
    uint64_t turn_id,
    time_t timestamp,
    uint64_t* memory_id_out
) {
    if (!store || !store->is_initialized || !text) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Validate inputs
    if (tag_count > ETHERVOX_MEMORY_MAX_TAGS) {
        tag_count = ETHERVOX_MEMORY_MAX_TAGS;
    }
    
    if (importance < 0.0f) importance = 0.0f;
    if (importance > 1.0f) importance = 1.0f;
    
    // Grow entries array if needed
    if (store->entry_count >= store->entry_capacity) {
        uint32_t new_capacity = store->entry_capacity * 2;
        if (new_capacity > ETHERVOX_MEMORY_MAX_ENTRIES) {
            new_capacity = ETHERVOX_MEMORY_MAX_ENTRIES;
        }
        
        ethervox_memory_entry_t* new_entries = realloc(
            store->entries,
            new_capacity * sizeof(ethervox_memory_entry_t)
        );
        if (!new_entries) {
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
        store->entries = new_entries;
        store->entry_capacity = new_capacity;
    }
    
    // Create new entry with explicit IDs and timestamp
    ethervox_memory_entry_t* entry = &store->entries[store->entry_count];
    memset(entry, 0, sizeof(ethervox_memory_entry_t));
    
    entry->memory_id = memory_id;
    entry->turn_id = turn_id;
    entry->timestamp = timestamp;
    entry->importance = importance;
    entry->is_user_message = is_user_message;
    
    // Copy text (truncate if too long)
    snprintf(entry->text, sizeof(entry->text), "%s", text);
    
    // Copy tags
    entry->tag_count = tag_count;
    for (uint32_t i = 0; i < tag_count; i++) {
        snprintf(entry->tags[i], ETHERVOX_MEMORY_TAG_LEN, "%s", tags[i]);
        
        // Update tag index
        ethervox_memory_tag_index_t* idx = get_tag_index(store, tags[i]);
        if (idx && idx->count < 1024) {
            idx->memory_ids[idx->count++] = entry->memory_id;
        }
    }
    
    store->entry_count++;
    
    // Ensure storage is ready before writing (handles directory deletion during session)
    ensure_storage_ready(store);
    
    // Append to persistent log (JSONL format)
    if (store->append_log) {
        fprintf(store->append_log,
                "{\"id\":%llu,\"turn\":%llu,\"ts\":%ld,\"user\":%s,\"imp\":%.2f,\"text\":",
                (unsigned long long)entry->memory_id, (unsigned long long)entry->turn_id, entry->timestamp,
                is_user_message ? "true" : "false", importance);
        
        // Write JSON-escaped text
        fputc('"', store->append_log);
        for (const char* p = text; *p && (p - text) < ETHERVOX_MEMORY_MAX_TEXT_LEN - 1; p++) {
            if (*p == '"' || *p == '\\') {
                fputc('\\', store->append_log);
            }
            fputc(*p, store->append_log);
        }
        fputc('"', store->append_log);
        
        // Write tags
        fprintf(store->append_log, ",\"tags\":[");
        for (uint32_t i = 0; i < tag_count; i++) {
            fprintf(store->append_log, "%s\"%s\"", i > 0 ? "," : "", tags[i]);
        }
        fprintf(store->append_log, "]}\n");
        fflush(store->append_log);
    }
    
    if (memory_id_out) {
        *memory_id_out = entry->memory_id;
    }
    
    // Update counters to ensure next auto-generated IDs don't collide
    if (entry->memory_id >= store->total_memories_stored) {
        store->total_memories_stored = entry->memory_id + 1;
    }
    if (entry->turn_id >= store->current_turn_id) {
        store->current_turn_id = entry->turn_id + 1;
    }
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_memory_store_add(
    ethervox_memory_store_t* store,
    const char* text,
    const char* tags[],
    uint32_t tag_count,
    float importance,
    bool is_user_message,
    uint64_t* memory_id_out
) {
    if (!store) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    // Generate new ID and timestamp
    uint64_t memory_id = store->total_memories_stored;
    uint64_t turn_id = store->current_turn_id;
    time_t timestamp = time(NULL);
    
    return memory_store_add_internal(store, text, tags, tag_count, importance,
                                    is_user_message, memory_id, turn_id,
                                    timestamp, memory_id_out);
}

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
) {
    if (!store || !store->is_initialized || !text) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Validate inputs
    if (tag_count > ETHERVOX_MEMORY_MAX_TAGS) {
        tag_count = ETHERVOX_MEMORY_MAX_TAGS;
    }
    
    if (tools_count > 16) {
        tools_count = 16;  // Max tools per memory entry
    }
    
    if (importance < 0.0f) importance = 0.0f;
    if (importance > 1.0f) importance = 1.0f;
    
    // Grow entries array if needed
    if (store->entry_count >= store->entry_capacity) {
        uint32_t new_capacity = store->entry_capacity * 2;
        if (new_capacity > ETHERVOX_MEMORY_MAX_ENTRIES) {
            new_capacity = ETHERVOX_MEMORY_MAX_ENTRIES;
        }
        
        ethervox_memory_entry_t* new_entries = realloc(
            store->entries,
            new_capacity * sizeof(ethervox_memory_entry_t)
        );
        if (!new_entries) {
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
        store->entries = new_entries;
        store->entry_capacity = new_capacity;
    }
    
    // Generate new ID and timestamp
    uint64_t memory_id = store->total_memories_stored;
    uint64_t turn_id = store->current_turn_id;
    time_t timestamp = time(NULL);
    
    // Create new entry
    ethervox_memory_entry_t* entry = &store->entries[store->entry_count];
    memset(entry, 0, sizeof(ethervox_memory_entry_t));
    
    entry->memory_id = memory_id;
    entry->turn_id = turn_id;
    entry->timestamp = timestamp;
    entry->importance = importance;
    entry->is_user_message = is_user_message;
    
    // Copy text (truncate if too long)
    snprintf(entry->text, sizeof(entry->text), "%s", text);
    
    // Copy tags
    entry->tag_count = tag_count;
    for (uint32_t i = 0; i < tag_count; i++) {
        snprintf(entry->tags[i], ETHERVOX_MEMORY_TAG_LEN, "%s", tags[i]);
        
        // Update tag index
        ethervox_memory_tag_index_t* idx = get_tag_index(store, tags[i]);
        if (idx && idx->count < 1024) {
            idx->memory_ids[idx->count++] = entry->memory_id;
        }
    }
    
    // Copy tools called
    entry->tools_called_count = tools_count;
    for (uint32_t i = 0; i < tools_count; i++) {
        snprintf(entry->tools_called[i], 64, "%s", tools_called[i]);
    }
    
    store->entry_count++;
    
    // Ensure storage is ready before writing (handles directory deletion during session)
    ensure_storage_ready(store);
    
    // Append to persistent log (JSONL format) with tools
    if (store->append_log) {
        fprintf(store->append_log,
                "{\"id\":%llu,\"turn\":%llu,\"ts\":%ld,\"user\":%s,\"imp\":%.2f,\"text\":",
                (unsigned long long)entry->memory_id, (unsigned long long)entry->turn_id, entry->timestamp,
                is_user_message ? "true" : "false", importance);
        
        // Write JSON-escaped text
        fputc('"', store->append_log);
        for (const char* p = text; *p && (p - text) < ETHERVOX_MEMORY_MAX_TEXT_LEN - 1; p++) {
            if (*p == '"' || *p == '\\') {
                fputc('\\', store->append_log);
            }
            fputc(*p, store->append_log);
        }
        fputc('"', store->append_log);
        
        // Write tags
        fprintf(store->append_log, ",\"tags\":[");
        for (uint32_t i = 0; i < tag_count; i++) {
            fprintf(store->append_log, "%s\"%s\"", i > 0 ? "," : "", tags[i]);
        }
        fprintf(store->append_log, "]");
        
        // Write tools if any
        if (tools_count > 0) {
            fprintf(store->append_log, ",\"tools\":[");
            for (uint32_t i = 0; i < tools_count; i++) {
                fprintf(store->append_log, "%s\"%s\"", i > 0 ? "," : "", tools_called[i]);
            }
            fprintf(store->append_log, "]");
        }
        
        fprintf(store->append_log, "}\n");
        fflush(store->append_log);
    }
    
    if (memory_id_out) {
        *memory_id_out = entry->memory_id;
    }
    
    // Update counters
    store->total_memories_stored = memory_id + 1;
    store->current_turn_id = turn_id + 1;
    
    return ETHERVOX_SUCCESS;
}

// Internal update_tags function with optional persistence (non-static for use in memory_export.c)
ethervox_result_t memory_update_tags_internal(
    ethervox_memory_store_t* store,
    uint64_t memory_id,
    const char* tags[],
    uint32_t tag_count,
    bool persist_to_log
) {
    if (!store || !store->is_initialized || !tags || tag_count == 0 || tag_count > ETHERVOX_MEMORY_MAX_TAGS) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Find the entry
    ethervox_memory_entry_t* entry = NULL;
    for (uint32_t i = 0; i < store->entry_count; i++) {
        if (store->entries[i].memory_id == memory_id) {
            entry = &store->entries[i];
            break;
        }
    }
    
    if (!entry) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;  // Not found
    }
    
    // Update tags in memory
    for (uint32_t i = 0; i < tag_count && i < ETHERVOX_MEMORY_MAX_TAGS; i++) {
        snprintf(entry->tags[i], ETHERVOX_MEMORY_TAG_LEN, "%s", tags[i]);
    }
    entry->tag_count = tag_count;
    
    // Persist the update to JSONL file as an UPDATE record (if requested)
    if (persist_to_log && store->append_log) {
        fprintf(store->append_log,
                "{\"op\":\"update\",\"id\":%llu,\"tags\":[",
                (unsigned long long)memory_id);
        
        for (uint32_t i = 0; i < tag_count; i++) {
            fprintf(store->append_log, "%s\"%s\"", i > 0 ? "," : "", tags[i]);
        }
        fprintf(store->append_log, "]}\n");
        fflush(store->append_log);
    }
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_memory_update_tags(
    ethervox_memory_store_t* store,
    uint64_t memory_id,
    const char* tags[],
    uint32_t tag_count
) {
    return memory_update_tags_internal(store, memory_id, tags, tag_count, true);
}

ethervox_result_t ethervox_memory_update_text(
    ethervox_memory_store_t* store,
    uint64_t memory_id,
    const char* new_text
) {
    if (!store || !store->is_initialized || !new_text) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Find the entry
    ethervox_memory_entry_t* entry = NULL;
    for (uint32_t i = 0; i < store->entry_count; i++) {
        if (store->entries[i].memory_id == memory_id) {
            entry = &store->entries[i];
            break;
        }
    }
    
    if (!entry) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;  // Not found
    }
    
    // Update the text
    strncpy(entry->text, new_text, ETHERVOX_MEMORY_MAX_TEXT_LEN - 1);
    entry->text[ETHERVOX_MEMORY_MAX_TEXT_LEN - 1] = '\0';
    
    // Write UPDATE record with new text
    if (store->append_log) {
        fprintf(store->append_log, "{\"op\":\"update_text\",\"id\":%llu,\"text\":\"", memory_id);
        
        // Escape special characters in JSON string
        for (const char* p = new_text; *p && (p - new_text) < ETHERVOX_MEMORY_MAX_TEXT_LEN - 1; p++) {
            if (*p == '"' || *p == '\\') {
                fputc('\\', store->append_log);
            } else if (*p == '\n') {
                fputs("\\n", store->append_log);
                continue;
            } else if (*p == '\r') {
                fputs("\\r", store->append_log);
                continue;
            } else if (*p == '\t') {
                fputs("\\t", store->append_log);
                continue;
            }
            fputc(*p, store->append_log);
        }
        
        fprintf(store->append_log, "\"}\n");
        fflush(store->append_log);
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Updated text for memory %llu", (unsigned long long)memory_id);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_memory_get_by_id(
    ethervox_memory_store_t* store,
    uint64_t memory_id,
    const ethervox_memory_entry_t** entry_out
) {
    if (!store || !store->is_initialized || !entry_out) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Linear search (could optimize with hash map)
    for (uint32_t i = 0; i < store->entry_count; i++) {
        if (store->entries[i].memory_id == memory_id) {
            *entry_out = &store->entries[i];
            return ETHERVOX_SUCCESS;
        }
    }
    
    return ETHERVOX_ERROR_INVALID_ARGUMENT;  // Not found
}

ethervox_result_t ethervox_memory_store_correction(
    ethervox_memory_store_t* store,
    const char* correction_text,
    const char* context,
    uint64_t* memory_id_out
) {
    if (!store || !correction_text) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build complete text with context
    char full_text[ETHERVOX_MEMORY_MAX_TEXT_LEN];
    if (context && context[0]) {
        snprintf(full_text, sizeof(full_text), 
                "CORRECTION: %s (Context: %s)", correction_text, context);
    } else {
        snprintf(full_text, sizeof(full_text), 
                "CORRECTION: %s", correction_text);
    }
    
    // Corrections are tagged as high-priority with importance=0.99
    const char* correction_tags[] = {"correction", "high_priority"};
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Storing correction for adaptive learning: %s", correction_text);
    
    return ethervox_memory_store_add(
        store,
        full_text,
        correction_tags,
        2,  // 2 tags
        0.99f,  // High importance
        true,  // User message
        memory_id_out
    );
}

ethervox_result_t ethervox_memory_store_pattern(
    ethervox_memory_store_t* store,
    const char* pattern_description,
    uint64_t* memory_id_out
) {
    if (!store || !pattern_description) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build pattern text
    char pattern_text[ETHERVOX_MEMORY_MAX_TEXT_LEN];
    snprintf(pattern_text, sizeof(pattern_text), 
            "SUCCESS PATTERN: %s", pattern_description);
    
    // Patterns are tagged with success indicators and importance=0.90
    const char* pattern_tags[] = {"pattern", "success"};
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Storing success pattern: %s", pattern_description);
    
    return ethervox_memory_store_add(
        store,
        pattern_text,
        pattern_tags,
        2,  // 2 tags
        0.90f,  // High importance (slightly lower than corrections)
        false,  // Assistant message
        memory_id_out
    );
}

ethervox_result_t ethervox_memory_get_corrections(
    ethervox_memory_store_t* store,
    ethervox_memory_search_result_t** results,
    uint32_t* result_count,
    uint32_t limit
) {
    if (!store || !results || !result_count) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Search for correction tag
    const char* correction_tag = "correction";
    return ethervox_memory_search(
        store,
        NULL,  // No text query, just tag filter
        &correction_tag,
        1,  // 1 tag
        limit,
        results,
        result_count
    );
}

ethervox_result_t ethervox_memory_get_patterns(
    ethervox_memory_store_t* store,
    ethervox_memory_search_result_t** results,
    uint32_t* result_count,
    uint32_t limit
) {
    if (!store || !results || !result_count) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Search for pattern tag
    const char* pattern_tag = "pattern";
    return ethervox_memory_search(
        store,
        NULL,  // No text query, just tag filter
        &pattern_tag,
        1,  // 1 tag
        limit,
        results,
        result_count
    );
}

