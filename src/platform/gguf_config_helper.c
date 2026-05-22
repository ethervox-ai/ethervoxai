/**
 * @file gguf_config_helper.c
 * @brief Helper functions for extracting optimal config from GGUF metadata
 * 
 * Real implementation using llama.cpp APIs
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/gguf_config_helper.h"
#include "ethervox/logging.h"
#include "ethervox/device_profile.h"
#include <string.h>
#include <stdlib.h>

// Include llama.cpp headers when available
#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
#include <llama.h>
#define LLAMA_HEADER_AVAILABLE 1
#else
#define LLAMA_HEADER_AVAILABLE 0
#endif

/**
 * Extract metadata from loaded GGUF model
 */
int ethervox_extract_model_metadata(void* model, ethervox_model_metadata_t* metadata) {
    if (!model || !metadata) {
        return -1;
    }
    
#if LLAMA_HEADER_AVAILABLE
    // Extract metadata using llama.cpp API (current API as of 2026)
    struct llama_model* llama_model = (struct llama_model*)model;
    
    metadata->context_length_train = llama_model_n_ctx_train(llama_model);
    metadata->layer_count = llama_model_n_layer(llama_model);
    metadata->embedding_dims = llama_model_n_embd(llama_model);
    metadata->model_size_bytes = llama_model_size(llama_model);
    
    // Get vocabulary size via vocab API
    const struct llama_vocab* vocab = llama_model_get_vocab(llama_model);
    if (vocab) {
        metadata->vocab_size = llama_vocab_n_tokens(vocab);
    } else {
        metadata->vocab_size = 0;
    }
    
    // Try to get architecture name
    char arch_buf[256] = {0};
    int arch_len = llama_model_meta_val_str(llama_model, "general.architecture", arch_buf, sizeof(arch_buf));
    if (arch_len > 0) {
        snprintf(metadata->architecture, sizeof(metadata->architecture), "%s", arch_buf);
    } else {
        snprintf(metadata->architecture, sizeof(metadata->architecture), "unknown");
    }
    
    ETHERVOX_LOG_INFO("[GGUF Metadata] Extracted from model:");
    ETHERVOX_LOG_INFO("  Architecture: %s", metadata->architecture);
    ETHERVOX_LOG_INFO("  Context (trained): %d", metadata->context_length_train);
    ETHERVOX_LOG_INFO("  Layers: %d", metadata->layer_count);
    ETHERVOX_LOG_INFO("  Embedding dims: %d", metadata->embedding_dims);
    ETHERVOX_LOG_INFO("  Vocab size: %d", metadata->vocab_size);
    ETHERVOX_LOG_INFO("  Model size: %.2f GB", metadata->model_size_bytes / (1024.0*1024.0*1024.0));
    
    return 0;
#else
    // Stub: Return safe defaults when llama.cpp not available
    ETHERVOX_LOG_WARN("[GGUF Metadata] llama.cpp not available, using safe defaults");
    metadata->context_length_train = 2048;
    metadata->layer_count = 32;
    metadata->embedding_dims = 3072;
    metadata->vocab_size = 32000;
    metadata->model_size_bytes = 2ULL * 1024 * 1024 * 1024;  // 2GB estimate
    snprintf(metadata->architecture, sizeof(metadata->architecture), "unknown");
    return -1;
#endif
}

/**
 * Estimate KV cache memory usage
 */
size_t ethervox_estimate_kv_cache_mb(
    const ethervox_model_metadata_t* metadata,
    int context_length
) {
    // KV cache formula:
    // memory = num_layers * 2 * context_length * sizeof(fp16) * 2 (key+value)
    // 
    // For Phi-3.5 (32 layers, 8192 context, F16):
    // 32 layers * 2 heads * 8192 ctx * 2 bytes * 2 (K+V) = ~2GB
    
    size_t bytes_per_token = metadata->layer_count * 2 * sizeof(uint16_t) * 2;
    size_t total_bytes = bytes_per_token * context_length;
    size_t mb = total_bytes / (1024 * 1024);
    
    return mb;
}

/**
 * Check if device can support requested context length
 */
bool ethervox_can_support_context(
    const ethervox_model_metadata_t* metadata,
    int context_length
) {
    // Initialize device profiling if not already done
    ethervox_device_profile_init();
    
    long total_ram_mb = ethervox_device_profile_get_total_ram_mb();
    size_t model_size_mb = metadata->model_size_bytes / (1024 * 1024);
    size_t kv_cache_mb = ethervox_estimate_kv_cache_mb(metadata, context_length);
    
    // Reserve 1GB for system, app, and working memory
    long reserved_mb = 1024;
    long required_mb = model_size_mb + kv_cache_mb + reserved_mb;
    
    ETHERVOX_LOG_DEBUG("[Memory Check] Context %d requires %ld MB (model=%zu, KV=%zu, reserved=%ld)",
                       context_length, required_mb, model_size_mb, kv_cache_mb, reserved_mb);
    ETHERVOX_LOG_DEBUG("[Memory Check] Device has %ld MB total RAM", total_ram_mb);
    
    return (required_mb <= total_ram_mb);
}

/**
 * Calculate optimal runtime configuration
 */
int ethervox_calculate_optimal_config(
    const ethervox_model_metadata_t* metadata,
    ethervox_runtime_config_t* config
) {
    if (!metadata || !config) {
        return -1;
    }
    
    // Initialize device profiling
    ethervox_device_profile_init();
    
    int device_tier = ethervox_device_profile_get_tier();
    long total_ram_mb = ethervox_device_profile_get_total_ram_mb();
    
    ETHERVOX_LOG_INFO("[Adaptive Config] Calculating optimal settings...");
    ETHERVOX_LOG_INFO("  Model trained context: %d", metadata->context_length_train);
    ETHERVOX_LOG_INFO("  Device tier: %d (0=LOW, 1=MED, 2=HIGH, 3=ULTRA)", device_tier);
    ETHERVOX_LOG_INFO("  Device RAM: %ld MB", total_ram_mb);
    
    // Start with model's maximum context
    int max_context = metadata->context_length_train;
    
    // Scale down based on device tier (UX optimization)
    int tier_max_context[] = {2048, 4096, 8192, 16384};  // LOW, MED, HIGH, ULTRA
    max_context = (max_context < tier_max_context[device_tier]) ? 
                   max_context : tier_max_context[device_tier];
    
    // Verify device can support this context
    while (max_context > 512 && !ethervox_can_support_context(metadata, max_context)) {
        max_context /= 2;  // Halve until it fits
        ETHERVOX_LOG_WARN("[Adaptive Config] Reduced context to %d to fit in RAM", max_context);
    }
    
    // Set optimal context
    config->context_length = max_context;
    
    // Use device profile functions for other settings
    config->threads = ethervox_device_profile_get_optimal_threads();
    config->batch_size = ethervox_device_profile_get_optimal_batch_size();
    config->kv_cache_type = ethervox_device_profile_get_optimal_kv_cache_type();
    config->use_flash_attention = ethervox_device_profile_should_use_flash_attention();
    
    // GPU layers: Use actual layer count from model (llama.cpp auto-adjusts if GPU unavailable)
    // If we have the layer count, use it. Otherwise default to 99 (will be clamped by llama.cpp)
    config->gpu_layers = (metadata->layer_count > 0) ? metadata->layer_count : 99;
    
    ETHERVOX_LOG_INFO("[Adaptive Config] Final settings:");
    ETHERVOX_LOG_INFO("  Context: %d", config->context_length);
    ETHERVOX_LOG_INFO("  Batch size: %d", config->batch_size);
    ETHERVOX_LOG_INFO("  Threads: %d", config->threads);
    ETHERVOX_LOG_INFO("  KV cache type: %d", config->kv_cache_type);
    ETHERVOX_LOG_INFO("  Flash attention: %s", config->use_flash_attention ? "YES" : "NO");
    ETHERVOX_LOG_INFO("  GPU layers: %d", config->gpu_layers);
    
    return 0;
}
