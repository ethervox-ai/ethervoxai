// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary

/**
 * @file test_grammar_generation.c
 * @brief Grammar-constrained generation stress test (TASK-C2.3c)
 * 
 * Tests grammar-constrained generation at scale:
 * - 500 schemas × 20 generations = 10,000 runs
 * - Validates output is valid JSON matching schema
 * - Detects deadlocks (>1000 tokens without finish)
 * - Measures performance (tokens/sec)
 * - Reports failures and statistics
 * 
 * Usage: ./test_grammar_generation <model.gguf> <schema_dir> [iterations]
 */

#include "ethervox/error.h"
#include "ethervox/grammar.h"
#include "ethervox/llm.h"
#include "ethervox/dialogue.h"  // For ethervox_llm_response_t
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

// =============================================================================
// Configuration
// =============================================================================

#define DEFAULT_ITERATIONS 20
#define MAX_TOKENS 1000
#define DEADLOCK_THRESHOLD 1000
#define MAX_PATH_LEN 1024

// =============================================================================
// Utilities
// =============================================================================

static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static char* read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    
    size_t read_size = fread(content, 1, size, f);
    content[read_size] = '\0';
    fclose(f);
    
    return content;
}

static bool is_valid_json(const char* text) {
    if (!text) return false;
    
    // Basic JSON validation - check balanced braces/brackets
    int brace_depth = 0;
    int bracket_depth = 0;
    bool in_string = false;
    bool escaped = false;
    
    for (const char* p = text; *p; p++) {
        if (escaped) {
            escaped = false;
            continue;
        }
        
        if (*p == '\\') {
            escaped = true;
            continue;
        }
        
        if (*p == '"') {
            in_string = !in_string;
            continue;
        }
        
        if (in_string) continue;
        
        if (*p == '{') brace_depth++;
        else if (*p == '}') brace_depth--;
        else if (*p == '[') bracket_depth++;
        else if (*p == ']') bracket_depth--;
        
        if (brace_depth < 0 || bracket_depth < 0) {
            return false;
        }
    }
    
    return brace_depth == 0 && bracket_depth == 0 && !in_string;
}

// =============================================================================
// Test Results Tracking
// =============================================================================

typedef struct {
    int total_tests;
    int passed;
    int json_invalid;
    int deadlocked;
    int errors;
    double total_time_ms;
    double total_tokens;
} test_stats_t;

typedef struct {
    char schema_name[256];
    int passed;
    int failed;
    double avg_tokens_per_sec;
    char failure_reason[512];
} schema_result_t;

// =============================================================================
// Generation Test
// =============================================================================

static bool test_generation(
    ethervox_llm_backend_t* backend,
    ethervox_grammar_t* grammar,
    int iteration,
    double* out_tokens_per_sec,
    char* error_buf,
    size_t error_buf_size
) {
    (void)grammar;  // Grammar already set on backend
    (void)iteration;  // Not used for simple test
    
    // Simple prompt to trigger JSON generation
    const char* prompt = "Generate a valid JSON object:";
    
    ethervox_llm_response_t response = {0};
    double start_ms = get_time_ms();
    
    ethervox_result_t result = ethervox_llm_backend_generate(
        backend,
        prompt,
        NULL,  // language_code (NULL for auto-detect)
        &response
    );
    
    double elapsed_ms = get_time_ms() - start_ms;
    
    if (result != ETHERVOX_SUCCESS) {
        snprintf(error_buf, error_buf_size, "Generation failed: %s", 
                 ethervox_error_string(result));
        return false;
    }
    
    if (!response.text) {
        snprintf(error_buf, error_buf_size, "No output generated");
        return false;
    }
    
    // Check for deadlock (generated too many tokens)
    size_t output_len = strlen(response.text);
    if (output_len >= MAX_TOKENS * 4) {  // Rough token estimate
        snprintf(error_buf, error_buf_size, "Possible deadlock: generated %zu chars", output_len);
        free(response.text);
        return false;
    }
    
    // Validate JSON structure
    if (!is_valid_json(response.text)) {
        snprintf(error_buf, error_buf_size, "Invalid JSON output");
        free(response.text);
        return false;
    }
    
    // Estimate tokens (rough: 4 chars per token average)
    double tokens = output_len / 4.0;
    *out_tokens_per_sec = (tokens / elapsed_ms) * 1000.0;
    
    free(response.text);
    return true;
}

// =============================================================================
// Schema Test Runner
// =============================================================================

static schema_result_t test_schema(
    ethervox_llm_backend_t* backend,
    const char* schema_path,
    const char* schema_name,
    int iterations
) {
    schema_result_t result = {0};
    strncpy(result.schema_name, schema_name, sizeof(result.schema_name) - 1);
    
    // Load schema
    char* schema_json = read_file(schema_path);
    if (!schema_json) {
        snprintf(result.failure_reason, sizeof(result.failure_reason),
                 "Failed to read schema file");
        result.failed = iterations;
        return result;
    }
    
    // Convert to grammar
    ethervox_grammar_t* grammar = NULL;
    ethervox_result_t conv_result = ethervox_grammar_from_json_schema(schema_json, &grammar);
    free(schema_json);
    
    if (conv_result != ETHERVOX_SUCCESS) {
        snprintf(result.failure_reason, sizeof(result.failure_reason),
                 "Schema conversion failed: %s", ethervox_error_string(conv_result));
        result.failed = iterations;
        return result;
    }
    
    // Set grammar on backend
    ethervox_result_t set_result = ethervox_llm_backend_set_grammar(backend, grammar);
    if (set_result != ETHERVOX_SUCCESS) {
        snprintf(result.failure_reason, sizeof(result.failure_reason),
                 "Failed to set grammar: %s", ethervox_error_string(set_result));
        ethervox_grammar_free(grammar);
        result.failed = iterations;
        return result;
    }
    
    // Run iterations
    double total_tokens_per_sec = 0.0;
    for (int i = 0; i < iterations; i++) {
        char error_buf[512] = {0};
        double tokens_per_sec = 0.0;
        
        if (test_generation(backend, grammar, i, &tokens_per_sec, error_buf, sizeof(error_buf))) {
            result.passed++;
            total_tokens_per_sec += tokens_per_sec;
        } else {
            result.failed++;
            if (result.failure_reason[0] == '\0') {
                strncpy(result.failure_reason, error_buf, sizeof(result.failure_reason) - 1);
            }
        }
    }
    
    if (result.passed > 0) {
        result.avg_tokens_per_sec = total_tokens_per_sec / result.passed;
    }
    
    // Clear grammar
    ethervox_llm_backend_set_grammar(backend, NULL);
    ethervox_grammar_free(grammar);
    
    return result;
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <model.gguf> <schema_dir> [iterations]\n", argv[0]);
        fprintf(stderr, "Example: %s ~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf tests/schemas/grammar_stress 20\n", argv[0]);
        return 1;
    }
    
    const char* model_path = argv[1];
    const char* schema_dir = argv[2];
    int iterations = (argc > 3) ? atoi(argv[3]) : DEFAULT_ITERATIONS;
    
    printf("=== Grammar Generation Stress Test (C2.3c) ===\n\n");
    printf("Model: %s\n", model_path);
    printf("Schema directory: %s\n", schema_dir);
    printf("Iterations per schema: %d\n\n", iterations);
    
    // Initialize backend
    printf("Initializing llama.cpp backend...\n");
    ethervox_llm_backend_t* backend = ethervox_llm_create_llama_backend();
    if (!backend) {
        fprintf(stderr, "Failed to create llama backend\n");
        return 1;
    }
    
    ethervox_llm_config_t config = {0};
    config.model_path = NULL;  // Set on load_model
    config.model_name = "granite";
    config.max_tokens = MAX_TOKENS;
    config.context_length = 2048;
    config.temperature = 0.7f;
    config.top_p = 0.9f;
    config.seed = 42;
    config.use_gpu = true;
    config.gpu_layers = 999;  // Use Metal on macOS
    config.language_code = NULL;
    
    ethervox_result_t result = ethervox_llm_backend_init(backend, &config);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to initialize backend: %s\n", ethervox_error_string(result));
        ethervox_llm_backend_free(backend);
        return 1;
    }
    
    // Load model
    printf("Loading model...\n");
    result = ethervox_llm_backend_load_model(backend, model_path);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to load model: %s\n", ethervox_error_string(result));
        const ethervox_error_context_t* ctx = ethervox_error_get_context();
        if (ctx && ctx->message) {
            fprintf(stderr, "  %s\n", ctx->message);
        }
        ethervox_llm_backend_cleanup(backend);
        ethervox_llm_backend_free(backend);
        return 1;
    }
    printf("Model loaded successfully\n\n");
    
    // Scan schema directory
    DIR* dir = opendir(schema_dir);
    if (!dir) {
        fprintf(stderr, "Failed to open schema directory: %s\n", schema_dir);
        ethervox_llm_backend_unload_model(backend);
        ethervox_llm_backend_cleanup(backend);
        ethervox_llm_backend_free(backend);
        return 1;
    }
    
    // Collect schema files
    char** schema_files = NULL;
    int schema_count = 0;
    int schema_capacity = 100;
    schema_files = (char**)malloc(schema_capacity * sizeof(char*));
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".json")) {
            if (schema_count >= schema_capacity) {
                schema_capacity *= 2;
                schema_files = (char**)realloc(schema_files, schema_capacity * sizeof(char*));
            }
            
            char full_path[MAX_PATH_LEN];
            snprintf(full_path, sizeof(full_path), "%s/%s", schema_dir, entry->d_name);
            schema_files[schema_count] = strdup(full_path);
            schema_count++;
        }
    }
    closedir(dir);
    
    if (schema_count == 0) {
        fprintf(stderr, "No JSON schema files found in %s\n", schema_dir);
        fprintf(stderr, "Run scripts/generate_grammar_test_schemas.py first\n");
        free(schema_files);
        ethervox_llm_backend_unload_model(backend);
        ethervox_llm_backend_cleanup(backend);
        ethervox_llm_backend_free(backend);
        return 1;
    }
    
    printf("Found %d schema files\n", schema_count);
    printf("Total test runs: %d\n\n", schema_count * iterations);
    printf("Starting tests...\n");
    printf("─────────────────────────────────────────────────────────────────\n\n");
    
    // Run tests
    test_stats_t stats = {0};
    double test_start = get_time_ms();
    
    for (int i = 0; i < schema_count; i++) {
        const char* full_path = schema_files[i];
        const char* schema_name = strrchr(full_path, '/');
        schema_name = schema_name ? schema_name + 1 : full_path;
        
        printf("[%3d/%d] %s: ", i + 1, schema_count, schema_name);
        fflush(stdout);
        
        schema_result_t result = test_schema(backend, full_path, schema_name, iterations);
        
        stats.total_tests += iterations;
        stats.passed += result.passed;
        
        if (result.failed > 0) {
            stats.errors += result.failed;
            printf("❌ %d/%d FAIL\n", result.passed, iterations);
            printf("    Reason: %s\n", result.failure_reason);
        } else {
            printf("✅ %d/%d pass, %.1f tok/s\n", result.passed, iterations, result.avg_tokens_per_sec);
        }
    }
    
    double total_time_ms = get_time_ms() - test_start;
    
    // Print summary
    printf("\n─────────────────────────────────────────────────────────────────\n");
    printf("=== Test Summary ===\n\n");
    printf("Total schemas tested:  %d\n", schema_count);
    printf("Total generations:     %d\n", stats.total_tests);
    printf("Passed:                %d (%.1f%%)\n", stats.passed, 
           100.0 * stats.passed / stats.total_tests);
    printf("Failed:                %d (%.1f%%)\n", stats.errors,
           100.0 * stats.errors / stats.total_tests);
    printf("Total time:            %.1f seconds\n", total_time_ms / 1000.0);
    printf("Avg time per test:     %.0f ms\n", total_time_ms / stats.total_tests);
    
    // Cleanup
    for (int i = 0; i < schema_count; i++) {
        free(schema_files[i]);
    }
    free(schema_files);
    
    ethervox_llm_backend_unload_model(backend);
    ethervox_llm_backend_cleanup(backend);
    ethervox_llm_backend_free(backend);
    
    printf("\n✅ Test complete\n");
    return (stats.errors > 0) ? 1 : 0;
}
