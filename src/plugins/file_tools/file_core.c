/**
#include "ethervox/error.h"
 * @file file_core.c
 * @brief Core file system access implementation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/file_tools.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <stdlib.h>  // For _fullpath
#define PATH_SEPARATOR '\\'
// Windows doesn't have dirent.h, but we can use FindFirstFile for directory operations
#include <io.h>
#else
#include <dirent.h>
#include <libgen.h>
#include <unistd.h>
#define PATH_SEPARATOR '/'
#endif

#ifdef _WIN32
// Simple dirname implementation for Windows
static char* win_dirname(char* path) {
    static char dir[PATH_MAX];
    strncpy(dir, path, PATH_MAX - 1);
    dir[PATH_MAX - 1] = '\0';
    char* last_slash = strrchr(dir, '\\');
    if (!last_slash) last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        strcpy(dir, ".");
    }
    return dir;
}
#define dirname win_dirname
#endif

// Check if path is within allowed base paths
static bool is_path_allowed(
    ethervox_file_tools_config_t* config,
    const char* path
) {
    if (!config || !path) {
        return false;
    }
    
    // Resolve to absolute path
    char resolved[PATH_MAX];
#ifdef _WIN32
    if (!_fullpath(resolved, path, PATH_MAX)) {
#else
    if (!realpath(path, resolved)) {
#endif
        // If realpath fails (e.g., file doesn't exist yet), try resolving the directory
        char path_copy[PATH_MAX];
        strncpy(path_copy, path, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';
        
        char* dir = dirname(path_copy);
        if (!dir) return false;
        
#ifdef _WIN32
        if (!_fullpath(resolved, dir, PATH_MAX)) {
#else
        if (!realpath(dir, resolved)) {
#endif
            ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                        "Cannot resolve path: %s", path);
            return false;
        }
        
        // Append the filename back
        const char* filename = strrchr(path, '/');
        if (!filename) filename = strrchr(path, '\\');
        if (filename) {
            strncat(resolved, filename, sizeof(resolved) - strlen(resolved) - 1);
        } else {
            // No directory separator, just append the whole path
            strncat(resolved, "/", sizeof(resolved) - strlen(resolved) - 1);
            strncat(resolved, path, sizeof(resolved) - strlen(resolved) - 1);
        }
    }
    
    // Check against each allowed base path
    for (uint32_t i = 0; i < config->allowed_base_path_count; i++) {
        size_t base_len = strlen(config->allowed_base_paths[i]);
        if (strncmp(resolved, config->allowed_base_paths[i], base_len) == 0) {
            return true;
        }
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                "Path access denied (not in allowed base paths): %s (resolved: %s)", path, resolved);
    
    return false;
}

// Check if file extension is allowed
static bool is_extension_allowed(
    ethervox_file_tools_config_t* config,
    const char* filename
) {
    if (!config || !filename) {
        return false;
    }
    
    // Find extension
    const char* ext = strrchr(filename, '.');
    if (!ext) {
        return false;  // No extension
    }
    
    // Check against filters
    for (uint32_t i = 0; i < config->file_filter_count; i++) {
        if (config->file_filters[i].enabled &&
            strcmp(ext, config->file_filters[i].extension) == 0) {
            return true;
        }
    }
    
    return false;
}

ethervox_result_t ethervox_file_tools_init(
    ethervox_file_tools_config_t* config,
    const char* base_paths[],
    ethervox_file_access_mode_t access_mode
) {
    if (!config) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    memset(config, 0, sizeof(ethervox_file_tools_config_t));
    
    // Add base paths
    config->allowed_base_path_count = 0;
    if (base_paths) {
        for (int i = 0; base_paths[i] && i < 8; i++) {
            // Resolve to absolute path
#ifdef _WIN32
            if (_fullpath(config->allowed_base_paths[config->allowed_base_path_count], base_paths[i], PATH_MAX)) {
#else
            if (realpath(base_paths[i], config->allowed_base_paths[config->allowed_base_path_count])) {
#endif
                config->allowed_base_path_count++;
            } else {
                ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                            "Could not resolve base path: %s", base_paths[i]);
            }
        }
    }
    
    config->access_mode = access_mode;
    config->follow_symlinks = false;
    config->allow_hidden_files = false;
    config->max_file_size = ETHERVOX_FILE_MAX_SIZE;
    config->is_initialized = true;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Initialized file tools: %u base paths, access=%s",
                config->allowed_base_path_count,
                access_mode == ETHERVOX_FILE_ACCESS_READ_ONLY ? "read-only" : "read-write");
    
    return ETHERVOX_SUCCESS;
}

void ethervox_file_tools_cleanup(ethervox_file_tools_config_t* config) {
    if (!config || !config->is_initialized) {
        return;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Cleaned up file tools");
    
    config->is_initialized = false;
}

ethervox_result_t ethervox_file_tools_add_filter(
    ethervox_file_tools_config_t* config,
    const char* extension
) {
    if (!config || !config->is_initialized || !extension) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (config->file_filter_count >= 16) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ethervox_file_filter_t* filter = &config->file_filters[config->file_filter_count];
    snprintf(filter->extension, sizeof(filter->extension), "%s", extension);
    filter->enabled = true;
    config->file_filter_count++;
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_file_list(
    ethervox_file_tools_config_t* config,
    const char* directory_path,
    bool recursive,
    ethervox_file_entry_t** entries,
    uint32_t* entry_count
) {
    if (!config || !config->is_initialized || !directory_path || !entries || !entry_count) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check if path is allowed
    if (!is_path_allowed(config, directory_path)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Allocate entries array
    ethervox_file_entry_t* temp_entries = malloc(ETHERVOX_FILE_MAX_ENTRIES * sizeof(ethervox_file_entry_t));
    if (!temp_entries) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint32_t count = 0;
    
#ifdef _WIN32
    // Windows implementation using FindFirstFile
    WIN32_FIND_DATAA find_data;
    char search_path[ETHERVOX_FILE_MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", directory_path);
    
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        free(temp_entries);
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to open directory: %s", directory_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    do {
        // Skip . and ..
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        // Skip hidden files if not allowed
        if (!config->allow_hidden_files && find_data.cFileName[0] == '.') {
            continue;
        }
        
        // Build full path
        char full_path[ETHERVOX_FILE_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", directory_path, find_data.cFileName);
        
        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }
        
        bool is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        
        // Filter files by extension
        if (!is_dir && !is_extension_allowed(config, find_data.cFileName)) {
            continue;
        }
        
        // Add entry
        ethervox_file_entry_t* file_entry = &temp_entries[count];
        snprintf(file_entry->path, sizeof(file_entry->path), "%s", full_path);
        snprintf(file_entry->name, sizeof(file_entry->name), "%s", find_data.cFileName);
        file_entry->is_directory = is_dir;
        file_entry->size = st.st_size;
        file_entry->modified_time = st.st_mtime;
        count++;
        
        // Recurse into subdirectories
        if (recursive && is_dir) {
            ethervox_file_entry_t* sub_entries = NULL;
            uint32_t sub_count = 0;
            
            if (ethervox_file_list(config, full_path, recursive, &sub_entries, &sub_count) == 0) {
                // Append sub-entries
                for (uint32_t i = 0; i < sub_count && count < ETHERVOX_FILE_MAX_ENTRIES; i++) {
                    temp_entries[count++] = sub_entries[i];
                }
                free(sub_entries);
            }
        }
        
        if (count >= ETHERVOX_FILE_MAX_ENTRIES) break;
    } while (FindNextFileA(hFind, &find_data));
    
    FindClose(hFind);
#else
    // Unix implementation using readdir
    DIR* dir = opendir(directory_path);
    if (!dir) {
        free(temp_entries);
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to open directory: %s", directory_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && count < ETHERVOX_FILE_MAX_ENTRIES) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Skip hidden files if not allowed
        if (!config->allow_hidden_files && entry->d_name[0] == '.') {
            continue;
        }
        
        // Build full path
        char full_path[ETHERVOX_FILE_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s%c%s", directory_path, PATH_SEPARATOR, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }
        
        bool is_dir = S_ISDIR(st.st_mode);
        
        // Filter files by extension
        if (!is_dir && !is_extension_allowed(config, entry->d_name)) {
            continue;
        }
        
        // Add entry
        ethervox_file_entry_t* file_entry = &temp_entries[count];
        snprintf(file_entry->path, sizeof(file_entry->path), "%s", full_path);
        snprintf(file_entry->name, sizeof(file_entry->name), "%s", entry->d_name);
        file_entry->is_directory = is_dir;
        file_entry->size = st.st_size;
        file_entry->modified_time = st.st_mtime;
        count++;
        
        // Recurse into subdirectories
        if (recursive && is_dir) {
            ethervox_file_entry_t* sub_entries = NULL;
            uint32_t sub_count = 0;
            
            if (ethervox_file_list(config, full_path, recursive, &sub_entries, &sub_count) == 0) {
                // Append sub-entries
                for (uint32_t i = 0; i < sub_count && count < ETHERVOX_FILE_MAX_ENTRIES; i++) {
                    temp_entries[count++] = sub_entries[i];
                }
                free(sub_entries);
            }
        }
    }
    
    closedir(dir);
#endif
    
    *entries = temp_entries;
    *entry_count = count;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "Listed %u entries in %s", count, directory_path);
    
    return ETHERVOX_SUCCESS;
}

/**
 * Detect if file content is binary (non-text)
 * Checks for null bytes and high proportion of control characters
 */
static bool is_binary_content(const char* buffer, size_t size) {
    if (size == 0) return false;
    
    size_t control_chars = 0;
    size_t null_bytes = 0;
    size_t check_size = size < 8192 ? size : 8192;  // Check first 8KB
    
    for (size_t i = 0; i < check_size; i++) {
        unsigned char c = (unsigned char)buffer[i];
        
        // Null byte = definitely binary
        if (c == 0) {
            null_bytes++;
            if (null_bytes > 0) return true;  // Even one null byte = binary
        }
        
        // Count non-printable control chars (exclude common text chars like \n, \r, \t)
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            control_chars++;
        }
    }
    
    // If > 30% control characters, likely binary
    float control_ratio = (float)control_chars / check_size;
    return control_ratio > 0.3f;
}

ethervox_result_t ethervox_file_read(
    ethervox_file_tools_config_t* config,
    const char* file_path,
    char** content,
    uint64_t* size
) {
    if (!config || !config->is_initialized || !file_path || !content || !size) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check if path is allowed
    if (!is_path_allowed(config, file_path)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check extension
    if (!is_extension_allowed(config, file_path)) {
        ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                    "File extension not allowed: %s", file_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Get file size
    struct stat st;
    if (stat(file_path, &st) != 0) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to stat file: %s", file_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if ((uint64_t)st.st_size > config->max_file_size) {
        ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                    "File too large: %s (%llu bytes > %llu max)",
                    file_path, (unsigned long long)st.st_size, (unsigned long long)config->max_file_size);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Open file
    FILE* fp = fopen(file_path, "rb");
    if (!fp) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to open file: %s", file_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Allocate buffer
    char* buffer = malloc(st.st_size + 1);
    if (!buffer) {
        fclose(fp);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Read file
    size_t bytes_read = fread(buffer, 1, st.st_size, fp);
    fclose(fp);
    
    if (bytes_read != (size_t)st.st_size) {
        free(buffer);
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to read complete file: %s", file_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    buffer[bytes_read] = '\0';  // Null-terminate
    
    // Check for binary content
    if (is_binary_content(buffer, bytes_read)) {
        free(buffer);
        ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                    "Binary content detected in file: %s", file_path);
        return -2;  // Special error code for binary files
    }
    
    *content = buffer;
    *size = bytes_read;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "Read %llu bytes from %s", (unsigned long long)bytes_read, file_path);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_file_search(
    ethervox_file_tools_config_t* config,
    const char* directory_path,
    const char* pattern,
    char*** results,
    uint32_t* result_count
) {
    if (!config || !config->is_initialized || !directory_path || !pattern || !results || !result_count) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // List all files
    ethervox_file_entry_t* entries = NULL;
    uint32_t entry_count = 0;
    
    if (ethervox_file_list(config, directory_path, true, &entries, &entry_count) != 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Allocate results array
    char** matches = malloc(entry_count * sizeof(char*));
    if (!matches) {
        free(entries);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint32_t match_count = 0;
    
    // Search each file
    for (uint32_t i = 0; i < entry_count; i++) {
        if (entries[i].is_directory) {
            continue;
        }
        
        // Read file
        char* content = NULL;
        uint64_t size = 0;
        
        if (ethervox_file_read(config, entries[i].path, &content, &size) == 0) {
            // Search for pattern
            if (strstr(content, pattern) != NULL) {
                matches[match_count] = strdup(entries[i].path);
                match_count++;
            }
            free(content);
        }
    }
    
    free(entries);
    
    *results = matches;
    *result_count = match_count;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "Found %u matches for '%s' in %s", match_count, pattern, directory_path);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_file_write(
    ethervox_file_tools_config_t* config,
    const char* file_path,
    const char* content
) {
    if (!config || !config->is_initialized || !file_path || !content) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check write access
    if (config->access_mode != ETHERVOX_FILE_ACCESS_READ_WRITE) {
        ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                    "Write access denied (read-only mode)");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check if path is allowed
    if (!is_path_allowed(config, file_path)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check extension
    if (!is_extension_allowed(config, file_path)) {
        ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                    "File extension not allowed: %s", file_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Write file
    FILE* fp = fopen(file_path, "w");
    if (!fp) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to open file for writing: %s", file_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    fclose(fp);
    
    if (written != len) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to write complete file: %s", file_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Wrote %zu bytes to %s", written, file_path);
    
    return ETHERVOX_SUCCESS;
}
