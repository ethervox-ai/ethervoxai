/**
#include "ethervox/error.h"
 * @file memory_search.c
 * @brief Memory search and retrieval with tag filtering and text similarity
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include "ethervox/logging.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple text similarity using word overlap (Jaccard-like)
static float calculate_text_similarity(const char* text1, const char* text2) {
    if (!text1 || !text2) {
        return 0.0f;
    }
    
    // Convert to lowercase for comparison
    char buf1[512], buf2[512];
    snprintf(buf1, sizeof(buf1), "%s", text1);
    snprintf(buf2, sizeof(buf2), "%s", text2);
    
    for (char* p = buf1; *p; p++) *p = tolower((unsigned char)*p);
    for (char* p = buf2; *p; p++) *p = tolower((unsigned char)*p);
    
    // Tokenize both texts into word sets
    char* words1[64] = {0};
    char* words2[64] = {0};
    uint32_t count1 = 0, count2 = 0;
    
    // Tokenize query
    char* word = strtok(buf1, " \t\n.,!?;:");
    while (word && count1 < 64) {
        // Skip very short words (articles, etc.)
        if (strlen(word) > 2) {
            words1[count1++] = word;
        }
        word = strtok(NULL, " \t\n.,!?;:");
    }
    
    // Tokenize entry text
    word = strtok(buf2, " \t\n.,!?;:");
    while (word && count2 < 64) {
        if (strlen(word) > 2) {
            words2[count2++] = word;
        }
        word = strtok(NULL, " \t\n.,!?;:");
    }
    
    if (count1 == 0 || count2 == 0) {
        return 0.0f;
    }
    
    // Count matching words
    uint32_t matches = 0;
    for (uint32_t i = 0; i < count1; i++) {
        for (uint32_t j = 0; j < count2; j++) {
            if (strcmp(words1[i], words2[j]) == 0) {
                matches++;
                break;  // Each query word matches at most once
            }
        }
    }
    
    float similarity = (count1 > 0) ? ((float)matches / (float)count1) : 0.0f;
    
    // Debug logging
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "Similarity: query_words=%u, entry_words=%u, matches=%u, score=%.2f | Query: '%s' | Entry: '%.50s...'",
                count1, count2, matches, similarity, text1, text2);
    
    return similarity;
}

// Check if entry has all required tags
static bool has_all_tags(
    const ethervox_memory_entry_t* entry,
    const char* tag_filter[],
    uint32_t tag_filter_count
) {
    if (tag_filter_count == 0) {
        return true;  // No filter = all pass
    }
    
    for (uint32_t i = 0; i < tag_filter_count; i++) {
        bool found = false;
        for (uint32_t j = 0; j < entry->tag_count; j++) {
            if (strcmp(entry->tags[j], tag_filter[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;  // Missing a required tag
        }
    }
    
    return true;
}

// Comparison for qsort - descending relevance
static int compare_results(const void* a, const void* b) {
    const ethervox_memory_search_result_t* r1 = a;
    const ethervox_memory_search_result_t* r2 = b;
    
    if (r1->relevance > r2->relevance) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    if (r1->relevance < r2->relevance) return ETHERVOX_SUCCESS;
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_memory_search(
    ethervox_memory_store_t* store,
    const char* query,
    const char* tag_filter[],
    uint32_t tag_filter_count,
    uint32_t limit,
    ethervox_memory_search_result_t** results,
    uint32_t* result_count
) {
    if (!store || !store->is_initialized || !results || !result_count) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    store->total_searches++;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "memory_search START: query='%s', query_ptr=%p, query[0]=%d, limit=%u, entry_count=%u",
                query ? query : "(null)", (void*)query, query ? (int)query[0] : -1, limit, store->entry_count);
    
    if (limit == 0) {
        limit = 10;  // Default limit
    }
    
    // Allocate results buffer for ALL matching entries (not just limit)
    // We need to find all matches, sort them, then return top N
    ethervox_memory_search_result_t* temp_results = malloc(
        store->entry_count * sizeof(ethervox_memory_search_result_t)
    );
    if (!temp_results) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint32_t found_count = 0;
    
    // Search through ALL entries to find best matches
    for (uint32_t i = 0; i < store->entry_count; i++) {
        ethervox_memory_entry_t* entry = &store->entries[i];
        
        // Apply tag filter
        if (!has_all_tags(entry, tag_filter, tag_filter_count)) {
            continue;
        }
        
        // Calculate relevance
        float relevance = 1.0f;  // Base relevance
        
        if (query && query[0]) {
            // Text similarity scoring
            relevance = calculate_text_similarity(query, entry->text);
            
            ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                        "Entry %u: similarity=%.2f, importance=%.2f, text='%.60s...'",
                        i, relevance, entry->importance, entry->text);
            
            // Boost by importance
            relevance = relevance * 0.7f + entry->importance * 0.3f;
            
            ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                        "Entry %u: final relevance=%.2f", i, relevance);
        } else {
            // No query = just use importance and recency
            // More recent entries score higher
            float recency = 1.0f - ((float)i / (float)store->entry_count);
            relevance = entry->importance * 0.6f + recency * 0.4f;
        }
        
        // Add to results
        temp_results[found_count].entry = *entry;
        temp_results[found_count].relevance = relevance;
        found_count++;
    }
    
    // Sort by relevance (descending)
    if (found_count > 0) {
        qsort(temp_results, found_count, sizeof(ethervox_memory_search_result_t),
              compare_results);
    }
    
    // Limit to requested count and resize
    uint32_t return_count = (found_count > limit) ? limit : found_count;
    
    if (return_count > 0 && return_count < store->entry_count) {
        // Allocate final results array with exact size needed
        ethervox_memory_search_result_t* final_results = malloc(
            return_count * sizeof(ethervox_memory_search_result_t)
        );
        if (final_results) {
            // Copy only the top N results
            memcpy(final_results, temp_results, return_count * sizeof(ethervox_memory_search_result_t));
            free(temp_results);
            *results = final_results;
            *result_count = return_count;
        } else {
            // Allocation failed, return original buffer
            *results = temp_results;
            *result_count = return_count;
        }
    } else {
        // No need to resize
        *results = temp_results;
        *result_count = return_count;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "Search completed: query='%s', filters=%u, found=%u",
                query ? query : "(none)", tag_filter_count, found_count);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_memory_summarize(
    ethervox_memory_store_t* store,
    uint32_t window_size,
    const char* focus_topic,
    char** summary_out,
    char*** key_points_out,
    uint32_t* key_points_count
) {
    if (!store || !store->is_initialized || !summary_out) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (window_size == 0) {
        window_size = 10;  // Default: last 10 turns
    }
    
    // Calculate start index for window
    uint32_t start_idx = 0;
    if (store->entry_count > window_size) {
        start_idx = store->entry_count - window_size;
    }
    
    // Build summary text
    size_t summary_len = 4096;
    char* summary = malloc(summary_len);
    if (!summary) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int pos = snprintf(summary, summary_len,
                      "Conversation summary (last %u turns):\n\n",
                      window_size);
    
    // Collect key points
    char** key_points = malloc(window_size * sizeof(char*));
    uint32_t kp_count = 0;
    
    for (uint32_t i = start_idx; i < store->entry_count; i++) {
        ethervox_memory_entry_t* entry = &store->entries[i];
        
        // If focus topic specified, filter by tag or text match
        if (focus_topic && focus_topic[0]) {
            bool matches = false;
            
            // Check tags
            for (uint32_t t = 0; t < entry->tag_count; t++) {
                if (strstr(entry->tags[t], focus_topic)) {
                    matches = true;
                    break;
                }
            }
            
            // Check text
            if (!matches && strstr(entry->text, focus_topic)) {
                matches = true;
            }
            
            if (!matches) {
                continue;
            }
        }
        
        // Add to summary
        const char* speaker = entry->is_user_message ? "User" : "Assistant";
        int written = snprintf(summary + pos, summary_len - pos,
                              "- %s: %s\n",
                              speaker, entry->text);
        
        if (written > 0 && (size_t)written < summary_len - pos) {
            pos += written;
        }
        
        // Extract key points (high importance entries)
        if (entry->importance >= 0.7f && kp_count < window_size) {
            key_points[kp_count] = malloc(strlen(entry->text) + 1);
            if (key_points[kp_count]) {
                strcpy(key_points[kp_count], entry->text);
                kp_count++;
            }
        }
    }
    
    *summary_out = summary;
    
    if (key_points_out && key_points_count) {
        *key_points_out = key_points;
        *key_points_count = kp_count;
    } else {
        // Free key points if not requested
        for (uint32_t i = 0; i < kp_count; i++) {
            free(key_points[i]);
        }
        free(key_points);
    }
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_memory_forget(
    ethervox_memory_store_t* store,
    uint64_t older_than_seconds,
    float importance_threshold,
    uint32_t* items_pruned
) {
    if (!store || !store->is_initialized) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    time_t cutoff_time = 0;
    if (older_than_seconds > 0) {
        cutoff_time = time(NULL) - older_than_seconds;
    }
    
    uint32_t pruned = 0;
    uint32_t write_idx = 0;
    
    // Compact array by removing unwanted entries
    for (uint32_t i = 0; i < store->entry_count; i++) {
        ethervox_memory_entry_t* entry = &store->entries[i];
        bool should_keep = true;
        
        // Check age
        if (older_than_seconds > 0 && entry->timestamp < cutoff_time) {
            should_keep = false;
        }
        
        // Check importance
        if (importance_threshold > 0.0f && entry->importance < importance_threshold) {
            should_keep = false;
        }
        
        if (should_keep) {
            if (write_idx != i) {
                store->entries[write_idx] = *entry;
            }
            write_idx++;
        } else {
            // Write DELETE record to persist the removal
            if (store->append_log) {
                fprintf(store->append_log, "{\"op\":\"delete\",\"id\":%llu}\n", entry->memory_id);
            }
            pruned++;
        }
    }
    
    // Flush to ensure deletions are persisted
    if (store->append_log && pruned > 0) {
        fflush(store->append_log);
    }
    
    store->entry_count = write_idx;
    
    // TODO: Rebuild tag index after pruning
    
    if (items_pruned) {
        *items_pruned = pruned;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Pruned %u memories (age>%llu s, importance<%.2f)",
                pruned, (unsigned long long)older_than_seconds, importance_threshold);
    
    return ETHERVOX_SUCCESS;
}

// Internal delete function - doesn't write to JSONL (for use during import)
ethervox_result_t memory_delete_by_ids_internal(
    ethervox_memory_store_t* store,
    const uint64_t* memory_ids,
    uint32_t id_count,
    bool persist_to_log,
    uint32_t* items_deleted
) {
    if (!store || !store->is_initialized || !memory_ids || id_count == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint32_t deleted = 0;
    uint32_t write_idx = 0;
    
    // Compact array by removing entries with matching IDs
    for (uint32_t i = 0; i < store->entry_count; i++) {
        ethervox_memory_entry_t* entry = &store->entries[i];
        bool should_delete = false;
        
        // Check if this entry's ID is in the delete list
        for (uint32_t j = 0; j < id_count; j++) {
            if (entry->memory_id == memory_ids[j]) {
                should_delete = true;
                break;
            }
        }
        
        if (!should_delete) {
            if (write_idx != i) {
                store->entries[write_idx] = *entry;
            }
            write_idx++;
        } else {
            deleted++;
            
            // Write DELETE record to JSONL file (only if persisting)
            if (persist_to_log && store->append_log) {
                fprintf(store->append_log, "{\"op\":\"delete\",\"id\":%llu}\n", 
                        (unsigned long long)entry->memory_id);
                fflush(store->append_log);
            }
        }
    }
    
    store->entry_count = write_idx;
    
    // TODO: Rebuild tag index after deletion
    
    if (items_deleted) {
        *items_deleted = deleted;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Deleted %u memories by ID", deleted);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_memory_delete_by_ids(
    ethervox_memory_store_t* store,
    const uint64_t* memory_ids,
    uint32_t id_count,
    uint32_t* items_deleted
) {
    return memory_delete_by_ids_internal(store, memory_ids, id_count, true, items_deleted);
}
