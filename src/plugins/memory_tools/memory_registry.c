/**
#include "ethervox/error.h"
 * @file memory_registry.c
 * @brief Register memory tools with Governor tool registry
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Global memory store pointer for tool wrappers
// Note: Not thread-safe, but sufficient for single-threaded CLI
static ethervox_memory_store_t* g_memory_store = NULL;

// Global privacy flag for secret mode (when true, memory logging is disabled)
static bool g_disable_memory_logging = false;

// JSON parsing helper (simplified - would use cJSON in production)
static int parse_json_string(const char* json, const char* key, char* value, size_t value_len) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    
    const char* start = strstr(json, search);
    if (!start) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    start += strlen(search);
    
    // Skip whitespace after colon
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
        start++;
    }
    
    // Expect opening quote
    if (*start != '"') return ETHERVOX_ERROR_INVALID_ARGUMENT;
    start++;
    
    const char* end = strchr(start, '"');
    if (!end) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    size_t len = end - start;
    if (len >= value_len) len = value_len - 1;
    
    strncpy(value, start, len);
    value[len] = '\0';
    return ETHERVOX_SUCCESS;
}

static int parse_json_float(const char* json, const char* key, float* value) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    
    const char* start = strstr(json, search);
    if (!start) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    start += strlen(search);
    *value = strtof(start, NULL);
    return ETHERVOX_SUCCESS;
}

static int parse_json_uint(const char* json, const char* key, uint32_t* value) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    
    const char* start = strstr(json, search);
    if (!start) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    start += strlen(search);
    *value = strtoul(start, NULL, 10);
    return ETHERVOX_SUCCESS;
}

static int parse_json_bool(const char* json, const char* key, bool* value) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    
    const char* start = strstr(json, search);
    if (!start) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    start += strlen(search);
    while (*start == ' ' || *start == '\t') start++;
    
    *value = (strncmp(start, "true", 4) == 0);
    return ETHERVOX_SUCCESS;
}

// Tool wrappers that bridge between Governor API and memory functions


// Parse a JSON array of strings: "tags": ["reminder", "recurring"]
// OR a single string: "tags": "reminder"
static int parse_json_string_array(const char* json, const char* key, char tags[][32], uint32_t* tag_count, uint32_t max_tags) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* start = strstr(json, search);
    if (!start) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    start += strlen(search);
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
    
    *tag_count = 0;
    
    // Handle single string: "tags": "reminder"
    if (*start == '"') {
        start++;
        const char* end = strchr(start, '"');
        if (!end) return ETHERVOX_ERROR_INVALID_ARGUMENT;
        size_t len = end - start;
        if (len > 0 && len < 32) {
            strncpy(tags[0], start, len);
            tags[0][len] = '\0';
            (*tag_count) = 1;
            return ETHERVOX_SUCCESS;
        }
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Handle array: "tags": ["reminder", "recurring"]
    if (*start != '[') return ETHERVOX_ERROR_INVALID_ARGUMENT;
    start++;
    while (*start && *start != ']') {
        while (*start == ' ' || *start == '\t' || *start == ',') start++;
        if (*start == '"') {
            start++;
            const char* end = strchr(start, '"');
            if (!end) break;
            size_t len = end - start;
            if (len > 0 && len < 32 && *tag_count < max_tags) {
                strncpy(tags[*tag_count], start, len);
                tags[*tag_count][len] = '\0';
                (*tag_count)++;
            }
            start = end + 1;
        } else {
            break;
        }
    }
    return (*tag_count > 0) ? 0 : -1;
}

static int tool_memory_store_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    // SECRET MODE: Skip memory logging if privacy flag is set
    if (g_disable_memory_logging) {
        char* res = malloc(256);
        snprintf(res, 256, "{\"success\":true,\"memory_id\":0,\"note\":\"Secret mode - memory not saved\"}");
        *result = res;
        return ETHERVOX_SUCCESS;  // Success, but no actual storage
    }
    
    ethervox_memory_store_t* store = g_memory_store;
    char text[ETHERVOX_MEMORY_MAX_TEXT_LEN];
    float importance = 0.5f;
    bool is_user = false;
    char tags[8][32];
    uint32_t tag_count = 0;

    // Parse arguments - support 'key', 'text', or 'value' as parameter name
    if (parse_json_string(args_json, "key", text, sizeof(text)) != 0) {
        // Try 'text' as fallback parameter name
        if (parse_json_string(args_json, "text", text, sizeof(text)) != 0) {
            // Try 'value' as alternative parameter name
            if (parse_json_string(args_json, "value", text, sizeof(text)) != 0) {
                *error = strdup("Missing 'key', 'text', or 'value' parameter");
                return ETHERVOX_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    parse_json_float(args_json, "importance", &importance);
    parse_json_bool(args_json, "is_user", &is_user);
    
    // Try to parse tags; if none provided, default to "general"
    if (parse_json_string_array(args_json, "tags", tags, &tag_count, 8) != 0) {
        // Only use "general" if tags were not provided at all
        strncpy(tags[0], "general", 32);
        tag_count = 1;
    }

    // Convert to const char* array
    const char* tag_ptrs[8];
    for (uint32_t i = 0; i < tag_count; i++) tag_ptrs[i] = tags[i];

    uint64_t memory_id;
    if (ethervox_memory_store_add(store, text, tag_ptrs, tag_count,
                                  importance, is_user, &memory_id) != 0) {
        *error = strdup("Failed to store memory");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    char* res = malloc(256);
    snprintf(res, 256, "{\"success\":true,\"memory_id\":%llu}", (unsigned long long)memory_id);
    *result = res;
    return ETHERVOX_SUCCESS;
}
// Tool: memory_reminder_list - returns all reminders (tagged 'reminder')
static int tool_memory_reminder_list_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_memory_store_t* store = g_memory_store;
    ethervox_memory_search_result_t* results = NULL;
    uint32_t result_count = 0;
    const char* tag_filter[] = {"reminder"};
    // Find up to 32 reminders
    if (ethervox_memory_search(store, NULL, tag_filter, 1, 32, &results, &result_count) != 0) {
        *error = strdup("Search failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    // Build JSON response
    size_t res_len = 4096;
    char* res = malloc(res_len);
    int pos = snprintf(res, res_len, "{\"reminders\":[");
    for (uint32_t i = 0; i < result_count; i++) {
        const ethervox_memory_entry_t* e = &results[i].entry;
        pos += snprintf(res + pos, res_len - pos,
            "%s{\"memory_id\":%llu,\"text\":\"%s\",\"tags\":[",
            i > 0 ? "," : "",
            (unsigned long long)e->memory_id,
            e->text);
        for (uint32_t t = 0; t < e->tag_count; t++) {
            pos += snprintf(res + pos, res_len - pos, "%s\"%s\"", t > 0 ? "," : "", e->tags[t]);
        }
        pos += snprintf(res + pos, res_len - pos, "],\"timestamp\":%llu}", (unsigned long long)e->timestamp);
    }
    snprintf(res + pos, res_len - pos, "],\"count\":%u}", result_count);
    free(results);
    *result = res;
    return ETHERVOX_SUCCESS;
}

static int tool_memory_search_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_memory_store_t* store = g_memory_store;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "memory_search_wrapper INPUT: args_json='%s'", args_json ? args_json : "(null)");
    
    char query[512] = {0};
    uint32_t limit = 10;
    float min_importance = 0.0f;  // Default: no importance filter
    
    parse_json_string(args_json, "query", query, sizeof(query));
    parse_json_uint(args_json, "limit", &limit);
    parse_json_float(args_json, "min_importance", &min_importance);
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "memory_search_wrapper: query='%s', limit=%u, min_importance=%.2f, query[0]=%d",
                query, limit, min_importance, (int)query[0]);
    
    ethervox_memory_search_result_t* results = NULL;
    uint32_t result_count = 0;
    
    // If empty query, return most recent message instead
    if (query[0] == '\0' && store && store->entry_count > 0) {
        // Find the most recent entry
        ethervox_memory_entry_t* most_recent = &store->entries[0];
        for (uint32_t i = 1; i < store->entry_count; i++) {
            if (store->entries[i].timestamp > most_recent->timestamp) {
                most_recent = &store->entries[i];
            }
        }
        
        // Build JSON response with most recent entry
        size_t res_len = 4096;
        char* res = malloc(res_len);
        snprintf(res, res_len,
                "{\"results\":[{\"text\":\"%s\",\"relevance\":1.0,\"importance\":%.2f}],\"count\":1}",
                most_recent->text,
                most_recent->importance);
        
        *result = res;
        return ETHERVOX_SUCCESS;
    }
    
    if (ethervox_memory_search(store, query, NULL, 0, limit,
                              &results, &result_count) != 0) {
        *error = strdup("Search failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build JSON response, filtering by min_importance
    size_t res_len = 4096;
    char* res = malloc(res_len);
    int pos = snprintf(res, res_len, "{\"results\":[");
    
    uint32_t output_count = 0;
    for (uint32_t i = 0; i < result_count && output_count < limit; i++) {
        // Skip entries below importance threshold
        if (min_importance > 0.0f && results[i].entry.importance < min_importance) {
            continue;
        }
        pos += snprintf(res + pos, res_len - pos,
                       "%s{\"text\":\"%s\",\"relevance\":%.2f,\"importance\":%.2f}",
                       output_count > 0 ? "," : "",
                       results[i].entry.text,
                       results[i].relevance,
                       results[i].entry.importance);
        output_count++;
    }
    
    snprintf(res + pos, res_len - pos, "],\"count\":%u}", output_count);
    
    free(results);
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

static int tool_memory_summarize_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_memory_store_t* store = g_memory_store;
    
    uint32_t window_size = 10;
    char focus[256] = {0};
    
    parse_json_uint(args_json, "window_size", &window_size);
    parse_json_string(args_json, "focus_topic", focus, sizeof(focus));
    
    char* summary = NULL;
    if (ethervox_memory_summarize(store, window_size,
                                  focus[0] ? focus : NULL,
                                  &summary, NULL, NULL) != 0) {
        *error = strdup("Summarization failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Return as JSON
    size_t res_len = strlen(summary) + 256;
    char* res = malloc(res_len);
    snprintf(res, res_len, "{\"summary\":\"%s\"}", summary);
    
    free(summary);
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

// Tool: memory_complete_reminder - marks a reminder as completed by adding a 'completed' tag
static int tool_memory_complete_reminder_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_memory_store_t* store = g_memory_store;
    if (!store) {
        *error = strdup("Memory store not initialized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint64_t memory_id = 0;
    char id_str[32];
    if (parse_json_string(args_json, "memory_id", id_str, sizeof(id_str)) != 0) {
        *error = strdup("Missing 'memory_id' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    memory_id = strtoull(id_str, NULL, 10);
    
    // Find the entry and check if already completed
    ethervox_memory_entry_t* entry = NULL;
    for (uint32_t i = 0; i < store->entry_count; i++) {
        if (store->entries[i].memory_id == memory_id) {
            entry = &store->entries[i];
            break;
        }
    }
    
    if (!entry) {
        *error = strdup("Reminder not found");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check if already completed
    int already_completed = 0;
    for (uint32_t t = 0; t < entry->tag_count; t++) {
        if (strncmp(entry->tags[t], "completed", ETHERVOX_MEMORY_TAG_LEN) == 0) {
            already_completed = 1;
            break;
        }
    }
    
    if (!already_completed && entry->tag_count < ETHERVOX_MEMORY_MAX_TAGS) {
        // Build new tag array with existing tags + "completed"
        const char* new_tags[ETHERVOX_MEMORY_MAX_TAGS];
        uint32_t new_tag_count = 0;
        
        // Copy existing tags
        for (uint32_t t = 0; t < entry->tag_count && new_tag_count < ETHERVOX_MEMORY_MAX_TAGS; t++) {
            new_tags[new_tag_count++] = entry->tags[t];
        }
        
        // Add "completed" tag
        if (new_tag_count < ETHERVOX_MEMORY_MAX_TAGS) {
            new_tags[new_tag_count++] = "completed";
        }
        
        // Update tags and persist to JSONL
        ethervox_memory_update_tags(store, memory_id, new_tags, new_tag_count);
    }
    
    char* res = malloc(64);
    snprintf(res, 64, "{\"success\":true,\"memory_id\":%llu}", (unsigned long long)memory_id);
    *result = res;
    return ETHERVOX_SUCCESS;
}

// Tool: memory_export_wrapper - export conversation to file
static int tool_memory_export_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_memory_store_t* store = g_memory_store;
    if (!store) {
        *error = strdup("Memory store not initialized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char filepath[512] = {0};
    char format[32] = "json";
    
    if (parse_json_string(args_json, "filepath", filepath, sizeof(filepath)) != 0) {
        *error = strdup("Missing 'filepath' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    parse_json_string(args_json, "format", format, sizeof(format));
    
    uint64_t bytes_written = 0;
    if (ethervox_memory_export(store, filepath, format, &bytes_written) != 0) {
        *error = strdup("Export failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char* res = malloc(256);
    snprintf(res, 256, "{\"success\":true,\"bytes_written\":%llu}", (unsigned long long)bytes_written);
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

// Forward declaration for delegation
static int tool_memory_delete_wrapper(const char* args_json, char** result, char** error);

static int tool_memory_forget_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_memory_store_t* store = g_memory_store;
    
    // Check if memory_ids parameter is present (delegate to delete functionality)
    if (strstr(args_json, "\"memory_ids\"")) {
        // Delegate to delete logic
        return tool_memory_delete_wrapper(args_json, result, error);
    }
    
    uint64_t older_than = 0;
    float importance_threshold = 0.0f;
    
    parse_json_uint(args_json, "older_than_seconds", (uint32_t*)&older_than);
    parse_json_float(args_json, "importance_threshold", &importance_threshold);
    
    uint32_t pruned = 0;
    if (ethervox_memory_forget(store, older_than, importance_threshold, &pruned) != 0) {
        *error = strdup("Forget operation failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char* res = malloc(256);
    snprintf(res, 256, "{\"success\":true,\"items_pruned\":%u}", pruned);
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

// Tool: memory_delete - delete specific memories by ID
static int tool_memory_delete_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_memory_store_t* store = g_memory_store;
    if (!store) {
        *error = strdup("Memory store not initialized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Parse memory_ids - can be either:
    // 1. JSON array: "memory_ids":[123,456,789]
    // 2. String with array: "memory_ids":"[123,456,789]"
    // 3. Single ID: "memory_id":"123"
    
    uint64_t ids[100]; // Max 100 IDs at once
    uint32_t id_count = 0;
    
    // Try to find memory_ids as JSON array
    char* ids_start = strstr(args_json, "\"memory_ids\":[");
    if (ids_start) {
        ids_start += 14; // Skip "memory_ids":[
        
        // Parse array of IDs
        char* cursor = ids_start;
        while (*cursor && *cursor != ']' && id_count < 100) {
            // Skip whitespace and commas
            while (*cursor && (*cursor == ' ' || *cursor == ',')) cursor++;
            
            if (*cursor >= '0' && *cursor <= '9') {
                ids[id_count++] = strtoull(cursor, &cursor, 10);
            } else {
                break;
            }
        }
    } else {
        // Try memory_ids as string containing array: "memory_ids":"[123,456]"
        char ids_str[512];
        if (parse_json_string(args_json, "memory_ids", ids_str, sizeof(ids_str)) == 0) {
            // Parse the string as an array
            char* cursor = ids_str;
            
            // Skip opening bracket if present
            if (*cursor == '[') cursor++;
            
            while (*cursor && id_count < 100) {
                // Skip whitespace and commas
                while (*cursor && (*cursor == ' ' || *cursor == ',' || *cursor == '[')) cursor++;
                
                if (*cursor >= '0' && *cursor <= '9') {
                    ids[id_count++] = strtoull(cursor, &cursor, 10);
                } else if (*cursor == ']') {
                    break;
                } else if (*cursor == '\0') {
                    break;
                } else {
                    cursor++;
                }
            }
        } else {
            // Try single memory_id parameter
            char id_str[32];
            if (parse_json_string(args_json, "memory_id", id_str, sizeof(id_str)) == 0) {
                uint64_t single_id = strtoull(id_str, NULL, 10);
                uint32_t deleted = 0;
                if (ethervox_memory_delete_by_ids(store, &single_id, 1, &deleted) != 0) {
                    *error = strdup("Delete operation failed");
                    return ETHERVOX_ERROR_INVALID_ARGUMENT;
                }
                char* res = malloc(256);
                snprintf(res, 256, "{\"success\":true,\"items_deleted\":%u}", deleted);
                *result = res;
                return ETHERVOX_SUCCESS;
            }
            
            *error = strdup("Missing 'memory_ids' or 'memory_id' parameter");
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    if (id_count == 0) {
        *error = strdup("No valid memory IDs provided");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint32_t deleted = 0;
    if (ethervox_memory_delete_by_ids(store, ids, id_count, &deleted) != 0) {
        *error = strdup("Delete operation failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char* res = malloc(256);
    snprintf(res, 256, "{\"success\":true,\"items_deleted\":%u}", deleted);
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

// Tool: memory_update_reminder - update reminder text by ID
static int tool_memory_update_reminder_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_memory_store_t* store = g_memory_store;
    
    // Parse memory_id
    char id_str[32];
    if (parse_json_string(args_json, "memory_id", id_str, sizeof(id_str)) != 0) {
        *error = strdup("Missing 'memory_id' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint64_t memory_id = strtoull(id_str, NULL, 10);
    
    // Parse new_text
    char new_text[ETHERVOX_MEMORY_MAX_TEXT_LEN];
    if (parse_json_string(args_json, "new_text", new_text, sizeof(new_text)) != 0) {
        *error = strdup("Missing 'new_text' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Update the reminder text
    if (ethervox_memory_update_text(store, memory_id, new_text) != 0) {
        *error = strdup("Failed to update reminder - memory_id not found");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char* res = malloc(512);
    snprintf(res, 512, "{\"success\":true,\"memory_id\":%llu,\"updated_text\":\"%s\"}", 
             (unsigned long long)memory_id, new_text);
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

// Tool: memory_store_correction - store user correction as high-priority learning
static int tool_memory_store_correction_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_memory_store_t* store = g_memory_store;
    if (!store) {
        *error = strdup("Memory store not initialized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char correction_text[ETHERVOX_MEMORY_MAX_TEXT_LEN];
    char context[ETHERVOX_MEMORY_MAX_TEXT_LEN] = {0};
    
    if (parse_json_string(args_json, "correction", correction_text, sizeof(correction_text)) != 0) {
        *error = strdup("Missing 'correction' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Context is optional
    parse_json_string(args_json, "context", context, sizeof(context));
    
    uint64_t memory_id;
    if (ethervox_memory_store_correction(store, correction_text, 
                                         context[0] ? context : NULL, 
                                         &memory_id) != 0) {
        *error = strdup("Failed to store correction");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char* res = malloc(256);
    snprintf(res, 256, 
             "{\"success\":true,\"memory_id\":%llu,\"message\":\"Correction stored for adaptive learning\"}",
             (unsigned long long)memory_id);
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

// Tool: memory_store_pattern - store successful interaction pattern
static int tool_memory_store_pattern_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_memory_store_t* store = g_memory_store;
    if (!store) {
        *error = strdup("Memory store not initialized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char pattern_description[ETHERVOX_MEMORY_MAX_TEXT_LEN];
    
    if (parse_json_string(args_json, "pattern", pattern_description, sizeof(pattern_description)) != 0) {
        *error = strdup("Missing 'pattern' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint64_t memory_id;
    if (ethervox_memory_store_pattern(store, pattern_description, &memory_id) != 0) {
        *error = strdup("Failed to store pattern");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char* res = malloc(256);
    snprintf(res, 256, 
             "{\"success\":true,\"memory_id\":%llu,\"message\":\"Success pattern recorded\"}",
             (unsigned long long)memory_id);
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_memory_tools_register(
    void* registry_ptr,
    ethervox_memory_store_t* store
) {


    ethervox_tool_registry_t* registry = (ethervox_tool_registry_t*)registry_ptr;
    if (!registry || !store) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    // Set global memory store for tool wrappers to access
    g_memory_store = store;
    int ret = 0;

    // Register memory_complete_reminder tool
    ethervox_tool_t tool_complete_reminder = {
        .name = "memory_complete_reminder",
        .description = "Mark a reminder as completed/done by its memory_id. Use the memory_id from memory_reminder_list to complete specific reminders.",
        .parameters_json_schema =
            "{"
            "  \"type\": \"object\","
            "  \"properties\": {"
            "    \"memory_id\": {\"type\": \"string\", \"description\": \"The unique ID of the reminder to mark complete (from memory_reminder_list)\"}"
            "  },"
            "  \"required\": [\"memory_id\"]"
            "}",
        .execute = tool_memory_complete_reminder_wrapper,
        .is_deterministic = false,
        .requires_confirmation = false,
        .is_stateful = true,
        .estimated_latency_ms = 5.0f
    };
    ret |= ethervox_tool_registry_add(registry, &tool_complete_reminder);
    
    // Register memory_update_reminder tool
    ethervox_tool_t tool_update_reminder = {
        .name = "memory_update_reminder",
        .description = "Update a reminder's text (e.g., change due date, modify description). Use memory_id from memory_reminder_list. Updates the full text - include all details like deadline.",
        .parameters_json_schema =
            "{"
            "  \"type\": \"object\","
            "  \"properties\": {"
            "    \"memory_id\": {\"type\": \"string\", \"description\": \"The unique ID of the reminder to update (from memory_reminder_list)\"},"
            "    \"new_text\": {\"type\": \"string\", \"description\": \"New reminder text with updated deadline/description (e.g., 'Call John at 3:00 PM today')\"}"
            "  },"
            "  \"required\": [\"memory_id\", \"new_text\"]"
            "}",
        .execute = tool_memory_update_reminder_wrapper,
        .is_deterministic = false,
        .requires_confirmation = false,
        .is_stateful = true,
        .estimated_latency_ms = 5.0f
    };
    ret |= ethervox_tool_registry_add(registry, &tool_update_reminder);
    
    // Register memory_store tool (single definition)
    ethervox_tool_t tool_store = {
        .name = "memory_store",
        .description = "Save facts, reminders, or events to memory. For reminders, use 'reminder' tag and include time/deadline in text. Importance: 0.9+ for reminders, 0.95 for personal facts, 0.8 for preferences.",
        .parameters_json_schema =
            "{"
            "  \"type\": \"object\","
            "  \"properties\": {"
            "    \"text\": {\"type\": \"string\", \"description\": \"Content to remember. For reminders, include deadline/time info in the text. (Aliases: 'key' or 'value' also work)\"},"
            "    \"key\": {\"type\": \"string\", \"description\": \"Alternative to 'text': content to remember.\"},"
            "    \"value\": {\"type\": \"string\", \"description\": \"Alternative to 'text': content to remember.\"},"
            "    \"tags\": {\"type\": \"array\", \"items\": {\"type\": \"string\"}, \"description\": \"Labels like 'reminder', 'important', 'urgent', etc.\"},"
            "    \"importance\": {\"type\": \"number\", \"minimum\": 0, \"maximum\": 1, \"description\": \"0.0=low to 1.0=critical. Use 0.9+ for urgent reminders.\"},"
            "    \"is_user\": {\"type\": \"boolean\", \"description\": \"True if this is user input, false if assistant generated\"}"
            "  },"
            "  \"required\": []"
            "}",
        .execute = tool_memory_store_wrapper,
        .is_deterministic = false,
        .requires_confirmation = false,
        .is_stateful = true,
        .estimated_latency_ms = 5.0f
    };
    ret |= ethervox_tool_registry_add(registry, &tool_store);
    
    // Register memory_search tool
    ethervox_tool_t tool_search = {
        .name = "memory_search",
        .description = "Search conversation memory by text similarity and tags. Results are sorted by relevance and importance. Use min_importance to filter for important memories (0.9+ for critical info, 0.8+ for important context).",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"query\":{\"type\":\"string\",\"description\":\"Search query\"},"
            "\"tag_filter\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}},"
            "\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":100},"
            "\"min_importance\":{\"type\":\"number\",\"minimum\":0.0,\"maximum\":1.0,\"description\":\"Minimum importance threshold (0.0-1.0). Use 0.8+ for important context, 0.9+ for critical info.\"}"
            "}}",
        .execute = tool_memory_search_wrapper,
        .is_deterministic = true,
        .requires_confirmation = false,
        .is_stateful = false,
        .estimated_latency_ms = 10.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_search);
    


    // Register memory_reminder_list tool
    ethervox_tool_t tool_reminder_list = {
        .name = "memory_reminder_list",
        .description = "List all active reminders (entries tagged with 'reminder'). Returns reminder text, deadlines, importance, and memory_id for completing reminders.",
        .parameters_json_schema = "{\"type\":\"object\",\"properties\":{}}",
        .execute = tool_memory_reminder_list_wrapper,
        .is_deterministic = true,
        .requires_confirmation = false,
        .is_stateful = false,
        .estimated_latency_ms = 10.0f
    };
    ret |= ethervox_tool_registry_add(registry, &tool_reminder_list);
    ethervox_tool_t tool_export = {
        .name = "memory_export",
        .description = "Export the entire conversation history to a file. Requires both filepath and format. Example: export to './notes.md' as markdown",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"filepath\":{\"type\":\"string\",\"description\":\"Path where to save the file (required). Example: './conversation.md' or './notes.json'\"},"
            "\"format\":{\"type\":\"string\",\"enum\":[\"json\",\"markdown\"],\"description\":\"File format: 'json' or 'markdown' (required)\"}"
            "},\"required\":[\"filepath\",\"format\"]}",
        .execute = tool_memory_export_wrapper,
        .is_deterministic = true,
        .requires_confirmation = true,
        .is_stateful = true,
        .estimated_latency_ms = 50.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_export);
    
    // Register memory_forget tool
    ethervox_tool_t tool_forget = {
        .name = "memory_forget",
        .description = "Prune old or low-importance memories",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"older_than_seconds\":{\"type\":\"integer\",\"minimum\":0},"
            "\"importance_threshold\":{\"type\":\"number\",\"minimum\":0,\"maximum\":1}"
            "}}",
        .execute = tool_memory_forget_wrapper,
        .is_deterministic = false,
        .requires_confirmation = true,
        .is_stateful = true,
        .estimated_latency_ms = 15.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_forget);
    
    // Register memory_delete tool
    ethervox_tool_t tool_delete = {
        .name = "memory_delete",
        .description = "Delete specific memories by their IDs. Use memory_ids array for multiple, or memory_id for single deletion.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"memory_ids\":{\"type\":\"array\",\"items\":{\"type\":\"integer\"}},"
            "\"memory_id\":{\"type\":\"string\"}"
            "}}",
        .execute = tool_memory_delete_wrapper,
        .is_deterministic = false,
        .requires_confirmation = true,
        .is_stateful = true,
        .estimated_latency_ms = 10.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_delete);
    
    // Register memory_store_correction tool
    ethervox_tool_t tool_correction = {
        .name = "memory_store_correction",
        .description = "Store user corrections as high-priority learning. Use when the user corrects your understanding, preferences, or mistakes. These corrections will be prioritized in future responses to 'teach' the model.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"correction\":{\"type\":\"string\",\"description\":\"What the user corrected (required)\"},"
            "\"context\":{\"type\":\"string\",\"description\":\"Optional: What was wrong or what this relates to\"}"
            "},\"required\":[\"correction\"]}",
        .execute = tool_memory_store_correction_wrapper,
        .is_deterministic = false,
        .requires_confirmation = false,
        .is_stateful = true,
        .estimated_latency_ms = 10.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_correction);
    
    // Register memory_store_pattern tool
    ethervox_tool_t tool_pattern = {
        .name = "memory_store_pattern",
        .description = "Store successful interaction patterns. Use when an approach works well or the user explicitly approves a method. These patterns reinforce successful behaviors.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"pattern\":{\"type\":\"string\",\"description\":\"Description of what worked well (required)\"}"
            "},\"required\":[\"pattern\"]}",
        .execute = tool_memory_store_pattern_wrapper,
        .is_deterministic = false,
        .requires_confirmation = false,
        .is_stateful = true,
        .estimated_latency_ms = 10.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_pattern);
    
    if (ethervox_is_success(ret)) {
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Registered 8 memory tools with Governor (including correction and pattern learning)");
    }
    
    return ret;
}

/**
 * Set privacy mode for memory logging (secret mode)
 */
void ethervox_memory_set_privacy_mode(bool disable_logging) {
    g_disable_memory_logging = disable_logging;
    if (disable_logging) {
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "SECRET MODE enabled - memory logging disabled for privacy");
    } else {
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Normal mode - memory logging enabled");
    }
}

/**
 * Get current privacy mode state
 */
bool ethervox_memory_get_privacy_mode(void) {
    return g_disable_memory_logging;
}

