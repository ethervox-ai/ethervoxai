/**
 * @file kv_cache_persistence.h
 * @brief KV Cache save/load for fast startup
 *
 * Saves the processed system prompt KV cache to disk on first run,
 * then loads it instantly on subsequent startups (~13s vs 300s).
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_KV_CACHE_PERSISTENCE_H
#define ETHERVOX_KV_CACHE_PERSISTENCE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct ethervox_governor;
struct llama_context;
typedef int32_t llama_token;

/**
 * KV Cache file header (for validation)
 */
typedef struct {
    uint32_t magic;              // 0x4B564341 ("KVCA")
    uint32_t version;            // Format version (currently 1)
    uint32_t token_count;        // Number of system prompt tokens
    uint32_t kv_size;            // Size of KV cache data in bytes
    uint64_t timestamp;          // Creation timestamp
    char model_name[128];        // Model filename for validation
    uint8_t checksum[32];        // SHA-256 of (tokens + kv_data)
} kv_cache_header_t;

/**
 * Check if KV cache file exists and is valid
 * 
 * @param cache_path Path to .kvcache file
 * @param model_path Path to model file (for validation)
 * @return true if cache exists and matches model
 */
bool ethervox_kv_cache_exists(const char* cache_path, const char* model_path);

/**
 * Save KV cache to disk
 * 
 * Serializes:
 * - System prompt tokens
 * - KV cache state for those tokens
 * - Metadata for validation
 * 
 * @param governor Governor instance (must have system prompt loaded)
 * @param cache_path Path to save .kvcache file
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_kv_cache_save(
    struct ethervox_governor* governor,
    const char* cache_path
);

/**
 * Load KV cache from disk
 * 
 * Deserializes and validates:
 * - System prompt tokens
 * - KV cache state
 * - Checksum verification
 * 
 * @param governor Governor instance (model must be loaded)
 * @param cache_path Path to .kvcache file
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_kv_cache_load(
    struct ethervox_governor* governor,
    const char* cache_path
);

/**
 * Get recommended cache file path
 * 
 * Returns: <files_dir>/cache/system_prompt_{model_name}.kvcache
 * 
 * @param model_path Path to model file
 * @param output Output buffer for cache path
 * @param output_size Size of output buffer
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_kv_cache_get_path(
    const char* model_path,
    const char* files_dir,
    char* output,
    size_t output_size
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_KV_CACHE_PERSISTENCE_H
