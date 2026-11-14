/**
 * @file kv_cache_persistence.h
 * @brief KV Cache save/load for fast startup
 *
 * Saves the processed system prompt KV cache to disk on first run,
 * then loads it instantly on subsequent startups (~13s vs 300s).
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_KV_CACHE_PERSISTENCE_H
#define ETHERVOX_KV_CACHE_PERSISTENCE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "error.h"
#include "paths.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct ethervox_governor;
struct llama_context;
struct ethervox_paths;
typedef int32_t llama_token;

/**
 * KV Cache file header (for validation)
 * 
 * Format version 2 (TASK-C1.6):
 * - Keys on hash(model digest ‖ prompt ‖ ctx size ‖ quant ‖ backend version)
 * - Supports multiple models caching simultaneously
 * - Mismatch results in silent miss (never wrong load)
 */
typedef struct {
    uint32_t magic;              // 0x4B564341 ("KVCA")
    uint32_t version;            // Format version (2 for C1.6)
    uint32_t token_count;        // Number of system prompt tokens
    uint32_t kv_size;            // Size of KV cache data in bytes
    uint64_t timestamp;          // Creation timestamp (Unix epoch)
    
    // Cache key components (version 2)
    uint8_t model_digest[32];    // SHA-256 of model file
    uint8_t prompt_hash[32];     // Hash of system prompt text
    uint32_t context_size;       // Context window size
    uint32_t quantization;       // Quantization type (from llama_ftype)
    uint32_t backend_version;    // llama.cpp backend version
    uint32_t reserved[8];        // Reserved for future use
    
    // Validation
    uint8_t checksum[32];        // SHA-256 of (header + tokens + kv_data)
} kv_cache_header_t;

/**
 * Check if a cache file exists for the current governor configuration.
 * 
 * @param paths Path configuration
 * @param governor Governor instance
 * @return true if cache exists and is readable
 */
bool ethervox_kv_cache_exists(
    const ethervox_paths_t* paths,
    const struct ethervox_governor* governor
);

/**
 * Save KV cache to disk using hash-based key
 * 
 * Computes cache key from:
 * - Model file digest
 * - System prompt hash
 * - Context size
 * - Quantization type
 * - Backend version
 * 
 * @param paths Path configuration
 * @param governor Governor instance (must have system prompt loaded)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_kv_cache_save(
    const ethervox_paths_t* paths,
    struct ethervox_governor* governor
);

/**
 * Load KV cache from disk using hash-based key
 * 
 * Computes cache key and loads if file exists.
 * llama.cpp handles validation - mismatches are rejected safely.
 * 
 * @param paths Path configuration
 * @param governor Governor instance
 * @return ETHERVOX_SUCCESS, ETHERVOX_ERROR_CACHE_MISS, or error code
 */
ethervox_result_t ethervox_kv_cache_load(
    const ethervox_paths_t* paths,
    struct ethervox_governor* governor
);

/**
 * Get recommended cache file path
 * 
 * Generates path based on cache key hash:
 * <paths->cache_dir>/kv_cache_<hex_hash>.bin
 * 
 * @param paths Path configuration (uses paths->cache_dir)
 * @param governor Governor instance (for extracting cache key components)
 * @param output Output buffer for cache path
 * @param output_size Size of output buffer
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_kv_cache_get_path(
    const ethervox_paths_t* paths,
    const struct ethervox_governor* governor,
    char* output,
    size_t output_size
);

/**
 * KV cache usage statistics
 */
typedef struct {
    uint64_t total_bytes;        // Total disk space used by all caches
    uint32_t cache_count;        // Number of cache files
    uint64_t oldest_timestamp;   // Timestamp of oldest cache
    uint64_t newest_timestamp;   // Timestamp of newest cache
} ethervox_kv_cache_usage_t;

/**
 * Get KV cache disk usage statistics
 * 
 * Scans paths->cache_dir for all .kvcache files and reports usage.
 * 
 * @param paths Path configuration (uses paths->cache_dir)
 * @param usage Output: usage statistics
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_kv_cache_usage(
    const ethervox_paths_t* paths,
    ethervox_kv_cache_usage_t* usage
);

/**
 * Evict KV cache files older than specified timestamp
 * 
 * Deletes cache files with timestamp < evict_before.
 * Useful for cleanup and managing disk space.
 * 
 * @param paths Path configuration (uses paths->cache_dir)
 * @param evict_before Unix timestamp - delete files older than this
 * @param evicted_count Output: number of files deleted (can be NULL)
 * @param evicted_bytes Output: bytes freed (can be NULL)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_kv_cache_evict_older_than(
    const ethervox_paths_t* paths,
    uint64_t evict_before,
    uint32_t* evicted_count,
    uint64_t* evicted_bytes
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_KV_CACHE_PERSISTENCE_H
