/**
 * @file file_tools.h
 * @brief File system access tools for Governor LLM
 *
 * Provides controlled file system operations for reading documents.
 * Write access is configurable and disabled by default for safety.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_FILE_TOOLS_H
#define ETHERVOX_FILE_TOOLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ETHERVOX_FILE_MAX_PATH 1024
#define ETHERVOX_FILE_MAX_SIZE (10 * 1024 * 1024)  // 10MB max file size
#define ETHERVOX_FILE_MAX_ENTRIES 1000
#define ETHERVOX_MAX_USER_PATHS 32

/**
 * File access permissions
 */
typedef enum {
    ETHERVOX_FILE_ACCESS_READ_ONLY = 0,
    ETHERVOX_FILE_ACCESS_READ_WRITE = 1
} ethervox_file_access_mode_t;

/**
 * User-defined path configuration
 */
typedef struct {
    char label[64];                           // Human-friendly name (e.g., "Notes", "Documents")
    char path[ETHERVOX_FILE_MAX_PATH];       // Absolute path
    char description[256];                    // Purpose/content description
    bool verified;                            // Path exists and is accessible
    uint64_t memory_id;                       // Memory entry ID for persistence
} ethervox_user_path_t;

/**
 * File type filter
 */
typedef struct {
    char extension[16];  // e.g., ".txt", ".md", ".org"
    bool enabled;
} ethervox_file_filter_t;

/**
 * File tools configuration
 */
typedef struct {
    char allowed_base_paths[8][ETHERVOX_FILE_MAX_PATH];  // Restrict to these directories
    uint32_t allowed_base_path_count;
    
    ethervox_file_filter_t file_filters[16];
    uint32_t file_filter_count;
    
    ethervox_file_access_mode_t access_mode;
    
    bool follow_symlinks;
    bool allow_hidden_files;
    uint64_t max_file_size;
    
    bool is_initialized;
} ethervox_file_tools_config_t;

/**
 * Path configuration manager
 */
typedef struct {
    ethervox_user_path_t paths[ETHERVOX_MAX_USER_PATHS];
    uint32_t path_count;
    
    void* memory;  // ethervox_memory_store_t* (to avoid circular dependency)
    
    bool is_initialized;
} ethervox_path_config_t;

/**
 * Directory entry
 */
typedef struct {
    char path[ETHERVOX_FILE_MAX_PATH];
    char name[256];
    bool is_directory;
    uint64_t size;
    time_t modified_time;
} ethervox_file_entry_t;

/**
 * Initialize file tools with configuration
 * 
 * @param config Configuration structure
 * @param base_paths Array of allowed base paths (NULL-terminated)
 * @param access_mode Read-only or read-write
 * @return 0 on success, -1 on error
 */
int ethervox_file_tools_init(
    ethervox_file_tools_config_t* config,
    const char* base_paths[],
    ethervox_file_access_mode_t access_mode
);

/**
 * Cleanup file tools
 * 
 * @param config Configuration to cleanup
 */
void ethervox_file_tools_cleanup(ethervox_file_tools_config_t* config);

/**
 * Add file extension filter
 * 
 * @param config Configuration
 * @param extension File extension (e.g., ".txt", ".md", ".org")
 * @return 0 on success, -1 on error
 */
int ethervox_file_tools_add_filter(
    ethervox_file_tools_config_t* config,
    const char* extension
);

/**
 * TOOL: file_list - List files in directory
 * 
 * @param config File tools config
 * @param directory_path Directory to list
 * @param recursive Recurse into subdirectories
 * @param entries Output: array of directory entries (caller must free)
 * @param entry_count Output: number of entries
 * @return 0 on success, negative on error
 */
int ethervox_file_list(
    ethervox_file_tools_config_t* config,
    const char* directory_path,
    bool recursive,
    ethervox_file_entry_t** entries,
    uint32_t* entry_count
);

/**
 * TOOL: file_read - Read text file contents
 * 
 * @param config File tools config
 * @param file_path Path to file
 * @param content Output: file contents (caller must free)
 * @param size Output: size of content
 * @return 0 on success, negative on error
 */
int ethervox_file_read(
    ethervox_file_tools_config_t* config,
    const char* file_path,
    char** content,
    uint64_t* size
);

/**
 * TOOL: file_search - Search for text in files
 * 
 * @param config File tools config
 * @param directory_path Directory to search
 * @param pattern Text pattern to search for
 * @param results Output: array of matching file paths (caller must free)
 * @param result_count Output: number of matches
 * @return 0 on success, negative on error
 */
int ethervox_file_search(
    ethervox_file_tools_config_t* config,
    const char* directory_path,
    const char* pattern,
    char*** results,
    uint32_t* result_count
);

/**
 * TOOL: file_write - Write content to file (only if write access enabled)
 * 
 * @param config File tools config
 * @param file_path Path to file
 * @param content Content to write
 * @return 0 on success, negative on error
 */
int ethervox_file_write(
    ethervox_file_tools_config_t* config,
    const char* file_path,
    const char* content
);

/**
 * Register file tools with Governor tool registry
 * 
 * @param registry Governor tool registry
 * @param config File tools config instance
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_file_tools_register(
    void* registry,
    ethervox_file_tools_config_t* config
);

/**
 * Register path configuration tools with Governor tool registry
 * 
 * @param registry Governor tool registry
 * @param config Path configuration instance
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_path_config_register(
    void* registry,
    ethervox_path_config_t* config
);

// ============================================================================
// Path Configuration API
// ============================================================================

/**
 * Initialize path configuration system
 * 
 * Loads default paths (Documents, Downloads, etc.) and any previously saved
 * user paths from memory. Verifies which paths actually exist.
 * 
 * @param config Path configuration structure
 * @param memory Memory store for persistence (can be NULL)
 * @return 0 on success, negative on error
 */
int ethervox_path_config_init(
    ethervox_path_config_t* config,
    void* memory
);

/**
 * TOOL: path_set - Set or update a user path
 * 
 * Allows LLM to remember important user directories. Paths are validated
 * and persisted to memory for future sessions.
 * 
 * @param config Path configuration
 * @param label Human-friendly name (e.g., "Notes", "Projects")
 * @param path Absolute directory path
 * @param description Optional purpose description
 * @return ETHERVOX_SUCCESS on success, ETHERVOX_ERROR_PATH_INVALID if path doesn't exist, 
 *         ETHERVOX_ERROR_BUFFER_TOO_SMALL if max paths reached
 */
ethervox_result_t ethervox_path_config_set(
    ethervox_path_config_t* config,
    const char* label,
    const char* path,
    const char* description
);

/**
 * TOOL: path_get - Retrieve a configured path by label
 * 
 * Allows LLM to look up user's configured directories.
 * 
 * @param config Path configuration
 * @param label Path label to find
 * @param path_out Output buffer for path
 * @param path_size Size of output buffer
 * @return ETHERVOX_SUCCESS on success, ETHERVOX_ERROR_NOT_FOUND if not found
 */
ethervox_result_t ethervox_path_config_get(
    ethervox_path_config_t* config,
    const char* label,
    char* path_out,
    size_t path_size
);

/**
 * TOOL: path_list - List all configured paths
 * 
 * Returns all user paths known to the system.
 * 
 * @param config Path configuration
 * @param paths_out Output: array of paths (caller must free)
 * @param count_out Output: number of paths
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_path_config_list(
    ethervox_path_config_t* config,
    ethervox_user_path_t** paths_out,
    uint32_t* count_out
);

/**
 * Get unverified paths (for prompting user to configure)
 * 
 * Returns paths that are in defaults but don't exist on the system.
 * LLM can use this to ask user for actual locations.
 * 
 * @param config Path configuration
 * @param paths_out Output: array of unverified paths (caller must free)
 * @param count_out Output: number of unverified paths
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_path_config_get_unverified(
    ethervox_path_config_t* config,
    ethervox_user_path_t** paths_out,
    uint32_t* count_out
);

/**
 * Cleanup path configuration
 * 
 * @param config Path configuration to cleanup
 */
void ethervox_path_config_cleanup(ethervox_path_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_FILE_TOOLS_H
