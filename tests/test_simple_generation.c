// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary

/**
 * @file test_simple_generation.c
 * @brief Simple test to isolate grammar vs setup issues
 * 
 * Tests:
 * 1. Multiple generations WITHOUT grammar
 * 2. Single generation WITH grammar
 * 3. Multiple generations WITH grammar (same as test_grammar_generation)
 * 
 * This isolates whether the issue is:
 * - Our backend setup/KV cache handling
 * - Grammar-specific
 * - Grammar + reuse combination
 */

#include "ethervox/error.h"
#include "ethervox/grammar.h"
#include "ethervox/llm.h"
#include "ethervox/dialogue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_no_grammar(ethervox_llm_backend_t* backend, int iterations) {
    printf("\n=== Test 1: %d generations WITHOUT grammar ===\n", iterations);
    
    const char* prompt = "Say 'hello' in JSON format:";
    int passed = 0;
    
    for (int i = 0; i < iterations; i++) {
        ethervox_llm_response_t response = {0};
        ethervox_result_t result = ethervox_llm_backend_generate(
            backend, prompt, NULL, &response
        );
        
        if (result == ETHERVOX_SUCCESS && response.text) {
            printf("  [%d] ✅ Generated: %.50s%s\n", i + 1, response.text,
                   strlen(response.text) > 50 ? "..." : "");
            free(response.text);
            passed++;
        } else {
            printf("  [%d] ❌ Failed: %s\n", i + 1, ethervox_error_string(result));
        }
    }
    
    printf("Result: %d/%d passed\n", passed, iterations);
}

static void test_single_grammar(ethervox_llm_backend_t* backend) {
    printf("\n=== Test 2: Single generation WITH simple grammar ===\n");
    
    // Simple schema with REQUIRED property to avoid empty object issue
    const char* schema = "{\"type\":\"object\",\"properties\":{\"message\":{\"type\":\"string\"}},\"required\":[\"message\"]}";
    
    ethervox_grammar_t* grammar = NULL;
    ethervox_result_t conv_result = ethervox_grammar_from_json_schema(schema, &grammar);
    
    if (conv_result != ETHERVOX_SUCCESS) {
        printf("❌ Schema conversion failed: %s\n", ethervox_error_string(conv_result));
        return;
    }
    
    printf("✅ Grammar compiled\n");
    
    // Debug: print the GBNF that was generated
    const char* gbnf_source = ethervox_grammar_get_source(grammar);
    const char* gbnf_root = ethervox_grammar_get_root(grammar);
    printf("DEBUG: GBNF root rule: %s\n", gbnf_root ? gbnf_root : "(null)");
    printf("DEBUG: GBNF source length: %zu\n", gbnf_source ? strlen(gbnf_source) : 0);
    if (gbnf_source && strlen(gbnf_source) < 500) {
        printf("DEBUG: GBNF source:\n%s\n", gbnf_source);
    }
    
    ethervox_result_t set_result = ethervox_llm_backend_set_grammar(backend, grammar);
    if (set_result != ETHERVOX_SUCCESS) {
        printf("❌ Set grammar failed: %s\n", ethervox_error_string(set_result));
        ethervox_grammar_free(grammar);
        return;
    }
    
    printf("✅ Grammar set on backend\n");
    
    const char* prompt = "Generate a JSON object:";
    ethervox_llm_response_t response = {0};
    ethervox_result_t result = ethervox_llm_backend_generate(
        backend, prompt, NULL, &response
    );
    
    if (result == ETHERVOX_SUCCESS && response.text) {
        printf("✅ Generated: %s\n", response.text);
        free(response.text);
    } else {
        printf("❌ Generation failed: %s\n", ethervox_error_string(result));
    }
    
    ethervox_llm_backend_set_grammar(backend, NULL);  // Clear grammar
    ethervox_grammar_free(grammar);
}

static void test_multiple_grammar(ethervox_llm_backend_t* backend, int iterations) {
    printf("\n=== Test 3: %d generations WITH grammar (reused) ===\n", iterations);
    
    const char* schema = "{\"type\":\"object\",\"properties\":{\"count\":{\"type\":\"integer\"}}}";
    
    ethervox_grammar_t* grammar = NULL;
    ethervox_result_t conv_result = ethervox_grammar_from_json_schema(schema, &grammar);
    
    if (conv_result != ETHERVOX_SUCCESS) {
        printf("❌ Schema conversion failed\n");
        return;
    }
    
    ethervox_llm_backend_set_grammar(backend, grammar);
    int passed = 0;
    
    for (int i = 0; i < iterations; i++) {
        const char* prompt = "Generate a JSON object:";
        ethervox_llm_response_t response = {0};
        ethervox_result_t result = ethervox_llm_backend_generate(
            backend, prompt, NULL, &response
        );
        
        if (result == ETHERVOX_SUCCESS && response.text) {
            printf("  [%d] ✅ Generated: %.40s%s\n", i + 1, response.text,
                   strlen(response.text) > 40 ? "..." : "");
            free(response.text);
            passed++;
        } else {
            printf("  [%d] ❌ Failed: %s\n", i + 1, ethervox_error_string(result));
            break;  // Stop on first failure
        }
    }
    
    printf("Result: %d/%d passed\n", passed, iterations);
    
    ethervox_llm_backend_set_grammar(backend, NULL);
    ethervox_grammar_free(grammar);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <model.gguf>\n", argv[0]);
        return 1;
    }
    
    const char* model_path = argv[1];
    
    printf("Loading model: %s\n", model_path);
    
    // Initialize backend
    ethervox_llm_backend_t* backend = ethervox_llm_create_llama_backend();
    if (!backend) {
        fprintf(stderr, "Failed to create backend\n");
        return 1;
    }
    
    ethervox_llm_config_t config = {0};
    config.model_path = NULL;  // Set on load_model
    config.model_name = "granite";
    config.max_tokens = 100;
    config.context_length = 2048;
    config.temperature = 0.7f;
    config.top_p = 0.9f;
    config.seed = 42;
    config.use_gpu = true;
    config.gpu_layers = 999;  // Use Metal on macOS
    config.language_code = NULL;
    
    ethervox_result_t init_result = ethervox_llm_backend_init(backend, &config);
    if (init_result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to initialize backend: %s\n", ethervox_error_string(init_result));
        ethervox_llm_backend_free(backend);
        return 1;
    }
    
    ethervox_result_t load_result = ethervox_llm_backend_load_model(backend, model_path);
    if (load_result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to load model: %s\n", ethervox_error_string(load_result));
        ethervox_llm_backend_cleanup(backend);
        ethervox_llm_backend_free(backend);
        return 1;
    }
    
    printf("✅ Model loaded successfully\n");
    
    // Run tests
    test_no_grammar(backend, 5);
    test_single_grammar(backend);
    test_multiple_grammar(backend, 5);
    
    printf("\n=== All tests complete ===\n");
    
    ethervox_llm_backend_cleanup(backend);
    ethervox_llm_backend_free(backend);
    return 0;
}
