/**
 * @file model_pool.h
 * @brief Multi-model pool for concurrent model loading and execution
 *
 * Manages multiple loaded models with memory budget enforcement. Models can run
 * concurrently with per-model mutex serialization. Provides accurate memory estimation
 * to prevent OOM kills.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_MODEL_POOL_H
#define ETHERVOX_MODEL_POOL_H

#include "ethervox/error.h"
#include "ethervox/paths.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque handle to a model pool
 */
typedef struct ethervox_model_pool ethervox_model_pool_t;

/**
 * Opaque handle to a loaded model within a pool
 */
typedef struct ethervox_model_handle ethervox_model_handle_t;

/**
 * Progress callback for model loading
 * 
 * @param progress Value between 0.0 and 1.0
 * @param user_data User-provided context
 */
typedef void (*ethervox_progress_cb)(float progress, void* user_data);

/**
 * Model residency class (C3.4)
 * 
 * Determines whether a model should be evicted under memory pressure.
 */
typedef enum {
    ETHERVOX_RESIDENCY_RESIDENT,   /**< Never evicted except explicitly; main models */
    ETHERVOX_RESIDENCY_ON_DEMAND   /**< Evictable under pressure or TTL; vision/speech */
} ethervox_residency_class_t;

/**
 * Model configuration for loading
 */
typedef struct {
    const char* model_path;        /**< Path to GGUF model file */
    const char* mmproj_path;       /**< Path to companion mmproj file (NULL if not multimodal) */
    const char* media_marker;      /**< Media placeholder marker used in prompts fed to mtmd_tokenize()
                                         (NULL = mtmd's own default, "<__media__>"). Must match the
                                         literal marker text embedded in prompts by the caller. */
    uint32_t context_size;         /**< Context size in tokens; 0 = model-trained size bounded by RAM */
    uint32_t n_threads;            /**< Number of threads for inference */
    uint32_t n_seq_max;            /**< Max number of sequences (0 = default 1) */
    bool use_gpu;                  /**< Whether to use GPU acceleration */
    bool kv_unified;               /**< Use unified KV buffer for shared prefixes */
    bool embeddings;               /**< Create the context with embeddings output enabled -
                                         required for ethervox_embed_texts(); llama_get_embeddings()/
                                         llama_get_embeddings_seq() return NULL otherwise, even on a
                                         model that otherwise supports it */
    const char* role;              /**< Model role (e.g., "main", "vision", "embed") */
    ethervox_residency_class_t residency;  /**< Residency class (C3.4) */
    uint32_t ttl_seconds;          /**< TTL for on-demand models (0 = default 90s) */
} ethervox_model_config_t;

/**
 * Create a model pool with a memory budget
 * 
 * The pool manages loaded models and enforces the memory budget. Models that would
 * exceed the budget are rejected at load time.
 * 
 * @param paths Path configuration
 * @param budget_bytes Maximum memory budget in bytes (0 = no limit)
 * @param out Receives the created pool (caller must destroy)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_model_pool_create(
    const ethervox_paths_t* paths,
    uint64_t budget_bytes,
    ethervox_model_pool_t** out
);

/**
 * Destroy a model pool
 * 
 * Unloads all models and frees resources. Any outstanding model handles become invalid.
 * 
 * @param pool Pool to destroy (NULL is safe)
 */
void ethervox_model_pool_destroy(ethervox_model_pool_t* pool);

/**
 * Load a model into the pool
 * 
 * Loads the specified model if it fits within the memory budget. The model is assigned
 * a unique handle for subsequent operations. Multiple models can be loaded and run
 * concurrently.
 * 
 * Thread-safe: Can be called concurrently from multiple threads.
 * 
 * @param pool Model pool
 * @param config Model configuration
 * @param progress_cb Optional progress callback
 * @param user_data User data for progress callback
 * @param out Receives model handle (caller must unload)
 * @return ETHERVOX_SUCCESS, ETHERVOX_ERROR_OUT_OF_MEMORY if budget exceeded, or error code
 */
ethervox_result_t ethervox_model_pool_load(
    ethervox_model_pool_t* pool,
    const ethervox_model_config_t* config,
    ethervox_progress_cb progress_cb,
    void* user_data,
    ethervox_model_handle_t** out
);

/**
 * Unload a model from the pool
 * 
 * Frees the model and its resources. The handle becomes invalid after this call.
 * 
 * Thread-safe: Can be called concurrently from multiple threads.
 * 
 * @param pool Model pool
 * @param handle Model handle (NULL is safe)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_model_pool_unload(
    ethervox_model_pool_t* pool,
    ethervox_model_handle_t* handle
);

/**
 * Check if a model would fit in the pool
 * 
 * Estimates memory requirements without loading. Calculation includes:
 * - Model file size
 * - KV cache (context_size × bytes_per_token)
 * - Per-architecture overhead constant
 * 
 * Must be accurate within ±10% to prevent OOM kills.
 * 
 * @param pool Model pool
 * @param config Model configuration to check
 * @param out_fits Receives true if model would fit, false otherwise
 * @param out_required Receives estimated bytes required (can be NULL)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_model_pool_would_fit(
    const ethervox_model_pool_t* pool,
    const ethervox_model_config_t* config,
    bool* out_fits,
    uint64_t* out_required
);

/**
 * Get current memory usage and budget
 * 
 * Reports current memory consumption across all loaded models.
 * 
 * @param pool Model pool
 * @param out_used Receives bytes currently used (can be NULL)
 * @param out_budget Receives total budget in bytes (can be NULL)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_model_pool_memory_usage(
    const ethervox_model_pool_t* pool,
    uint64_t* out_used,
    uint64_t* out_budget
);

/**
 * Get llama_model from a model handle
 * 
 * Returns the underlying llama_model pointer for session management.
 * The pointer remains owned by the handle; do not free it.
 * 
 * @param handle Model handle
 * @return llama_model pointer, or NULL if handle is invalid
 */
struct llama_model* ethervox_model_handle_get_model(
    ethervox_model_handle_t* handle
);

/**
 * Get llama_context from a model handle
 * 
 * Returns the underlying llama_context pointer for session management.
 * The pointer remains owned by the handle; do not free it.
 * 
 * @param handle Model handle
 * @return llama_context pointer, or NULL if handle is invalid
 */
struct llama_context* ethervox_model_handle_get_context(
    ethervox_model_handle_t* handle
);

/**
 * Get mtmd_context from a model handle (N6.3)
 * 
 * Returns the multimodal context pointer if the model was loaded with mmproj_path.
 * The pointer remains owned by the handle; do not free it.
 * 
 * @param handle Model handle
 * @return mtmd_context pointer, or NULL if handle is invalid or model has no mmproj
 */
struct mtmd_context* ethervox_model_handle_get_mtmd(
    ethervox_model_handle_t* handle
);

/**
 * Serialize inference on a model handle. Callers that drive the handle's llama_context directly
 * (e.g. ethervox_vision_generate) must hold this for the whole decode, the same lock the pool's
 * own generation paths take.
 */
void ethervox_model_handle_lock(ethervox_model_handle_t* handle);

/** Releases the lock taken by ethervox_model_handle_lock(). */
void ethervox_model_handle_unlock(ethervox_model_handle_t* handle);

/**
 * Memory pressure callback (C3.4)
 * 
 * Called by the host when the OS signals memory pressure. The pool will evict
 * on-demand models in LRU order until pressure is relieved or no more evictable
 * models remain.
 * 
 * Thread-safe: May be called from any thread.
 * 
 * @param user_data User data provided during registration
 */
typedef void (*ethervox_memory_pressure_cb)(void* user_data);

/**
 * Set memory pressure callback (C3.4)
 * 
 * Registers a callback that the host invokes when OS memory pressure occurs
 * (e.g., DispatchSource.memoryPressure on Apple, onTrimMemory on Android).
 * 
 * When invoked, the pool evicts on-demand models in LRU order until sufficient
 * memory is freed. Resident models are never evicted by this mechanism.
 * 
 * @param pool Model pool
 * @param callback Pressure callback (NULL to unregister)
 * @param user_data User data passed to callback
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_model_pool_set_pressure_callback(
    ethervox_model_pool_t* pool,
    ethervox_memory_pressure_cb callback,
    void* user_data
);

/**
 * Evict LRU on-demand models (C3.4)
 * 
 * Evicts least-recently-used on-demand models until the specified number of bytes
 * is freed, or until no more evictable models remain.
 * 
 * Protected roles are never evicted (e.g., protect "main" governor).
 * Models with active inference are skipped.
 * 
 * Thread-safe: Can be called concurrently.
 * 
 * @param pool Model pool
 * @param bytes_to_free Target bytes to free (0 = evict all on-demand)
 * @param protected_roles Array of role names to protect (NULL-terminated)
 * @param out_freed Receives actual bytes freed (can be NULL)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_model_pool_evict_lru(
    ethervox_model_pool_t* pool,
    uint64_t bytes_to_free,
    const char** protected_roles,
    uint64_t* out_freed
);

/**
 * Set max concurrent on-demand models (C3.4)
 * 
 * Enforces a limit on the number of simultaneously loaded on-demand models.
 * Used for tier S/XS devices where only 1 on-demand model is allowed.
 * 
 * When the limit is reached, loading a new on-demand model evicts the LRU one.
 * Resident models do not count toward this limit.
 * 
 * @param pool Model pool
 * @param max_concurrent Maximum concurrent on-demand models (0 = no limit)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_model_pool_set_max_on_demand(
    ethervox_model_pool_t* pool,
    uint32_t max_concurrent
);

/**
 * Attach a multimodal projector to an already-loaded handle at runtime (C3.6)
 *
 * Loads an mtmd projector bound to the handle's existing model and attaches it to
 * this handle's context, so a text-only handle can gain audio/vision capability
 * without reloading the model. Errors if a projector is already attached to this
 * handle - call `ethervox_model_pool_detach_projector()` first.
 *
 * @param pool Pool the handle belongs to
 * @param handle Handle to attach the projector to
 * @param mmproj_path Path to the mmproj GGUF
 * @param media_marker Media placeholder marker used in prompts (NULL = mtmd's own default)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_model_pool_attach_projector(
    ethervox_model_pool_t* pool,
    ethervox_model_handle_t* handle,
    const char* mmproj_path,
    const char* media_marker
);

/**
 * Detach and free a handle's multimodal projector at runtime (C3.6)
 *
 * Frees the projector's weights without touching the handle's underlying model or
 * context - text generation on that context is unaffected. Safe to call when no
 * projector is attached (no-op).
 *
 * @param pool Pool the handle belongs to
 * @param handle Handle to detach the projector from
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_model_pool_detach_projector(
    ethervox_model_pool_t* pool,
    ethervox_model_handle_t* handle
);

/**
 * Load a new context against an already-resident model, sharing its weights (C3.6)
 *
 * If a handle in this pool was already loaded from `config->model_path`, this skips
 * loading the model file again and creates a second `llama_context` (and, if
 * `config->mmproj_path` is set, its own mtmd projector) against the **same**
 * `llama_model`. Memory accounting only charges the new KV cache and any newly
 * loaded projector - not the model weights again - so `would_fit()`/
 * `ethervox_model_pool_memory_usage()` stay honest about shared weights.
 *
 * If no handle with `config->model_path` exists yet, behaves exactly like
 * `ethervox_model_pool_load()` (loads the model fresh, as the first sharer).
 *
 * The two resulting handles' contexts must never be used concurrently from the
 * caller's own generation and ASR threads without external synchronization - the
 * pool only serializes within a single handle's own inference mutex, not across
 * handles that share a model (see the C3.6 task packet's threading rule).
 *
 * @param pool Model pool
 * @param config Model configuration for the new context (model_path must match an
 *   existing handle's, or a fresh model load is performed)
 * @param progress_cb Optional progress callback (only invoked on a fresh model load)
 * @param user_data User data for progress callback
 * @param out Receives the new handle (caller must unload)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_model_pool_load_shared_context(
    ethervox_model_pool_t* pool,
    const ethervox_model_config_t* config,
    ethervox_progress_cb progress_cb,
    void* user_data,
    ethervox_model_handle_t** out
);

#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_MODEL_POOL_H */
