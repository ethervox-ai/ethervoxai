/**
#include "ethervox/error.h"
 * @file memory_archive.c
 * @brief Memory session archiving functionality
 * 
 * Provides cross-platform session archiving to move old memory files
 * to an archive subdirectory for cleanup while preserving history.
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include "ethervox/logging.h"
#include "ethervox/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#define mkdir(path, mode) _mkdir(path)
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

/**
 * Create archive directory if it doesn't exist
 * 
 * @param storage_dir Base storage directory
 * @param archive_dir_out Output buffer for archive directory path
 * @param archive_dir_size Size of output buffer
 * @return 0 on success, -1 on error
 */
static int ensure_archive_directory(
    const char* storage_dir,
    char* archive_dir_out,
    size_t archive_dir_size
) {
    snprintf(archive_dir_out, archive_dir_size, "%s/archive", storage_dir);
    
    // Check if archive directory exists
    struct stat st;
    if (stat(archive_dir_out, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return ETHERVOX_SUCCESS;  // Already exists
        } else {
            ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                        "Archive path exists but is not a directory: %s", archive_dir_out);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    // Create archive directory
#if defined(ETHERVOX_PLATFORM_ESP32)
    ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                "ESP32 may not support subdirectories in SPIFFS");
    // Try anyway - some ESP32 filesystems support it
#endif
    
    if (mkdir(archive_dir_out, 0755) != 0) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to create archive directory: %s (errno=%d)", 
                    archive_dir_out, errno);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Created archive directory: %s", archive_dir_out);
    
    return ETHERVOX_SUCCESS;
}

/**
 * Move a file to the archive directory
 * 
 * @param source_path Source file path
 * @param archive_dir Archive directory path
 * @param filename Filename to use in archive
 * @return 0 on success, -1 on error
 */
static int move_to_archive(
    const char* source_path,
    const char* archive_dir,
    const char* filename
) {
    char dest_path[512];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", archive_dir, filename);
    
    // Use rename() for atomic move on same filesystem
    if (rename(source_path, dest_path) == 0) {
        ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                    "Moved %s to archive", filename);
        return ETHERVOX_SUCCESS;
    }
    
    // If rename fails (e.g., cross-filesystem), try copy+delete
    ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                "rename() failed, attempting copy+delete for %s", filename);
    
    FILE* src = fopen(source_path, "rb");
    if (!src) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to open source file: %s", source_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    FILE* dst = fopen(dest_path, "wb");
    if (!dst) {
        fclose(src);
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to create destination file: %s", dest_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Copy file contents
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            fclose(src);
            fclose(dst);
            remove(dest_path);  // Clean up partial copy
            ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                        "Write failed during copy of %s", filename);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    fclose(src);
    fclose(dst);
    
    // Remove original file
    if (remove(source_path) != 0) {
        ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                    "Failed to remove original file after copy: %s", source_path);
        // Not a fatal error - file was copied successfully
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "Copied %s to archive and removed original", filename);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_memory_archive_sessions(
    ethervox_memory_store_t* store,
    uint32_t* files_archived
) {
    if (!store || !store->is_initialized) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (files_archived) {
        *files_archived = 0;
    }
    
    // If no storage directory is set, nothing to archive
    if (!store->storage_filepath[0]) {
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "No storage directory set, nothing to archive");
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
    
    // Ensure archive directory exists
    char archive_dir[512];
    if (ensure_archive_directory(storage_dir, archive_dir, sizeof(archive_dir)) != 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Open storage directory
    DIR* dir = opendir(storage_dir);
    if (!dir) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to open storage directory: %s", storage_dir);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Iterate through files and move old sessions to archive
    struct dirent* entry;
    uint32_t archived_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // Look for .jsonl files
        size_t len = strlen(entry->d_name);
        if (len > 6 && strcmp(entry->d_name + len - 6, ".jsonl") == 0) {
            // Skip current session file by name
            if (strcmp(entry->d_name, current_basename) == 0) {
                ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                            "Skipping current session: %s", entry->d_name);
                continue;
            }
            
            // Skip files already in archive subdirectory
            if (strstr(entry->d_name, "archive") != NULL) {
                continue;
            }
            
            // Build full source path
            char source_path[512];
            snprintf(source_path, sizeof(source_path), "%s/%s", 
                    storage_dir, entry->d_name);
            
            // Move to archive
            if (move_to_archive(source_path, archive_dir, entry->d_name) == 0) {
                archived_count++;
            }
        }
    }
    
    closedir(dir);
    
    if (files_archived) {
        *files_archived = archived_count;
    }
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Archived %u session file(s) to %s", archived_count, archive_dir);
    
    return ETHERVOX_SUCCESS;
}
