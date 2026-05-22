/**
 * @file gguf_config_helper.h
 * @brief Helper functions for extracting optimal config from GGUF metadata
 * 
 * FUTURE ENHANCEMENT - Not yet implemented
 * Shows how GGUF metadata could be combined with device profiling
 * for truly adaptive configuration.
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_GGUF_CONFIG_HELPER_H
#define ETHERVOX_GGUF_CONFIG_HELPER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ethervox/device_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Model metadata extracted from GGUF file
 */
typedef struct {
    int context_length_train;     // Model's training context (e.g., 8192)
    int layer_count;              // Number of transformer layers
    int embedding_dims;           // Embedding dimensions
    int vocab_size;               // Vocabulary size
    size_t model_size_bytes;      // Total model size in memory
    char architecture[64];        // e.g., "llama", "phi", "mistral"
} ethervox_model_metadata_t;

/**
 * Optimal runtime configuration calculated from model + device
 */
typedef struct {
    int context_length;           // Optimal context for this device
    int batch_size;               // Optimal batch size
    int threads;                  // Optimal thread count
    int kv_cache_type;            // Optimal KV cache quantization (GGML type)
    bool use_flash_attention;     // Whether to enable flash attention
    int gpu_layers;               // How many layers to offload to GPU
} ethervox_runtime_config_t;

/**
 * Extract metadata from loaded GGUF model
 * 
 * @param model Pointer to llama_model (void* to avoid llama.h dependency)
 * @param metadata Output structure to fill
 * @return 0 on success, -1 on error
 * 
 * Example:
 *   llama_model* model = llama_load_model_from_file(path, params);
 *   ethervox_model_metadata_t meta;
 *   ethervox_extract_model_metadata(model, &meta);
 */
int ethervox_extract_model_metadata(void* model, ethervox_model_metadata_t* metadata);

/**
 * Calculate optimal runtime configuration
 * 
 * Combines model metadata with device capabilities to determine
 * the best settings for this specific model + device combination.
 * 
 * @param metadata Model metadata from GGUF
 * @param config Output runtime configuration
 * @return 0 on success, -1 on error
 * 
 * Example:
 *   ethervox_model_metadata_t meta;
 *   ethervox_extract_model_metadata(model, &meta);
 *   
 *   ethervox_runtime_config_t config;
 *   ethervox_calculate_optimal_config(&meta, &config);
 *   
 *   // Apply to llama.cpp
 *   ctx_params.n_ctx = config.context_length;
 *   ctx_params.n_batch = config.batch_size;
 *   ctx_params.n_threads = config.threads;
 */
int ethervox_calculate_optimal_config(
    const ethervox_model_metadata_t* metadata,
    ethervox_runtime_config_t* config
);

/**
 * Estimate KV cache memory usage
 * 
 * @param metadata Model metadata
 * @param context_length Context length to estimate for
 * @return Estimated KV cache size in MB
 */
size_t ethervox_estimate_kv_cache_mb(
    const ethervox_model_metadata_t* metadata,
    int context_length
);

/**
 * Check if device can support requested context length
 * 
 * @param metadata Model metadata  
 * @param context_length Desired context length
 * @return true if device has sufficient RAM, false otherwise
 */
bool ethervox_can_support_context(
    const ethervox_model_metadata_t* metadata,
    int context_length
);

#ifdef __cplusplus
}
#endif

#endif  // ETHERVOX_GGUF_CONFIG_HELPER_H
