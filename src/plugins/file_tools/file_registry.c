/**
#include "ethervox/error.h"
 * @file file_registry.c
 * @brief Register file tools with Governor tool registry
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/file_tools.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Global file config pointer for tool wrappers
static ethervox_file_tools_config_t* g_file_config = NULL;
static ethervox_path_config_t* g_path_config = NULL;

// JSON parsing helpers (same as memory_registry.c)
static int parse_json_string(const char* json, const char* key, char* value, size_t value_len) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    
    const char* start = strstr(json, search);
    if (!start) {
        // Try with space after colon: "key": "value"
        snprintf(search, sizeof(search), "\"%s\": \"", key);
        start = strstr(json, search);
        if (!start) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    start += strlen(search);
    const char* end = strchr(start, '"');
    if (!end) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    size_t len = end - start;
    if (len >= value_len) len = value_len - 1;
    
    strncpy(value, start, len);
    value[len] = '\0';
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

// Tool wrapper: file_list
static int tool_file_list_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_file_tools_config_t* config = g_file_config;
    
    char directory[ETHERVOX_FILE_MAX_PATH];
    bool recursive = false;
    
    // Try both "directory" and "path" for compatibility with LLM optimized calls
    if (parse_json_string(args_json, "directory", directory, sizeof(directory)) != 0) {
        // Fall back to "path" parameter
        if (parse_json_string(args_json, "path", directory, sizeof(directory)) != 0) {
            *error = strdup("Missing 'directory' or 'path' parameter");
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    parse_json_bool(args_json, "recursive", &recursive);
    
    ethervox_file_entry_t* entries = NULL;
    uint32_t entry_count = 0;
    
    if (ethervox_file_list(config, directory, recursive, &entries, &entry_count) != 0) {
        *error = strdup("Failed to list directory");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build JSON response
    size_t res_len = entry_count * 512 + 256;
    char* res = malloc(res_len);
    int pos = snprintf(res, res_len, "{\"entries\":[");
    
    for (uint32_t i = 0; i < entry_count; i++) {
        pos += snprintf(res + pos, res_len - pos,
                       "%s{\"name\":\"%s\",\"path\":\"%s\",\"is_directory\":%s,\"size\":%llu}",
                       i > 0 ? "," : "",
                       entries[i].name,
                       entries[i].path,
                       entries[i].is_directory ? "true" : "false",
                       (unsigned long long)entries[i].size);
    }
    
    snprintf(res + pos, res_len - pos, "],\"count\":%u}", entry_count);
    
    free(entries);
    *result = res;
    
    return ETHERVOX_SUCCESS;
}
// Tool wrapper: file_read
static int tool_file_read_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_file_tools_config_t* config = g_file_config;
    
    char file_path[ETHERVOX_FILE_MAX_PATH];
    
    // Try both "file_path" and "path" for compatibility with LLM optimized calls
    if (parse_json_string(args_json, "file_path", file_path, sizeof(file_path)) != 0) {
        // Fall back to "path" parameter
        if (parse_json_string(args_json, "path", file_path, sizeof(file_path)) != 0) {
            *error = strdup("Missing 'file_path' or 'path' parameter");
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    // Check file size first to provide helpful error before reading
    struct stat st;
    if (stat(file_path, &st) == 0) {
        // Estimate tokens (rough: 1 token ≈ 4 characters)
        uint64_t estimated_tokens = st.st_size / 4;
        
        // If file would likely overflow context (> 1500 tokens ≈ 6KB)
        if (estimated_tokens > 1500) {
            // Build helpful error JSON
            size_t err_len = 1024;
            char* err_msg = malloc(err_len);
            if (err_msg) {
                // Count lines for better guidance
                FILE* fp = fopen(file_path, "r");
                uint32_t line_count = 0;
                if (fp) {
                    char line_buf[1024];
                    while (fgets(line_buf, sizeof(line_buf), fp)) {
                        line_count++;
                    }
                    fclose(fp);
                }
                
                snprintf(err_msg, err_len,
                    "{\"error\":\"file_too_large\","
                    "\"file_path\":\"%s\","
                    "\"size_bytes\":%llu,"
                    "\"size_kb\":%llu,"
                    "\"estimated_tokens\":%llu,"
                    "\"total_lines\":%u,"
                    "\"suggestion\":\"File is too large for context window. Use file_preview to see structure, then file_read_chunk with line ranges.\","
                    "\"recommended_chunk_size\":200}",
                    file_path,
                    (unsigned long long)st.st_size,
                    (unsigned long long)(st.st_size / 1024),
                    (unsigned long long)estimated_tokens,
                    line_count
                );
                *result = err_msg;  // Return as result, not error (LLM can parse JSON)
                return ETHERVOX_SUCCESS;  // Success - we provided useful info
            }
        }
    }
    
    char* content = NULL;
    uint64_t size = 0;
    
    int read_result = ethervox_file_read(config, file_path, &content, &size);
    
    if (read_result == -2) {
        // Binary file detected
        *result = strdup("{\"error\":\"binary_file\","
                        "\"message\":\"File contains binary data (non-text). Cannot read binary files.\","
                        "\"suggestion\":\"Ensure file is a text file (.txt, .md, .c, etc.)\"}");
        return ETHERVOX_SUCCESS;  // Return as result for LLM to understand
    } else if (read_result != 0) {
        *error = strdup("Failed to read file (permission denied or file not found)");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build JSON response (escape content)
    // Allocate 2x size for escaping + extra for JSON structure
    size_t res_len = (size * 2) + 1024;
    char* res = malloc(res_len);
    if (!res) {
        free(content);
        *error = strdup("Failed to allocate response buffer");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int pos = snprintf(res, res_len, "{\"content\":\"");
    
    // Simple JSON escaping - check bounds carefully
    for (uint64_t i = 0; i < size && pos < (int)res_len - 200; i++) {
        if (content[i] == '"' || content[i] == '\\') {
            res[pos++] = '\\';
            res[pos++] = content[i];
        } else if (content[i] == '\n') {
            res[pos++] = '\\';
            res[pos++] = 'n';
        } else if (content[i] == '\r') {
            res[pos++] = '\\';
            res[pos++] = 'r';
        } else if (content[i] == '\t') {
            res[pos++] = '\\';
            res[pos++] = 't';
        } else if ((unsigned char)content[i] >= 32) {
            res[pos++] = content[i];
        }
    }
    
    snprintf(res + pos, res_len - pos, "\",\"size\":%llu}", (unsigned long long)size);
    
    free(content);
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

// Tool wrapper: file_search
static int tool_file_search_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_file_tools_config_t* config = g_file_config;
    
    char directory[ETHERVOX_FILE_MAX_PATH];
    char pattern[512];
    
    if (parse_json_string(args_json, "directory", directory, sizeof(directory)) != 0) {
        *error = strdup("Missing 'directory' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (parse_json_string(args_json, "pattern", pattern, sizeof(pattern)) != 0) {
        *error = strdup("Missing 'pattern' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char** matches = NULL;
    uint32_t match_count = 0;
    
    if (ethervox_file_search(config, directory, pattern, &matches, &match_count) != 0) {
        *error = strdup("Search failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build JSON response
    size_t res_len = match_count * 256 + 256;
    char* res = malloc(res_len);
    int pos = snprintf(res, res_len, "{\"matches\":[");
    
    for (uint32_t i = 0; i < match_count; i++) {
        pos += snprintf(res + pos, res_len - pos,
                       "%s\"%s\"",
                       i > 0 ? "," : "",
                       matches[i]);
        free(matches[i]);
    }
    
    snprintf(res + pos, res_len - pos, "],\"count\":%u}", match_count);
    
    free(matches);
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

// Tool wrapper: file_write
static int tool_file_write_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_file_tools_config_t* config = g_file_config;
    
    char file_path[ETHERVOX_FILE_MAX_PATH];
    
    if (!config) {
        *error = strdup("File tools not initialized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (parse_json_string(args_json, "file_path", file_path, sizeof(file_path)) != 0) {
        *error = strdup("Missing 'file_path' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Extract content - need custom parsing for large content (can't use fixed-size buffer)
    // Find the content value in JSON: "content":"..."
    const char* content_key = "\"content\":\"";
    const char* content_start = strstr(args_json, content_key);
    if (!content_start) {
        // Try with space: "content": "..."
        content_key = "\"content\": \"";
        content_start = strstr(args_json, content_key);
        if (!content_start) {
            *error = strdup("Missing 'content' parameter");
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    content_start += strlen(content_key);
    
    // Find the closing quote by tracking escape sequences
    const char* content_end = content_start;
    while (*content_end) {
        if (*content_end == '\\') {
            // Skip escaped character
            content_end++;
            if (*content_end) content_end++;
        } else if (*content_end == '"') {
            // Found unescaped closing quote
            break;
        } else {
            content_end++;
        }
    }
    
    if (*content_end != '"' || content_end <= content_start) {
        *error = strdup("Invalid content format - missing closing quote");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Allocate buffer for content (heap, not stack)
    size_t content_len = content_end - content_start;
    char* content = malloc(content_len + 1);
    if (!content) {
        *error = strdup("Memory allocation failed for content");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    strncpy(content, content_start, content_len);
    content[content_len] = '\0';
    
    // Unescape JSON escape sequences in-place
    char* unescaped = malloc(content_len + 1);
    if (!unescaped) {
        free(content);
        *error = strdup("Memory allocation failed for unescaped content");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    size_t out_pos = 0;
    for (size_t i = 0; i < content_len; i++) {
        if (content[i] == '\\' && i + 1 < content_len) {
            switch (content[i + 1]) {
                case 'n':
                    unescaped[out_pos++] = '\n';
                    i++;
                    break;
                case 'r':
                    unescaped[out_pos++] = '\r';
                    i++;
                    break;
                case 't':
                    unescaped[out_pos++] = '\t';
                    i++;
                    break;
                case '"':
                    unescaped[out_pos++] = '"';
                    i++;
                    break;
                case '\\':
                    unescaped[out_pos++] = '\\';
                    i++;
                    break;
                default:
                    unescaped[out_pos++] = content[i];
            }
        } else {
            unescaped[out_pos++] = content[i];
        }
    }
    unescaped[out_pos] = '\0';
    
    if (ethervox_file_write(config, file_path, unescaped) != 0) {
        free(content);
        free(unescaped);
        *error = strdup("Write failed (check permissions and access mode)");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    free(content);
    free(unescaped);
    
    char* res = malloc(64);
    snprintf(res, 64, "{\"success\":true}");
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

// Tool wrapper: file_append
static int tool_file_append_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_file_tools_config_t* config = g_file_config;
    
    char file_path[ETHERVOX_FILE_MAX_PATH];
    
    if (!config) {
        *error = strdup("File tools not initialized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Try both "file_path" and "path" for compatibility
    if (parse_json_string(args_json, "file_path", file_path, sizeof(file_path)) != 0) {
        if (parse_json_string(args_json, "path", file_path, sizeof(file_path)) != 0) {
            *error = strdup("Missing 'file_path' or 'path' parameter");
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    // Extract content (same as file_write)
    const char* content_key = "\"content\":\"";
    const char* content_start = strstr(args_json, content_key);
    if (!content_start) {
        content_key = "\"content\": \"";
        content_start = strstr(args_json, content_key);
        if (!content_start) {
            *error = strdup("Missing 'content' parameter");
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    content_start += strlen(content_key);
    
    // Find the closing quote
    const char* content_end = content_start;
    while (*content_end) {
        if (*content_end == '\\') {
            content_end++;
            if (*content_end) content_end++;
        } else if (*content_end == '"') {
            break;
        } else {
            content_end++;
        }
    }
    
    if (*content_end != '"' || content_end <= content_start) {
        *error = strdup("Invalid content format - missing closing quote");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    size_t content_len = content_end - content_start;
    char* content = malloc(content_len + 1);
    if (!content) {
        *error = strdup("Memory allocation failed for content");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    strncpy(content, content_start, content_len);
    content[content_len] = '\0';
    
    // Unescape JSON
    char* unescaped = malloc(content_len + 1);
    if (!unescaped) {
        free(content);
        *error = strdup("Memory allocation failed for unescaped content");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    size_t out_pos = 0;
    for (size_t i = 0; i < content_len; i++) {
        if (content[i] == '\\' && i + 1 < content_len) {
            switch (content[i + 1]) {
                case 'n':
                    unescaped[out_pos++] = '\n';
                    i++;
                    break;
                case 'r':
                    unescaped[out_pos++] = '\r';
                    i++;
                    break;
                case 't':
                    unescaped[out_pos++] = '\t';
                    i++;
                    break;
                case '"':
                    unescaped[out_pos++] = '"';
                    i++;
                    break;
                case '\\':
                    unescaped[out_pos++] = '\\';
                    i++;
                    break;
                default:
                    unescaped[out_pos++] = content[i];
            }
        } else {
            unescaped[out_pos++] = content[i];
        }
    }
    unescaped[out_pos] = '\0';
    
    // Append to file (open in append mode)
    FILE* f = fopen(file_path, "a");
    if (!f) {
        free(content);
        free(unescaped);
        *error = strdup("Failed to open file for appending (check permissions)");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    fprintf(f, "%s", unescaped);
    fclose(f);
    
    free(content);
    free(unescaped);
    
    char* res = malloc(64);
    snprintf(res, 64, "{\"success\":true}");
    *result = res;
    
    return ETHERVOX_SUCCESS;
}

// Tool wrapper: path_list
static int tool_path_list_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    (void)args_json;  // No parameters needed
    
    ethervox_path_config_t* config = g_path_config;
    if (!config) {
        *error = strdup("Path configuration not initialized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ethervox_user_path_t* paths = NULL;
    uint32_t count = 0;
    
    ethervox_result_t list_result = ethervox_path_config_list(config, &paths, &count);
    if (ethervox_is_error(list_result)) {
        *error = strdup(ethervox_error_string(list_result));
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build JSON response
    size_t res_len = count * 512 + 256;
    char* res = malloc(res_len);
    int pos = snprintf(res, res_len, "{\"paths\":[");
    
    for (uint32_t i = 0; i < count; i++) {
        pos += snprintf(res + pos, res_len - pos,
                       "%s{\"label\":\"%s\",\"path\":\"%s\",\"description\":\"%s\",\"verified\":%s}",
                       i > 0 ? "," : "",
                       paths[i].label,
                       paths[i].path,
                       paths[i].description,
                       paths[i].verified ? "true" : "false");
    }
    
    snprintf(res + pos, res_len - pos, "],\"count\":%u}", count);
    
    free(paths);
    *result = res;
    return ETHERVOX_SUCCESS;
}

// Tool wrapper: path_get
static int tool_path_get_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_path_config_t* config = g_path_config;
    if (!config) {
        *error = strdup("Path configuration not initialized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char label[64];
    if (parse_json_string(args_json, "label", label, sizeof(label)) != 0) {
        *error = strdup("Missing 'label' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char path[ETHERVOX_FILE_MAX_PATH];
    ethervox_result_t ret = ethervox_path_config_get(config, label, path, sizeof(path));
    
    if (ethervox_is_success(ret)) {
        size_t res_len = strlen(path) + 128;
        char* res = malloc(res_len);
        snprintf(res, res_len, "{\"label\":\"%s\",\"path\":\"%s\",\"status\":\"verified\"}", label, path);
        *result = res;
        return ETHERVOX_SUCCESS;
    } else {
        size_t err_len = strlen(label) + 128;
        char* err = malloc(err_len);
        snprintf(err, err_len, "Path label '%s' not found or not accessible: %s", label, ethervox_error_string(ret));
        *error = err;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
}

// Tool wrapper: path_set
static int tool_path_set_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_path_config_t* config = g_path_config;
    if (!config) {
        *error = strdup("Path configuration not initialized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char label[64];
    char path[ETHERVOX_FILE_MAX_PATH];
    char description[256] = {0};
    
    if (parse_json_string(args_json, "label", label, sizeof(label)) != 0) {
        *error = strdup("Missing 'label' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (parse_json_string(args_json, "path", path, sizeof(path)) != 0) {
        *error = strdup("Missing 'path' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Description is optional
    parse_json_string(args_json, "description", description, sizeof(description));
    
    ethervox_result_t ret = ethervox_path_config_set(config, label, path,
                                        description[0] ? description : NULL);
    
    if (ethervox_is_success(ret)) {
        size_t res_len = strlen(label) + strlen(path) + 128;
        char* res = malloc(res_len);
        snprintf(res, res_len,
                "{\"label\":\"%s\",\"path\":\"%s\",\"status\":\"configured\",\"message\":\"Path successfully configured and will be remembered across sessions\"}",
                label, path);
        *result = res;
        return ETHERVOX_SUCCESS;
    } else {
        size_t err_len = strlen(path) + strlen(ethervox_error_string(ret)) + 128;
        char* err = malloc(err_len);
        snprintf(err, err_len,
                "Failed to set path: %s", ethervox_error_string(ret));
        *error = err;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
}

// Tool wrapper: path_check_unverified
static int tool_path_check_unverified_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    (void)args_json;  // No parameters needed
    
    ethervox_path_config_t* config = g_path_config;
    if (!config) {
        *error = strdup("Path configuration not initialized");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ethervox_user_path_t* paths = NULL;
    uint32_t count = 0;
    
    ethervox_result_t unver_result = ethervox_path_config_get_unverified(config, &paths, &count);
    if (ethervox_is_error(unver_result)) {
        *error = strdup(ethervox_error_string(unver_result));
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (count == 0) {
        *result = strdup("{\"unverified\":[],\"count\":0,\"message\":\"All configured paths are verified and accessible\"}");
        return ETHERVOX_SUCCESS;
    }
    
    // Build JSON response with suggestions
    size_t res_len = count * 512 + 512;
    char* res = malloc(res_len);
    int pos = snprintf(res, res_len,
                      "{\"unverified\":[");
    
    for (uint32_t i = 0; i < count; i++) {
        pos += snprintf(res + pos, res_len - pos,
                       "%s{\"label\":\"%s\",\"expected_path\":\"%s\",\"description\":\"%s\"}",
                       i > 0 ? "," : "",
                       paths[i].label,
                       paths[i].path,
                       paths[i].description);
    }
    
    snprintf(res + pos, res_len - pos,
            "],\"count\":%u,\"message\":\"Some default paths don't exist. Consider asking the user for the correct locations using path_set.\"}",
            count);
    
    free(paths);
    *result = res;
    return ETHERVOX_SUCCESS;
}

// Tool wrapper: file_set_safe_mode
static int tool_file_set_safe_mode_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_file_tools_config_t* config = g_file_config;
    if (!config) {
        *error = strdup("File tools not configured");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    bool enable = false;
    if (parse_json_bool(args_json, "enable", &enable) != 0) {
        *error = strdup("Missing 'enable' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Set access mode
    ethervox_file_access_mode_t old_mode = config->access_mode;
    config->access_mode = enable ? ETHERVOX_FILE_ACCESS_READ_ONLY : ETHERVOX_FILE_ACCESS_READ_WRITE;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "File access mode changed: %s -> %s (LLM self-restriction)",
                old_mode == ETHERVOX_FILE_ACCESS_READ_ONLY ? "READ_ONLY" : "READ_WRITE",
                config->access_mode == ETHERVOX_FILE_ACCESS_READ_ONLY ? "READ_ONLY" : "READ_WRITE");
    
    // Build response
    char res[512];
    snprintf(res, sizeof(res),
            "{\"safe_mode\":%s,\"access_mode\":\"%s\",\"message\":\"File access is now %s. %s\"}",
            enable ? "true" : "false",
            enable ? "read_only" : "read_write",
            enable ? "restricted to read-only (safe mode)" : "read-write enabled",
            enable ? "I can explore and read files but cannot modify them." : 
                     "I can now write and modify files when needed.");
    
    *result = strdup(res);
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_file_tools_register(
    void* registry_ptr,
    ethervox_file_tools_config_t* config
) {
    ethervox_tool_registry_t* registry = (ethervox_tool_registry_t*)registry_ptr;
    if (!registry || !config) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Set global config for tool wrappers
    g_file_config = config;
    
    int ret = 0;
    
    // Register file_list tool
    ethervox_tool_t tool_list = {
        .name = "file_list",
        .description = "List files and directories in a given path. Use '.' for current directory, '..' for parent, or provide an absolute path.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"directory\":{\"type\":\"string\",\"description\":\"Directory path to list (use '.' for current directory)\"},"
            "\"path\":{\"type\":\"string\",\"description\":\"Directory path to list (alternative to 'directory')\"},"
            "\"recursive\":{\"type\":\"boolean\",\"description\":\"Recurse into subdirectories\"}"
            "}}",
        .execute = tool_file_list_wrapper,
        .is_deterministic = true,
        .requires_confirmation = false,
        .is_stateful = false,
        .estimated_latency_ms = 50.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_list);
    
    // Register file_read tool
    ethervox_tool_t tool_read = {
        .name = "file_read",
        .description = "Read contents of a text based file (.txt, .md, .org, .c, etc.). Maximum 10MB.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"path\":{\"type\":\"string\",\"description\":\"Path to file to read\"},"
            "\"file_path\":{\"type\":\"string\",\"description\":\"Path to file to read (alternative to 'path')\"}"
            "}}",
        .execute = tool_file_read_wrapper,
        .is_deterministic = true,
        .requires_confirmation = false,
        .is_stateful = false,
        .estimated_latency_ms = 100.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_read);
    
    // Register file_search tool
    ethervox_tool_t tool_search = {
        .name = "file_search",
        .description = "Search for text pattern in all allowed files within a directory",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"directory\":{\"type\":\"string\",\"description\":\"Directory to search in\"},"
            "\"pattern\":{\"type\":\"string\",\"description\":\"Text pattern to search for\"}"
            "},\"required\":[\"directory\",\"pattern\"]}",
        .execute = tool_file_search_wrapper,
        .is_deterministic = true,
        .requires_confirmation = false,
        .is_stateful = false,
        .estimated_latency_ms = 500.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_search);
    
    // Register file_write tool (only if write access enabled)
    if (config->access_mode == ETHERVOX_FILE_ACCESS_READ_WRITE) {
        ethervox_tool_t tool_write = {
            .name = "file_write",
            .description = "Create or overwrite a file with specified content. Useful for saving notes, documentation, or generated text. Always provide both file_path and content.",
            .parameters_json_schema =
                "{\"type\":\"object\",\"properties\":{"
                "\"file_path\":{\"type\":\"string\",\"description\":\"Path to file to write. Use relative paths like './notes.md' or './output.txt'. File must have an allowed extension (.txt, .md, .org, .c, .cpp, .h, .sh)\"},"
                "\"content\":{\"type\":\"string\",\"description\":\"The complete text content to write to the file. Can be markdown, code, plain text, or any supported format.\"}"
                "},\"required\":[\"file_path\",\"content\"]}",
            .execute = tool_file_write_wrapper,
            .is_deterministic = false,
            .requires_confirmation = true,
            .is_stateful = true,
            .estimated_latency_ms = 100.0f
        };
        
        ret |= ethervox_tool_registry_add(registry, &tool_write);
        
        // Register file_append tool
        ethervox_tool_t tool_append = {
            .name = "file_append",
            .description = "Append content to the end of an existing file. Useful for adding to notes, logs, or summaries without overwriting. Always provide both path and content.",
            .parameters_json_schema =
                "{\"type\":\"object\",\"properties\":{"
                "\"path\":{\"type\":\"string\",\"description\":\"Path to file to append to\"},"
                "\"file_path\":{\"type\":\"string\",\"description\":\"Path to file to append to (alternative to 'path')\"},"
                "\"content\":{\"type\":\"string\",\"description\":\"The text content to append to the file. Can include newlines.\"}"
                "},\"required\":[\"content\"]}",
            .execute = tool_file_append_wrapper,
            .is_deterministic = false,
            .requires_confirmation = false,
            .is_stateful = true,
            .estimated_latency_ms = 100.0f
        };
        
        ret |= ethervox_tool_registry_add(registry, &tool_append);
        
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Registered 5 file tools (including write access)");
    } else {
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Registered 3 file tools (read-only mode)");
    }
    
    return ret;
}

ethervox_result_t ethervox_path_config_register(
    void* registry_ptr,
    ethervox_path_config_t* config
) {
    ethervox_tool_registry_t* registry = (ethervox_tool_registry_t*)registry_ptr;
    if (!registry || !config) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Set global config for tool wrappers
    g_path_config = config;
    
    int ret = 0;
    
    // Register path_list tool
    ethervox_tool_t tool_path_list = {
        .name = "path_list",
        .description = "List all configured user paths (Documents, Notes, etc.). Shows which paths are verified and accessible. Use this to discover where the user keeps important files.",
        .parameters_json_schema = "{\"type\":\"object\",\"properties\":{}}",
        .execute = tool_path_list_wrapper,
        .is_deterministic = true,
        .requires_confirmation = false,
        .is_stateful = false,
        .estimated_latency_ms = 10.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_path_list);
    
    // Register path_get tool
    ethervox_tool_t tool_path_get = {
        .name = "path_get",
        .description = "Get the absolute path for a specific label (e.g., 'Notes', 'Documents', 'Downloads'). Use this to get the exact path before reading or listing files.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"label\":{\"type\":\"string\",\"description\":\"Path label to retrieve (e.g., 'Notes', 'Documents')\"}"
            "},\"required\":[\"label\"]}",
        .execute = tool_path_get_wrapper,
        .is_deterministic = true,
        .requires_confirmation = false,
        .is_stateful = false,
        .estimated_latency_ms = 5.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_path_get);
    
    // Register path_set tool
    ethervox_tool_t tool_path_set = {
        .name = "path_set",
        .description = "Configure or update a user path. Use this to remember important directories the user mentions. Paths are persisted across sessions. Ask the user for the actual path if defaults don't exist.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"label\":{\"type\":\"string\",\"description\":\"Human-friendly label (e.g., 'Notes', 'Projects', 'Documents')\"},"
            "\"path\":{\"type\":\"string\",\"description\":\"Absolute directory path\"},"
            "\"description\":{\"type\":\"string\",\"description\":\"Optional description of what this path contains\"}"
            "},\"required\":[\"label\",\"path\"]}",
        .execute = tool_path_set_wrapper,
        .is_deterministic = false,
        .requires_confirmation = false,
        .is_stateful = true,
        .estimated_latency_ms = 20.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_path_set);
    
    // Register path_check_unverified tool
    ethervox_tool_t tool_path_check = {
        .name = "path_check_unverified",
        .description = "Check for unverified paths (default paths that don't exist on this system). Use this to discover which paths need configuration and proactively ask the user for the correct locations.",
        .parameters_json_schema = "{\"type\":\"object\",\"properties\":{}}",
        .execute = tool_path_check_unverified_wrapper,
        .is_deterministic = true,
        .requires_confirmation = false,
        .is_stateful = false,
        .estimated_latency_ms = 10.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_path_check);
    
    // Register file_set_safe_mode tool (allows LLM to restrict itself)
    ethervox_tool_t tool_safe_mode = {
        .name = "file_set_safe_mode",
        .description = "Enable or disable safe mode to restrict file write access. Use this like 'plan mode' - enable safe mode before exploring user files, disable only when user explicitly asks to write/modify files. Returns current mode status.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"enable\":{\"type\":\"boolean\",\"description\":\"true to enable read-only safe mode, false to allow writes\"}"
            "},\"required\":[\"enable\"]}",
        .execute = tool_file_set_safe_mode_wrapper,
        .is_deterministic = false,
        .requires_confirmation = false,
        .is_stateful = true,
        .estimated_latency_ms = 5.0f
    };
    
    ret |= ethervox_tool_registry_add(registry, &tool_safe_mode);
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Registered 4 path configuration tools and 1 permission control tool");
    
    return ret;
}
