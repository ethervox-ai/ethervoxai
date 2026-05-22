/**
 * @file path_config.c
 * @brief User-specific path configuration and management
 *
 * Allows LLM to learn and remember user's important directories (Notes, Documents, etc.)
 * Paths are stored in memory with special tags and persisted across sessions.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/file_tools.h"
#include "ethervox/memory_tools.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define PATH_SEPARATOR '\\'
#else
#include <unistd.h>
#include <pwd.h>
#define PATH_SEPARATOR '/'
#endif

// Memory tag for path configurations
#define PATH_CONFIG_TAG "user_path"
#define PATH_CONFIG_PREFIX "USER_PATH:"

/**
 * Get platform-specific default paths
 */
static void get_default_paths(ethervox_user_path_t* paths, uint32_t* count) {
    if (!paths || !count) return;
    
    *count = 0;
    
#ifdef _WIN32
    // Windows default paths
    char* userprofile = getenv("USERPROFILE");
    if (userprofile) {
        snprintf(paths[(*count)].path, ETHERVOX_FILE_MAX_PATH, "%s\\Documents", userprofile);
        snprintf(paths[(*count)].label, 64, "Documents");
        snprintf(paths[(*count)].description, 256, "User documents folder");
        (*count)++;
        
        snprintf(paths[(*count)].path, ETHERVOX_FILE_MAX_PATH, "%s\\Downloads", userprofile);
        snprintf(paths[(*count)].label, 64, "Downloads");
        snprintf(paths[(*count)].description, 256, "Downloaded files");
        (*count)++;
        
        snprintf(paths[(*count)].path, ETHERVOX_FILE_MAX_PATH, "%s\\Desktop", userprofile);
        snprintf(paths[(*count)].label, 64, "Desktop");
        snprintf(paths[(*count)].description, 256, "Desktop folder");
        (*count)++;
    }
#else
    // Unix-like default paths
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    
    if (home) {
        snprintf(paths[(*count)].path, ETHERVOX_FILE_MAX_PATH, "%s/Documents", home);
        snprintf(paths[(*count)].label, 64, "Documents");
        snprintf(paths[(*count)].description, 256, "User documents folder");
        (*count)++;
        
        snprintf(paths[(*count)].path, ETHERVOX_FILE_MAX_PATH, "%s/Downloads", home);
        snprintf(paths[(*count)].label, 64, "Downloads");
        snprintf(paths[(*count)].description, 256, "Downloaded files");
        (*count)++;
        
        snprintf(paths[(*count)].path, ETHERVOX_FILE_MAX_PATH, "%s/Desktop", home);
        snprintf(paths[(*count)].label, 64, "Desktop");
        snprintf(paths[(*count)].description, 256, "Desktop folder");
        (*count)++;
        
        // Common for Notes apps
        snprintf(paths[(*count)].path, ETHERVOX_FILE_MAX_PATH, "%s/Notes", home);
        snprintf(paths[(*count)].label, 64, "Notes");
        snprintf(paths[(*count)].description, 256, "Personal notes");
        (*count)++;
    }
#endif
}

/**
 * Check if path exists and is accessible
 */
static bool validate_path(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

/**
 * Initialize path configuration system
 */
ethervox_result_t ethervox_path_config_init(
    ethervox_path_config_t* config,
    void* memory
) {
    ETHERVOX_CHECK_PTR(config);
    
    memset(config, 0, sizeof(ethervox_path_config_t));
    config->memory = memory;
    
    // Load default paths
    get_default_paths(config->paths, &config->path_count);
    
    // Validate and filter existing paths
    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < config->path_count; i++) {
        if (validate_path(config->paths[i].path)) {
            config->paths[i].verified = true;
            valid_count++;
        } else {
            config->paths[i].verified = false;
        }
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Path config initialized with %u paths (%u verified)",
                config->path_count, valid_count);
    
    // Load saved paths from memory if available
    if (memory) {
        ethervox_memory_store_t* mem_store = (ethervox_memory_store_t*)memory;
        if (mem_store->is_initialized) {
            const char* tags[] = {PATH_CONFIG_TAG};
            ethervox_memory_search_result_t* results = NULL;
            uint32_t result_count = 0;
            
            if (ethervox_memory_search(mem_store, NULL, tags, 1, 100, &results, &result_count) == 0) {
                for (uint32_t i = 0; i < result_count && config->path_count < ETHERVOX_MAX_USER_PATHS; i++) {
                    const char* text = results[i].entry.text;
                    
                    // Parse format: "USER_PATH:label|description|/path/to/dir"
                    if (strncmp(text, PATH_CONFIG_PREFIX, strlen(PATH_CONFIG_PREFIX)) == 0) {
                        const char* data = text + strlen(PATH_CONFIG_PREFIX);
                        
                        char label[64] = {0};
                        char description[256] = {0};
                        char path[ETHERVOX_FILE_MAX_PATH] = {0};
                        
                        const char* pipe1 = strchr(data, '|');
                        const char* pipe2 = pipe1 ? strchr(pipe1 + 1, '|') : NULL;
                        
                        if (pipe1 && pipe2) {
                            size_t label_len = pipe1 - data;
                            size_t desc_len = pipe2 - (pipe1 + 1);
                            
                            strncpy(label, data, label_len < 63 ? label_len : 63);
                            strncpy(description, pipe1 + 1, desc_len < 255 ? desc_len : 255);
                            strncpy(path, pipe2 + 1, ETHERVOX_FILE_MAX_PATH - 1);
                            
                            // Check if this path already exists
                            bool exists = false;
                            for (uint32_t j = 0; j < config->path_count; j++) {
                                if (strcmp(config->paths[j].path, path) == 0) {
                                    exists = true;
                                    break;
                                }
                            }
                            
                            if (!exists) {
                                strncpy(config->paths[config->path_count].label, label, 63);
                                strncpy(config->paths[config->path_count].description, description, 255);
                                strncpy(config->paths[config->path_count].path, path, ETHERVOX_FILE_MAX_PATH - 1);
                                config->paths[config->path_count].verified = validate_path(path);
                                config->paths[config->path_count].memory_id = results[i].entry.memory_id;
                                config->path_count++;
                                
                                ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                                            "Loaded saved path: %s -> %s", label, path);
                            }
                        }
                    }
                }
                free(results);
            }
        }
    }
    
    config->is_initialized = true;
    return ETHERVOX_SUCCESS;
}

/**
 * Set or update a user path
 */
ethervox_result_t ethervox_path_config_set(
    ethervox_path_config_t* config,
    const char* label,
    const char* path,
    const char* description
) {
    if (!config || !config->is_initialized) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Path config not initialized");
    }
    
    ETHERVOX_CHECK_PTR(label);
    ETHERVOX_CHECK_PTR(path);
    
    // Validate path exists
    if (!validate_path(path)) {
        ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                    "Path does not exist or is not accessible: %s", path);
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_PATH_INVALID, "Path does not exist or is not accessible");
    }
    
    // Check if label already exists
    int32_t existing_idx = -1;
    for (uint32_t i = 0; i < config->path_count; i++) {
        if (strcmp(config->paths[i].label, label) == 0) {
            existing_idx = i;
            break;
        }
    }
    
    if (existing_idx >= 0) {
        // Update existing path
        strncpy(config->paths[existing_idx].path, path, ETHERVOX_FILE_MAX_PATH - 1);
        if (description) {
            strncpy(config->paths[existing_idx].description, description, 255);
        }
        config->paths[existing_idx].verified = true;
        
        // Update in memory if it was previously saved
        if (config->memory && config->paths[existing_idx].memory_id > 0) {
            ethervox_memory_store_t* mem_store = (ethervox_memory_store_t*)config->memory;
            char text[ETHERVOX_MEMORY_MAX_TEXT_LEN];
            snprintf(text, sizeof(text), "%s%s|%s|%s",
                    PATH_CONFIG_PREFIX, label,
                    config->paths[existing_idx].description, path);
            
            ethervox_memory_update_text(mem_store, config->paths[existing_idx].memory_id, text);
        }
        
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Updated path: %s -> %s", label, path);
    } else {
        // Add new path
        if (config->path_count >= ETHERVOX_MAX_USER_PATHS) {
            ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                        "Maximum user paths reached (%d)", ETHERVOX_MAX_USER_PATHS);
            ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_BUFFER_TOO_SMALL, "Maximum user paths reached");
        }
        
        strncpy(config->paths[config->path_count].label, label, 63);
        strncpy(config->paths[config->path_count].path, path, ETHERVOX_FILE_MAX_PATH - 1);
        strncpy(config->paths[config->path_count].description,
                description ? description : "", 255);
        config->paths[config->path_count].verified = true;
        
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Added new path: %s -> %s", label, path);
        
        existing_idx = config->path_count;
        config->path_count++;
    }
    
    // Save to memory for persistence
    if (config->memory) {
        ethervox_memory_store_t* mem_store = (ethervox_memory_store_t*)config->memory;
        if (mem_store->is_initialized) {
            char text[ETHERVOX_MEMORY_MAX_TEXT_LEN];
            snprintf(text, sizeof(text), "%s%s|%s|%s",
                    PATH_CONFIG_PREFIX, label,
                    config->paths[existing_idx].description, path);
            
            const char* tags[] = {PATH_CONFIG_TAG};
            uint64_t memory_id = 0;
            
            if (ethervox_memory_store_add(mem_store, text, tags, 1, 0.8f, false, &memory_id) == 0) {
                config->paths[existing_idx].memory_id = memory_id;
                ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                            "Saved path to memory: %s (ID: %llu)", label, memory_id);
            }
        }
    }
    
    return ETHERVOX_SUCCESS;
}

/**
 * Get a user path by label
 */
ethervox_result_t ethervox_path_config_get(
    ethervox_path_config_t* config,
    const char* label,
    char* path_out,
    size_t path_size
) {
    if (!config || !config->is_initialized) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Path config not initialized");
    }
    ETHERVOX_CHECK_PTR(label);
    ETHERVOX_CHECK_PTR(path_out);
    
    for (uint32_t i = 0; i < config->path_count; i++) {
        if (strcmp(config->paths[i].label, label) == 0) {
            if (config->paths[i].verified) {
                strncpy(path_out, config->paths[i].path, path_size - 1);
                path_out[path_size - 1] = '\0';
                return ETHERVOX_SUCCESS;
            } else {
                // Path exists in config but not verified
                ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                            "Path exists but is not verified: %s", config->paths[i].path);
                ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_PATH_INVALID, "Path exists but is not verified");
            }
        }
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                "Path label not found: %s", label);
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_FOUND, "Path label not found");
}

/**
 * List all configured paths
 */
ethervox_result_t ethervox_path_config_list(
    ethervox_path_config_t* config,
    ethervox_user_path_t** paths_out,
    uint32_t* count_out
) {
    if (!config || !config->is_initialized) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Path config not initialized");
    }
    ETHERVOX_CHECK_PTR(paths_out);
    ETHERVOX_CHECK_PTR(count_out);
    
    if (config->path_count == 0) {
        *paths_out = NULL;
        *count_out = 0;
        return ETHERVOX_SUCCESS;
    }
    
    // Allocate array copy
    ethervox_user_path_t* paths = malloc(sizeof(ethervox_user_path_t) * config->path_count);
    if (!paths) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to allocate paths array (%u paths)", config->path_count);
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate paths array");
    }
    
    memcpy(paths, config->paths, sizeof(ethervox_user_path_t) * config->path_count);
    
    *paths_out = paths;
    *count_out = config->path_count;
    
    return ETHERVOX_SUCCESS;
}

/**
 * Check if any unverified paths exist and return suggestions
 */
ethervox_result_t ethervox_path_config_get_unverified(
    ethervox_path_config_t* config,
    ethervox_user_path_t** paths_out,
    uint32_t* count_out
) {
    if (!config || !config->is_initialized) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Path config not initialized");
    }
    ETHERVOX_CHECK_PTR(paths_out);
    ETHERVOX_CHECK_PTR(count_out);
    
    // Count unverified
    uint32_t unverified_count = 0;
    for (uint32_t i = 0; i < config->path_count; i++) {
        if (!config->paths[i].verified) {
            unverified_count++;
        }
    }
    
    if (unverified_count == 0) {
        *paths_out = NULL;
        *count_out = 0;
        return ETHERVOX_SUCCESS;
    }
    
    // Allocate and copy unverified paths
    ethervox_user_path_t* paths = malloc(sizeof(ethervox_user_path_t) * unverified_count);
    if (!paths) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to allocate unverified paths array (%u paths)", unverified_count);
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate unverified paths array");
    }
    
    uint32_t idx = 0;
    for (uint32_t i = 0; i < config->path_count; i++) {
        if (!config->paths[i].verified) {
            memcpy(&paths[idx++], &config->paths[i], sizeof(ethervox_user_path_t));
        }
    }
    
    *paths_out = paths;
    *count_out = unverified_count;
    
    return ETHERVOX_SUCCESS;
}

/**
 * Cleanup path configuration
 */
void ethervox_path_config_cleanup(ethervox_path_config_t* config) {
    if (config) {
        memset(config, 0, sizeof(ethervox_path_config_t));
    }
}
