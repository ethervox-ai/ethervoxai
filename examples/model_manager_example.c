/**
 * @file model_manager_example.c
 * @brief Example demonstrating automatic model download and management
 *
 * This example shows how to:
 * - Initialize the model manager
 * - Check model availability
 * - Automatically download missing models
 * - Use the model with LLM backend
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ethervox/model_manager.h"
#include "ethervox/llm.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"

// Progress callback for download
void download_progress(const char* model_name,
                      uint64_t downloaded_bytes,
                      uint64_t total_bytes,
                      float progress_percent,
                      void* user_data) {
    (void)user_data;
    
    // Update progress on same line
    printf("\r[%s] %.1f%% (%.2f MB / %.2f MB)  ",
           model_name,
           progress_percent,
           (float)downloaded_bytes / 1024.0f / 1024.0f,
           (float)total_bytes / 1024.0f / 1024.0f);
    fflush(stdout);
    
    if (progress_percent >= 100.0f) {
        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("=== EthervoxAI Model Manager Example ===\n\n");
    
    // Step 1: Create model manager
    printf("1. Creating model manager...\n");
    
    ethervox_model_manager_config_t config = ethervox_model_manager_get_default_config();
    config.models_dir = "models";
    config.auto_download = true;
    config.progress_callback = download_progress;
    config.callback_user_data = NULL;
    
    ethervox_model_manager_t* manager = ethervox_model_manager_create(&config);
    if (!manager) {
        fprintf(stderr, "Failed to create model manager\n");
        return 1;
    }
    printf("   Model manager created\n");
    printf("   Models directory: %s\n\n", config.models_dir);
    
    // Step 2: Check available models
    printf("2. Checking available models...\n\n");
    
    const ethervox_model_info_t* models[] = {
        &ETHERVOX_MODEL_TINYLLAMA_1B_Q4,
        &ETHERVOX_MODEL_PHI2_Q4,
        &ETHERVOX_MODEL_MISTRAL_7B_Q4,
    };
    const size_t num_models = sizeof(models) / sizeof(models[0]);
    
    printf("   Available models:\n");
    for (size_t i = 0; i < num_models; i++) {
        const ethervox_model_info_t* model = models[i];
        ethervox_model_status_t status = ethervox_model_manager_get_status(manager, model);
        
        printf("   [%zu] %s\n", i + 1, model->name);
        printf("       Description: %s\n", model->description);
        printf("       Size: %.2f MB\n", (float)model->size_bytes / 1024.0f / 1024.0f);
        printf("       Min RAM: %u MB\n", model->min_ram_mb);
        printf("       Status: %s\n", ethervox_model_status_to_string(status));
        
        if (model->recommended_for_embedded) {
            printf("       ⭐ Recommended for embedded devices\n");
        }
        printf("\n");
    }
    
    // Step 3: Check disk space
    printf("3. Checking disk space...\n");
    uint64_t available_space = ethervox_model_manager_get_available_space(config.models_dir);
    printf("   Available space: %.2f GB\n\n", (float)available_space / 1024.0f / 1024.0f / 1024.0f);
    
    // Step 4: Select and ensure model (TinyLlama recommended for testing)
    const ethervox_model_info_t* selected_model = &ETHERVOX_MODEL_TINYLLAMA_1B_Q4;
    
    printf("4. Ensuring model is available: %s\n", selected_model->name);
    
    if (!ethervox_model_manager_is_available(manager, selected_model)) {
        printf("   Model not found locally\n");
        
        if (!ethervox_model_manager_has_enough_space(config.models_dir, selected_model->size_bytes)) {
            fprintf(stderr, "   ERROR: Insufficient disk space\n");
            ethervox_model_manager_destroy(manager);
            return 1;
        }
        
        printf("   Starting download...\n\n");
        
        int result = ethervox_model_manager_ensure_available(manager, selected_model);
        if (result != ETHERVOX_SUCCESS) {
            fprintf(stderr, "\n   ERROR: Failed to download model (error %d)\n", result);
            fprintf(stderr, "   Please download manually from:\n");
            fprintf(stderr, "   %s\n", selected_model->url);
            ethervox_model_manager_destroy(manager);
            return 1;
        }
        
        printf("   Download completed!\n\n");
    } else {
        printf("   Model already available locally\n\n");
    }
    
    // Step 5: Get model path
    const char* model_path = ethervox_model_manager_get_path(manager, selected_model);
    printf("5. Model location: %s\n\n", model_path);
    
    // Step 6: Initialize LLM backend with the model
    printf("6. Initializing LLM backend...\n");
    
    ethervox_llm_backend_t* backend = ethervox_llm_create_llama_backend();
    if (!backend) {
        fprintf(stderr, "   ERROR: Failed to create LLM backend\n");
        ethervox_model_manager_destroy(manager);
        return 1;
    }
    
    ethervox_llm_config_t llm_config = ethervox_dialogue_get_default_llm_config();
    llm_config.context_length = 2048;
    llm_config.max_tokens = 128;
    llm_config.temperature = 0.7f;
    llm_config.use_gpu = false;
    
    int result = ethervox_llm_backend_init(backend, &llm_config);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "   ERROR: Failed to initialize backend\n");
        ethervox_llm_backend_free(backend);
        ethervox_model_manager_destroy(manager);
        return 1;
    }
    
    printf("   Backend initialized\n\n");
    
    // Step 7: Load model
    printf("7. Loading model...\n");
    result = ethervox_llm_backend_load_model(backend, model_path);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "   ERROR: Failed to load model\n");
        ethervox_llm_backend_cleanup(backend);
        ethervox_llm_backend_free(backend);
        ethervox_model_manager_destroy(manager);
        return 1;
    }
    printf("   Model loaded successfully\n\n");
    
    // Step 8: Test generation
    printf("8. Testing generation...\n");
    const char* test_prompt = "Hello! What is artificial intelligence?";
    printf("   Prompt: \"%s\"\n\n", test_prompt);
    
    ethervox_llm_response_t response;
    memset(&response, 0, sizeof(response));
    
    result = ethervox_llm_backend_generate(backend, test_prompt, "en", &response);
    if (result == ETHERVOX_SUCCESS) {
        printf("   Response: %s\n\n", response.text);
        printf("   Tokens: %u\n", response.token_count);
        printf("   Time: %u ms\n", response.processing_time_ms);
        
        if (response.token_count > 0 && response.processing_time_ms > 0) {
            float tok_per_sec = (float)response.token_count / ((float)response.processing_time_ms / 1000.0f);
            printf("   Speed: %.2f tokens/second\n", tok_per_sec);
        }
        
        ethervox_llm_response_free(&response);
    } else {
        fprintf(stderr, "   ERROR: Generation failed\n");
    }
    
    // Cleanup
    printf("\n9. Cleaning up...\n");
    ethervox_llm_backend_unload_model(backend);
    ethervox_llm_backend_cleanup(backend);
    ethervox_llm_backend_free(backend);
    ethervox_model_manager_destroy(manager);
    
    printf("   Done!\n\n");
    printf("✅ Model manager example completed successfully\n");
    
    return 0;
}
