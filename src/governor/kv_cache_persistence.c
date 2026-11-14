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
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/kv_cache_persistence.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"
#include "ethervox/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>  // For unlink(), fsync(), usleep()
#include <fcntl.h>   // For open(), O_RDONLY
#ifdef _WIN32
#include <direct.h>  // _mkdir
#include <io.h>      // _commit
#define mkdir(path, mode) _mkdir(path)
#endif
#include <dirent.h>  // For directory iteration (evict/usage functions)

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
#include <llama.h>
#define LLAMA_AVAILABLE 1
#else
#define LLAMA_AVAILABLE 0
#endif

#define KV_CACHE_MAGIC 0x4B564341  // "KVCA"
#define KV_CACHE_VERSION 2          // Version 2: hash-based keying (TASK-C1.6)

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

/**
 * Compute simple file digest (hash first 1MB + file size for speed)
 * Full SHA-256 would be better but requires dependency
 */
static ethervox_result_t compute_file_digest(const char* file_path, uint8_t* digest_out) {
    FILE* f = fopen(file_path, "rb");
    if (!f) {
        return ETHERVOX_ERROR_FILE_READ;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Hash first 1MB + file size for speed
    const size_t max_bytes = 1024 * 1024;  // 1MB
    size_t bytes_to_read = (size_t)file_size < max_bytes ? (size_t)file_size : max_bytes;
    
    uint8_t buffer[4096];
    size_t bytes_read = 0;
    memset(digest_out, 0, 32);
    
    while (bytes_read < bytes_to_read) {
        size_t chunk = sizeof(buffer);
        if (chunk > bytes_to_read - bytes_read) {
            chunk = bytes_to_read - bytes_read;
        }
        
        size_t n = fread(buffer, 1, chunk, f);
        if (n == 0) break;
        
        for (size_t i = 0; i < n; i++) {
            digest_out[bytes_read % 32] ^= buffer[i];
            bytes_read++;
        }
    }
    
    // Mix in file size
    for (int i = 0; i < 8; i++) {
        digest_out[i] ^= (file_size >> (i * 8)) & 0xFF;
    }
    
    fclose(f);
    return ETHERVOX_SUCCESS;
}

/**
 * Compute hash of string data
 */
static void compute_string_hash(const char* str, uint8_t* hash_out) {
    memset(hash_out, 0, 32);
    if (!str) return;
    
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        hash_out[i % 32] ^= (uint8_t)str[i];
    }
}

/**
 * Compute cache key hash from all components
 * hash(model digest ‖ prompt hash ‖ ctx size ‖ quant ‖ backend version)
 */
static void compute_cache_key_hash(
    const uint8_t* model_digest,
    const uint8_t* prompt_hash,
    uint32_t context_size,
    uint32_t quantization,
    uint32_t backend_version,
    uint8_t* key_hash_out
) {
    memset(key_hash_out, 0, 32);
    
    // Mix in all components
    for (int i = 0; i < 32; i++) {
        key_hash_out[i] ^= model_digest[i];
        key_hash_out[i] ^= prompt_hash[i];
    }
    
    // Mix in numeric parameters
    for (int i = 0; i < 4; i++) {
        key_hash_out[i] ^= (context_size >> (i * 8)) & 0xFF;
        key_hash_out[i + 4] ^= (quantization >> (i * 8)) & 0xFF;
        key_hash_out[i + 8] ^= (backend_version >> (i * 8)) & 0xFF;
    }
}

/**
 * Convert hash bytes to hex string
 */
static void hash_to_hex(const uint8_t* hash, size_t hash_len, char* hex_out, size_t hex_size) {
    const char* hex_chars = "0123456789abcdef";
    size_t out_pos = 0;
    
    for (size_t i = 0; i < hash_len && out_pos + 2 < hex_size; i++) {
        hex_out[out_pos++] = hex_chars[(hash[i] >> 4) & 0xF];
        hex_out[out_pos++] = hex_chars[hash[i] & 0xF];
    }
    hex_out[out_pos] = '\0';
}

bool ethervox_kv_cache_exists(
    const ethervox_paths_t* paths,
    const struct ethervox_governor* governor
) {
    if (!paths || !governor) {
        return false;
    }
    
    // Get the cache path for this configuration
    char cache_path[1024];
    ethervox_result_t result = ethervox_kv_cache_get_path(paths, governor, cache_path, sizeof(cache_path));
    if (result != ETHERVOX_SUCCESS) {
        return false;
    }
    
    // Check if file exists and is readable
    struct stat st;
    if (stat(cache_path, &st) != 0) {
        return false;
    }
    
    // Check if it's a regular file
    if (!S_ISREG(st.st_mode)) {
        ETHERVOX_LOG_WARN("KV cache path is not a regular file: %s", cache_path);
        return false;
    }
    
    // Sanity check: File should be at least 1KB (bare minimum for any valid cache)
    const size_t min_size = 1024;
    if (st.st_size < min_size) {
        ETHERVOX_LOG_WARN("KV cache file suspiciously small: %s (%lld bytes)", 
                         cache_path, (long long)st.st_size);
        ETHERVOX_LOG_WARN("Deleting likely corrupted cache file");
        unlink(cache_path);
        return false;
    }
    
    ETHERVOX_LOG_INFO("Found KV cache file: %s (%lld bytes)", cache_path, (long long)st.st_size);
    return true;
}

ethervox_result_t ethervox_kv_cache_save(
    const ethervox_paths_t* paths,
    struct ethervox_governor* governor
) {
    if (!paths || !governor) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
#if !LLAMA_AVAILABLE
    ETHERVOX_LOG_ERROR("KV cache save not available: llama.cpp not linked");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    
    // Compute hash-based cache path
    char cache_path[1024];
    ethervox_result_t result = ethervox_kv_cache_get_path(paths, governor, cache_path, sizeof(cache_path));
    if (result != ETHERVOX_SUCCESS) {
        ETHERVOX_LOG_ERROR("Failed to compute cache path");
        return result;
    }
    
    ETHERVOX_LOG_INFO("Saving KV cache to: %s", cache_path);
    
    // Get system prompt tokens
    int token_count = 0;
    llama_token* tokens = ethervox_governor_get_system_tokens(governor, &token_count);
    
    if (!tokens || token_count == 0) {
        ETHERVOX_LOG_ERROR("No system prompt tokens to save");
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    
    ETHERVOX_LOG_INFO("System prompt tokens to save: %d", token_count);
    
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
        
        ETHERVOX_LOG_INFO("Checking cache directory: %s", dir_path);
        
        // Create directory if it doesn't exist
        struct stat st;
        if (stat(dir_path, &st) != 0) {
            // Directory doesn't exist, create it
            ETHERVOX_LOG_WARN("Cache directory doesn't exist, creating: %s", dir_path);
            
            #if defined(__APPLE__)
            // iOS/macOS: Directory should be created by Swift, but create if needed
            if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
                ETHERVOX_LOG_ERROR("Failed to create cache directory: %s (errno=%d: %s)", 
                                  dir_path, errno, strerror(errno));
                return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
            }
            ETHERVOX_LOG_INFO("Created cache directory: %s", dir_path);
            #elif defined(__ANDROID__)
            // Android: create with permissions 0755
            if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
                ETHERVOX_LOG_ERROR("Failed to create cache directory: %s (errno=%d: %s)", 
                                  dir_path, errno, strerror(errno));
                return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
            }
            ETHERVOX_LOG_INFO("Created cache directory: %s", dir_path);
            #else
            // Other Unix systems
            if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
                ETHERVOX_LOG_ERROR("Failed to create cache directory: %s (errno=%d: %s)", 
                                  dir_path, errno, strerror(errno));
                return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
            }
            ETHERVOX_LOG_INFO("Created cache directory: %s", dir_path);
            #endif
        } else {
            ETHERVOX_LOG_INFO("Cache directory already exists: %s", dir_path);
        }
    } else {
        ETHERVOX_LOG_ERROR("Invalid cache path (no directory separator): %s", cache_path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ETHERVOX_LOG_INFO("Calling llama_state_seq_save_file for sequence 1 (permanent)...");
    
    // Use llama.cpp's sequence-based state save function
    // Save sequence 1 which contains the system prompt (generated in seq 1)
    // NOTE: We generate system prompt into seq 1 (permanent), so we save from seq 1
    size_t bytes_written = llama_state_seq_save_file(
        ctx, 
        cache_path,
        1,  // Save sequence 1 (system prompt lives here after generation)
        tokens, 
        (size_t)token_count
    );
    
    if (bytes_written == 0) {
        ETHERVOX_LOG_ERROR("llama_state_seq_save_file() failed for sequence 0 (returned 0 bytes)");
        ETHERVOX_LOG_ERROR("Check file permissions on: %s", cache_path);
        return ETHERVOX_ERROR_FILE_WRITE;
    }
    
    ETHERVOX_LOG_INFO("✓ llama_state_seq_save_file returned %zu bytes", bytes_written);
    
    // CRITICAL: Force flush to disk before verification (iOS file protection issue)
    // llama_state_seq_save_file uses FILE* which is buffered - must sync to disk
    ETHERVOX_LOG_INFO("Forcing file sync to disk...");
    
    #if defined(_WIN32)
    int fd = open(cache_path, O_WRONLY | O_BINARY);
    if (fd >= 0) {
        if (_commit(fd) != 0) {
            ETHERVOX_LOG_WARN("_commit() failed (errno=%d: %s) - file may not be fully written",
                             errno, strerror(errno));
        }
        close(fd);
    }
    #elif defined(__APPLE__)
    // iOS/macOS: Open file in write mode and call fsync() to force kernel to write all buffers to disk
    // MUST use O_WRONLY or O_RDWR - O_RDONLY will not sync writes!
    int fd = open(cache_path, O_WRONLY);
    if (fd >= 0) {
        if (fsync(fd) == 0) {
            ETHERVOX_LOG_INFO("✓ File synced to disk successfully");
        } else {
            ETHERVOX_LOG_WARN("fsync() failed (errno=%d: %s) - file may not be fully written", 
                             errno, strerror(errno));
        }
        close(fd);
    } else {
        ETHERVOX_LOG_WARN("Cannot open file for fsync (errno=%d: %s)", errno, strerror(errno));
    }
    #else
    // Android/other: Use sync() as fallback
    sync();
    ETHERVOX_LOG_INFO("✓ Called sync() to flush buffers");
    #endif
    
    // Small delay to let iOS/macOS file system stabilize (helps with file protection)
    #if defined(__APPLE__)
    usleep(200000);  // 200ms delay (increased from 100ms for better reliability)
    #endif
    
    // Verify file was actually written to disk
    struct stat verify_st;
    if (stat(cache_path, &verify_st) == 0) {
        ETHERVOX_LOG_INFO("✓ Verified file on disk: %lld bytes", (long long)verify_st.st_size);
        
        // Check if file size matches what we wrote
        if ((size_t)verify_st.st_size != bytes_written) {
            ETHERVOX_LOG_ERROR("Cache file size mismatch: disk=%lld bytes, expected=%zu bytes",
                              (long long)verify_st.st_size, bytes_written);
            
            // Only fail if file is significantly smaller (>10% difference)
            // Small differences might be due to file system alignment
            size_t size_diff = bytes_written > (size_t)verify_st.st_size 
                             ? bytes_written - (size_t)verify_st.st_size
                             : (size_t)verify_st.st_size - bytes_written;
            
            if (size_diff > bytes_written / 10) {  // More than 10% difference
                ETHERVOX_LOG_ERROR("File was not fully written - iOS buffering issue");
                ETHERVOX_LOG_ERROR("Deleting incomplete cache file");
                unlink(cache_path);  // Delete corrupted partial file
                return ETHERVOX_ERROR_FILE_WRITE;
            } else {
                ETHERVOX_LOG_WARN("Minor file size difference (%zu bytes) - likely file system alignment",
                                 size_diff);
            }
        }
    } else {
        ETHERVOX_LOG_ERROR("Failed to verify saved file: %s (errno=%d: %s)",
                          cache_path, errno, strerror(errno));
        return ETHERVOX_ERROR_FILE_WRITE;
    }
    
    ETHERVOX_LOG_INFO("✓ KV cache saved: %d tokens + KV state (seq 1 permanent) to %s (%zu bytes)", 
                     token_count, cache_path, bytes_written);
    
    return ETHERVOX_SUCCESS;
#endif
}

ethervox_result_t ethervox_kv_cache_load(
    const ethervox_paths_t* paths,
    struct ethervox_governor* governor
) {
    if (!paths || !governor) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
#if !LLAMA_AVAILABLE
    ETHERVOX_LOG_ERROR("KV cache load not available: llama.cpp not linked");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    
    // Compute hash-based cache path
    char cache_path[1024];
    ethervox_result_t result = ethervox_kv_cache_get_path(paths, governor, cache_path, sizeof(cache_path));
    if (result != ETHERVOX_SUCCESS) {
        ETHERVOX_LOG_ERROR("Failed to compute cache path");
        return result;
    }
    
    ETHERVOX_LOG_INFO("Loading KV cache from: %s", cache_path);
    
    // Check for old (non-sequence) format and delete if found
    // Magic number for old format: "ggsn" (0x6767736e)
    FILE* check_file = fopen(cache_path, "rb");
    if (check_file) {
        uint32_t magic = 0;
        if (fread(&magic, sizeof(magic), 1, check_file) == 1) {
            if (magic == 0x6767736e) {  // Old general state format
                fclose(check_file);
                ETHERVOX_LOG_WARN("Detected old (non-sequence) cache format - deleting for regeneration");
                unlink(cache_path);
                return ETHERVOX_ERROR_FILE_READ;  // Trigger regeneration
            }
        }
        fclose(check_file);
    }
    
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
    
    // Use llama.cpp's sequence-based state load function
    // Load into sequence 1 where system prompt belongs (permanent master)
    size_t n_tokens_loaded = 0;
    size_t bytes_read = llama_state_seq_load_file(
        ctx, 
        cache_path,
        1,  // Load into sequence 1 (system prompt permanent location)
        tokens, 
        max_tokens, 
        &n_tokens_loaded
    );
    
    if (bytes_read == 0) {
        ETHERVOX_LOG_ERROR("llama_state_seq_load_file() failed for sequence 1");
        ETHERVOX_LOG_WARN("Deleting incompatible cache file");
        unlink(cache_path);  // Delete incompatible cache
        free(tokens);
        return ETHERVOX_ERROR_FILE_READ;
    }
    
    // Validate loaded token count
    if (n_tokens_loaded == 0 || n_tokens_loaded < 100) {
        ETHERVOX_LOG_ERROR("Invalid token count loaded: %zu (expected >= 100)", n_tokens_loaded);
        
        // CRITICAL: Clear sequence 1 to avoid leaving it in a corrupt state
        llama_memory_t mem = llama_get_memory(ctx);
        llama_memory_seq_rm(mem, 1, -1, -1);  // Clear all of sequence 1
        ETHERVOX_LOG_WARN("Cleared corrupt sequence 1 from partial cache load");
        
        // Delete the invalid cache file
        unlink(cache_path);
        ETHERVOX_LOG_WARN("Deleted invalid cache file (will regenerate)");
        
        free(tokens);
        return ETHERVOX_ERROR_FILE_READ;
    }
    
    // Store loaded tokens in governor for recovery/reset
    ethervox_governor_set_system_tokens(governor, tokens, (int)n_tokens_loaded);
    
    ETHERVOX_LOG_INFO("✓ KV cache loaded instantly: %zu tokens + KV state (seq 1 permanent) ready (%zu bytes)", 
                     n_tokens_loaded, bytes_read);
    
    return ETHERVOX_SUCCESS;
#endif
}

ethervox_result_t ethervox_kv_cache_get_path(
    const ethervox_paths_t* paths,
    const struct ethervox_governor* governor,
    char* output,
    size_t output_size
) {
    if (!paths || !governor || !output || output_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
#if !LLAMA_AVAILABLE
    (void)paths; (void)governor;
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    
    // Get model path from governor (cast away const - accessor is read-only)
    const char* model_path = ethervox_governor_get_model_path((struct ethervox_governor*)governor);
    if (!model_path) {
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    
    // Compute model file digest
    uint8_t model_digest[32];
    ethervox_result_t result = compute_file_digest(model_path, model_digest);
    if (result != ETHERVOX_SUCCESS) {
        ETHERVOX_LOG_ERROR("Failed to compute model digest: %s", model_path);
        return result;
    }
    
    // prompt_hash is intentionally zeros: tokens aren't available at exists-check time,
    // only at save time, so including them makes the key mismatch and the cache invisible.
    uint8_t prompt_hash[32];
    memset(prompt_hash, 0, sizeof(prompt_hash));
    
    // Get context and model info (cast away const - accessor is read-only)
    struct llama_context* ctx = ethervox_governor_get_llm_context((struct ethervox_governor*)governor);
    if (!ctx) {
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    
    uint32_t context_size = (uint32_t)llama_n_ctx(ctx);
    
    // Get quantization type from model (if available)
    // Note: Quantization detection is complex and version-dependent
    // For now, use 0 as placeholder - the other cache key components are sufficient
    uint32_t quantization = 0;
    
    // Get backend version (use 1 as placeholder if build number not available)
    #ifdef LLAMA_BUILD_NUMBER
    uint32_t backend_version = (uint32_t)LLAMA_BUILD_NUMBER;
    #else
    uint32_t backend_version = 1;
    #endif
    
    // Compute cache key hash
    uint8_t cache_key[32];
    compute_cache_key_hash(model_digest, prompt_hash, context_size, 
                          quantization, backend_version, cache_key);
    
    // Convert hash to hex string (use first 16 bytes = 32 hex chars)
    char hash_hex[65];
    hash_to_hex(cache_key, 16, hash_hex, sizeof(hash_hex));
    
    // Build path: <cache_dir>/kv_cache_<hash>.bin
    int written = snprintf(output, output_size, 
                          "%s/kv_cache_%s.bin",
                          paths->cache_dir, hash_hex);
    
    if (written < 0 || (size_t)written >= output_size) {
        return ETHERVOX_ERROR_BUFFER_TOO_SMALL;
    }
    
    ETHERVOX_LOG_INFO("KV cache path: %s", output);
    return ETHERVOX_SUCCESS;
#endif
}

ethervox_result_t ethervox_kv_cache_usage(
    const ethervox_paths_t* paths,
    ethervox_kv_cache_usage_t* usage
) {
    if (!paths || !usage) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    memset(usage, 0, sizeof(*usage));
    usage->oldest_timestamp = UINT64_MAX;
    usage->newest_timestamp = 0;
    
    DIR* dir = opendir(paths->cache_dir);
    if (!dir) {
        // Directory doesn't exist or can't be opened - that's okay, just means no caches
        return ETHERVOX_SUCCESS;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Look for .bin files (our cache format)
        const char* name = entry->d_name;
        size_t name_len = strlen(name);
        
        if (name_len < 4 || strcmp(name + name_len - 4, ".bin") != 0) {
            continue;  // Not a cache file
        }
        
        // Check if it starts with "kv_cache_"
        if (strncmp(name, "kv_cache_", 9) != 0) {
            continue;  // Not our cache file
        }
        
        // Build full path
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", paths->cache_dir, name);
        
        // Get file info
        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;  // Can't stat file
        }
        
        // Update statistics
        usage->total_bytes += (uint64_t)st.st_size;
        usage->cache_count++;
        
        uint64_t file_time = (uint64_t)st.st_mtime;
        if (file_time < usage->oldest_timestamp) {
            usage->oldest_timestamp = file_time;
        }
        if (file_time > usage->newest_timestamp) {
            usage->newest_timestamp = file_time;
        }
    }
    
    closedir(dir);
    
    // Fix timestamps if no caches found
    if (usage->cache_count == 0) {
        usage->oldest_timestamp = 0;
        usage->newest_timestamp = 0;
    }
    
    ETHERVOX_LOG_INFO("KV cache usage: %u files, %llu bytes, oldest=%llu, newest=%llu",
                     usage->cache_count, (unsigned long long)usage->total_bytes,
                     (unsigned long long)usage->oldest_timestamp,
                     (unsigned long long)usage->newest_timestamp);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_kv_cache_evict_older_than(
    const ethervox_paths_t* paths,
    uint64_t evict_before,
    uint32_t* evicted_count,
    uint64_t* evicted_bytes
) {
    if (!paths) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint32_t count = 0;
    uint64_t bytes = 0;
    
    DIR* dir = opendir(paths->cache_dir);
    if (!dir) {
        // Directory doesn't exist - that's okay, nothing to evict
        if (evicted_count) *evicted_count = 0;
        if (evicted_bytes) *evicted_bytes = 0;
        return ETHERVOX_SUCCESS;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Look for .bin files (our cache format)
        const char* name = entry->d_name;
        size_t name_len = strlen(name);
        
        if (name_len < 4 || strcmp(name + name_len - 4, ".bin") != 0) {
            continue;
        }
        
        // Check if it starts with "kv_cache_"
        if (strncmp(name, "kv_cache_", 9) != 0) {
            continue;
        }
        
        // Build full path
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", paths->cache_dir, name);
        
        // Get file info
        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }
        
        // Check if file is old enough to evict
        if ((uint64_t)st.st_mtime < evict_before) {
            uint64_t file_size = (uint64_t)st.st_size;
            
            // Delete the file
            if (unlink(full_path) == 0) {
                count++;
                bytes += file_size;
                ETHERVOX_LOG_INFO("Evicted cache file: %s (%llu bytes, mtime=%llu)",
                                 name, (unsigned long long)file_size,
                                 (unsigned long long)st.st_mtime);
            } else {
                ETHERVOX_LOG_WARN("Failed to delete cache file: %s (errno=%d: %s)",
                                 full_path, errno, strerror(errno));
            }
        }
    }
    
    closedir(dir);
    
    if (evicted_count) *evicted_count = count;
    if (evicted_bytes) *evicted_bytes = bytes;
    
    ETHERVOX_LOG_INFO("Evicted %u cache files, freed %llu bytes", 
                     count, (unsigned long long)bytes);
    
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
