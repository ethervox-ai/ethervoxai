/**
 * @file paths.c
 * @brief Path configuration implementation
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/paths.h"
#include "ethervox/error.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <shlobj.h>
#define PATH_SEP '\\'
#define mkdir_p(path) _mkdir(path)
#define access _access
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#else
#include <unistd.h>
#define PATH_SEP '/'
#define mkdir_p(path) mkdir(path, 0755)
#endif

/**
 * Check if path is absolute
 */
static bool is_absolute_path(const char* path) {
    if (!path || !path[0]) return false;
    
#ifdef _WIN32
    // Windows: C:\... or \\server\share
    if (path[0] == '\\' && path[1] == '\\') return true;  // UNC path
    if (path[1] == ':' && (path[0] >= 'A' && path[0] <= 'Z' || 
                           path[0] >= 'a' && path[0] <= 'z')) {
        return path[2] == '\\' || path[2] == '/';
    }
    return false;
#else
    // Unix: starts with /
    return path[0] == '/';
#endif
}

/**
 * Check if directory exists
 */
static bool directory_exists(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

/**
 * Check if we have read/write access to a directory
 */
static bool has_rw_access(const char* path) {
    // Try to access the directory
    return access(path, R_OK | W_OK) == 0;
}

ethervox_result_t ethervox_paths_validate(const ethervox_paths_t* paths) {
    if (!paths) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check all paths are non-NULL and non-empty
    if (!paths->data_dir || !paths->data_dir[0]) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    if (!paths->cache_dir || !paths->cache_dir[0]) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    if (!paths->models_dir || !paths->models_dir[0]) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    if (!paths->temp_dir || !paths->temp_dir[0]) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check all paths are absolute
    if (!is_absolute_path(paths->data_dir)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    if (!is_absolute_path(paths->cache_dir)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    if (!is_absolute_path(paths->models_dir)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    if (!is_absolute_path(paths->temp_dir)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check directories exist or can be created
    const char* dirs[] = {
        paths->data_dir,
        paths->cache_dir,
        paths->models_dir,
        paths->temp_dir
    };
    
    for (int i = 0; i < 4; i++) {
        if (!directory_exists(dirs[i])) {
            // Try to create it
            ethervox_result_t result = ethervox_ensure_directory(dirs[i]);
            if (result != ETHERVOX_SUCCESS) {
                return result;
            }
        }
        
        // Check we have read/write access
        if (!has_rw_access(dirs[i])) {
            return ETHERVOX_ERROR_FILE_PERMISSION;
        }
    }
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_paths_get_default(ethervox_paths_t* paths,
                                             char* buffer,
                                             size_t buffer_size) {
    if (!paths || !buffer || buffer_size < 2048) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Zero out the buffer
    memset(buffer, 0, buffer_size);
    
    char* buf_ptr = buffer;
    size_t remaining = buffer_size;
    
#ifdef __APPLE__
    // macOS: Use ~/Library/Application Support and ~/Library/Caches
    const char* home = getenv("HOME");
    if (!home) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // data_dir: ~/Library/Application Support/EthervoxAI/
    int written = snprintf(buf_ptr, remaining, "%s/Library/Application Support/EthervoxAI", home);
    if (written < 0 || (size_t)written >= remaining) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    paths->data_dir = buf_ptr;
    buf_ptr += written + 1;  // +1 for null terminator
    remaining -= written + 1;
    
    // cache_dir: ~/Library/Caches/EthervoxAI/
    written = snprintf(buf_ptr, remaining, "%s/Library/Caches/EthervoxAI", home);
    if (written < 0 || (size_t)written >= remaining) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    paths->cache_dir = buf_ptr;
    buf_ptr += written + 1;
    remaining -= written + 1;
    
    // models_dir: ~/Library/Application Support/EthervoxAI/models/
    written = snprintf(buf_ptr, remaining, "%s/Library/Application Support/EthervoxAI/models", home);
    if (written < 0 || (size_t)written >= remaining) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    paths->models_dir = buf_ptr;
    buf_ptr += written + 1;
    remaining -= written + 1;
    
    // temp_dir: ~/Library/Caches/EthervoxAI/tmp/
    written = snprintf(buf_ptr, remaining, "%s/Library/Caches/EthervoxAI/tmp", home);
    if (written < 0 || (size_t)written >= remaining) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    paths->temp_dir = buf_ptr;
    
#elif defined(__linux__)
    // Linux: Use XDG Base Directory spec
    const char* home = getenv("HOME");
    if (!home) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    const char* xdg_data_home = getenv("XDG_DATA_HOME");
    const char* xdg_cache_home = getenv("XDG_CACHE_HOME");
    
    // data_dir: $XDG_DATA_HOME/ethervoxai or ~/.local/share/ethervoxai
    if (xdg_data_home) {
        int written = snprintf(buf_ptr, remaining, "%s/ethervoxai", xdg_data_home);
        if (written < 0 || (size_t)written >= remaining) {
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    } else {
        int written = snprintf(buf_ptr, remaining, "%s/.local/share/ethervoxai", home);
        if (written < 0 || (size_t)written >= remaining) {
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    paths->data_dir = buf_ptr;
    buf_ptr += strlen(buf_ptr) + 1;
    remaining -= strlen(paths->data_dir) + 1;
    
    // cache_dir: $XDG_CACHE_HOME/ethervoxai or ~/.cache/ethervoxai
    if (xdg_cache_home) {
        int written = snprintf(buf_ptr, remaining, "%s/ethervoxai", xdg_cache_home);
        if (written < 0 || (size_t)written >= remaining) {
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    } else {
        int written = snprintf(buf_ptr, remaining, "%s/.cache/ethervoxai", home);
        if (written < 0 || (size_t)written >= remaining) {
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    paths->cache_dir = buf_ptr;
    buf_ptr += strlen(buf_ptr) + 1;
    remaining -= strlen(paths->cache_dir) + 1;
    
    // models_dir: <data_dir>/models
    int written = snprintf(buf_ptr, remaining, "%s/models", paths->data_dir);
    if (written < 0 || (size_t)written >= remaining) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    paths->models_dir = buf_ptr;
    buf_ptr += written + 1;
    remaining -= written + 1;
    
    // temp_dir: <cache_dir>/tmp
    written = snprintf(buf_ptr, remaining, "%s/tmp", paths->cache_dir);
    if (written < 0 || (size_t)written >= remaining) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    paths->temp_dir = buf_ptr;
    
#elif defined(_WIN32)
    // Windows: Use known folders API
    char app_data[MAX_PATH];
    char local_app_data[MAX_PATH];
    
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, app_data) != S_OK) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_app_data) != S_OK) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // data_dir: %APPDATA%\EthervoxAI\
    int written = snprintf(buf_ptr, remaining, "%s\\EthervoxAI", app_data);
    if (written < 0 || (size_t)written >= remaining) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    paths->data_dir = buf_ptr;
    buf_ptr += written + 1;
    remaining -= written + 1;
    
    // cache_dir: %LOCALAPPDATA%\EthervoxAI\Cache\
    written = snprintf(buf_ptr, remaining, "%s\\EthervoxAI\\Cache", local_app_data);
    if (written < 0 || (size_t)written >= remaining) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    paths->cache_dir = buf_ptr;
    buf_ptr += written + 1;
    remaining -= written + 1;
    
    // models_dir: %APPDATA%\EthervoxAI\models\
    written = snprintf(buf_ptr, remaining, "%s\\EthervoxAI\\models", app_data);
    if (written < 0 || (size_t)written >= remaining) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    paths->models_dir = buf_ptr;
    buf_ptr += written + 1;
    remaining -= written + 1;
    
    // temp_dir: %LOCALAPPDATA%\EthervoxAI\Temp\
    written = snprintf(buf_ptr, remaining, "%s\\EthervoxAI\\Temp", local_app_data);
    if (written < 0 || (size_t)written >= remaining) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    paths->temp_dir = buf_ptr;
    
#else
    // Android/iOS: No defaults - caller must provide paths
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#endif
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_path_join(const char* base,
                                     const char* component,
                                     char* out,
                                     size_t out_size) {
    if (!base || !component || !out || out_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    size_t base_len = strlen(base);
    size_t comp_len = strlen(component);
    
    // Check if we need a separator
    bool needs_sep = (base_len > 0 && base[base_len - 1] != PATH_SEP);
    
    // Calculate total length needed
    size_t total_len = base_len + (needs_sep ? 1 : 0) + comp_len + 1;  // +1 for null
    
    if (total_len > out_size) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build the path
    if (needs_sep) {
        snprintf(out, out_size, "%s%c%s", base, PATH_SEP, component);
    } else {
        snprintf(out, out_size, "%s%s", base, component);
    }
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_ensure_directory(const char* path) {
    if (!path || !path[0]) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Already exists?
    if (directory_exists(path)) {
        return ETHERVOX_SUCCESS;
    }
    
    // Try to create it
    // Note: This is a simplified version - doesn't handle nested directory creation
    // A full implementation would need to recursively create parent directories
    if (mkdir_p(path) != 0) {
        if (errno == EEXIST) {
            // Race condition - another thread created it
            return ETHERVOX_SUCCESS;
        }
        return ETHERVOX_ERROR_DIRECTORY_CREATE_FAILED;
    }
    
    return ETHERVOX_SUCCESS;
}
