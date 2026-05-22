/**
 * @file kv_cache_persistence.c
 * @brief KV Cache save/load implementation
 *
 * Saves processed system prompt to disk for instant loading.
 * 
 * File format:
 * [Header 512 bytes]
 * [System Prompt Tokens: N * 4 bytes]
 * [KV Cache Data: variable size]
 * [Checksum: 32 bytes]
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/kv_cache_persistence.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
#include <llama.h>
#define LLAMA_AVAILABLE 1
#else
#define LLAMA_AVAILABLE 0
#endif

#define KV_CACHE_MAGIC 0x4B564341  // "KVCA"
#define KV_CACHE_VERSION 1

// Access governor internals (defined in governor.c)
#if LLAMA_AVAILABLE
extern llama_token* ethervox_governor_get_system_tokens(struct ethervox_governor* gov, int* len);
extern struct llama_context* ethervox_governor_get_context(struct ethervox_governor* gov);
extern void ethervox_governor_set_system_tokens(struct ethervox_governor* gov, 
                                               llama_token* tokens, int len);
#endif

/**
 * Extract model name from path
 */
static void extract_model_name(const char* model_path, char* name_out, size_t name_size) {
    const char* filename = strrchr(model_path, '/');
    if (!filename) filename = strrchr(model_path, '\\');
    if (!filename) filename = model_path;
    else filename++;
    
    // Copy and remove extension
    strncpy(name_out, filename, name_size - 1);
    name_out[name_size - 1] = '\0';
    
    char* ext = strrchr(name_out, '.');
    if (ext) *ext = '\0';
}

/**
 * Simple checksum (XOR-based for speed, SHA-256 would be better but adds dependency)
 */
static void calculate_checksum(const uint8_t* data, size_t data_size, uint8_t* checksum_out) {
    memset(checksum_out, 0, 32);
    for (size_t i = 0; i < data_size; i++) {
        checksum_out[i % 32] ^= data[i];
    }
}

bool ethervox_kv_cache_exists(const char* cache_path, const char* model_path) {
    if (!cache_path || !model_path) return false;
    
    // Check if file exists and is readable
    struct stat st;
    if (stat(cache_path, &st) != 0) {
        return false;
    }
    
    // Check if it's a regular file and has reasonable size (> 1MB)
    if (!S_ISREG(st.st_mode)) {
        ETHERVOX_LOG_WARN("KV cache path is not a regular file: %s", cache_path);
        return false;
    }
    
    if (st.st_size < 1048576) {  // Less than 1MB is suspicious
        ETHERVOX_LOG_WARN("KV cache file too small: %s (%lld bytes)", cache_path, (long long)st.st_size);
        return false;
    }
    
    // File exists and looks valid - llama.cpp will do its own validation when loading
    ETHERVOX_LOG_INFO("Found KV cache file: %s (%lld bytes)", cache_path, (long long)st.st_size);
    return true;
}

ethervox_result_t ethervox_kv_cache_save(
    struct ethervox_governor* governor,
    const char* cache_path
) {
    if (!governor || !cache_path) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
#if !LLAMA_AVAILABLE
    ETHERVOX_LOG_ERROR("KV cache save not available: llama.cpp not linked");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    
    ETHERVOX_LOG_INFO("Saving KV cache to: %s", cache_path);
    
    // Get system prompt tokens
    int token_count = 0;
    llama_token* tokens = ethervox_governor_get_system_tokens(governor, &token_count);
    
    if (!tokens || token_count == 0) {
        ETHERVOX_LOG_ERROR("No system prompt tokens to save");
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    
    // Get llama context
    struct llama_context* ctx = ethervox_governor_get_context(governor);
    if (!ctx) {
        ETHERVOX_LOG_ERROR("No llama context available");
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    
    // Ensure cache directory exists
    // Extract directory from cache_path
    char dir_path[512];
    strncpy(dir_path, cache_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    
    char* last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';  // Terminate at last slash to get directory
        
        // Create directory if it doesn't exist (Android/Unix)
        struct stat st;
        if (stat(dir_path, &st) != 0) {
            // Directory doesn't exist, create it
            #ifdef __ANDROID__
            // Android: create with permissions 0755
            if (mkdir(dir_path, 0755) != 0) {
                ETHERVOX_LOG_ERROR("Failed to create cache directory: %s (errno: %d)", dir_path, errno);
                return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
            }
            #else
            // Other Unix systems
            if (mkdir(dir_path, 0755) != 0) {
                ETHERVOX_LOG_ERROR("Failed to create cache directory: %s (errno: %d)", dir_path, errno);
                return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
            }
            #endif
            ETHERVOX_LOG_INFO("Created cache directory: %s", dir_path);
        }
    }
    
    // Use llama.cpp's built-in state save function
    // This saves both tokens AND the complete KV cache state
    bool success = llama_state_save_file(ctx, cache_path, tokens, (size_t)token_count);
    
    if (!success) {
        ETHERVOX_LOG_ERROR("llama_state_save_file() failed");
        return ETHERVOX_ERROR_FILE_WRITE;
    }
    
    ETHERVOX_LOG_INFO("✓ KV cache saved: %d tokens + KV state to %s", token_count, cache_path);
    
    return ETHERVOX_SUCCESS;
#endif
}

ethervox_result_t ethervox_kv_cache_load(
    struct ethervox_governor* governor,
    const char* cache_path
) {
    if (!governor || !cache_path) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
#if !LLAMA_AVAILABLE
    ETHERVOX_LOG_ERROR("KV cache load not available: llama.cpp not linked");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    
    ETHERVOX_LOG_INFO("Loading KV cache from: %s", cache_path);
    
    // Get llama context
    struct llama_context* ctx = ethervox_governor_get_context(governor);
    if (!ctx) {
        ETHERVOX_LOG_ERROR("No llama context available");
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    
    // Allocate buffer for tokens (max 8192 from context size)
    const size_t max_tokens = 8192;
    llama_token* tokens = (llama_token*)malloc(max_tokens * sizeof(llama_token));
    if (!tokens) {
        ETHERVOX_LOG_ERROR("Failed to allocate token buffer");
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    // Use llama.cpp's built-in state load function
    // This loads both tokens AND the complete KV cache state (instant!)
    size_t n_tokens_loaded = 0;
    bool success = llama_state_load_file(ctx, cache_path, tokens, max_tokens, &n_tokens_loaded);
    
    if (!success) {
        ETHERVOX_LOG_ERROR("llama_state_load_file() failed");
        free(tokens);
        return ETHERVOX_ERROR_FILE_READ;
    }
    
    // Store loaded tokens in governor for recovery/reset
    ethervox_governor_set_system_tokens(governor, tokens, (int)n_tokens_loaded);
    
    ETHERVOX_LOG_INFO("✓ KV cache loaded instantly: %zu tokens + KV state ready", n_tokens_loaded);
    
    return ETHERVOX_SUCCESS;
#endif
}

ethervox_result_t ethervox_kv_cache_get_path(
    const char* model_path,
    const char* files_dir,
    char* output,
    size_t output_size
) {
    if (!model_path || !files_dir || !output || output_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Extract model name
    char model_name[128];
    extract_model_name(model_path, model_name, sizeof(model_name));
    
    // Build path: <files_dir>/cache/system_prompt_{model}.kvcache
    int written = snprintf(output, output_size, 
                          "%s/cache/system_prompt_%s.kvcache",
                          files_dir, model_name);
    
    if (written < 0 || (size_t)written >= output_size) {
        return ETHERVOX_ERROR_BUFFER_TOO_SMALL;
    }
    
    return ETHERVOX_SUCCESS;
}

// Accessor functions (to be added to governor.c)
#if LLAMA_AVAILABLE
// These need to be implemented in governor.c to access private fields
__attribute__((weak))
llama_token* ethervox_governor_get_system_tokens(struct ethervox_governor* gov, int* len) {
    // Implemented in governor.c
    (void)gov; (void)len;
    return NULL;
}

__attribute__((weak))
struct llama_context* ethervox_governor_get_context(struct ethervox_governor* gov) {
    // Implemented in governor.c
    (void)gov;
    return NULL;
}

__attribute__((weak))
void ethervox_governor_set_system_tokens(struct ethervox_governor* gov, 
                                        llama_token* tokens, int len) {
    // Implemented in governor.c
    (void)gov; (void)tokens; (void)len;
}

__attribute__((weak))
const char* ethervox_governor_get_model_path(struct ethervox_governor* gov) {
    // Implemented in governor.c
    (void)gov;
    return "";
}
#endif
