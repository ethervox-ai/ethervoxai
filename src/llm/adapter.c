/**
 * @file adapter.c
 * @brief LoRA adapter loading and management implementation
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/adapter.h"
#include "ethervox/logging.h"
#include "ethervox/platform_thread.h"
#include <stdlib.h>
#include <string.h>

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
#include <llama.h>
#define LLAMA_AVAILABLE 1
#else
#define LLAMA_AVAILABLE 0
#endif

/**
 * Adapter structure
 * 
 * Wraps llama_adapter_lora and tracks association with parent model.
 */
struct ethervox_adapter {
#if LLAMA_AVAILABLE
    struct llama_adapter_lora* lora;  // Underlying llama.cpp adapter
#else
    void* lora;  // Stub for non-llama builds
#endif
    ethervox_model_handle_t* parent_handle;  // Model this adapter is bound to
    char path[512];  // Path for debugging/logging
};

ethervox_result_t ethervox_adapter_load(
    ethervox_model_handle_t* handle,
    const char* adapter_path,
    ethervox_adapter_t** out
) {
    if (!handle || !adapter_path || !out) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
#if !LLAMA_AVAILABLE
    ETHERVOX_LOG_ERROR("[Adapter] llama.cpp not available, cannot load adapters");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    
    // Get underlying llama_model
    struct llama_model* model = ethervox_model_handle_get_model(handle);
    if (!model) {
        ETHERVOX_LOG_ERROR("[Adapter] Invalid model handle");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Allocate adapter structure
    ethervox_adapter_t* adapter = (ethervox_adapter_t*)calloc(1, sizeof(ethervox_adapter_t));
    if (!adapter) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    // Load LoRA adapter via llama.cpp
    ETHERVOX_LOG_INFO("[Adapter] Loading adapter from: %s", adapter_path);
    adapter->lora = llama_adapter_lora_init(model, adapter_path);
    if (!adapter->lora) {
        ETHERVOX_LOG_ERROR("[Adapter] Failed to load adapter: %s", adapter_path);
        free(adapter);
        return ETHERVOX_ERROR_FILE_READ;
    }
    
    // Store metadata
    adapter->parent_handle = handle;
    strncpy(adapter->path, adapter_path, sizeof(adapter->path) - 1);
    adapter->path[sizeof(adapter->path) - 1] = '\0';
    
    // Read adapter metadata for logging
    char name[256] = "unknown";
    ethervox_adapter_get_metadata(adapter, "general.name", name, sizeof(name));
    ETHERVOX_LOG_INFO("[Adapter] Loaded adapter '%s' from %s", name, adapter_path);
    
    *out = adapter;
    return ETHERVOX_SUCCESS;
#endif
}

void ethervox_adapter_free(ethervox_adapter_t* adapter) {
    if (!adapter) {
        return;
    }
    
#if LLAMA_AVAILABLE
    if (adapter->lora) {
        ETHERVOX_LOG_DEBUG("[Adapter] Freeing adapter: %s", adapter->path);
        llama_adapter_lora_free(adapter->lora);
    }
#endif
    
    free(adapter);
}

ethervox_result_t ethervox_model_set_adapters(
    ethervox_model_handle_t* handle,
    ethervox_adapter_t** adapters,
    size_t n_adapters,
    const float* scales
) {
    if (!handle) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Validate: if adapters provided, scales must also be provided
    if (n_adapters > 0 && (!adapters || !scales)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
#if !LLAMA_AVAILABLE
    ETHERVOX_LOG_ERROR("[Adapter] llama.cpp not available, cannot set adapters");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    
    // Get underlying llama_context
    struct llama_context* ctx = ethervox_model_handle_get_context(handle);
    if (!ctx) {
        ETHERVOX_LOG_ERROR("[Adapter] Invalid model handle or context");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Clear adapters if n_adapters == 0
    if (n_adapters == 0) {
        ETHERVOX_LOG_DEBUG("[Adapter] Clearing all adapters");
        int32_t ret = llama_set_adapters_lora(ctx, NULL, 0, NULL);
        if (ret < 0) {
            ETHERVOX_LOG_ERROR("[Adapter] Failed to clear adapters");
            return ETHERVOX_ERROR_FAILED;
        }
        return ETHERVOX_SUCCESS;
    }
    
    // Build array of llama_adapter_lora pointers
    struct llama_adapter_lora** lora_adapters = 
        (struct llama_adapter_lora**)malloc(n_adapters * sizeof(struct llama_adapter_lora*));
    if (!lora_adapters) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    for (size_t i = 0; i < n_adapters; i++) {
        if (!adapters[i] || !adapters[i]->lora) {
            ETHERVOX_LOG_ERROR("[Adapter] Invalid adapter at index %zu", i);
            free(lora_adapters);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
        lora_adapters[i] = adapters[i]->lora;
    }
    
    // Apply adapters via llama.cpp
    ETHERVOX_LOG_INFO("[Adapter] Applying %zu adapter(s) to model", n_adapters);
    
    // Make a mutable copy of scales array for llama.cpp API
    float* scales_copy = (float*)malloc(n_adapters * sizeof(float));
    if (!scales_copy) {
        free(lora_adapters);
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    memcpy(scales_copy, scales, n_adapters * sizeof(float));
    
    int32_t ret = llama_set_adapters_lora(ctx, lora_adapters, n_adapters, scales_copy);
    
    free(scales_copy);
    free(lora_adapters);
    
    if (ret < 0) {
        ETHERVOX_LOG_ERROR("[Adapter] Failed to set adapters (llama.cpp returned %d)", ret);
        return ETHERVOX_ERROR_FAILED;
    }
    
    return ETHERVOX_SUCCESS;
#endif
}

int32_t ethervox_adapter_get_metadata(
    const ethervox_adapter_t* adapter,
    const char* key,
    char* buf,
    size_t buf_size
) {
    if (!adapter || !key || !buf || buf_size == 0) {
        if (buf && buf_size > 0) {
            buf[0] = '\0';
        }
        return -1;
    }
    
#if !LLAMA_AVAILABLE
    buf[0] = '\0';
    return -1;
#else
    if (!adapter->lora) {
        buf[0] = '\0';
        return -1;
    }
    
    return llama_adapter_meta_val_str(adapter->lora, key, buf, buf_size);
#endif
}

int32_t ethervox_adapter_meta_count(const ethervox_adapter_t* adapter) {
    if (!adapter) {
        return -1;
    }
    
#if !LLAMA_AVAILABLE
    return -1;
#else
    if (!adapter->lora) {
        return -1;
    }
    
    return llama_adapter_meta_count(adapter->lora);
#endif
}

int32_t ethervox_adapter_meta_key_by_index(
    const ethervox_adapter_t* adapter,
    int32_t index,
    char* buf,
    size_t buf_size
) {
    if (!adapter || !buf || buf_size == 0) {
        if (buf && buf_size > 0) {
            buf[0] = '\0';
        }
        return -1;
    }
    
#if !LLAMA_AVAILABLE
    buf[0] = '\0';
    return -1;
#else
    if (!adapter->lora) {
        buf[0] = '\0';
        return -1;
    }
    
    return llama_adapter_meta_key_by_index(adapter->lora, index, buf, buf_size);
#endif
}

int32_t ethervox_adapter_meta_value_by_index(
    const ethervox_adapter_t* adapter,
    int32_t index,
    char* buf,
    size_t buf_size
) {
    if (!adapter || !buf || buf_size == 0) {
        if (buf && buf_size > 0) {
            buf[0] = '\0';
        }
        return -1;
    }
    
#if !LLAMA_AVAILABLE
    buf[0] = '\0';
    return -1;
#else
    if (!adapter->lora) {
        buf[0] = '\0';
        return -1;
    }
    
    return llama_adapter_meta_val_str_by_index(adapter->lora, index, buf, buf_size);
#endif
}
