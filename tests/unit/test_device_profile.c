/**
 * @file test_device_profile.c
 * @brief Unit tests for device profiling and adaptive configuration
 * 
 * Tests hardware detection, tier classification, and optimal parameter selection.
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/device_profile.h"
#include "ethervox/logging.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Test basic device profiling initialization
 */
static void test_device_profile_init(void) {
    printf("Testing device_profile_init...\n");
    
    // Initialize should not crash
    ethervox_device_profile_init();
    
    // Should be able to call multiple times safely
    ethervox_device_profile_init();
    
    printf("  ✓ Device profile initialization works\n");
}

/**
 * Test CPU core detection
 */
static void test_cpu_cores(void) {
    printf("Testing CPU core detection...\n");
    
    ethervox_device_profile_init();
    int cores = ethervox_device_profile_get_cpu_cores();
    
    // Should return reasonable values (1-64 cores)
    assert(cores >= 1);
    assert(cores <= 64);
    
    printf("  ✓ Detected %d CPU cores\n", cores);
}

/**
 * Test RAM detection
 */
static void test_ram_detection(void) {
    printf("Testing RAM detection...\n");
    
    ethervox_device_profile_init();
    long total_ram_mb = ethervox_device_profile_get_total_ram_mb();
    
    // Should return reasonable values (512MB - 64GB)
    assert(total_ram_mb >= 512);
    assert(total_ram_mb <= 65536);
    
    printf("  ✓ Detected %ld MB total RAM\n", total_ram_mb);
}

/**
 * Test device tier classification
 */
static void test_device_tier(void) {
    printf("Testing device tier classification...\n");
    
    ethervox_device_profile_init();
    int tier = ethervox_device_profile_get_tier();
    
    // Should return valid tier (0-3)
    assert(tier >= 0);
    assert(tier <= 3);
    
    const char* tier_names[] = {"LOW", "MEDIUM", "HIGH", "ULTRA"};
    printf("  ✓ Device classified as: %s tier\n", tier_names[tier]);
}

/**
 * Test optimal thread count selection
 */
static void test_optimal_threads(void) {
    printf("Testing optimal thread count...\n");
    
    ethervox_device_profile_init();
    int threads = ethervox_device_profile_get_optimal_threads();
    
    // Should return reasonable thread count (2-6)
    assert(threads >= 2);
    assert(threads <= 6);
    
    printf("  ✓ Optimal threads: %d\n", threads);
}

/**
 * Test optimal batch size selection
 */
static void test_optimal_batch_size(void) {
    printf("Testing optimal batch size...\n");
    
    ethervox_device_profile_init();
    int batch = ethervox_device_profile_get_optimal_batch_size();
    
    // Should return valid batch sizes (256, 512, or 1024)
    assert(batch == 256 || batch == 512 || batch == 1024);
    
    printf("  ✓ Optimal batch size: %d\n", batch);
}

/**
 * Test KV cache type selection
 */
static void test_optimal_kv_cache_type(void) {
    printf("Testing optimal KV cache type...\n");
    
    ethervox_device_profile_init();
    int kv_type = ethervox_device_profile_get_optimal_kv_cache_type();
    
    // Should return valid GGML type (1=F16, 7=Q8_0, 8=Q4_0)
    assert(kv_type == 1 || kv_type == 7 || kv_type == 8);
    
    const char* type_names[] = {"", "F16", "", "", "", "", "", "Q8_0", "Q4_0"};
    printf("  ✓ Optimal KV cache type: %s (enum %d)\n", 
           (kv_type < 9) ? type_names[kv_type] : "unknown", kv_type);
}

/**
 * Test flash attention decision
 */
static void test_flash_attention(void) {
    printf("Testing flash attention decision...\n");
    
    ethervox_device_profile_init();
    bool flash = ethervox_device_profile_should_use_flash_attention();
    
    // Should return boolean
    assert(flash == true || flash == false);
    
    printf("  ✓ Flash attention: %s\n", flash ? "ENABLED" : "DISABLED");
}

/**
 * Test tier-specific settings consistency
 */
static void test_tier_consistency(void) {
    printf("Testing tier-specific settings consistency...\n");
    
    ethervox_device_profile_init();
    
    int tier = ethervox_device_profile_get_tier();
    int threads = ethervox_device_profile_get_optimal_threads();
    int batch = ethervox_device_profile_get_optimal_batch_size();
    int kv_type = ethervox_device_profile_get_optimal_kv_cache_type();
    bool flash = ethervox_device_profile_should_use_flash_attention();
    
    // Verify tier-specific expectations
    switch (tier) {
        case 0:  // LOW
            assert(threads == 2);
            assert(batch == 256);
            assert(flash == false);
            break;
        case 1:  // MEDIUM
            assert(threads == 4);
            assert(batch == 512);
            assert(flash == true);
            break;
        case 2:  // HIGH
            assert(threads == 4);
            assert(batch == 1024);
            assert(flash == true);
            break;
        case 3:  // ULTRA
            assert(threads == 6);
            assert(batch == 1024);
            assert(flash == true);
            break;
    }
    
    printf("  ✓ Tier-specific settings are consistent\n");
}

/**
 * Test multiple initialization calls
 */
static void test_reinit_safety(void) {
    printf("Testing re-initialization safety...\n");
    
    // Initialize multiple times
    ethervox_device_profile_init();
    int tier1 = ethervox_device_profile_get_tier();
    
    ethervox_device_profile_init();
    int tier2 = ethervox_device_profile_get_tier();
    
    ethervox_device_profile_init();
    int tier3 = ethervox_device_profile_get_tier();
    
    // Should return same values
    assert(tier1 == tier2);
    assert(tier2 == tier3);
    
    printf("  ✓ Re-initialization is safe and idempotent\n");
}

/**
 * Test that settings are appropriate for detected hardware
 */
static void test_settings_appropriateness(void) {
    printf("Testing settings appropriateness for hardware...\n");
    
    ethervox_device_profile_init();
    
    int cores = ethervox_device_profile_get_cpu_cores();
    long ram_mb = ethervox_device_profile_get_total_ram_mb();
    int threads = ethervox_device_profile_get_optimal_threads();
    int batch = ethervox_device_profile_get_optimal_batch_size();
    
    // Threads should not exceed CPU cores
    assert(threads <= cores);
    
    // Batch size should be reasonable for RAM
    // 1024 batch needs ~2GB RAM during processing
    if (ram_mb < 4096) {
        assert(batch <= 512);  // Low/medium RAM should use smaller batches
    }
    
    printf("  ✓ Settings are appropriate for detected hardware\n");
    printf("    Hardware: %d cores, %ld MB RAM\n", cores, ram_mb);
    printf("    Settings: %d threads, %d batch size\n", threads, batch);
}

int main(void) {
    printf("=== Device Profile Tests ===\n\n");
    
    test_device_profile_init();
    test_cpu_cores();
    test_ram_detection();
    test_device_tier();
    test_optimal_threads();
    test_optimal_batch_size();
    test_optimal_kv_cache_type();
    test_flash_attention();
    test_tier_consistency();
    test_reinit_safety();
    test_settings_appropriateness();
    
    printf("\n=== All Device Profile Tests Passed ===\n");
    return 0;
}
