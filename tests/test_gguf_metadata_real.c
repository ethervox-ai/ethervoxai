/**
 * @file test_gguf_metadata_real.c
 * @brief Test real GGUF metadata extraction from actual model file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ETHERVOX_WITH_LLAMA)
#include <llama.h>
#define LLAMA_AVAILABLE 1
#else
#define LLAMA_AVAILABLE 0
#endif

int main(int argc, char** argv) {
    printf("=== GGUF Metadata Extraction Test ===\n\n");
    
#if !LLAMA_AVAILABLE
    printf("❌ LLAMA not available - cannot test metadata extraction\n");
    return 1;
#else
    
    const char* model_path = "/Users/timk/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf";
    if (argc > 1) {
        model_path = argv[1];
    }
    
    printf("Testing with model: %s\n\n", model_path);
    
    // Initialize llama backend
    printf("Initializing llama backend...\n");
    llama_backend_init();
    
    // Load model with default params
    printf("Loading model...\n");
    struct llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0;  // CPU only for testing
    model_params.use_mmap = true;
    
    struct llama_model* model = llama_model_load_from_file(model_path, model_params);
    
    if (!model) {
        printf("❌ Failed to load model from: %s\n", model_path);
        llama_backend_free();
        return 1;
    }
    
    printf("✓ Model loaded successfully\n\n");
    
    // Extract metadata using llama.cpp API
    printf("=== Model Metadata ===\n");
    
    int32_t n_ctx_train = llama_model_n_ctx_train(model);
    printf("Training context length: %d\n", n_ctx_train);
    
    int32_t n_layer = llama_model_n_layer(model);
    printf("Number of layers: %d\n", n_layer);
    
    int32_t n_embd = llama_model_n_embd(model);
    printf("Embedding dimensions: %d\n", n_embd);
    
    int32_t n_head = llama_model_n_head(model);
    printf("Number of heads: %d\n", n_head);
    
    uint64_t model_size = llama_model_size(model);
    printf("Model size: %llu bytes (%.2f GB)\n", 
           (unsigned long long)model_size, 
           model_size / (1024.0 * 1024.0 * 1024.0));
    
    // Try to get architecture name
    char arch_buf[256] = {0};
    int arch_len = llama_model_meta_val_str(model, "general.architecture", arch_buf, sizeof(arch_buf));
    if (arch_len > 0) {
        printf("Architecture: %s\n", arch_buf);
    } else {
        printf("Architecture: unknown\n");
    }
    
    // Get vocab size via vocab API
    const struct llama_vocab* vocab = llama_model_get_vocab(model);
    if (vocab) {
        int32_t n_vocab = llama_vocab_n_tokens(vocab);
        printf("Vocabulary size: %d\n", n_vocab);
    }
    
    printf("\n=== Memory Estimates ===\n");
    
    // Calculate KV cache size for different context lengths
    int test_contexts[] = {2048, 4096, 8192};
    for (int i = 0; i < 3; i++) {
        int ctx = test_contexts[i];
        // KV cache formula: layers * 2 * context * sizeof(fp16) * 2 (K+V)
        size_t kv_bytes = n_layer * 2 * ctx * sizeof(uint16_t) * 2;
        float kv_mb = kv_bytes / (1024.0 * 1024.0);
        printf("KV cache @ %d context: %.1f MB\n", ctx, kv_mb);
    }
    
    // List some metadata keys
    printf("\n=== Model Metadata Keys ===\n");
    int n_meta = llama_model_meta_count(model);
    printf("Total metadata keys: %d\n", n_meta);
    
    printf("First 20 keys:\n");
    for (int i = 0; i < n_meta && i < 20; i++) {
        char key_buf[256] = {0};
        int key_len = llama_model_meta_key_by_index(model, i, key_buf, sizeof(key_buf));
        if (key_len > 0) {
            char val_buf[256] = {0};
            int val_len = llama_model_meta_val_str_by_index(model, i, val_buf, sizeof(val_buf));
            if (val_len > 0) {
                printf("  %s = %s\n", key_buf, val_buf);
            } else {
                printf("  %s = <non-string>\n", key_buf);
            }
        }
    }
    
    // Clean up
    printf("\n=== Cleanup ===\n");
    llama_model_free(model);
    llama_backend_free();
    
    printf("✓ Test completed successfully\n");
    return 0;
    
#endif
}
