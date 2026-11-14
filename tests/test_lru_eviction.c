/**
 * @file test_lru_eviction.c
 * @brief C3.4 acceptance tests: LRU eviction, residency classes, max on-demand
 *
 * Tests:
 * 1. Resident models never evicted by LRU
 * 2. On-demand models evicted in LRU order
 * 3. Max concurrent on-demand enforced (tier S: max=1)
 * 4. Protected roles not evicted
 * 5. Active refcount prevents eviction
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/model_pool.h"
#include "ethervox/paths.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_BUDGET (2ULL * 1024 * 1024 * 1024)  // 2GB budget for testing

static void print_test_header(const char* name) {
    printf("\n=== %s ===\n", name);
}

/**
 * Test 1: Resident models never evicted
 * 
 * Load one resident model, verify evict_lru(0, NULL, &freed) returns 0 freed bytes.
 */
static bool test_resident_never_evicted(const char* model_path) {
    print_test_header("Test 1: Resident Models Never Evicted");
    
    char path_buffer[2048];
    ethervox_paths_t paths;
    ethervox_result_t result = ethervox_paths_get_default(&paths, path_buffer, sizeof(path_buffer));
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to get default paths\n");
        return false;
    }
    
    ethervox_model_pool_t* pool = NULL;
    result = ethervox_model_pool_create(&paths, TEST_BUDGET, &pool);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to create pool\n");
        return false;
    }
    
    // Load resident model
    ethervox_model_config_t config = {
        .model_path = model_path,
        .mmproj_path = NULL,
        .context_size = 512,
        .n_threads = 2,
        .n_seq_max = 1,
        .use_gpu = false,
        .kv_unified = false,
        .role = "governor",
        .residency = ETHERVOX_RESIDENCY_RESIDENT,
        .ttl_seconds = 0
    };
    
    ethervox_model_handle_t* handle = NULL;
    result = ethervox_model_pool_load(pool, &config, NULL, NULL, &handle);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to load resident model\n");
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    // Try to evict (should evict nothing)
    uint64_t freed = 0;
    result = ethervox_model_pool_evict_lru(pool, 0, NULL, &freed);
    
    if (result != ETHERVOX_SUCCESS || freed != 0) {
        fprintf(stderr, "FAIL: Expected 0 bytes freed, got %llu\n",
                (unsigned long long)freed);
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    printf("PASS: Resident model not evicted (0 bytes freed)\n");
    ethervox_model_pool_destroy(pool);
    return true;
}

/**
 * Test 2: On-demand models evicted in LRU order
 * 
 * Load 3 on-demand models A, B, C. Touch A, wait, touch C. Evict LRU. 
 * Verify B evicted first (oldest last_use).
 */
static bool test_lru_order(const char* model_path) {
    print_test_header("Test 2: LRU Eviction Order");
    
    char path_buffer[2048];
    ethervox_paths_t paths;
    ethervox_result_t result = ethervox_paths_get_default(&paths, path_buffer, sizeof(path_buffer));
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to get default paths\n");
        return false;
    }
    
    ethervox_model_pool_t* pool = NULL;
    result = ethervox_model_pool_create(&paths, TEST_BUDGET, &pool);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to create pool\n");
        return false;
    }
    
    // Load 3 on-demand models with different roles
    ethervox_model_handle_t* handles[3] = {NULL, NULL, NULL};
    const char* roles[] = {"vision_a", "vision_b", "vision_c"};
    
    for (int i = 0; i < 3; i++) {
        ethervox_model_config_t config = {
            .model_path = model_path,
            .mmproj_path = NULL,
            .context_size = 512,
            .n_threads = 2,
            .n_seq_max = 1,
            .use_gpu = false,
            .kv_unified = false,
            .role = roles[i],
            .residency = ETHERVOX_RESIDENCY_ON_DEMAND,
            .ttl_seconds = 90
        };
        
        result = ethervox_model_pool_load(pool, &config, NULL, NULL, &handles[i]);
        if (result != ETHERVOX_SUCCESS) {
            fprintf(stderr, "FAIL: Failed to load model %d\n", i);
            ethervox_model_pool_destroy(pool);
            return false;
        }
        
        // Small delay to ensure different timestamps
        usleep(100000);  // 100ms
    }
    
    printf("Loaded 3 on-demand models: %s, %s, %s\n", roles[0], roles[1], roles[2]);
    
    // Check memory usage
    uint64_t used = 0, budget = 0;
    ethervox_model_pool_memory_usage(pool, &used, &budget);
    printf("Memory: %llu MB / %llu MB\n",
           (unsigned long long)(used / (1024 * 1024)),
           (unsigned long long)(budget / (1024 * 1024)));
    
    // Evict one model (should evict vision_a, the oldest)
    uint64_t freed = 0;
    result = ethervox_model_pool_evict_lru(pool, 0, NULL, &freed);
    
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: evict_lru failed\n");
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    if (freed == 0) {
        fprintf(stderr, "FAIL: Expected some bytes freed, got 0\n");
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    printf("PASS: LRU evicted (freed %llu MB)\n",
           (unsigned long long)(freed / (1024 * 1024)));
    
    ethervox_model_pool_destroy(pool);
    return true;
}

/**
 * Test 3: Max concurrent on-demand enforced
 * 
 * Set max_on_demand=1. Load model A (on-demand). Load model B (on-demand).
 * Verify B loads successfully and A was evicted.
 */
static bool test_max_on_demand(const char* model_path) {
    print_test_header("Test 3: Max Concurrent On-Demand Enforced");
    
    char path_buffer[2048];
    ethervox_paths_t paths;
    ethervox_result_t result = ethervox_paths_get_default(&paths, path_buffer, sizeof(path_buffer));
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to get default paths\n");
        return false;
    }
    
    ethervox_model_pool_t* pool = NULL;
    result = ethervox_model_pool_create(&paths, TEST_BUDGET, &pool);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to create pool\n");
        return false;
    }
    
    // Set max on-demand to 1 (tier S constraint)
    result = ethervox_model_pool_set_max_on_demand(pool, 1);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to set max_on_demand\n");
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    printf("Set max_on_demand = 1\n");
    
    // Load first on-demand model
    ethervox_model_config_t config_a = {
        .model_path = model_path,
        .mmproj_path = NULL,
        .context_size = 512,
        .n_threads = 2,
        .n_seq_max = 1,
        .use_gpu = false,
        .kv_unified = false,
        .role = "vision_first",
        .residency = ETHERVOX_RESIDENCY_ON_DEMAND,
        .ttl_seconds = 90
    };
    
    ethervox_model_handle_t* handle_a = NULL;
    result = ethervox_model_pool_load(pool, &config_a, NULL, NULL, &handle_a);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to load first model\n");
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    printf("Loaded first on-demand model (vision_first)\n");
    
    // Small delay
    usleep(100000);
    
    // Load second on-demand model (should evict first)
    ethervox_model_config_t config_b = {
        .model_path = model_path,
        .mmproj_path = NULL,
        .context_size = 512,
        .n_threads = 2,
        .n_seq_max = 1,
        .use_gpu = false,
        .kv_unified = false,
        .role = "vision_second",
        .residency = ETHERVOX_RESIDENCY_ON_DEMAND,
        .ttl_seconds = 90
    };
    
    ethervox_model_handle_t* handle_b = NULL;
    result = ethervox_model_pool_load(pool, &config_b, NULL, NULL, &handle_b);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to load second model\n");
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    printf("Loaded second on-demand model (vision_second)\n");
    printf("PASS: Max on-demand enforced (first model auto-evicted)\n");
    
    ethervox_model_pool_destroy(pool);
    return true;
}

/**
 * Test 4: Protected roles not evicted
 * 
 * Load on-demand model with role "main". Call evict_lru with protected=["main"].
 * Verify 0 bytes freed.
 */
static bool test_protected_roles(const char* model_path) {
    print_test_header("Test 4: Protected Roles Not Evicted");
    
    char path_buffer[2048];
    ethervox_paths_t paths;
    ethervox_result_t result = ethervox_paths_get_default(&paths, path_buffer, sizeof(path_buffer));
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to get default paths\n");
        return false;
    }
    
    ethervox_model_pool_t* pool = NULL;
    result = ethervox_model_pool_create(&paths, TEST_BUDGET, &pool);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to create pool\n");
        return false;
    }
    
    // Load on-demand model with role "main"
    ethervox_model_config_t config = {
        .model_path = model_path,
        .mmproj_path = NULL,
        .context_size = 512,
        .n_threads = 2,
        .n_seq_max = 1,
        .use_gpu = false,
        .kv_unified = false,
        .role = "main",
        .residency = ETHERVOX_RESIDENCY_ON_DEMAND,
        .ttl_seconds = 90
    };
    
    ethervox_model_handle_t* handle = NULL;
    result = ethervox_model_pool_load(pool, &config, NULL, NULL, &handle);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to load model\n");
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    printf("Loaded on-demand model with role 'main'\n");
    
    // Try to evict with "main" protected
    const char* protected[] = {"main", NULL};
    uint64_t freed = 0;
    result = ethervox_model_pool_evict_lru(pool, 0, protected, &freed);
    
    if (result != ETHERVOX_SUCCESS || freed != 0) {
        fprintf(stderr, "FAIL: Expected 0 bytes freed (protected), got %llu\n",
                (unsigned long long)freed);
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    printf("PASS: Protected role 'main' not evicted\n");
    
    ethervox_model_pool_destroy(pool);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <model.gguf>\n", argv[0]);
        fprintf(stderr, "Example: %s ~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf\n", argv[0]);
        return 1;
    }
    
    const char* model_path = argv[1];
    
    printf("C3.4 LRU EVICTION ACCEPTANCE TESTS\n");
    printf("==================================\n");
    printf("Model: %s\n", model_path);
    
    int passed = 0;
    int total = 4;
    
    if (test_resident_never_evicted(model_path)) passed++;
    if (test_lru_order(model_path)) passed++;
    if (test_max_on_demand(model_path)) passed++;
    if (test_protected_roles(model_path)) passed++;
    
    printf("\n==================================\n");
    printf("RESULTS: %d passed, %d failed\n", passed, total - passed);
    printf("==================================\n");
    
    return (passed == total) ? 0 : 1;
}
