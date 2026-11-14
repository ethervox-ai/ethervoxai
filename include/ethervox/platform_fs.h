/**
 * @file platform_fs.h
 * @brief Cross-platform file system operations
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 *
 * Thin static inline wrappers over POSIX and Win32 file APIs.
 * Zero overhead, header-only, chosen by CMake defines.
 */

#ifndef ETHERVOX_PLATFORM_FS_H
#define ETHERVOX_PLATFORM_FS_H

#include "ethervox/error.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// File Existence and Type Checks
// ============================================================================

#ifdef _WIN32
#include <windows.h>
#include <sys/stat.h>

/**
 * Check if file or directory exists
 */
static inline bool ethervox_fs_exists(const char* path) {
    if (!path) return false;
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES);
}

/**
 * Check if path is a regular file
 */
static inline bool ethervox_fs_is_file(const char* path) {
    if (!path) return false;
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

/**
 * Check if path is a directory
 */
static inline bool ethervox_fs_is_dir(const char* path) {
    if (!path) return false;
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

#else
#include <sys/stat.h>
#include <unistd.h>

static inline bool ethervox_fs_exists(const char* path) {
    if (!path) return false;
    struct stat st;
    return (stat(path, &st) == 0);
}

static inline bool ethervox_fs_is_file(const char* path) {
    if (!path) return false;
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static inline bool ethervox_fs_is_dir(const char* path) {
    if (!path) return false;
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

#endif

// ============================================================================
// File Size
// ============================================================================

/**
 * Get file size in bytes
 * 
 * @param path Path to file
 * @param out_size Output size in bytes
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_fs_file_size(const char* path, uint64_t* out_size) {
    if (!path || !out_size) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) {
        return ETHERVOX_ERROR_FILE_NOT_FOUND;
    }
    
    LARGE_INTEGER size;
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    *out_size = (uint64_t)size.QuadPart;
    return ETHERVOX_SUCCESS;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return ETHERVOX_ERROR_FILE_NOT_FOUND;
    }
    *out_size = (uint64_t)st.st_size;
    return ETHERVOX_SUCCESS;
#endif
}

// ============================================================================
// File Operations
// ============================================================================

/**
 * Delete file
 * 
 * @param path Path to file
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_fs_delete(const char* path) {
    if (!path) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
#ifdef _WIN32
    if (!DeleteFileA(path)) {
        return ETHERVOX_ERROR_FILE_DELETE_FAILED;
    }
    return ETHERVOX_SUCCESS;
#else
    if (unlink(path) != 0) {
        return ETHERVOX_ERROR_FILE_DELETE_FAILED;
    }
    return ETHERVOX_SUCCESS;
#endif
}

/**
 * Rename/move file
 * 
 * @param old_path Current path
 * @param new_path New path
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_fs_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
#ifdef _WIN32
    if (!MoveFileA(old_path, new_path)) {
        return ETHERVOX_ERROR_FAILED;
    }
    return ETHERVOX_SUCCESS;
#else
    if (rename(old_path, new_path) != 0) {
        return ETHERVOX_ERROR_FAILED;
    }
    return ETHERVOX_SUCCESS;
#endif
}

/**
 * Copy file
 * 
 * @param src_path Source path
 * @param dst_path Destination path
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_fs_copy(const char* src_path, const char* dst_path) {
    if (!src_path || !dst_path) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
#ifdef _WIN32
    if (!CopyFileA(src_path, dst_path, FALSE)) {
        return ETHERVOX_ERROR_FAILED;
    }
    return ETHERVOX_SUCCESS;
#else
    // POSIX doesn't have a direct copy function, use read/write
    FILE* src = fopen(src_path, "rb");
    if (!src) return ETHERVOX_ERROR_FILE_NOT_FOUND;
    
    FILE* dst = fopen(dst_path, "wb");
    if (!dst) {
        fclose(src);
        return ETHERVOX_ERROR_FILE_WRITE;
    }
    
    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            fclose(src);
            fclose(dst);
            return ETHERVOX_ERROR_FILE_WRITE;
        }
    }
    
    fclose(src);
    fclose(dst);
    return ETHERVOX_SUCCESS;
#endif
}

// ============================================================================
// Path Operations
// ============================================================================

/**
 * Get absolute path
 * 
 * @param path Relative or absolute path
 * @param out_abs Output buffer for absolute path
 * @param out_size Size of output buffer
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_fs_absolute_path(
    const char* path,
    char* out_abs,
    size_t out_size
) {
    if (!path || !out_abs || out_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
#ifdef _WIN32
    DWORD result = GetFullPathNameA(path, (DWORD)out_size, out_abs, NULL);
    if (result == 0 || result >= out_size) {
        return ETHERVOX_ERROR_BUFFER_TOO_SMALL;
    }
    return ETHERVOX_SUCCESS;
#else
    char* result = realpath(path, out_abs);
    if (!result) {
        // If realpath fails, try a simple approach
        if (path[0] == '/') {
            // Already absolute
            strncpy(out_abs, path, out_size - 1);
            out_abs[out_size - 1] = '\0';
            return ETHERVOX_SUCCESS;
        }
        
        // Get current directory
        if (!getcwd(out_abs, out_size)) {
            return ETHERVOX_ERROR_BUFFER_TOO_SMALL;
        }
        
        // Append path
        size_t cwd_len = strlen(out_abs);
        if (cwd_len + strlen(path) + 2 > out_size) {
            return ETHERVOX_ERROR_BUFFER_TOO_SMALL;
        }
        
        if (out_abs[cwd_len - 1] != '/') {
            out_abs[cwd_len++] = '/';
        }
        strcpy(out_abs + cwd_len, path);
    }
    return ETHERVOX_SUCCESS;
#endif
}

/**
 * Join path components
 * 
 * @param base Base path
 * @param component Component to append
 * @param out Output buffer
 * @param out_size Size of output buffer
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_fs_path_join(
    const char* base,
    const char* component,
    char* out,
    size_t out_size
) {
    if (!base || !component || !out || out_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    size_t base_len = strlen(base);
    size_t comp_len = strlen(component);
    bool need_sep = (base_len > 0 && base[base_len - 1] != '/' && base[base_len - 1] != '\\');
    
    size_t required = base_len + (need_sep ? 1 : 0) + comp_len + 1;
    if (required > out_size) {
        return ETHERVOX_ERROR_BUFFER_TOO_SMALL;
    }
    
    strcpy(out, base);
    if (need_sep) {
#ifdef _WIN32
        out[base_len] = '\\';
#else
        out[base_len] = '/';
#endif
        out[base_len + 1] = '\0';
    }
    strcat(out, component);
    
    return ETHERVOX_SUCCESS;
}

/**
 * Get file extension (without dot)
 * 
 * @param path File path
 * @param out_ext Output buffer for extension
 * @param out_size Size of output buffer
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_fs_get_extension(
    const char* path,
    char* out_ext,
    size_t out_size
) {
    if (!path || !out_ext || out_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    const char* dot = strrchr(path, '.');
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');
    const char* last_sep = (slash > backslash) ? slash : backslash;
    
    // Extension must be after last path separator
    if (!dot || (last_sep && dot < last_sep)) {
        out_ext[0] = '\0';
        return ETHERVOX_SUCCESS;
    }
    
    strncpy(out_ext, dot + 1, out_size - 1);
    out_ext[out_size - 1] = '\0';
    return ETHERVOX_SUCCESS;
}

#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_PLATFORM_FS_H */
