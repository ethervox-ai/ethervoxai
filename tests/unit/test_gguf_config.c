/**
 * @file test_gguf_config.c
 * @brief Unit tests for GGUF metadata extraction and config calculation
 * 
 * Tests model metadata extraction and optimal config calculation.
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/gguf_config_helper.h"
#include "ethervox/device_profile.h"
#include "ethervox/logging.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/**
 * Test metadata extraction (stub mode)
 */
static void test_metadata_extraction_stub(void) {
    printf("Testing GGUF metadata extraction (stub mode)...\n");
    
    ethervox_model_metadata_t metadata;
    memset(&metadata, 0, sizeof(metadata));
    
    // In stub mode (no llama.cpp), should return defaults
    int result = ethervox_extract_model_metadata(NULL, &metadata);
    
    // Should fail gracefully (stub returns -1)
    assert(result == -1);
    
    // But should still populate safe defaults
    assert(metadata.context_length_train == 2048);
    assert(metadata.layer_count == 32);
    assert(metadata.model_size_bytes > 0);
    assert(strcmp(metadata.architecture, "unknown") == 0);
    
    printf("  ✓ Stub mode returns safe defaults\n");
}

/**
 * Test KV cache memory estimation
 */
static void test_kv_cache_estimation(void) {
    printf("Testing KV cache memory estimation...\n");
    
    ethervox_model_metadata_t metadata;
    metadata.context_length_train = 8192;
    metadata.layer_count = 32;
    metadata.embedding_dims = 3072;
    metadata.vocab_size = 32000;
    metadata.model_size_bytes = 2ULL * 1024 * 1024 * 1024;  // 2GB
    
    // Test various context lengths
    size_t kv_2k = ethervox_estimate_kv_cache_mb(&metadata, 2048);
    size_t kv_4k = ethervox_estimate_kv_cache_mb(&metadata, 4096);
    size_t kv_8k = ethervox_estimate_kv_cache_mb(&metadata, 8192);
    
    // KV cache should scale linearly with context
    assert(kv_4k > kv_2k);
    assert(kv_8k > kv_4k);
    assert(kv_8k == kv_4k * 2);  // Should be exactly 2x
    
    // For Phi-3.5 (32 layers), 8K context should be ~2GB
    assert(kv_8k > 1500);  // At least 1.5GB
    assert(kv_8k < 3000);  // Less than 3GB
    
    printf("  ✓ KV cache estimation: 2K=%zu MB, 4K=%zu MB, 8K=%zu MB\n",
           kv_2k, kv_4k, kv_8k);
}

/**
 * Test context support checking
 */
static void test_context_support(void) {
    printf("Testing context support checking...\n");
    
    // Initialize device profiling
    ethervox_device_profile_init();
    
    ethervox_model_metadata_t metadata;
    metadata.context_length_train = 8192;
    metadata.layer_count = 32;
    metadata.model_size_bytes = 2ULL * 1024 * 1024 * 1024;  // 2GB
    
    // Small context should always be supported
    bool can_support_2k = ethervox_can_support_context(&metadata, 2048);
    assert(can_support_2k == true);
    
    // Very large context might not be supported on low-RAM devices
    bool can_support_32k = ethervox_can_support_context(&metadata, 32768);
    // Don't assert - depends on device RAM
    
    printf("  ✓ Context support: 2K=%s, 32K=%s\n",
           can_support_2k ? "YES" : "NO",
           can_support_32k ? "YES" : "NO");
}

/**
 * Test optimal config calculation
 */
static void test_optimal_config_calculation(void) {
    printf("Testing optimal config calculation...\n");
    
    // Initialize device profiling
    ethervox_device_profile_init();
    
    // Simulate Phi-3.5 model
    ethervox_model_metadata_t metadata;
    metadata.context_length_train = 8192;
    metadata.layer_count = 32;
    metadata.embedding_dims = 3072;
    metadata.vocab_size = 32000;
    metadata.model_size_bytes = 2ULL * 1024 * 1024 * 1024;
    snprintf(metadata.architecture, sizeof(metadata.architecture), "phi");
    
    ethervox_runtime_config_t config;
    memset(&config, 0, sizeof(config));
    
    int result = ethervox_calculate_optimal_config(&metadata, &config);
    assert(result == 0);
    
    // Context should be reasonable
    assert(config.context_length >= 2048);
    assert(config.context_length <= metadata.context_length_train);
    
    // Batch size should be valid
    assert(config.batch_size == 256 || config.batch_size == 512 || config.batch_size == 1024);
    
    // Threads should be reasonable
    assert(config.threads >= 2);
    assert(config.threads <= 6);
    
    // KV cache type should be valid
    assert(config.kv_cache_type == 1 || config.kv_cache_type == 7 || config.kv_cache_type == 8);
    
    printf("  ✓ Calculated config: context=%d, batch=%d, threads=%d, kv_type=%d\n",
           config.context_length, config.batch_size, config.threads, config.kv_cache_type);
}

/**
 * Test tier-based context scaling
 */
static void test_tier_based_scaling(void) {
    printf("Testing tier-based context scaling...\n");
    
    ethervox_device_profile_init();
    
    ethervox_model_metadata_t metadata;
    metadata.context_length_train = 16384;  // Large context model
    metadata.layer_count = 32;
    metadata.model_size_bytes = 2ULL * 1024 * 1024 * 1024;
    
    ethervox_runtime_config_t config;
    ethervox_calculate_optimal_config(&metadata, &config);
    
    int tier = ethervox_device_profile_get_tier();
    
    // Context should be capped based on tier
    int tier_max_contexts[] = {2048, 4096, 8192, 16384};
    assert(config.context_length <= tier_max_contexts[tier]);
    
    printf("  ✓ Context capped at %d for tier %d\n", config.context_length, tier);
}

/**
 * Test config consistency with device profile
 */
static void test_config_consistency(void) {
    printf("Testing config consistency with device profile...\n");
    
    ethervox_device_profile_init();
    
    ethervox_model_metadata_t metadata;
    metadata.context_length_train = 8192;
    metadata.layer_count = 32;
    metadata.model_size_bytes = 2ULL * 1024 * 1024 * 1024;
    
    ethervox_runtime_config_t config;
    ethervox_calculate_optimal_config(&metadata, &config);
    
    // Config should match device profile recommendations
    int profile_threads = ethervox_device_profile_get_optimal_threads();
    int profile_batch = ethervox_device_profile_get_optimal_batch_size();
    int profile_kv = ethervox_device_profile_get_optimal_kv_cache_type();
    bool profile_flash = ethervox_device_profile_should_use_flash_attention();
    
    assert(config.threads == profile_threads);
    assert(config.batch_size == profile_batch);
    assert(config.kv_cache_type == profile_kv);
    assert(config.use_flash_attention == profile_flash);
    
    printf("  ✓ Config matches device profile recommendations\n");
}

/**
 * Test memory-constrained scenarios
 */
static void test_memory_constraints(void) {
    printf("Testing memory-constrained scenarios...\n");
    
    ethervox_device_profile_init();
    long total_ram_mb = ethervox_device_profile_get_total_ram_mb();
    
    // Simulate large model (4GB)
    ethervox_model_metadata_t large_model;
    large_model.context_length_train = 32768;
    large_model.layer_count = 32;
    large_model.model_size_bytes = 4ULL * 1024 * 1024 * 1024;
    
    ethervox_runtime_config_t config;
    ethervox_calculate_optimal_config(&large_model, &config);
    
    // Context should be reduced to fit in RAM
    size_t kv_cache_mb = ethervox_estimate_kv_cache_mb(&large_model, config.context_length);
    size_t model_mb = large_model.model_size_bytes / (1024 * 1024);
    size_t total_needed = model_mb + kv_cache_mb + 1024;  // +1GB reserved
    
    // Should fit in available RAM (with some tolerance for overheads)
    assert(total_needed <= (size_t)(total_ram_mb * 1.2));  // 20% tolerance
    
    printf("  ✓ Config respects memory constraints: %zu MB needed, %ld MB available\n",
           total_needed, total_ram_mb);
}

/**
 * Test NULL pointer handling
 */
static void test_null_handling(void) {
    printf("Testing NULL pointer handling...\n");
    
    ethervox_model_metadata_t metadata;
    ethervox_runtime_config_t config;
    
    // NULL inputs should be handled gracefully
    assert(ethervox_extract_model_metadata(NULL, NULL) == -1);
    assert(ethervox_extract_model_metadata(NULL, &metadata) == -1);
    
    assert(ethervox_calculate_optimal_config(NULL, &config) == -1);
    assert(ethervox_calculate_optimal_config(&metadata, NULL) == -1);
    assert(ethervox_calculate_optimal_config(NULL, NULL) == -1);
    
    printf("  ✓ NULL pointers handled safely\n");
}

/**
 * Test small model optimization
 */
static void test_small_model_optimization(void) {
    printf("Testing small model optimization...\n");
    
    ethervox_device_profile_init();
    
    // Simulate small model (1GB, 2K context)
    ethervox_model_metadata_t small_model;
    small_model.context_length_train = 2048;
    small_model.layer_count = 16;
    small_model.model_size_bytes = 1ULL * 1024 * 1024 * 1024;
    
    ethervox_runtime_config_t config;
    ethervox_calculate_optimal_config(&small_model, &config);
    
    // Should use model's full context (it's small)
    assert(config.context_length == small_model.context_length_train);
    
    printf("  ✓ Small model uses full context: %d\n", config.context_length);
}

int main(void) {
    printf("=== GGUF Config Helper Tests ===\n\n");
    
    test_metadata_extraction_stub();
    test_kv_cache_estimation();
    test_context_support();
    test_optimal_config_calculation();
    test_tier_based_scaling();
    test_config_consistency();
    test_memory_constraints();
    test_null_handling();
    test_small_model_optimization();
    
    printf("\n=== All GGUF Config Helper Tests Passed ===\n");
    return 0;
}
