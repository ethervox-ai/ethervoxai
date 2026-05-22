/**
#include "ethervox/error.h"
 * @file memory_export.c
 * @brief Memory export/import in JSON and Markdown formats
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

// Internal function from memory_core.c for importing with explicit ID/timestamp
extern int memory_store_add_internal(
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
);

// Internal function from memory_search.c for deleting without persisting to log
extern int memory_delete_by_ids_internal(
    ethervox_memory_store_t* store,
    const uint64_t* memory_ids,
    uint32_t id_count,
    bool persist_to_log,
    uint32_t* items_deleted
);

// Internal function from memory_core.c for updating tags with optional persistence
extern int memory_update_tags_internal(
    ethervox_memory_store_t* store,
    uint64_t memory_id,
    const char* tags[],
    uint32_t tag_count,
    bool persist_to_log
);

// Format timestamp for display
static void format_timestamp(time_t timestamp, char* buf, size_t len) {
    struct tm* tm_info = localtime(&timestamp);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Write JSON-escaped string
static void write_json_escaped(FILE* fp, const char* str) {
    fputc('"', fp);
    for (const char* p = str; *p; p++) {
        switch (*p) {
            case '"':  fputs("\\\"", fp); break;
            case '\\': fputs("\\\\", fp); break;
            case '\n': fputs("\\n", fp); break;
            case '\r': fputs("\\r", fp); break;
            case '\t': fputs("\\t", fp); break;
            default:   fputc(*p, fp); break;
        }
    }
    fputc('"', fp);
}

ethervox_result_t ethervox_memory_export(
    ethervox_memory_store_t* store,
    const char* filepath,
    const char* format,
    uint64_t* bytes_written
) {
    if (!store || !store->is_initialized || !filepath || !format) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to open export file: %s", filepath);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint64_t bytes = 0;
    
    if (strcmp(format, "json") == 0) {
        // Export as JSON
        fprintf(fp, "{\n");
        fprintf(fp, "  \"session_id\": \"%s\",\n", store->session_id);
        fprintf(fp, "  \"session_started\": %ld,\n", store->session_started);
        fprintf(fp, "  \"total_turns\": %llu,\n", (unsigned long long)store->current_turn_id);
        fprintf(fp, "  \"entry_count\": %u,\n", store->entry_count);
        fprintf(fp, "  \"entries\": [\n");
        
        for (uint32_t i = 0; i < store->entry_count; i++) {
            ethervox_memory_entry_t* entry = &store->entries[i];
            
            fprintf(fp, "    {\n");
            fprintf(fp, "      \"memory_id\": %llu,\n", (unsigned long long)entry->memory_id);
            fprintf(fp, "      \"turn_id\": %llu,\n", (unsigned long long)entry->turn_id);
            fprintf(fp, "      \"timestamp\": %ld,\n", entry->timestamp);
            fprintf(fp, "      \"is_user\": %s,\n", entry->is_user_message ? "true" : "false");
            fprintf(fp, "      \"importance\": %.2f,\n", entry->importance);
            fprintf(fp, "      \"text\": ");
            write_json_escaped(fp, entry->text);
            fprintf(fp, ",\n");
            
            fprintf(fp, "      \"tags\": [");
            for (uint32_t t = 0; t < entry->tag_count; t++) {
                fprintf(fp, "%s\"%s\"", t > 0 ? ", " : "", entry->tags[t]);
            }
            fprintf(fp, "]");
            
            if (entry->tools_called_count > 0) {
                fprintf(fp, ",\n      \"tools_called\": [");
                for (uint32_t t = 0; t < entry->tools_called_count; t++) {
                    fprintf(fp, "%s\"%s\"", t > 0 ? ", " : "", entry->tools_called[t]);
                }
                fprintf(fp, "]");
            }
            
            fprintf(fp, "\n    }%s\n", i < store->entry_count - 1 ? "," : "");
        }
        
        fprintf(fp, "  ],\n");
        fprintf(fp, "  \"statistics\": {\n");
        fprintf(fp, "    \"total_memories\": %llu,\n", (unsigned long long)store->total_memories_stored);
        fprintf(fp, "    \"total_searches\": %llu,\n", (unsigned long long)store->total_searches);
        fprintf(fp, "    \"total_exports\": %llu\n", (unsigned long long)store->total_exports);
        fprintf(fp, "  }\n");
        fprintf(fp, "}\n");
        
    } else if (strcmp(format, "markdown") == 0) {
        // Export as Markdown
        char time_buf[64];
        format_timestamp(store->session_started, time_buf, sizeof(time_buf));
        
        fprintf(fp, "# Conversation Memory Export\n\n");
        fprintf(fp, "**Session ID:** %s  \n", store->session_id);
        fprintf(fp, "**Started:** %s  \n", time_buf);
        fprintf(fp, "**Total Turns:** %llu  \n", (unsigned long long)store->current_turn_id);
        fprintf(fp, "**Entries:** %u  \n\n", store->entry_count);
        
        fprintf(fp, "---\n\n");
        
        uint64_t last_turn = 0;
        for (uint32_t i = 0; i < store->entry_count; i++) {
            ethervox_memory_entry_t* entry = &store->entries[i];
            
            // Turn separator
            if (entry->turn_id != last_turn) {
                fprintf(fp, "\n## Turn %llu\n\n", (unsigned long long)entry->turn_id);
                last_turn = entry->turn_id;
            }
            
            // Format timestamp
            format_timestamp(entry->timestamp, time_buf, sizeof(time_buf));
            
            // Message
            fprintf(fp, "**%s** `[%s]` _importance: %.2f_\n\n",
                   entry->is_user_message ? "User" : "Assistant",
                   time_buf, entry->importance);
            
            fprintf(fp, "%s\n\n", entry->text);
            
            // Tags
            if (entry->tag_count > 0) {
                fprintf(fp, "_Tags:_ ");
                for (uint32_t t = 0; t < entry->tag_count; t++) {
                    fprintf(fp, "`%s`%s", entry->tags[t],
                           t < entry->tag_count - 1 ? ", " : "");
                }
                fprintf(fp, "\n\n");
            }
            
            // Tools called
            if (entry->tools_called_count > 0) {
                fprintf(fp, "_Tools:_ ");
                for (uint32_t t = 0; t < entry->tools_called_count; t++) {
                    fprintf(fp, "`%s`%s", entry->tools_called[t],
                           t < entry->tools_called_count - 1 ? ", " : "");
                }
                fprintf(fp, "\n\n");
            }
            
            fprintf(fp, "---\n\n");
        }
        
        // Statistics
        fprintf(fp, "\n## Statistics\n\n");
        fprintf(fp, "- Total memories stored: %llu\n", (unsigned long long)store->total_memories_stored);
        fprintf(fp, "- Total searches performed: %llu\n", (unsigned long long)store->total_searches);
        fprintf(fp, "- Total exports: %llu\n", (unsigned long long)store->total_exports);
        
    } else {
        fclose(fp);
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Unknown export format: %s", format);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    bytes = ftell(fp);
    fclose(fp);
    
    store->total_exports++;
    
    if (bytes_written) {
        *bytes_written = bytes;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Exported %u entries to %s (%s format, %llu bytes)",
                store->entry_count, filepath, format, (unsigned long long)bytes);
    
    return ETHERVOX_SUCCESS;
}

// Helper function to process a single JSON entry (used by both structured JSON and JSONL import)
static bool process_json_entry(ethervox_memory_store_t* store, const char* line) {
    // Check for special operations first
    if (strstr(line, "\"op\":\"delete\"") || strstr(line, "\"op\":\"update\"") || 
        strstr(line, "\"op\":\"update_text\"")) {
        // These operations are only relevant for JSONL session logs, not for JSON exports
        // JSON exports contain only the final state of memories, not operation history
        return false;
    }
    
    // Parse memory entry fields
    uint64_t id = 0, turn_id = 0;
    long timestamp = 0;
    float importance = 0.5f;
    bool is_user = false;
    char text[ETHERVOX_MEMORY_MAX_TEXT_LEN] = {0};
    
    // Extract numeric fields
    char* id_ptr = strstr(line, "\"memory_id\":");
    if (!id_ptr) id_ptr = strstr(line, "\"id\":");  // Fallback for JSONL format
    
    char* turn_ptr = strstr(line, "\"turn_id\":");
    if (!turn_ptr) turn_ptr = strstr(line, "\"turn\":");  // Fallback
    
    char* ts_ptr = strstr(line, "\"timestamp\":");
    if (!ts_ptr) ts_ptr = strstr(line, "\"ts\":");  // Fallback
    
    char* imp_ptr = strstr(line, "\"importance\":");
    if (!imp_ptr) imp_ptr = strstr(line, "\"imp\":");  // Fallback
    
    char* user_ptr = strstr(line, "\"is_user\":");
    if (!user_ptr) user_ptr = strstr(line, "\"user\":");  // Fallback
    
    if (!id_ptr || !turn_ptr || !ts_ptr || !imp_ptr || !user_ptr) {
        return false;  // Missing required fields
    }
    
    // Parse values
    id = strtoull(strchr(id_ptr, ':') + 1, NULL, 10);
    turn_id = strtoull(strchr(turn_ptr, ':') + 1, NULL, 10);
    timestamp = strtol(strchr(ts_ptr, ':') + 1, NULL, 10);
    importance = strtof(strchr(imp_ptr, ':') + 1, NULL);
    
    // Check is_user boolean - only check the value immediately after the colon
    // Find the value start (skip whitespace after colon)
    char* user_value = strchr(user_ptr, ':');
    if (user_value) {
        user_value++;  // Skip colon
        while (*user_value == ' ' || *user_value == '\t') user_value++;
        
        // Check if value starts with 'true' (not just contains it somewhere)
        if (strncmp(user_value, "true", 4) == 0) {
            is_user = true;
        }
    }
    
    // Extract text field
    char* text_start = strstr(line, "\"text\":");
    if (!text_start) return false;
    
    // Find the colon after "text"
    text_start = strchr(text_start, ':');
    if (!text_start) return false;
    text_start++;  // Skip ':'
    
    // Skip whitespace
    while (*text_start == ' ' || *text_start == '\t') text_start++;
    
    // Find opening quote of the value
    if (*text_start != '"') {
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Expected quote after 'text:', found: '%c' (0x%02x)", *text_start, (unsigned char)*text_start);
        return false;
    }
    text_start++;  // Skip opening quote
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "Parsing text field, first 50 chars: '%.50s'", text_start);
    
    // Find closing quote (handle escaped quotes)
    char* text_end = text_start;
    while (*text_end) {
        if (*text_end == '"' && (text_end == text_start || *(text_end - 1) != '\\')) {
            break;
        }
        text_end++;
    }
    
    if (*text_end != '"') return false;
    
    // Copy and unescape text
    size_t text_len = text_end - text_start;
    if (text_len >= sizeof(text)) text_len = sizeof(text) - 1;
    
    size_t out_idx = 0;
    for (size_t i = 0; i < text_len && out_idx < sizeof(text) - 1; i++) {
        if (text_start[i] == '\\' && i + 1 < text_len) {
            i++;
            if (text_start[i] == 'n') text[out_idx++] = '\n';
            else if (text_start[i] == 'r') text[out_idx++] = '\r';
            else if (text_start[i] == 't') text[out_idx++] = '\t';
            else if (text_start[i] == '"') text[out_idx++] = '"';
            else if (text_start[i] == '\\') text[out_idx++] = '\\';
            else text[out_idx++] = text_start[i];
        } else {
            text[out_idx++] = text_start[i];
        }
    }
    text[out_idx] = '\0';
    
    // Parse tags array
    const char* tag_array[ETHERVOX_MEMORY_MAX_TAGS];
    uint32_t tag_count = 0;
    char tag_storage[ETHERVOX_MEMORY_MAX_TAGS][ETHERVOX_MEMORY_TAG_LEN];
    
    char* tags_start = strstr(line, "\"tags\":");
    if (tags_start) {
        tags_start = strchr(tags_start, '[');
        if (tags_start) {
            tags_start++;  // Skip '['
            char* tags_end = strchr(tags_start, ']');
            
            if (tags_end) {
                char* tag_cursor = tags_start;
                while (tag_cursor < tags_end && tag_count < ETHERVOX_MEMORY_MAX_TAGS) {
                    // Skip whitespace and commas
                    while (tag_cursor < tags_end && (*tag_cursor == ' ' || *tag_cursor == ',' || 
                           *tag_cursor == '\n' || *tag_cursor == '\r' || *tag_cursor == '\t')) {
                        tag_cursor++;
                    }
                    
                    if (*tag_cursor == '"') {
                        tag_cursor++;  // Skip opening quote
                        char* tag_end = tag_cursor;
                        
                        // Find closing quote
                        while (tag_end < tags_end && *tag_end != '"') {
                            tag_end++;
                        }
                        
                        if (tag_end < tags_end && *tag_end == '"') {
                            size_t tag_len = tag_end - tag_cursor;
                            if (tag_len > 0 && tag_len < ETHERVOX_MEMORY_TAG_LEN) {
                                memcpy(tag_storage[tag_count], tag_cursor, tag_len);
                                tag_storage[tag_count][tag_len] = '\0';
                                tag_array[tag_count] = tag_storage[tag_count];
                                tag_count++;
                            }
                            tag_cursor = tag_end + 1;
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }
        }
    }
    
    // Add "imported" tag if not already present
    bool has_imported = false;
    for (uint32_t i = 0; i < tag_count; i++) {
        if (strcmp(tag_array[i], "imported") == 0) {
            has_imported = true;
            break;
        }
    }
    
    if (!has_imported && tag_count < ETHERVOX_MEMORY_MAX_TAGS) {
        strcpy(tag_storage[tag_count], "imported");
        tag_array[tag_count] = tag_storage[tag_count];
        tag_count++;
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Added 'imported' tag (total tags now: %u)", tag_count);
    }
    
    // Ensure at least one tag
    if (tag_count == 0) {
        strcpy(tag_storage[0], "imported");
        tag_array[0] = tag_storage[0];
        tag_count = 1;
    }
    
    // Parse tools array (if present)
    char tools_storage[16][64];
    uint32_t tools_count = 0;
    
    // Try both "tools_called" (current format) and "tools" (legacy format)
    char* tools_start = strstr(line, "\"tools_called\":");
    if (!tools_start) {
        tools_start = strstr(line, "\"tools\":");
    }
    
    if (tools_start) {
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Found tools field in JSONL line");
        tools_start = strchr(tools_start, '[');
        if (tools_start) {
            tools_start++;  // Skip '['
            char* tools_end = strchr(tools_start, ']');
            
            if (tools_end) {
                char* tool_cursor = tools_start;
                while (tool_cursor < tools_end && tools_count < 16) {
                    // Skip whitespace and commas
                    while (tool_cursor < tools_end && (*tool_cursor == ' ' || *tool_cursor == ',' || 
                           *tool_cursor == '\n' || *tool_cursor == '\r' || *tool_cursor == '\t')) {
                        tool_cursor++;
                    }
                    
                    if (*tool_cursor == '"') {
                        tool_cursor++;  // Skip opening quote
                        char* tool_end = tool_cursor;
                        
                        // Find closing quote
                        while (tool_end < tools_end && *tool_end != '"') {
                            tool_end++;
                        }
                        
                        if (tool_end < tools_end && *tool_end == '"') {
                            size_t tool_len = tool_end - tool_cursor;
                            if (tool_len > 0 && tool_len < 64) {
                                memcpy(tools_storage[tools_count], tool_cursor, tool_len);
                                tools_storage[tools_count][tool_len] = '\0';
                                ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                                            "Parsed tool %u: '%s'", tools_count, tools_storage[tools_count]);
                                tools_count++;
                            }
                            tool_cursor = tool_end + 1;
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }
        }
    }
    
    // Add memory with original metadata
    uint64_t memory_id_out;
    ethervox_result_t result = memory_store_add_internal(store, text, tag_array, tag_count,
                                     importance, is_user, id, turn_id,
                                     timestamp, &memory_id_out);
    
    // If tools were found, add them to the entry
    if (result == 0 && tools_count > 0) {
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Attempting to set %u tools on memory ID %llu", 
                    tools_count, (unsigned long long)memory_id_out);
        // Find the entry we just added and set its tools
        bool found = false;
        for (uint32_t i = 0; i < store->entry_count; i++) {
            if (store->entries[i].memory_id == memory_id_out) {
                store->entries[i].tools_called_count = tools_count;
                for (uint32_t t = 0; t < tools_count; t++) {
                    snprintf(store->entries[i].tools_called[t], 64, "%s", tools_storage[t]);
                }
                ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                            "Successfully set %u tools on memory ID %llu", 
                            tools_count, (unsigned long long)memory_id_out);
                found = true;
                break;
            }
        }
        if (!found) {
            ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                        "Failed to find entry with memory ID %llu to set tools", 
                        (unsigned long long)memory_id_out);
        }
    }
    
    return (result == 0);
}

ethervox_result_t ethervox_memory_import(
    ethervox_memory_store_t* store,
    const char* filepath,
    uint32_t* turns_loaded
) {
    if (!store || !store->is_initialized || !filepath) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to open import file: %s", filepath);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Detect format: read first few lines to check for structured JSON vs JSONL
    char buffer[1024];
    bool is_structured_json = false;
    
    // Read first chunk to detect format
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[bytes_read] = '\0';
    fseek(fp, 0, SEEK_SET);  // Rewind
    
    // If we find "entries" key, it's a structured JSON export
    // JSONL files have one object per line, without "entries" wrapper
    if (strstr(buffer, "\"entries\"")) {
        is_structured_json = true;
    }
    
    if (is_structured_json) {
        // Read entire file for structured JSON parsing
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        char* json_data = malloc(file_size + 1);
        if (!json_data) {
            fclose(fp);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
        
        fread(json_data, 1, file_size, fp);
        json_data[file_size] = '\0';
        fclose(fp);
        
        // Parse structured JSON and extract entries array
        uint32_t loaded = 0;
        char* entries_start = strstr(json_data, "\"entries\"");
        if (entries_start) {
            entries_start = strchr(entries_start, '[');
            if (entries_start) {
                entries_start++;  // Skip '['
                
                // Find each entry object within the array
                char* entry_cursor = entries_start;
                while (*entry_cursor && *entry_cursor != ']') {
                    // Skip whitespace
                    while (*entry_cursor == ' ' || *entry_cursor == '\n' || *entry_cursor == '\r' || *entry_cursor == '\t') {
                        entry_cursor++;
                    }
                    
                    if (*entry_cursor == '{') {
                        // Found entry object start
                        char* entry_end = entry_cursor + 1;
                        int brace_depth = 1;
                        
                        // Find matching closing brace
                        while (*entry_end && brace_depth > 0) {
                            if (*entry_end == '{') brace_depth++;
                            else if (*entry_end == '}') brace_depth--;
                            entry_end++;
                        }
                        
                        if (brace_depth == 0) {
                            // Extract this entry as a line and process it
                            size_t entry_len = entry_end - entry_cursor;
                            char entry_line[ETHERVOX_MEMORY_MAX_TEXT_LEN + 512];
                            
                            if (entry_len < sizeof(entry_line)) {
                                memcpy(entry_line, entry_cursor, entry_len);
                                entry_line[entry_len] = '\0';
                                
                                // Process this entry using the same logic as JSONL
                                if (process_json_entry(store, entry_line)) {
                                    loaded++;
                                }
                            }
                            
                            entry_cursor = entry_end;
                        } else {
                            break;  // Malformed JSON
                        }
                    } else if (*entry_cursor == ',') {
                        entry_cursor++;  // Skip comma between entries
                    } else {
                        break;  // Unexpected character
                    }
                }
            }
        }
        
        free(json_data);
        
        if (turns_loaded) {
            *turns_loaded = loaded;
        }
        
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Imported %u entries from structured JSON: %s", loaded, filepath);
        
        return ETHERVOX_SUCCESS;
    }
    
    // JSONL format: one entry per line
    char line[ETHERVOX_MEMORY_MAX_TEXT_LEN + 512];
    uint32_t loaded = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        // Check if this is a DELETE operation
        char* delete_op = strstr(line, "\"op\":\"delete\"");
        if (delete_op) {
            // Handle DELETE record - remove memory by ID
            char* id_ptr = strstr(line, "\"id\":");
            if (!id_ptr) continue;
            
            uint64_t memory_id = strtoull(id_ptr + 5, NULL, 10);
            
            // Delete the memory entry without persisting (we're already reading from file)
            uint32_t deleted = 0;
            memory_delete_by_ids_internal(store, &memory_id, 1, false, &deleted);
            
            // Write DELETE record to new session file to preserve operation history
            // NOTE: This may be redundant - memories are already added with final state,
            // so preserving operation history isn't strictly necessary for correctness.
            // Kept for auditability and potential future replay/undo features.
            if (store->append_log && deleted > 0) {
                fprintf(store->append_log, "{\"op\":\"delete\",\"id\":%llu}\n", memory_id);
                fflush(store->append_log);
            }
            
            continue;  // Skip to next line
        }
        
        // Check if this is an UPDATE_TEXT operation
        char* update_text_op = strstr(line, "\"op\":\"update_text\"");
        if (update_text_op) {
            // Handle UPDATE_TEXT record - update text content for existing memory
            char* id_ptr = strstr(line, "\"id\":");
            if (!id_ptr) continue;
            
            uint64_t memory_id = strtoull(id_ptr + 5, NULL, 10);
            
            // Parse new text
            char* text_start = strstr(line, "\"text\":\"");
            if (text_start) {
                text_start += 8;  // Skip "text":"
                char* text_end = text_start;
                
                // Find end quote, handling escaped quotes
                while (*text_end) {
                    if (*text_end == '"' && (text_end == text_start || *(text_end - 1) != '\\')) {
                        break;
                    }
                    text_end++;
                }
                
                if (*text_end == '"') {
                    char new_text[ETHERVOX_MEMORY_MAX_TEXT_LEN];
                    size_t text_len = text_end - text_start;
                    if (text_len >= ETHERVOX_MEMORY_MAX_TEXT_LEN) {
                        text_len = ETHERVOX_MEMORY_MAX_TEXT_LEN - 1;
                    }
                    
                    // Copy and unescape
                    size_t out_idx = 0;
                    for (size_t i = 0; i < text_len && out_idx < ETHERVOX_MEMORY_MAX_TEXT_LEN - 1; i++) {
                        if (text_start[i] == '\\' && i + 1 < text_len) {
                            i++;
                            if (text_start[i] == 'n') new_text[out_idx++] = '\n';
                            else if (text_start[i] == 'r') new_text[out_idx++] = '\r';
                            else if (text_start[i] == 't') new_text[out_idx++] = '\t';
                            else new_text[out_idx++] = text_start[i];
                        } else {
                            new_text[out_idx++] = text_start[i];
                        }
                    }
                    new_text[out_idx] = '\0';
                    
                    // Apply update
                    ethervox_memory_update_text(store, memory_id, new_text);
                }
            }
            
            continue;  // Skip to next line
        }
        
        // Check if this is an UPDATE operation (tags)
        char* op_ptr = strstr(line, "\"op\":\"update\"");
        if (op_ptr) {
            // Handle UPDATE record - update tags for existing memory
            char* id_ptr = strstr(line, "\"id\":");
            if (!id_ptr) continue;
            
            uint64_t memory_id = strtoull(id_ptr + 5, NULL, 10);
            
            // Parse updated tags
            const char* tag_array[ETHERVOX_MEMORY_MAX_TAGS];
            uint32_t tag_count = 0;
            char tag_storage[ETHERVOX_MEMORY_MAX_TAGS][ETHERVOX_MEMORY_TAG_LEN];
            
            char* tags_start = strstr(line, "\"tags\":[");
            if (tags_start) {
                tags_start += 8;  // Skip "tags":[
                char* tags_end = strchr(tags_start, ']');
                
                if (tags_end) {
                    char* tag_cursor = tags_start;
                    while (tag_cursor < tags_end && tag_count < ETHERVOX_MEMORY_MAX_TAGS) {
                        char* tag_open = strchr(tag_cursor, '"');
                        if (!tag_open || tag_open >= tags_end) break;
                        
                        tag_open++;
                        char* tag_close = strchr(tag_open, '"');
                        if (!tag_close || tag_close >= tags_end) break;
                        
                        size_t tag_len = tag_close - tag_open;
                        if (tag_len > 0 && tag_len < ETHERVOX_MEMORY_TAG_LEN) {
                            strncpy(tag_storage[tag_count], tag_open, tag_len);
                            tag_storage[tag_count][tag_len] = '\0';
                            tag_array[tag_count] = tag_storage[tag_count];
                            tag_count++;
                        }
                        
                        tag_cursor = tag_close + 1;
                    }
                }
            }
            
            // Apply the tag update to the memory entry
            if (tag_count > 0) {
                // Don't persist during update (we're reading from old file)
                int update_result = memory_update_tags_internal(store, memory_id, tag_array, tag_count, false);
                
                // Write UPDATE record to new session file to preserve operation history
                // NOTE: This may be redundant - when memories are later imported, they're
                // added with their current tag state, so operation history isn't needed.
                // Kept for auditability and potential future replay/undo features.
                if (store->append_log && update_result == 0) {
                    fprintf(store->append_log, "{\"op\":\"update\",\"id\":%llu,\"tags\":[", memory_id);
                    for (uint32_t i = 0; i < tag_count; i++) {
                        fprintf(store->append_log, "\"%s\"", tag_array[i]);
                        if (i < tag_count - 1) {
                            fprintf(store->append_log, ",");
                        }
                    }
                    fprintf(store->append_log, "]}\n");
                    fflush(store->append_log);
                }
            }
            
            continue;  // Skip to next line
        }
        
        // For regular ADD records or entries, use the helper function
        if (process_json_entry(store, line)) {
            loaded++;
        }
    }
    
    fclose(fp);
    
    if (turns_loaded) {
        *turns_loaded = loaded;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Imported %u entries from %s", loaded, filepath);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_memory_load_previous_session(
    ethervox_memory_store_t* store,
    uint32_t* turns_loaded
) {
    if (!store || !store->is_initialized) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // If no storage directory is set, nothing to load
    if (!store->storage_filepath[0]) {
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "No storage directory set, skipping previous session load");
        if (turns_loaded) *turns_loaded = 0;
        return ETHERVOX_SUCCESS;
    }
    
    // Extract directory path from current storage filepath
    char storage_dir[512];
    strncpy(storage_dir, store->storage_filepath, sizeof(storage_dir) - 1);
    storage_dir[sizeof(storage_dir) - 1] = '\0';
    
    char* last_slash = strrchr(storage_dir, '/');
    if (!last_slash) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Invalid storage filepath: %s", store->storage_filepath);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    *last_slash = '\0';  // Truncate to get directory
    
    // Get basename of current session for comparison
    const char* current_basename = last_slash + 1;
    
    // Find most recent previous session file
    DIR* dir = opendir(storage_dir);
    if (!dir) {
        ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                    "Failed to open storage directory: %s", storage_dir);
        if (turns_loaded) *turns_loaded = 0;
        return ETHERVOX_SUCCESS;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Searching for previous sessions in %s (current: %s)", storage_dir, current_basename);
    
    struct dirent* entry;
    time_t latest_time = 0;
    char latest_session[512] = {0};
    int files_checked = 0, files_skipped = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // Look for .jsonl files
        size_t len = strlen(entry->d_name);
        if (len > 6 && strcmp(entry->d_name + len - 6, ".jsonl") == 0) {
            files_checked++;
            
            // Skip current session file by name
            if (strcmp(entry->d_name, current_basename) == 0) {
                ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                            "Skipping current session: %s", entry->d_name);
                files_skipped++;
                continue;
            }
            
            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", storage_dir, entry->d_name);
            
            struct stat st;
            if (stat(fullpath, &st) == 0) {
                ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                            "Found candidate: %s (mtime=%ld, size=%lld)", 
                            entry->d_name, st.st_mtime, (long long)st.st_size);
                            
                if (st.st_mtime > latest_time) {
                    latest_time = st.st_mtime;
                    snprintf(latest_session, sizeof(latest_session), "%s", fullpath);
                }
            }
        }
    }
    closedir(dir);
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Checked %d .jsonl files, skipped %d, selected: %s", 
                files_checked, files_skipped, latest_session[0] ? latest_session : "(none)");
    
    // Load the latest previous session if found
    if (latest_session[0] != '\0') {
        uint32_t loaded = 0;
        ethervox_result_t result = ethervox_memory_import(store, latest_session, &loaded);
        
        if (ethervox_is_success(result) && loaded > 0) {
            ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                        "Loaded %u previous memories from %s", loaded, latest_session);
            if (turns_loaded) *turns_loaded = loaded;
            return ETHERVOX_SUCCESS;
        } else if (result != 0) {
            ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                        "Failed to import previous session from %s", latest_session);
            if (turns_loaded) *turns_loaded = 0;
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    // No previous session found
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "No previous session file found");
    if (turns_loaded) *turns_loaded = 0;
    return ETHERVOX_SUCCESS;
}
