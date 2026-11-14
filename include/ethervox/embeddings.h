/**
 * @file embeddings.h
 * @brief Text embedding API for semantic similarity and search
 *
 * Provides batched text embedding with configurable pooling strategies and
 * L2 normalization. Enables semantic search over document chunks.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_EMBEDDINGS_H
#define ETHERVOX_EMBEDDINGS_H

#include "ethervox/error.h"
#include "ethervox/cancel_token.h"
#include "ethervox/model_pool.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Embedding pooling strategies
 * 
 * Determines how token embeddings are combined into a single text embedding.
 */
typedef enum {
    ETHERVOX_EMBED_POOLING_MEAN = 1,  /**< Average all token embeddings */
    ETHERVOX_EMBED_POOLING_CLS = 2,   /**< Use [CLS] token embedding */
    ETHERVOX_EMBED_POOLING_LAST = 3,  /**< Use last token embedding */
} ethervox_embed_pooling_t;

/**
 * Get embedding dimension count for a model
 * 
 * Returns the size of the embedding vector produced by the model. All embeddings
 * from the same model have this dimension.
 * 
 * @param handle Model handle (must support embeddings)
 * @param out_dims Receives embedding dimension count
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_embed_dimensions(
    ethervox_model_handle_t* handle,
    int* out_dims
);

/**
 * Get maximum batch size for embeddings
 * 
 * Returns the maximum number of texts that can be embedded in a single batch.
 * Batching improves throughput by processing multiple texts together.
 * 
 * @param handle Model handle
 * @param out_max Receives maximum batch size
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_embed_max_batch(
    ethervox_model_handle_t* handle,
    size_t* out_max
);

/**
 * Embed multiple texts in batch
 * 
 * Generates embedding vectors for the provided texts using the specified pooling
 * strategy. Texts are processed in a single batch for efficiency.
 * 
 * Over-long texts are truncated at a token boundary and a warning is logged per item.
 * 
 * If normalise is true, vectors are L2-normalized so that dot(a, b) = cosine(a, b),
 * enabling cosine similarity via simple dot product.
 * 
 * Output format: vectors are written contiguously to out_vectors.
 * For N texts with dimension D, out_vectors must be float[N * D].
 * Text i's embedding is at out_vectors[i * D : (i+1) * D].
 * 
 * @param handle Model handle (must support embeddings)
 * @param texts Array of text strings to embed
 * @param count Number of texts (must be > 0 and <= max_batch)
 * @param pooling Pooling strategy to use
 * @param normalise If true, L2-normalize each embedding vector
 * @param cancel_token Optional cancellation token (NULL if none)
 * @param out_vectors Output buffer for embeddings (caller-allocated: float[count * dims])
 * @return ETHERVOX_SUCCESS or error code
 * 
 * @note Target performance: ≥400 chunks/s for 512-token chunks on M1 Mac
 */
ethervox_result_t ethervox_embed_texts(
    ethervox_model_handle_t* handle,
    const char* const* texts,
    size_t count,
    ethervox_embed_pooling_t pooling,
    bool normalise,
    ethervox_cancel_token_t* cancel_token,
    float* out_vectors
);

#ifdef __cplusplus
}
#endif

#endif  // ETHERVOX_EMBEDDINGS_H
