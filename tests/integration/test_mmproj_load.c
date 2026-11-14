/**
 * Minimal test to load mmproj file
 */
#include <stdio.h>
#include <stdlib.h>
#include "llama.h"
#include "mtmd.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <text_model.gguf> <mmproj.gguf>\n", argv[0]);
        return 1;
    }
    
    const char* model_path = argv[1];
    const char* mmproj_path = argv[2];
    
    printf("Initializing llama backend...\n");
    llama_backend_init();
    ggml_backend_load_all();
    printf("Backends loaded: %d\n", (int)ggml_backend_reg_count());
    
    printf("\nLoading text model: %s\n", model_path);
    struct llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 99;
    
    struct llama_model* model = llama_model_load_from_file(model_path, model_params);
    if (!model) {
        fprintf(stderr, "Failed to load model\n");
        return 1;
    }
    printf("✅ Text model loaded\n");
    
    printf("\nLoading mmproj: %s\n", mmproj_path);
    struct mtmd_context_params mtmd_params = mtmd_context_params_default();
    mtmd_params.use_gpu = true;
    mtmd_params.print_timings = true;
    mtmd_params.n_threads = 4;
    
    struct mtmd_context* mctx = mtmd_init_from_file(mmproj_path, model, mtmd_params);
    if (!mctx) {
        fprintf(stderr, "Failed to load mmproj\n");
        llama_model_free(model);
        return 1;
    }
    printf("✅ mmproj loaded\n");
    
    printf("\nCapabilities:\n");
    printf("  Vision: %d\n", mtmd_support_vision(mctx));
    printf("  Audio: %d\n", mtmd_support_audio(mctx));
    
    mtmd_free(mctx);
    llama_model_free(model);
    llama_backend_free();
    
    printf("\n✅ Test passed\n");
    return 0;
}
