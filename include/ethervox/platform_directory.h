/**
 * @file platform_directory.h
 * @brief Cross-platform directory operations
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 *
 * Thin static inline wrappers over POSIX dirent and Win32 FindFirstFile APIs.
 * Zero overhead, header-only, chosen by CMake defines.
 */

#ifndef ETHERVOX_PLATFORM_DIRECTORY_H
#define ETHERVOX_PLATFORM_DIRECTORY_H

#include "ethervox/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Directory Handle
// ============================================================================

#ifdef _WIN32
#include <windows.h>

typedef struct {
    HANDLE handle;
    WIN32_FIND_DATAA find_data;
    bool first;
    bool valid;
} ethervox_dir_t;

#else
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

typedef struct {
    DIR* handle;
    struct dirent* entry;
} ethervox_dir_t;

#endif

// ============================================================================
// Directory Operations
// ============================================================================

/**
 * Open a directory for reading
 * 
 * @param path Path to directory
 * @param dir Output directory handle
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_dir_open(const char* path, ethervox_dir_t* dir) {
    if (!path || !dir) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
#ifdef _WIN32
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);
    
    dir->handle = FindFirstFileA(search_path, &dir->find_data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        return ETHERVOX_ERROR_DIRECTORY_NOT_FOUND;
    }
    
    dir->first = true;
    dir->valid = true;
    return ETHERVOX_SUCCESS;
#else
    dir->handle = opendir(path);
    if (!dir->handle) {
        return ETHERVOX_ERROR_DIRECTORY_NOT_FOUND;
    }
    dir->entry = NULL;
    return ETHERVOX_SUCCESS;
#endif
}

/**
 * Read next entry from directory
 * 
 * @param dir Directory handle
 * @param out_name Output buffer for entry name
 * @param name_size Size of output buffer
 * @param out_is_dir Set to true if entry is a directory
 * @return ETHERVOX_SUCCESS, ETHERVOX_ERROR_NOT_FOUND (end of dir), or error code
 */
static inline ethervox_result_t ethervox_dir_read(
    ethervox_dir_t* dir,
    char* out_name,
    size_t name_size,
    bool* out_is_dir
) {
    if (!dir || !out_name || name_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
#ifdef _WIN32
    if (!dir->valid) {
        return ETHERVOX_ERROR_NOT_FOUND;
    }
    
    if (dir->first) {
        dir->first = false;
    } else {
        if (!FindNextFileA(dir->handle, &dir->find_data)) {
            dir->valid = false;
            return ETHERVOX_ERROR_NOT_FOUND;
        }
    }
    
    strncpy(out_name, dir->find_data.cFileName, name_size - 1);
    out_name[name_size - 1] = '\0';
    
    if (out_is_dir) {
        *out_is_dir = (dir->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    
    return ETHERVOX_SUCCESS;
#else
    dir->entry = readdir(dir->handle);
    if (!dir->entry) {
        return ETHERVOX_ERROR_NOT_FOUND;
    }
    
    strncpy(out_name, dir->entry->d_name, name_size - 1);
    out_name[name_size - 1] = '\0';
    
    if (out_is_dir) {
        *out_is_dir = (dir->entry->d_type == DT_DIR);
    }
    
    return ETHERVOX_SUCCESS;
#endif
}

/**
 * Close directory handle
 * 
 * @param dir Directory handle
 */
static inline void ethervox_dir_close(ethervox_dir_t* dir) {
    if (!dir) return;
    
#ifdef _WIN32
    if (dir->handle != INVALID_HANDLE_VALUE) {
        FindClose(dir->handle);
        dir->handle = INVALID_HANDLE_VALUE;
    }
#else
    if (dir->handle) {
        closedir(dir->handle);
        dir->handle = NULL;
    }
#endif
}

/**
 * Create directory
 * 
 * @param path Path to directory
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_dir_create(const char* path) {
    if (!path) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
#ifdef _WIN32
    if (!CreateDirectoryA(path, NULL)) {
        DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS) {
            return ETHERVOX_ERROR_FILE_EXISTS;
        }
        return ETHERVOX_ERROR_DIRECTORY_CREATE_FAILED;
    }
    return ETHERVOX_SUCCESS;
#else
    if (mkdir(path, 0755) != 0) {
        if (errno == EEXIST) {
            return ETHERVOX_ERROR_FILE_EXISTS;
        }
        return ETHERVOX_ERROR_DIRECTORY_CREATE_FAILED;
    }
    return ETHERVOX_SUCCESS;
#endif
}

/**
 * Create directory recursively (like mkdir -p)
 * 
 * @param path Path to directory
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_dir_create_recursive(const char* path) {
    if (!path) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    char tmp[1024];
    char* p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\') {
        tmp[len - 1] = '\0';
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            ethervox_result_t result = ethervox_dir_create(tmp);
            if (result != ETHERVOX_SUCCESS && result != ETHERVOX_ERROR_FILE_EXISTS) {
                return result;
            }
            *p = '/';
        }
    }
    
    ethervox_result_t result = ethervox_dir_create(tmp);
    if (result == ETHERVOX_ERROR_FILE_EXISTS) {
        return ETHERVOX_SUCCESS;
    }
    return result;
}

/**
 * Remove empty directory
 * 
 * @param path Path to directory
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_dir_remove(const char* path) {
    if (!path) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
#ifdef _WIN32
    if (!RemoveDirectoryA(path)) {
        return ETHERVOX_ERROR_FILE_DELETE_FAILED;
    }
    return ETHERVOX_SUCCESS;
#else
    if (rmdir(path) != 0) {
        return ETHERVOX_ERROR_FILE_DELETE_FAILED;
    }
    return ETHERVOX_SUCCESS;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_PLATFORM_DIRECTORY_H */
