/**
 * @file model_pool.c
 * @brief Multi-model pool implementation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/model_pool.h"
#include "ethervox/logging.h"
#include "ethervox/platform_thread.h"
#include "ethervox/platform_mem.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
#include <llama.h>
#define LLAMA_AVAILABLE 1
#else
#define LLAMA_AVAILABLE 0
#endif

#if defined(MTMD_AVAILABLE) && MTMD_AVAILABLE
#include "mtmd.h"
#endif

// Convenience macros for mutex operations (ignore return values for now)
#define MUTEX_INIT(m) ethervox_mutex_init(&(m))
#define MUTEX_DESTROY(m) ethervox_mutex_destroy(&(m))
#define MUTEX_LOCK(m) ethervox_mutex_lock(&(m))
#define MUTEX_UNLOCK(m) ethervox_mutex_unlock(&(m))

// Memory estimation constants (bytes per token for KV cache)
#define KV_BYTES_PER_TOKEN_F16 2048  // FP16 KV cache, typical for 7-8B models
#define OVERHEAD_BYTES 128 * 1024 * 1024  // 128MB overhead per model

#if LLAMA_AVAILABLE
// Routes llama.cpp/ggml's own log output (grammar trigger tracing, warnings, etc.) through
// ethervox_log instead of llama.cpp's default raw-stderr logger -- otherwise those lines never
// get a timestamp or the rest of the system's log formatting at all.
static void model_pool_ggml_log_callback(enum ggml_log_level level, const char* text, void* user_data) {
    (void)user_data;
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        len--;
    }
    if (len == 0) {
        return;
    }
    char clean_text[1024];
    size_t copy_len = len < sizeof(clean_text) - 1 ? len : sizeof(clean_text) - 1;
    memcpy(clean_text, text, copy_len);
    clean_text[copy_len] = '\0';

    switch (level) {
        case GGML_LOG_LEVEL_ERROR:
            ETHERVOX_LOG_ERROR("[llama.cpp] %s", clean_text);
            break;
        case GGML_LOG_LEVEL_WARN:
            ETHERVOX_LOG_WARN("[llama.cpp] %s", clean_text);
            break;
        case GGML_LOG_LEVEL_DEBUG:
            ETHERVOX_LOG_DEBUG("[llama.cpp] %s", clean_text);
            break;
        default:
            ETHERVOX_LOG_INFO("[llama.cpp] %s", clean_text);
            break;
    }
}
#endif

// Global backend state (refcounted)
static struct {
    ethervox_mutex_t mutex;
    int refcount;
    bool initialized;
} g_backend = {
    .mutex = ETHERVOX_MUTEX_INITIALIZER,
    .refcount = 0,
    .initialized = false
};

/**
 * Model handle structure
 */
struct ethervox_model_handle {
    struct llama_model* model;
    struct llama_context* ctx;
    void* mtmd_ctx;              // mtmd_context* for multimodal support (NULL if not supported)
    uint64_t mmproj_bytes;       // C3.6: projector size, so attach/detach can charge/refund exactly
    ethervox_mutex_t inference_mutex;     // Serializes inference on this model
    uint64_t memory_bytes;       // Actual memory used
    char role[64];               // Model role (main, vision, embed)
    char model_path[512];        // C3.6: so load_shared_context() can find an existing handle to share
    bool use_gpu;                // C3.6: remembered so attach_projector() matches the model's own load flags
    
    // C3.4: Memory pressure and LRU eviction
    ethervox_residency_class_t residency;  // RESIDENT or ON_DEMAND
    time_t last_use;             // Last access timestamp (LRU tracking)
    time_t loaded_at;            // Load timestamp (for TTL calculation)
    int refcount;                // Reference count (active generations)
    uint32_t ttl_seconds;        // TTL for on-demand models (0 = no expiry)
    
    struct ethervox_model_handle* next;  // Linked list
};

/**
 * C3.6: registry entry tracking one resident llama_model shared by potentially
 * several handles (their own context, optionally their own projector). The model's
 * weights are freed only when the last handle referencing it unloads.
 */
typedef struct ethervox_shared_model {
    struct llama_model* model;
    char model_path[512];
    int refcount;
    struct ethervox_shared_model* next;
} ethervox_shared_model_t;

/**
 * Model pool structure
 */
struct ethervox_model_pool {
    ethervox_paths_t paths;
    uint64_t budget_bytes;
    uint64_t used_bytes;
    ethervox_mutex_t pool_mutex;  // Protects pool state
    ethervox_model_handle_t* models;  // Linked list of loaded models
    ethervox_shared_model_t* shared_models;  // C3.6: models loaded via load_shared_context()
    
    // C3.4: Memory pressure and LRU eviction
    ethervox_memory_pressure_cb pressure_callback;  // OS pressure handler
    void* pressure_user_data;                       // User data for callback
    uint32_t max_on_demand;                         // Max concurrent on-demand (0 = no limit)
    uint32_t current_on_demand;                     // Current on-demand count
};

/**
 * Initialize global llama backend (refcounted)
 */
static ethervox_result_t backend_init(void) {
    MUTEX_LOCK(g_backend.mutex);
    
    if (!g_backend.initialized) {
#if LLAMA_AVAILABLE
        ggml_log_set(model_pool_ggml_log_callback, NULL);
        llama_log_set(model_pool_ggml_log_callback, NULL);
        llama_backend_init();
        ggml_backend_load_all();
        if (ggml_backend_reg_count() == 0) {
            ETHERVOX_LOG_ERROR("[ModelPool] No ggml backends loaded, cannot load models");
            MUTEX_UNLOCK(g_backend.mutex);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
        g_backend.initialized = true;
        ETHERVOX_LOG_INFO("[ModelPool] Initialized llama backend (%d ggml backends available)", 
                         (int)ggml_backend_reg_count());
#else
        MUTEX_UNLOCK(g_backend.mutex);
        return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#endif
    }
    
    g_backend.refcount++;
    ETHERVOX_LOG_DEBUG("[ModelPool] Backend refcount: %d", g_backend.refcount);
    
    MUTEX_UNLOCK(g_backend.mutex);
    return ETHERVOX_SUCCESS;
}

/**
 * Cleanup global llama backend (refcounted)
 */
static void backend_cleanup(void) {
    MUTEX_LOCK(g_backend.mutex);
    
    if (g_backend.refcount > 0) {
        g_backend.refcount--;
        ETHERVOX_LOG_DEBUG("[ModelPool] Backend refcount: %d", g_backend.refcount);
        
        if (g_backend.refcount == 0 && g_backend.initialized) {
#if LLAMA_AVAILABLE
            llama_backend_free();
            g_backend.initialized = false;
            ETHERVOX_LOG_INFO("[ModelPool] Cleaned up llama backend");
#endif
        }
    }
    
    MUTEX_UNLOCK(g_backend.mutex);
}

/**
 * Estimate memory requirements for a model
 */
static ethervox_result_t estimate_memory(
    const ethervox_model_config_t* config,
    uint64_t* out_bytes
) {
    if (!config || !out_bytes) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Get model file size
    struct stat st;
    if (stat(config->model_path, &st) != 0) {
        ETHERVOX_LOG_ERROR("[ModelPool] Cannot stat model file: %s", config->model_path);
        return ETHERVOX_ERROR_FILE_READ;
    }
    
    uint64_t file_size = (uint64_t)st.st_size;
    uint64_t kv_cache_size = config->context_size * KV_BYTES_PER_TOKEN_F16;
    
    *out_bytes = file_size + kv_cache_size + OVERHEAD_BYTES;
    
    ETHERVOX_LOG_DEBUG("[ModelPool] Memory estimate: file=%llu KB, kv=%llu KB, overhead=%llu KB, total=%llu KB",
                      (unsigned long long)(file_size / 1024),
                      (unsigned long long)(kv_cache_size / 1024),
                      (unsigned long long)(OVERHEAD_BYTES / 1024),
                      (unsigned long long)(*out_bytes / 1024));
    
    return ETHERVOX_SUCCESS;
}

/**
 * A requested context size of 0 means "as much of the model's trained context as fits": the KV
 * cache (f16, from the model's own layer and head shape) may take up to an eighth of the
 * machine's RAM, and never more than AUTO_CONTEXT_MAX: a model trained for 128k tokens would
 * otherwise reserve gigabytes of KV cache that an assistant conversation never fills, which
 * slows loading and takes memory from everything else. Any non-zero request is honoured as given.
 */
#define AUTO_CONTEXT_MAX 32768ull
/* Prompt tokens evaluated per step; larger than llama.cpp's 512 default so long prompts prefill
 * in fewer GPU passes. */
#define PROMPT_UBATCH 1024u
static uint32_t resolve_context_size(const struct llama_model* model, uint32_t requested) {
    if (requested != 0) {
        return requested;
    }
    int32_t train = llama_model_n_ctx_train(model);
    int32_t heads = llama_model_n_head(model);
    uint64_t bytes_per_token = 0;
    if (heads > 0) {
        bytes_per_token = 2ull * 2ull * (uint64_t)llama_model_n_layer(model) *
                          (uint64_t)llama_model_n_head_kv(model) *
                          ((uint64_t)llama_model_n_embd(model) / (uint64_t)heads);
    }
    uint64_t ram_mb = 0;
    if (ethervox_mem_get_info(&ram_mb, NULL) != ETHERVOX_SUCCESS) {
        ram_mb = 0;
    }
    uint64_t fit = (bytes_per_token > 0 && ram_mb > 0)
                       ? (ram_mb * 1024ull * 1024ull / 8ull) / bytes_per_token
                       : 8192;
    uint64_t size = fit < (uint64_t)train ? fit : (uint64_t)train;
    if (size > AUTO_CONTEXT_MAX) {
        size = AUTO_CONTEXT_MAX;
    }
    size &= ~1023ull;
    if (size < 4096) {
        size = 4096;
    }
    ETHERVOX_LOG_INFO("[ModelPool] Auto context: %llu tokens (model trained for %d, %llu MB RAM)",
                      (unsigned long long)size, train, (unsigned long long)ram_mb);
    return (uint32_t)size;
}

ethervox_result_t ethervox_model_pool_create(
    const ethervox_paths_t* paths,
    uint64_t budget_bytes,
    ethervox_model_pool_t** out
) {
    if (!paths || !out) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Global backend mutex is statically initialized, safe to call backend_init()
    ethervox_result_t result = backend_init();
    if (result != ETHERVOX_SUCCESS) {
        return result;
    }
    
    ethervox_model_pool_t* pool = (ethervox_model_pool_t*)calloc(1, sizeof(ethervox_model_pool_t));
    if (!pool) {
        backend_cleanup();
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy paths structure
    memcpy(&pool->paths, paths, sizeof(ethervox_paths_t));
    pool->budget_bytes = budget_bytes;
    pool->used_bytes = 0;
    pool->models = NULL;
    
    MUTEX_INIT(pool->pool_mutex);
    
    ETHERVOX_LOG_INFO("[ModelPool] Created pool with budget %llu MB",
                     (unsigned long long)(budget_bytes / (1024 * 1024)));
    
    *out = pool;
    return ETHERVOX_SUCCESS;
}

void ethervox_model_pool_destroy(ethervox_model_pool_t* pool) {
    if (!pool) {
        return;
    }
    
    ETHERVOX_LOG_INFO("[ModelPool] Destroying pool");
    
    // Unload all models
    ethervox_model_handle_t* handle = pool->models;
    while (handle) {
        ethervox_model_handle_t* next = handle->next;
        ethervox_model_pool_unload(pool, handle);
        handle = next;
    }
    
    MUTEX_DESTROY(pool->pool_mutex);
    free(pool);
    
    backend_cleanup();
}

ethervox_result_t ethervox_model_pool_load(
    ethervox_model_pool_t* pool,
    const ethervox_model_config_t* config,
    ethervox_progress_cb progress_cb,
    void* user_data,
    ethervox_model_handle_t** out
) {
    if (!pool || !config || !out) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
#if !LLAMA_AVAILABLE
    (void)progress_cb;
    (void)user_data;
    ETHERVOX_LOG_ERROR("[ModelPool] llama.cpp not available");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    
    // Check if model would fit
    uint64_t required_bytes = 0;
    bool fits = false;
    ethervox_result_t result = ethervox_model_pool_would_fit(pool, config, &fits, &required_bytes);
    if (result != ETHERVOX_SUCCESS) {
        return result;
    }
    
    // C3.4: Retry-once logic - if doesn't fit, try evicting and retry
    if (!fits && config->residency == ETHERVOX_RESIDENCY_ON_DEMAND) {
        ETHERVOX_LOG_INFO("[ModelPool] Model doesn't fit, attempting LRU eviction (need %llu MB)",
                         (unsigned long long)(required_bytes / (1024 * 1024)));
        
        // Try to free enough space for this model
        uint64_t bytes_needed = required_bytes - (pool->budget_bytes - pool->used_bytes);
        uint64_t freed = 0;
        
        // Protect the role we're trying to load (don't evict same role)
        const char* protected[2] = { config->role, NULL };
        result = ethervox_model_pool_evict_lru(pool, bytes_needed, protected, &freed);
        
        if (result == ETHERVOX_SUCCESS && freed > 0) {
            ETHERVOX_LOG_INFO("[ModelPool] Evicted %llu MB, retrying load",
                             (unsigned long long)(freed / (1024 * 1024)));
            
            // Retry would_fit check
            result = ethervox_model_pool_would_fit(pool, config, &fits, &required_bytes);
            if (result != ETHERVOX_SUCCESS) {
                return result;
            }
        }
    }
    
    if (!fits) {
        ETHERVOX_LOG_ERROR("[ModelPool] Model requires %llu MB but only %llu MB available (after eviction attempt)",
                          (unsigned long long)(required_bytes / (1024 * 1024)),
                          (unsigned long long)((pool->budget_bytes - pool->used_bytes) / (1024 * 1024)));
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    // C3.4: Check max_on_demand limit
    if (config->residency == ETHERVOX_RESIDENCY_ON_DEMAND && pool->max_on_demand > 0) {
        MUTEX_LOCK(pool->pool_mutex);
        uint32_t current = pool->current_on_demand;
        MUTEX_UNLOCK(pool->pool_mutex);
        
        if (current >= pool->max_on_demand) {
            ETHERVOX_LOG_INFO("[ModelPool] Max on-demand limit reached (%u), evicting LRU", pool->max_on_demand);
            
            // Evict one on-demand model (protect the role we're loading)
            const char* protected[2] = { config->role, NULL };
            uint64_t freed = 0;
            result = ethervox_model_pool_evict_lru(pool, 0, protected, &freed);
            
            if (result != ETHERVOX_SUCCESS) {
                ETHERVOX_LOG_ERROR("[ModelPool] Failed to evict for max_on_demand enforcement");
                return ETHERVOX_ERROR_OUT_OF_MEMORY;
            }
        }
    }
    
    ETHERVOX_LOG_INFO("[ModelPool] Loading model: %s (role=%s, ctx=%u)",
                     config->model_path, config->role ? config->role : "none", config->context_size);
    
    // Load model
    struct llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = config->use_gpu ? 99 : 0;  // Use GPU if requested
    
    struct llama_model* model = llama_model_load_from_file(config->model_path, model_params);
    if (!model) {
        ETHERVOX_LOG_ERROR("[ModelPool] Failed to load model: %s", config->model_path);
        return ETHERVOX_ERROR_FILE_READ;
    }
    
    // Create context
    struct llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = resolve_context_size(model, config->context_size);
    ctx_params.n_threads = config->n_threads;
    ctx_params.n_threads_batch = config->n_threads;
    ctx_params.n_ubatch = ctx_params.n_batch < PROMPT_UBATCH ? ctx_params.n_batch : PROMPT_UBATCH;
    ctx_params.n_seq_max = config->n_seq_max > 0 ? config->n_seq_max : 1;
    ctx_params.kv_unified = config->kv_unified;
    ctx_params.embeddings = config->embeddings;
    if (config->embeddings) {
        // llama_get_embeddings_seq() returns NULL unless pooling_type is set at context-creation
        // time (it cannot be chosen per-call) - see llama.h's own "Returns NULL if pooling_type is
        // LLAMA_POOLING_TYPE_NONE" contract. ethervox_embed_texts()'s `pooling` parameter is
        // therefore only meaningful for MEAN today; a caller requesting CLS/LAST still gets a
        // context pooled as MEAN, since ethervox_model_config_t has no per-strategy field yet.
        // Tracked as a known limitation (E4.7), not fixed here - MEAN is this API's only verified
        // working strategy.
        ctx_params.pooling_type = LLAMA_POOLING_TYPE_MEAN;
    }
    
    struct llama_context* ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
        llama_free_model(model);
        ETHERVOX_LOG_ERROR("[ModelPool] Failed to create context");
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    // Create handle
    ethervox_model_handle_t* handle = (ethervox_model_handle_t*)calloc(1, sizeof(ethervox_model_handle_t));
    if (!handle) {
        llama_free(ctx);
        llama_free_model(model);
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    handle->model = model;
    handle->ctx = ctx;
    handle->mtmd_ctx = NULL;  // Initialize to NULL
    handle->memory_bytes = required_bytes;
    handle->use_gpu = config->use_gpu;
    if (config->role) {
        strncpy(handle->role, config->role, sizeof(handle->role) - 1);
    }
    if (config->model_path) {
        strncpy(handle->model_path, config->model_path, sizeof(handle->model_path) - 1);
    }
    
    // C3.4: Initialize LRU and residency fields
    time_t now = time(NULL);
    handle->residency = config->residency;
    handle->last_use = now;
    handle->loaded_at = now;
    handle->refcount = 0;
    handle->ttl_seconds = (config->ttl_seconds > 0) ? config->ttl_seconds : 90;  // Default 90s
    
    // Initialize mutex BEFORE any error paths that might destroy it
    MUTEX_INIT(handle->inference_mutex);
    
    // Load mmproj if provided (for multimodal support)
#if defined(MTMD_AVAILABLE) && MTMD_AVAILABLE
    if (config->mmproj_path) {
        ETHERVOX_LOG_INFO("[ModelPool] About to load mmproj: %s", config->mmproj_path);
        ETHERVOX_LOG_INFO("[ModelPool] Model pointer: %p, Context pointer: %p", (void*)model, (void*)ctx);
        
        struct mtmd_context_params mtmd_params = mtmd_context_params_default();
        mtmd_params.use_gpu = config->use_gpu;
        mtmd_params.print_timings = false;
        mtmd_params.n_threads = (int)config->n_threads;
        // mtmd_tokenize() splits prompts on this marker; NULL keeps mtmd's own
        // default ("<__media__>"). Callers whose prompts embed a different
        // literal marker (e.g. Granite Speech's "<|audio|>") must set it.
        if (config->media_marker) {
            mtmd_params.media_marker = config->media_marker;
        }
        
        ETHERVOX_LOG_INFO("[ModelPool] Calling mtmd_init_from_file...");
        void* mctx = mtmd_init_from_file(config->mmproj_path, model, mtmd_params);
        ETHERVOX_LOG_INFO("[ModelPool] mtmd_init_from_file returned: %p", mctx);
        if (!mctx) {
            ETHERVOX_LOG_ERROR("[ModelPool] Failed to load mmproj: %s", config->mmproj_path);
            MUTEX_DESTROY(handle->inference_mutex);
            free(handle);
            llama_free(ctx);
            llama_free_model(model);
            return ETHERVOX_ERROR_FILE_READ;
        }
        
        handle->mtmd_ctx = mctx;
        ETHERVOX_LOG_INFO("[ModelPool] Loaded mmproj: %s (vision=%d, audio=%d)",
                         config->mmproj_path,
                         mtmd_support_vision(mctx),
                         mtmd_support_audio(mctx));
    }
#else
    if (config->mmproj_path) {
        ETHERVOX_LOG_WARN("[ModelPool] mmproj requested but MTMD not available in this build");
    }
#endif
    
    // Add to pool
    MUTEX_LOCK(pool->pool_mutex);
    handle->next = pool->models;
    pool->models = handle;
    pool->used_bytes += required_bytes;
    
    // C3.4: Update on-demand counter
    if (handle->residency == ETHERVOX_RESIDENCY_ON_DEMAND) {
        pool->current_on_demand++;
    }
    MUTEX_UNLOCK(pool->pool_mutex);
    
    ETHERVOX_LOG_INFO("[ModelPool] Loaded model successfully (using %llu MB / %llu MB)",
                     (unsigned long long)(pool->used_bytes / (1024 * 1024)),
                     (unsigned long long)(pool->budget_bytes / (1024 * 1024)));
    
    *out = handle;
    return ETHERVOX_SUCCESS;
#endif
}

ethervox_result_t ethervox_model_pool_unload(
    ethervox_model_pool_t* pool,
    ethervox_model_handle_t* handle
) {
    if (!pool || !handle) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ETHERVOX_LOG_INFO("[ModelPool] Unloading model (role=%s)", handle->role[0] ? handle->role : "none");
    
    // Remove from pool
    MUTEX_LOCK(pool->pool_mutex);
    ethervox_model_handle_t** prev = &pool->models;
    while (*prev && *prev != handle) {
        prev = &(*prev)->next;
    }
    if (*prev) {
        *prev = handle->next;
        pool->used_bytes -= handle->memory_bytes;
        
        // C3.4: Update on-demand counter
        if (handle->residency == ETHERVOX_RESIDENCY_ON_DEMAND) {
            pool->current_on_demand--;
        }
    }

    // C3.6: if this handle's model is shared (loaded via load_shared_context()),
    // only the last sharer to unload actually frees the weights.
    ethervox_shared_model_t** shared_prev = &pool->shared_models;
    ethervox_shared_model_t* shared = NULL;
    while (*shared_prev) {
        if ((*shared_prev)->model == handle->model) {
            shared = *shared_prev;
            break;
        }
        shared_prev = &(*shared_prev)->next;
    }
    bool free_model = true;
    if (shared) {
        shared->refcount--;
        free_model = (shared->refcount <= 0);
        if (free_model) {
            *shared_prev = shared->next;
        }
    }
    MUTEX_UNLOCK(pool->pool_mutex);
    
    // Free resources
#if LLAMA_AVAILABLE
    // Free mtmd context if loaded
#if defined(MTMD_AVAILABLE) && MTMD_AVAILABLE
    if (handle->mtmd_ctx) {
        mtmd_free((mtmd_context*)handle->mtmd_ctx);
        handle->mtmd_ctx = NULL;
    }
#endif
    
    if (handle->ctx) {
        llama_free(handle->ctx);
    }
    if (free_model && handle->model) {
        llama_free_model(handle->model);
    }
#endif

    if (shared && free_model) {
        free(shared);
    }
    
    MUTEX_DESTROY(handle->inference_mutex);
    free(handle);
    
    ETHERVOX_LOG_INFO("[ModelPool] Model unloaded (using %llu MB / %llu MB)",
                     (unsigned long long)(pool->used_bytes / (1024 * 1024)),
                     (unsigned long long)(pool->budget_bytes / (1024 * 1024)));
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_model_pool_would_fit(
    const ethervox_model_pool_t* pool,
    const ethervox_model_config_t* config,
    bool* out_fits,
    uint64_t* out_required
) {
    if (!pool || !config || !out_fits) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint64_t required = 0;
    ethervox_result_t result = estimate_memory(config, &required);
    if (result != ETHERVOX_SUCCESS) {
        return result;
    }
    
    if (out_required) {
        *out_required = required;
    }
    
    // Check against budget (if budget is 0, no limit)
    if (pool->budget_bytes == 0) {
        *out_fits = true;
    } else {
        uint64_t available = pool->budget_bytes - pool->used_bytes;
        *out_fits = (required <= available);
    }
    
    ETHERVOX_LOG_DEBUG("[ModelPool] would_fit: required=%llu MB, available=%llu MB, fits=%d",
                      (unsigned long long)(required / (1024 * 1024)),
                      (unsigned long long)((pool->budget_bytes - pool->used_bytes) / (1024 * 1024)),
                      *out_fits);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_model_pool_memory_usage(
    const ethervox_model_pool_t* pool,
    uint64_t* out_used,
    uint64_t* out_budget
) {
    if (!pool) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (out_used) {
        *out_used = pool->used_bytes;
    }
    if (out_budget) {
        *out_budget = pool->budget_bytes;
    }
    
    return ETHERVOX_SUCCESS;
}

struct llama_model* ethervox_model_handle_get_model(
    ethervox_model_handle_t* handle
) {
    return handle ? handle->model : NULL;
}

struct llama_context* ethervox_model_handle_get_context(
    ethervox_model_handle_t* handle
) {
    return handle ? handle->ctx : NULL;
}

// N6.3: mtmd getter for multimodal models
struct mtmd_context* ethervox_model_handle_get_mtmd(
    ethervox_model_handle_t* handle
) {
    return handle ? handle->mtmd_ctx : NULL;
}

const char* ethervox_model_handle_get_path(ethervox_model_handle_t* handle) {
    return handle ? handle->model_path : NULL;
}

void ethervox_model_handle_lock(ethervox_model_handle_t* handle) {
    if (handle) {
        MUTEX_LOCK(handle->inference_mutex);
    }
}

void ethervox_model_handle_unlock(ethervox_model_handle_t* handle) {
    if (handle) {
        MUTEX_UNLOCK(handle->inference_mutex);
    }
}

// ============================================================================
// C3.4: Memory Pressure and LRU Eviction
// ============================================================================

ethervox_result_t ethervox_model_pool_set_pressure_callback(
    ethervox_model_pool_t* pool,
    ethervox_memory_pressure_cb callback,
    void* user_data
) {
    if (!pool) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    MUTEX_LOCK(pool->pool_mutex);
    pool->pressure_callback = callback;
    pool->pressure_user_data = user_data;
    MUTEX_UNLOCK(pool->pool_mutex);
    
    ETHERVOX_LOG_INFO("[ModelPool] Memory pressure callback %s",
                     callback ? "registered" : "unregistered");
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_model_pool_set_max_on_demand(
    ethervox_model_pool_t* pool,
    uint32_t max_concurrent
) {
    if (!pool) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    MUTEX_LOCK(pool->pool_mutex);
    pool->max_on_demand = max_concurrent;
    MUTEX_UNLOCK(pool->pool_mutex);
    
    ETHERVOX_LOG_INFO("[ModelPool] Max concurrent on-demand models: %u %s",
                     max_concurrent, max_concurrent == 0 ? "(no limit)" : "");
    return ETHERVOX_SUCCESS;
}

/**
 * Helper: Check if role is protected
 */
static bool is_role_protected(const char* role, const char** protected_roles) {
    if (!protected_roles || !role) {
        return false;
    }
    
    for (int i = 0; protected_roles[i] != NULL; i++) {
        if (strcmp(role, protected_roles[i]) == 0) {
            return true;
        }
    }
    return false;
}

ethervox_result_t ethervox_model_pool_evict_lru(
    ethervox_model_pool_t* pool,
    uint64_t bytes_to_free,
    const char** protected_roles,
    uint64_t* out_freed
) {
    if (!pool) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    MUTEX_LOCK(pool->pool_mutex);
    
    uint64_t freed = 0;
    bool continue_evicting = true;
    
    while (continue_evicting) {
        // Find LRU on-demand model that can be evicted
        ethervox_model_handle_t* lru_candidate = NULL;
        time_t oldest_use = 0;
        
        for (ethervox_model_handle_t* h = pool->models; h != NULL; h = h->next) {
            // Skip resident models
            if (h->residency != ETHERVOX_RESIDENCY_ON_DEMAND) {
                continue;
            }
            
            // Skip models with active references (in-flight generation)
            if (h->refcount > 0) {
                continue;
            }
            
            // Skip protected roles
            if (is_role_protected(h->role, protected_roles)) {
                continue;
            }
            
            // Track oldest (LRU)
            if (lru_candidate == NULL || h->last_use < oldest_use) {
                lru_candidate = h;
                oldest_use = h->last_use;
            }
        }
        
        // No more evictable models
        if (lru_candidate == NULL) {
            ETHERVOX_LOG_INFO("[ModelPool] LRU eviction: no more evictable models (freed %llu MB)",
                             (unsigned long long)(freed / (1024 * 1024)));
            break;
        }
        
        // Evict this model
        ETHERVOX_LOG_INFO("[ModelPool] Evicting LRU model: role=%s, last_use=%ld seconds ago",
                         lru_candidate->role,
                         (long)(time(NULL) - lru_candidate->last_use));
        
        uint64_t model_bytes = lru_candidate->memory_bytes;
        freed += model_bytes;
        
        // Unlock before calling unload (unload acquires the lock)
        MUTEX_UNLOCK(pool->pool_mutex);
        ethervox_result_t result = ethervox_model_pool_unload(pool, lru_candidate);
        MUTEX_LOCK(pool->pool_mutex);
        
        if (result != ETHERVOX_SUCCESS) {
            ETHERVOX_LOG_ERROR("[ModelPool] Failed to unload LRU model");
            break;
        }
        
        // Check if we've freed enough (if target was specified)
        if (bytes_to_free > 0 && freed >= bytes_to_free) {
            ETHERVOX_LOG_INFO("[ModelPool] LRU eviction target met: freed %llu MB",
                             (unsigned long long)(freed / (1024 * 1024)));
            continue_evicting = false;
        }
    }
    
    MUTEX_UNLOCK(pool->pool_mutex);
    
    if (out_freed) {
        *out_freed = freed;
    }
    
    return ETHERVOX_SUCCESS;
}

// ============================================================================
// C3.6: Shared model handle (one llama_model, multiple contexts) + runtime
// projector attach/detach
// ============================================================================

ethervox_result_t ethervox_model_pool_attach_projector(
    ethervox_model_pool_t* pool,
    ethervox_model_handle_t* handle,
    const char* mmproj_path,
    const char* media_marker
) {
    if (!pool || !handle || !mmproj_path) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

#if !LLAMA_AVAILABLE || !(defined(MTMD_AVAILABLE) && MTMD_AVAILABLE)
    (void)media_marker;
    ETHERVOX_LOG_ERROR("[ModelPool] MTMD not available in this build - cannot attach a projector");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    if (handle->mtmd_ctx) {
        ETHERVOX_LOG_ERROR("[ModelPool] Projector already attached to this handle - detach first");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    struct mtmd_context_params mtmd_params = mtmd_context_params_default();
    mtmd_params.print_timings = false;
    if (media_marker) {
        mtmd_params.media_marker = media_marker;
    }

    void* mctx = mtmd_init_from_file(mmproj_path, handle->model, mtmd_params);
    if (!mctx) {
        ETHERVOX_LOG_ERROR("[ModelPool] Failed to attach projector: %s", mmproj_path);
        return ETHERVOX_ERROR_FILE_READ;
    }

    struct stat st;
    uint64_t mmproj_bytes = (stat(mmproj_path, &st) == 0) ? (uint64_t)st.st_size : 0;

    handle->mtmd_ctx = mctx;
    handle->mmproj_bytes = mmproj_bytes;

    MUTEX_LOCK(pool->pool_mutex);
    handle->memory_bytes += mmproj_bytes;
    pool->used_bytes += mmproj_bytes;
    MUTEX_UNLOCK(pool->pool_mutex);

    ETHERVOX_LOG_INFO("[ModelPool] Attached projector %s (+%llu MB, vision=%d, audio=%d)",
                     mmproj_path,
                     (unsigned long long)(mmproj_bytes / (1024 * 1024)),
                     mtmd_support_vision(mctx),
                     mtmd_support_audio(mctx));
    return ETHERVOX_SUCCESS;
#endif
}

ethervox_result_t ethervox_model_pool_detach_projector(
    ethervox_model_pool_t* pool,
    ethervox_model_handle_t* handle
) {
    if (!pool || !handle) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

#if defined(MTMD_AVAILABLE) && MTMD_AVAILABLE
    if (!handle->mtmd_ctx) {
        return ETHERVOX_SUCCESS;  // Nothing attached - no-op.
    }

    mtmd_free((mtmd_context*)handle->mtmd_ctx);
    handle->mtmd_ctx = NULL;

    MUTEX_LOCK(pool->pool_mutex);
    handle->memory_bytes -= handle->mmproj_bytes;
    pool->used_bytes -= handle->mmproj_bytes;
    MUTEX_UNLOCK(pool->pool_mutex);

    ETHERVOX_LOG_INFO("[ModelPool] Detached projector (-%llu MB)",
                     (unsigned long long)(handle->mmproj_bytes / (1024 * 1024)));
    handle->mmproj_bytes = 0;
#endif
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_model_pool_load_shared_context(
    ethervox_model_pool_t* pool,
    const ethervox_model_config_t* config,
    ethervox_progress_cb progress_cb,
    void* user_data,
    ethervox_model_handle_t** out
) {
    if (!pool || !config || !out) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

#if !LLAMA_AVAILABLE
    (void)progress_cb;
    (void)user_data;
    ETHERVOX_LOG_ERROR("[ModelPool] llama.cpp not available");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    MUTEX_LOCK(pool->pool_mutex);
    ethervox_shared_model_t* shared = pool->shared_models;
    while (shared && strncmp(shared->model_path, config->model_path, sizeof(shared->model_path)) != 0) {
        shared = shared->next;
    }
    MUTEX_UNLOCK(pool->pool_mutex);

    if (!shared) {
        // First caller for this model_path: load it exactly as ethervox_model_pool_load()
        // would, then register it so later callers share its weights instead of reloading.
        ethervox_result_t result = ethervox_model_pool_load(pool, config, progress_cb, user_data, out);
        if (result != ETHERVOX_SUCCESS) {
            return result;
        }

        ethervox_shared_model_t* entry = (ethervox_shared_model_t*)calloc(1, sizeof(*entry));
        if (!entry) {
            ETHERVOX_LOG_WARN("[ModelPool] Out of memory registering shared model entry - "
                             "%s loaded but future callers will not share its weights",
                             config->model_path);
            return ETHERVOX_SUCCESS;
        }
        entry->model = (*out)->model;
        strncpy(entry->model_path, config->model_path, sizeof(entry->model_path) - 1);
        entry->refcount = 1;

        MUTEX_LOCK(pool->pool_mutex);
        entry->next = pool->shared_models;
        pool->shared_models = entry;
        MUTEX_UNLOCK(pool->pool_mutex);

        return ETHERVOX_SUCCESS;
    }

    // Model already resident: a new context (and, if requested, a new projector)
    // is all that's needed - the weights are not reloaded and are not charged again.
    ETHERVOX_LOG_INFO("[ModelPool] Sharing resident model for a new context: %s", config->model_path);

    struct llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = resolve_context_size(shared->model, config->context_size);
    ctx_params.n_threads = config->n_threads;
    ctx_params.n_threads_batch = config->n_threads;
    ctx_params.n_ubatch = ctx_params.n_batch < PROMPT_UBATCH ? ctx_params.n_batch : PROMPT_UBATCH;
    ctx_params.n_seq_max = config->n_seq_max > 0 ? config->n_seq_max : 1;
    ctx_params.kv_unified = config->kv_unified;

    struct llama_context* ctx = llama_init_from_model(shared->model, ctx_params);
    if (!ctx) {
        ETHERVOX_LOG_ERROR("[ModelPool] Failed to create shared context for %s", config->model_path);
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }

    ethervox_model_handle_t* handle = (ethervox_model_handle_t*)calloc(1, sizeof(ethervox_model_handle_t));
    if (!handle) {
        llama_free(ctx);
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }

    handle->model = shared->model;
    handle->ctx = ctx;
    handle->mtmd_ctx = NULL;
    // Only the new KV cache is charged here - the weights are already accounted for
    // on whichever handle first loaded this model_path.
    handle->memory_bytes = (uint64_t)config->context_size * KV_BYTES_PER_TOKEN_F16;
    if (config->role) {
        strncpy(handle->role, config->role, sizeof(handle->role) - 1);
    }

    time_t now = time(NULL);
    handle->residency = config->residency;
    handle->last_use = now;
    handle->loaded_at = now;
    handle->refcount = 0;
    handle->ttl_seconds = (config->ttl_seconds > 0) ? config->ttl_seconds : 90;

    MUTEX_INIT(handle->inference_mutex);

    if (config->mmproj_path) {
        ethervox_result_t attach_result = ethervox_model_pool_attach_projector(
            pool, handle, config->mmproj_path, config->media_marker);
        if (attach_result != ETHERVOX_SUCCESS) {
            MUTEX_DESTROY(handle->inference_mutex);
            free(handle);
            llama_free(ctx);
            return attach_result;
        }
    }

    MUTEX_LOCK(pool->pool_mutex);
    handle->next = pool->models;
    pool->models = handle;
    // attach_projector() (above) already added handle->mmproj_bytes to pool->used_bytes
    // directly when it attached - only the KV-cache portion of memory_bytes is new here,
    // or the whole thing double-counts the projector.
    pool->used_bytes += (handle->memory_bytes - handle->mmproj_bytes);
    if (handle->residency == ETHERVOX_RESIDENCY_ON_DEMAND) {
        pool->current_on_demand++;
    }
    shared->refcount++;
    MUTEX_UNLOCK(pool->pool_mutex);

    ETHERVOX_LOG_INFO("[ModelPool] Shared-context load complete: model refcount now %d, "
                     "new handle uses %llu MB (weights not recharged)",
                     shared->refcount, (unsigned long long)(handle->memory_bytes / (1024 * 1024)));

    *out = handle;
    return ETHERVOX_SUCCESS;
#endif
}

