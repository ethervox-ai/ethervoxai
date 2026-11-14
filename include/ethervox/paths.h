/**
 * @file paths.h
 * @brief Caller-supplied path configuration for EthervoxAI backend
 * 
 * Replaces hardcoded `getenv("HOME")` and `Documents/` derivations with
 * explicit path configuration. Required for macOS App Sandbox, security-scoped
 * vault access, and Android/iOS file-access restrictions.
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_PATHS_H
#define ETHERVOX_PATHS_H

#include "ethervox/error.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Path configuration for EthervoxAI backend
 * 
 * All paths must be absolute and accessible to the calling process.
 * On sandboxed platforms (macOS, iOS), paths must be within security-scoped
 * bookmarks or app container directories.
 * 
 * Lifetime: Caller must ensure paths remain valid for the duration of
 * backend usage. Backend does not copy or take ownership of these strings.
 */
typedef struct {
    /**
     * Data directory for persistent application data
     * 
     * Contains:
     * - Memory archives (episodic/working memory)
     * - Tool manifests (binary + optimized prompts)
     * - Adapter weights
     * - Conversation history
     * 
     * Platform examples:
     * - macOS: ~/Library/Application Support/EthervoxAI/
     * - iOS: <app container>/Library/Application Support/
     * - Android: Context.getFilesDir() + "/ethervox/"
     * - Linux: ~/.local/share/ethervoxai/
     */
    const char* data_dir;
    
    /**
     * Cache directory for evictable data
     * 
     * Contains:
     * - KV cache snapshots
     * - Tokenizer caches
     * - Model metadata caches
     * 
     * Platform examples:
     * - macOS: ~/Library/Caches/EthervoxAI/
     * - iOS: <app container>/Library/Caches/
     * - Android: Context.getCacheDir() + "/ethervox/"
     * - Linux: ~/.cache/ethervoxai/
     */
    const char* cache_dir;
    
    /**
     * Models directory for GGUF model files
     * 
     * Contains:
     * - Quantized LLM models (*.gguf)
     * - Vision models
     * - Embedding models
     * - STT models (if applicable)
     * 
     * Platform examples:
     * - macOS: ~/Library/Application Support/EthervoxAI/models/
     * - iOS: <app container>/Documents/models/
     * - Android: Context.getFilesDir() + "/models/"
     * - Linux: ~/.local/share/ethervoxai/models/
     */
    const char* models_dir;
    
    /**
     * Temporary directory for scratch/transient files
     * 
     * Contains:
     * - In-progress downloads
     * - Temporary audio buffers
     * - Intermediate extraction results
     * 
     * Platform examples:
     * - macOS: ~/Library/Caches/EthervoxAI/tmp/
     * - iOS: <app container>/tmp/
     * - Android: Context.getCacheDir() + "/tmp/"
     * - Linux: /tmp/ethervoxai-<uid>/
     */
    const char* temp_dir;
} ethervox_paths_t;

/**
 * Validate path configuration
 * 
 * Checks that:
 * - All paths are non-NULL and non-empty
 * - All paths are absolute (platform-specific check)
 * - Directories exist or can be created
 * - Process has read/write permissions
 * 
 * @param paths Path configuration to validate
 * @return ETHERVOX_SUCCESS if valid, or:
 *         ETHERVOX_ERROR_INVALID_ARGUMENT if paths is NULL or any path is NULL/empty
 *         ETHERVOX_ERROR_FILE_NOT_FOUND if directory doesn't exist and can't be created
 *         ETHERVOX_ERROR_FILE_PERMISSION if permission denied
 */
ethervox_result_t ethervox_paths_validate(const ethervox_paths_t* paths);

/**
 * Get default path configuration for the current platform
 * 
 * Constructs platform-appropriate paths using:
 * - macOS: NSSearchPathForDirectoriesInDomains equivalents
 * - Linux: XDG Base Directory spec
 * - Android/iOS: Must be provided by caller (no defaults)
 * 
 * NOTE: This is a compatibility shim for legacy code. New code should
 * receive paths from the host platform explicitly.
 * 
 * @param paths Output structure to populate (caller-owned)
 * @param buffer Buffer to hold path strings (caller-owned, ≥2048 bytes recommended)
 * @param buffer_size Size of buffer in bytes
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_paths_get_default(ethervox_paths_t* paths, 
                                             char* buffer, 
                                             size_t buffer_size);

/**
 * Helper: Join path components
 * 
 * Platform-aware path joining with correct separator ('/' or '\\')
 * 
 * @param base Base path (must not be NULL)
 * @param component Component to append (must not be NULL)
 * @param out Output buffer
 * @param out_size Size of output buffer
 * @return ETHERVOX_SUCCESS or ETHERVOX_ERROR_INVALID_ARGUMENT
 */
ethervox_result_t ethervox_path_join(const char* base,
                                     const char* component,
                                     char* out,
                                     size_t out_size);

/**
 * Helper: Ensure directory exists
 * 
 * Creates directory and parent directories if needed (like mkdir -p)
 * 
 * @param path Directory path to create
 * @return ETHERVOX_SUCCESS, ETHERVOX_ERROR_DIRECTORY_CREATE_FAILED, or ETHERVOX_ERROR_INVALID_ARGUMENT
 */
ethervox_result_t ethervox_ensure_directory(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_PATHS_H */
