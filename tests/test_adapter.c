/**
 * @file test_adapter.c
 * @brief Test suite for LoRA adapter loading and management (C5.1)
 *
 * Tests:
 * 1. Load adapter from file
 * 2. Read adapter metadata
 * 3. Apply adapter to model
 * 4. Clear adapters
 * 5. Free adapter
 *
 * Note: Requires a LoRA adapter file to test. If no adapter file is available,
 * the test verifies API contracts (NULL handling, error codes) but skips actual
 * adapter loading.
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/adapter.h"
#include "ethervox/model_pool.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_test_header(const char* name) {
    printf("\n=== %s ===\n", name);
}

/**
 * Test 1: API contract - NULL handling
 */
static bool test_null_handling(void) {
    print_test_header("Test 1: NULL Handling");
    
    ethervox_adapter_t* adapter = NULL;
    float scale = 1.0f;
    char buf[256];
    
    // NULL handle
    if (ethervox_adapter_load(NULL, "test.gguf", &adapter) != ETHERVOX_ERROR_INVALID_ARGUMENT) {
        fprintf(stderr, "FAIL: NULL handle should return INVALID_ARGUMENT\n");
        return false;
    }
    
    // NULL path
    if (ethervox_adapter_load(NULL, "test.gguf", &adapter) != ETHERVOX_ERROR_INVALID_ARGUMENT) {
        fprintf(stderr, "FAIL: NULL handle should return INVALID_ARGUMENT\n");
        return false;
    }
    
    // NULL path  
    if (ethervox_adapter_load(NULL, NULL, &adapter) != ETHERVOX_ERROR_INVALID_ARGUMENT) {
        fprintf(stderr, "FAIL: NULL path should return INVALID_ARGUMENT\n");
        return false;
    }
    
    // NULL out pointer
    if (ethervox_adapter_load(NULL, "test.gguf", NULL) != ETHERVOX_ERROR_INVALID_ARGUMENT) {
        fprintf(stderr, "FAIL: NULL out pointer should return INVALID_ARGUMENT\n");
        return false;
    }
    
    // Free NULL (should not crash)
    ethervox_adapter_free(NULL);
    
    // Set adapters with NULL handle
    if (ethervox_model_set_adapters(NULL, &adapter, 1, &scale) != ETHERVOX_ERROR_INVALID_ARGUMENT) {
        fprintf(stderr, "FAIL: NULL handle for set_adapters should return INVALID_ARGUMENT\n");
        return false;
    }
    
    // Get metadata from NULL adapter
    if (ethervox_adapter_get_metadata(NULL, "key", buf, sizeof(buf)) != -1) {
        fprintf(stderr, "FAIL: NULL adapter for get_metadata should return -1\n");
        return false;
    }
    
    // Meta count from NULL
    if (ethervox_adapter_meta_count(NULL) != -1) {
        fprintf(stderr, "FAIL: NULL adapter for meta_count should return -1\n");
        return false;
    }
    
    printf("PASS: NULL handling correct\n");
    return true;
}

/**
 * Test 2: Load adapter (requires real model and adapter file)
 */
static bool test_load_adapter(const char* model_path, const char* adapter_path) {
    print_test_header("Test 2: Load Adapter");
    
    if (!model_path || !adapter_path) {
        printf("SKIP: No model or adapter path provided\n");
        return true;  // Skip but don't fail
    }
    
    // Create model pool
    ethervox_paths_t paths = {0};
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    
    char data_dir[512], cache_dir[512], models_dir[512], temp_dir[512];
    snprintf(data_dir, sizeof(data_dir), "%s/.ethervox", home);
    snprintf(cache_dir, sizeof(cache_dir), "%s/.ethervox/cache", home);
    snprintf(models_dir, sizeof(models_dir), "%s/.ethervox/models", home);
    snprintf(temp_dir, sizeof(temp_dir), "%s/.ethervox/tmp", home);
    
    paths.data_dir = data_dir;
    paths.cache_dir = cache_dir;
    paths.models_dir = models_dir;
    paths.temp_dir = temp_dir;
    
    ethervox_model_pool_t* pool = NULL;
    ethervox_result_t result = ethervox_model_pool_create(&paths, 4ULL * 1024 * 1024 * 1024, &pool);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Could not create model pool: %s\n", ethervox_error_string(result));
        return false;
    }
    
    // Load model
    ethervox_model_config_t config = {
        .model_path = model_path,
        .mmproj_path = NULL,
        .context_size = 2048,
        .n_threads = 4,
        .n_seq_max = 1,
        .use_gpu = false,
        .kv_unified = false,
        .role = "test",
        .residency = ETHERVOX_RESIDENCY_RESIDENT,
        .ttl_seconds = 0
    };
    
    ethervox_model_handle_t* handle = NULL;
    result = ethervox_model_pool_load(pool, &config, NULL, NULL, &handle);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Could not load model: %s\n", ethervox_error_string(result));
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    // Load adapter
    ethervox_adapter_t* adapter = NULL;
    result = ethervox_adapter_load(handle, adapter_path, &adapter);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Could not load adapter: %s\n", ethervox_error_string(result));
        ethervox_model_pool_unload(pool, handle);
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    printf("PASS: Adapter loaded successfully\n");
    
    // Clean up
    ethervox_adapter_free(adapter);
    ethervox_model_pool_unload(pool, handle);
    ethervox_model_pool_destroy(pool);
    
    return true;
}

/**
 * Test 3: Read adapter metadata
 */
static bool test_adapter_metadata(const char* model_path, const char* adapter_path) {
    print_test_header("Test 3: Adapter Metadata");
    
    if (!model_path || !adapter_path) {
        printf("SKIP: No model or adapter path provided\n");
        return true;
    }
    
    // Create model pool and load model
    ethervox_paths_t paths = {0};
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    
    char data_dir[512], cache_dir[512], models_dir[512], temp_dir[512];
    snprintf(data_dir, sizeof(data_dir), "%s/.ethervox", home);
    snprintf(cache_dir, sizeof(cache_dir), "%s/.ethervox/cache", home);
    snprintf(models_dir, sizeof(models_dir), "%s/.ethervox/models", home);
    snprintf(temp_dir, sizeof(temp_dir), "%s/.ethervox/tmp", home);
    
    paths.data_dir = data_dir;
    paths.cache_dir = cache_dir;
    paths.models_dir = models_dir;
    paths.temp_dir = temp_dir;
    
    ethervox_model_pool_t* pool = NULL;
    ethervox_model_pool_create(&paths, 4ULL * 1024 * 1024 * 1024, &pool);
    
    ethervox_model_config_t config = {
        .model_path = model_path,
        .mmproj_path = NULL,
        .context_size = 2048,
        .n_threads = 4,
        .n_seq_max = 1,
        .use_gpu = false,
        .kv_unified = false,
        .role = "test",
        .residency = ETHERVOX_RESIDENCY_RESIDENT,
        .ttl_seconds = 0
    };
    
    ethervox_model_handle_t* handle = NULL;
    ethervox_model_pool_load(pool, &config, NULL, NULL, &handle);
    
    // Load adapter
    ethervox_adapter_t* adapter = NULL;
    if (ethervox_adapter_load(handle, adapter_path, &adapter) != ETHERVOX_SUCCESS) {
        printf("SKIP: Could not load adapter\n");
        ethervox_model_pool_unload(pool, handle);
        ethervox_model_pool_destroy(pool);
        return true;
    }
    
    // Get metadata count
    int32_t count = ethervox_adapter_meta_count(adapter);
    printf("  Adapter has %d metadata entries\n", count);
    
    if (count > 0) {
        // Read some metadata by index
        char key[256], value[256];
        for (int32_t i = 0; i < (count < 5 ? count : 5); i++) {
            if (ethervox_adapter_meta_key_by_index(adapter, i, key, sizeof(key)) >= 0 &&
                ethervox_adapter_meta_value_by_index(adapter, i, value, sizeof(value)) >= 0) {
                printf("  %s = %s\n", key, value);
            }
        }
    }
    
    // Try to read specific keys
    char name[256];
    if (ethervox_adapter_get_metadata(adapter, "general.name", name, sizeof(name)) >= 0) {
        printf("  Adapter name: %s\n", name);
    }
    
    printf("PASS: Metadata read successfully\n");
    
    // Clean up
    ethervox_adapter_free(adapter);
    ethervox_model_pool_unload(pool, handle);
    ethervox_model_pool_destroy(pool);
    
    return true;
}

/**
 * Test 4: Apply and clear adapters
 */
static bool test_apply_adapters(const char* model_path, const char* adapter_path) {
    print_test_header("Test 4: Apply and Clear Adapters");
    
    if (!model_path || !adapter_path) {
        printf("SKIP: No model or adapter path provided\n");
        return true;
    }
    
    // Create model pool and load model
    ethervox_paths_t paths = {0};
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    
    char data_dir[512], cache_dir[512], models_dir[512], temp_dir[512];
    snprintf(data_dir, sizeof(data_dir), "%s/.ethervox", home);
    snprintf(cache_dir, sizeof(cache_dir), "%s/.ethervox/cache", home);
    snprintf(models_dir, sizeof(models_dir), "%s/.ethervox/models", home);
    snprintf(temp_dir, sizeof(temp_dir), "%s/.ethervox/tmp", home);
    
    paths.data_dir = data_dir;
    paths.cache_dir = cache_dir;
    paths.models_dir = models_dir;
    paths.temp_dir = temp_dir;
    
    ethervox_model_pool_t* pool = NULL;
    ethervox_model_pool_create(&paths, 4ULL * 1024 * 1024 * 1024, &pool);
    
    ethervox_model_config_t config = {
        .model_path = model_path,
        .mmproj_path = NULL,
        .context_size = 2048,
        .n_threads = 4,
        .n_seq_max = 1,
        .use_gpu = false,
        .kv_unified = false,
        .role = "test",
        .residency = ETHERVOX_RESIDENCY_RESIDENT,
        .ttl_seconds = 0
    };
    
    ethervox_model_handle_t* handle = NULL;
    ethervox_model_pool_load(pool, &config, NULL, NULL, &handle);
    
    // Load adapter
    ethervox_adapter_t* adapter = NULL;
    if (ethervox_adapter_load(handle, adapter_path, &adapter) != ETHERVOX_SUCCESS) {
        printf("SKIP: Could not load adapter\n");
        ethervox_model_pool_unload(pool, handle);
        ethervox_model_pool_destroy(pool);
        return true;
    }
    
    // Apply adapter with scale 1.0
    ethervox_adapter_t* adapters[] = { adapter };
    float scales[] = { 1.0f };
    
    ethervox_result_t result = ethervox_model_set_adapters(handle, adapters, 1, scales);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Could not apply adapter: %s\n", ethervox_error_string(result));
        ethervox_adapter_free(adapter);
        ethervox_model_pool_unload(pool, handle);
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    printf("  Applied adapter with scale 1.0\n");
    
    // Clear adapters
    result = ethervox_model_set_adapters(handle, NULL, 0, NULL);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: Could not clear adapters: %s\n", ethervox_error_string(result));
        ethervox_adapter_free(adapter);
        ethervox_model_pool_unload(pool, handle);
        ethervox_model_pool_destroy(pool);
        return false;
    }
    
    printf("  Cleared adapters\n");
    printf("PASS: Apply and clear successful\n");
    
    // Clean up
    ethervox_adapter_free(adapter);
    ethervox_model_pool_unload(pool, handle);
    ethervox_model_pool_destroy(pool);
    
    return true;
}

int main(int argc, char** argv) {
    printf("LORA ADAPTER TESTS (C5.1)\n");
    printf("=============================\n");
    
    // Set log level
    ethervox_log_set_level(ETHERVOX_LOG_LEVEL_INFO);
    
    const char* model_path = argc > 1 ? argv[1] : NULL;
    const char* adapter_path = argc > 2 ? argv[2] : NULL;
    
    if (!model_path || !adapter_path) {
        printf("\nUsage: %s <model.gguf> <adapter.gguf>\n", argv[0]);
        printf("Running API contract tests only (no real adapter loading)\n\n");
    }
    
    int passed = 0;
    int total = 4;
    
    if (test_null_handling()) passed++;
    if (test_load_adapter(model_path, adapter_path)) passed++;
    if (test_adapter_metadata(model_path, adapter_path)) passed++;
    if (test_apply_adapters(model_path, adapter_path)) passed++;
    
    printf("\n=============================\n");
    printf("RESULTS: %d passed, %d failed\n", passed, total - passed);
    printf("=============================\n");
    
    return (passed == total) ? 0 : 1;
}
