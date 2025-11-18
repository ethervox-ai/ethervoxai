/**
 * @file llm_example.c
 * @brief Example demonstrating LLM backend usage in EthervoxAI
 *
 * This example shows how to:
 * - Initialize an LLM backend
 * - Load a GGUF model
 * - Generate text responses
 * - Use the LLM with the dialogue engine
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ethervox/llm.h"
#include "ethervox/dialogue.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"

void print_usage(const char* program_name) {
    printf("Usage: %s <model_path> [prompt]\n", program_name);
    printf("\n");
    printf("Examples:\n");
    printf("  %s models/tinyllama-1.1b-chat.gguf\n", program_name);
    printf("  %s models/llama-2-7b.gguf \"What is the capital of France?\"\n", program_name);
    printf("\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* model_path = argv[1];
    const char* prompt = argc > 2 ? argv[2] : "Hello! How are you today?";
    
    printf("=== EthervoxAI LLM Backend Example ===\n\n");
    
    // Step 1: Create backend
    printf("1. Creating Llama backend...\n");
    ethervox_llm_backend_t* backend = ethervox_llm_create_llama_backend();
    if (!backend) {
        fprintf(stderr, "Failed to create Llama backend\n");
        return 1;
    }
    printf("   Backend created: %s\n\n", backend->name);
    
    // Step 2: Configure backend
    printf("2. Configuring backend...\n");
    ethervox_llm_config_t config = ethervox_dialogue_get_default_llm_config();
    config.context_length = 2048;
    config.max_tokens = 256;
    config.temperature = 0.7f;
    config.top_p = 0.9f;
    config.use_gpu = false;  // Set to true if you have GPU
    config.gpu_layers = 0;    // Increase if using GPU
    
    printf("   Context length: %u\n", config.context_length);
    printf("   Max tokens: %u\n", config.max_tokens);
    printf("   Temperature: %.2f\n", config.temperature);
    printf("   GPU enabled: %s\n\n", config.use_gpu ? "yes" : "no");
    
    // Step 3: Initialize backend
    printf("3. Initializing backend...\n");
    int result = ethervox_llm_backend_init(backend, &config);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to initialize backend: %d\n", result);
        ethervox_llm_backend_free(backend);
        return 1;
    }
    printf("   Backend initialized successfully\n\n");
    
    // Step 4: Load model
    printf("4. Loading model: %s\n", model_path);
    result = ethervox_llm_backend_load_model(backend, model_path);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to load model: %d\n", result);
        fprintf(stderr, "   Make sure the model file exists and is a valid GGUF file\n");
        ethervox_llm_backend_cleanup(backend);
        ethervox_llm_backend_free(backend);
        return 1;
    }
    printf("   Model loaded successfully\n\n");
    
    // Step 5: Get capabilities
    printf("5. Backend capabilities:\n");
    ethervox_llm_capabilities_t capabilities;
    result = backend->get_capabilities(backend, &capabilities);
    if (result == ETHERVOX_SUCCESS) {
        printf("   Model format: %s\n", capabilities.model_format);
        printf("   Max context: %u tokens\n", capabilities.max_context_length);
        printf("   Supports GPU: %s\n", capabilities.supports_gpu ? "yes" : "no");
        printf("   Supports streaming: %s\n", capabilities.supports_streaming ? "yes" : "no");
    }
    printf("\n");
    
    // Step 6: Generate response
    printf("6. Generating response...\n");
    printf("   Prompt: \"%s\"\n\n", prompt);
    
    ethervox_llm_response_t response;
    memset(&response, 0, sizeof(response));
    
    result = ethervox_llm_backend_generate(backend, prompt, "en", &response);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to generate response: %d\n", result);
        ethervox_llm_backend_unload_model(backend);
        ethervox_llm_backend_cleanup(backend);
        ethervox_llm_backend_free(backend);
        return 1;
    }
    
    // Step 7: Display results
    printf("=== Response ===\n");
    printf("%s\n\n", response.text);
    
    printf("=== Metadata ===\n");
    printf("Tokens generated: %u\n", response.token_count);
    printf("Processing time: %u ms\n", response.processing_time_ms);
    printf("Confidence: %.2f\n", response.confidence);
    printf("Finish reason: %s\n", response.finish_reason);
    printf("Truncated: %s\n", response.truncated ? "yes" : "no");
    
    if (response.token_count > 0 && response.processing_time_ms > 0) {
        float tokens_per_sec = (float)response.token_count / ((float)response.processing_time_ms / 1000.0f);
        printf("Generation speed: %.2f tokens/second\n", tokens_per_sec);
    }
    
    // Cleanup
    printf("\n8. Cleaning up...\n");
    ethervox_llm_response_free(&response);
    ethervox_llm_backend_unload_model(backend);
    ethervox_llm_backend_cleanup(backend);
    ethervox_llm_backend_free(backend);
    
    printf("   Done!\n\n");
    
    return 0;
}
