// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary

/**
 * @file test_session_leak.c
 * @brief Session forking leak test - verify 300 forks don't leak memory
 *
 * Acceptance criterion: "300 sequential forks run without leaking; memory returns
 * to baseline after destroy (ASan/LSan)."
 *
 * Test procedure:
 *   1. Create parent session, prefill short prompt
 *   2. Fork 300 times sequentially (fork → use → destroy child)
 *   3. Destroy parent
 *   4. ASan/LSan should report zero leaks
 */

#include "ethervox/session.h"
#include "ethervox/model_pool.h"
#include "ethervox/paths.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#define NUM_FORKS 300

static char* expand_tilde(const char* path) {
    if (path[0] != '~') {
        return strdup(path);
    }
    
    const char* home = getenv("HOME");
    if (!home) {
        home = getpwuid(getuid())->pw_dir;
    }
    
    size_t len = strlen(home) + strlen(path);
    char* expanded = malloc(len);
    if (!expanded) {
        return NULL;
    }
    
    snprintf(expanded, len, "%s%s", home, path + 1);
    return expanded;
}

int main(int argc, char** argv) {
    printf("=== Session Forking Leak Test ===\n");
    printf("Target: %d sequential forks with zero leaks\n\n", NUM_FORKS);
    
    const char* model_path = argc > 1 ? argv[1] : "~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf";
    char* expanded_path = expand_tilde(model_path);
    if (!expanded_path || access(expanded_path, F_OK) != 0) {
        fprintf(stderr, "Model not found: %s\n", expanded_path ? expanded_path : model_path);
        free(expanded_path);
        return 1;
    }
    
    printf("Using model: %s\n", expanded_path);
    
    // Create paths
    ethervox_paths_t paths = {0};
    char path_buffer[4096];
    ethervox_result_t result = ethervox_paths_get_default(&paths, path_buffer, sizeof(path_buffer));
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to initialize paths: %s\n", ethervox_error_string(result));
        free(expanded_path);
        return 1;
    }
    
    // Create model pool
    ethervox_model_pool_t* pool = NULL;
    result = ethervox_model_pool_create(&paths, 4ULL * 1024 * 1024 * 1024, &pool);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to create model pool: %s\n", ethervox_error_string(result));
        free(expanded_path);
        return 1;
    }
    
    // Load model with small n_seq_max (we reuse sequence IDs by sequential create/destroy)
    ethervox_model_config_t config = {
        .model_path = expanded_path,
        .mmproj_path = NULL,
        .context_size = 1024,
        .n_threads = 8,
        .n_seq_max = 8,  // Small pool; sequence IDs recycled as children are destroyed
        .use_gpu = false,  // CPU-only for deterministic ASan
        .kv_unified = true,
        .role = "leak_test"
    };
    
    ethervox_model_handle_t* handle = NULL;
    result = ethervox_model_pool_load(pool, &config, NULL, NULL, &handle);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to load model: %s\n", ethervox_error_string(result));
        ethervox_model_pool_destroy(pool);
        free(expanded_path);
        return 1;
    }
    
    free(expanded_path);
    printf("Model loaded\n");
    
    // Create parent session
    ethervox_session_config_t sess_config = {0};
    sess_config.kv_unified = true;
    ethervox_session_t* parent = NULL;
    
    result = ethervox_session_create(handle, &sess_config, &parent);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to create parent: %s\n", ethervox_error_string(result));
        ethervox_model_pool_unload(pool, handle);
        ethervox_model_pool_destroy(pool);
        return 1;
    }
    
    // Prefill parent (short prompt to minimize time)
    const char* prompt = "Classify this text: ";
    result = ethervox_session_prefill(parent, prompt, NULL, NULL);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to prefill parent: %s\n", ethervox_error_string(result));
        ethervox_session_destroy(parent);
        ethervox_model_pool_unload(pool, handle);
        ethervox_model_pool_destroy(pool);
        return 1;
    }
    
    printf("Parent prefilled, starting %d forks...\n", NUM_FORKS);
    
    // Fork 300 times sequentially
    for (int i = 0; i < NUM_FORKS; ++i) {
        ethervox_session_t* child = NULL;
        result = ethervox_session_fork(parent, &child);
        if (result != ETHERVOX_SUCCESS) {
            fprintf(stderr, "Failed to fork at iteration %d: %s\n", i, ethervox_error_string(result));
            ethervox_session_destroy(parent);
            ethervox_model_pool_unload(pool, handle);
            ethervox_model_pool_destroy(pool);
            return 1;
        }
        
        // Use child briefly (prefill a short text)
        result = ethervox_session_prefill(child, "test document", NULL, NULL);
        if (result != ETHERVOX_SUCCESS) {
            fprintf(stderr, "Failed to prefill child at iteration %d: %s\n", i, ethervox_error_string(result));
            ethervox_session_destroy(child);
            ethervox_session_destroy(parent);
            ethervox_model_pool_unload(pool, handle);
            ethervox_model_pool_destroy(pool);
            return 1;
        }
        
        // Destroy child (frees sequence ID for reuse)
        ethervox_session_destroy(child);
        
        if ((i + 1) % 50 == 0) {
            printf("  Completed %d/%d forks...\n", i + 1, NUM_FORKS);
        }
    }
    
    printf("All %d forks completed successfully\n", NUM_FORKS);
    
    // Clean up
    ethervox_session_destroy(parent);
    ethervox_model_pool_unload(pool, handle);
    ethervox_model_pool_destroy(pool);
    
    printf("✅ Leak test complete - check ASan/LSan output for leaks\n");
    return 0;
}
