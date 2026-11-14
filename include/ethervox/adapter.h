/**
 * @file adapter.h
 * @brief LoRA adapter loading and management
 *
 * Exposes llama.cpp's LoRA adapter support for parameter-efficient fine-tuning.
 * Adapters can be loaded independently and applied to models at runtime without
 * modifying base model weights.
 *
 * Use case: Granite Libraries provides LoRA adapters for certainty estimation
 * and hallucination detection. Load these adapters to specialize model behavior
 * without shipping multiple full models.
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_ADAPTER_H
#define ETHERVOX_ADAPTER_H

#include "ethervox/error.h"
#include "ethervox/model_pool.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque handle to a LoRA adapter
 * 
 * An adapter is associated with a specific model and remains valid as long as
 * that model is loaded. Adapters are freed automatically when the model is unloaded,
 * or can be freed manually.
 */
typedef struct ethervox_adapter ethervox_adapter_t;

/**
 * Load a LoRA adapter from file
 * 
 * Loads a LoRA adapter (GGUF format) and associates it with the given model.
 * The adapter remains valid as long as the model handle is valid.
 * 
 * Multiple adapters can be loaded for the same model and applied together with
 * different scales via ethervox_model_set_adapters().
 * 
 * Thread-safe: Can be called concurrently for different models.
 * 
 * @param handle Model handle to associate adapter with
 * @param adapter_path Path to LoRA adapter file (.gguf)
 * @param out Receives adapter handle (caller must free or will be freed on model unload)
 * @return ETHERVOX_SUCCESS or error code
 * 
 * Example:
 * @code
 *   ethervox_adapter_t* certainty_adapter;
 *   result = ethervox_adapter_load(model, "/path/to/certainty.gguf", &certainty_adapter);
 * @endcode
 */
ethervox_result_t ethervox_adapter_load(
    ethervox_model_handle_t* handle,
    const char* adapter_path,
    ethervox_adapter_t** out
);

/**
 * Free a LoRA adapter
 * 
 * Manually frees an adapter. If not called, the adapter will be freed automatically
 * when the associated model is unloaded.
 * 
 * Thread-safe: Can be called concurrently for different adapters.
 * 
 * @param adapter Adapter to free (NULL is safe)
 */
void ethervox_adapter_free(ethervox_adapter_t* adapter);

/**
 * Apply LoRA adapters to a model
 * 
 * Sets one or more LoRA adapters on the model's context. Adapters are applied
 * with the given scales (typically 1.0). Pass NULL/0 to clear all adapters.
 * 
 * The same adapters remain applied across multiple generate calls until changed.
 * 
 * Thread-safe: Serialized per model via internal mutex.
 * 
 * @param handle Model handle to apply adapters to
 * @param adapters Array of adapter handles (NULL to clear)
 * @param n_adapters Number of adapters in array (0 to clear)
 * @param scales Array of scales, one per adapter (typically all 1.0)
 * @return ETHERVOX_SUCCESS or error code
 * 
 * Example:
 * @code
 *   ethervox_adapter_t* adapters[] = { certainty_adapter };
 *   float scales[] = { 1.0f };
 *   result = ethervox_model_set_adapters(model, adapters, 1, scales);
 * @endcode
 */
ethervox_result_t ethervox_model_set_adapters(
    ethervox_model_handle_t* handle,
    ethervox_adapter_t** adapters,
    size_t n_adapters,
    const float* scales
);

/**
 * Get adapter metadata value by key
 * 
 * Reads a string metadata value from the adapter's GGUF metadata.
 * Common keys: "lora.alpha", "lora.rank", "general.name", "general.description"
 * 
 * @param adapter Adapter handle
 * @param key Metadata key name
 * @param buf Buffer to receive value (always null-terminated)
 * @param buf_size Size of buffer
 * @return Length of string on success (excluding null terminator), -1 on failure
 * 
 * Example:
 * @code
 *   char name[256];
 *   if (ethervox_adapter_get_metadata(adapter, "general.name", name, sizeof(name)) >= 0) {
 *       printf("Adapter: %s\n", name);
 *   }
 * @endcode
 */
int32_t ethervox_adapter_get_metadata(
    const ethervox_adapter_t* adapter,
    const char* key,
    char* buf,
    size_t buf_size
);

/**
 * Get number of metadata key/value pairs
 * 
 * Returns the total count of metadata entries in the adapter's GGUF metadata.
 * 
 * @param adapter Adapter handle
 * @return Number of metadata pairs, or -1 on failure
 */
int32_t ethervox_adapter_meta_count(const ethervox_adapter_t* adapter);

/**
 * Get metadata key by index
 * 
 * Iterates through adapter metadata by index. Use with ethervox_adapter_meta_count()
 * to enumerate all metadata.
 * 
 * @param adapter Adapter handle
 * @param index Metadata index (0 to count-1)
 * @param buf Buffer to receive key name (always null-terminated)
 * @param buf_size Size of buffer
 * @return Length of key on success (excluding null terminator), -1 on failure
 */
int32_t ethervox_adapter_meta_key_by_index(
    const ethervox_adapter_t* adapter,
    int32_t index,
    char* buf,
    size_t buf_size
);

/**
 * Get metadata value by index
 * 
 * Gets the string value corresponding to a metadata key at the given index.
 * 
 * @param adapter Adapter handle
 * @param index Metadata index (0 to count-1)
 * @param buf Buffer to receive value (always null-terminated)
 * @param buf_size Size of buffer
 * @return Length of value on success (excluding null terminator), -1 on failure
 */
int32_t ethervox_adapter_meta_value_by_index(
    const ethervox_adapter_t* adapter,
    int32_t index,
    char* buf,
    size_t buf_size
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_ADAPTER_H
