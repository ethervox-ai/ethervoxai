/**
 * @file embeddings.c
 * @brief Text embedding implementation using llama.cpp
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/embeddings.h"
#include "ethervox/logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
#include <llama.h>
#define LLAMA_AVAILABLE 1
#else
#define LLAMA_AVAILABLE 0
#endif

// Maximum text length before truncation (tokens)
#define MAX_TEXT_TOKENS 512

// Forward declaration of model_handle internals (from model_pool.c)
struct ethervox_model_handle {
    struct llama_model* model;
    struct llama_context* ctx;
    void* inference_mutex;  // Platform-specific mutex
    uint64_t memory_bytes;
    char role[64];
    struct ethervox_model_handle* next;
};

/**
 * L2-normalize a vector in-place
 * 
 * After normalization, dot(v, v) = 1 and dot(a, b) = cosine(a, b)
 */
static void l2_normalize(float* vec, int dims) {
    float norm = 0.0f;
    
    // Calculate L2 norm (Euclidean length)
    for (int i = 0; i < dims; i++) {
        norm += vec[i] * vec[i];
    }
    norm = sqrtf(norm);
    
    // Avoid division by zero
    if (norm < 1e-12f) {
        ETHERVOX_LOG_WARN("[Embeddings] Zero-length vector, cannot normalize");
        return;
    }
    
    // Normalize: v = v / ||v||
    float inv_norm = 1.0f / norm;
    for (int i = 0; i < dims; i++) {
        vec[i] *= inv_norm;
    }
}

ethervox_result_t ethervox_embed_dimensions(
    ethervox_model_handle_t* handle,
    int* out_dims
) {
    if (!handle || !out_dims) {
        return ETHERVOX_ERROR_NULL_POINTER;
    }
    
#if !LLAMA_AVAILABLE
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    if (!handle->model) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Get embedding dimension from model
    int n_embd = llama_model_n_embd(handle->model);
    if (n_embd <= 0) {
        ETHERVOX_LOG_ERROR("[Embeddings] Model does not support embeddings");
        return ETHERVOX_ERROR_NOT_SUPPORTED;
    }
    
    *out_dims = n_embd;
    return ETHERVOX_SUCCESS;
#endif
}

ethervox_result_t ethervox_embed_max_batch(
    ethervox_model_handle_t* handle,
    size_t* out_max
) {
    if (!handle || !out_max) {
        return ETHERVOX_ERROR_NULL_POINTER;
    }
    
#if !LLAMA_AVAILABLE
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    if (!handle->ctx) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Get batch size from context
    int n_batch = llama_n_batch(handle->ctx);
    if (n_batch <= 0) {
        n_batch = 512;  // Default fallback
    }
    
    // Conservative estimate: allow half the batch size for embeddings
    // to account for token expansion
    *out_max = (size_t)(n_batch / 2);
    
    return ETHERVOX_SUCCESS;
#endif
}

ethervox_result_t ethervox_embed_texts(
    ethervox_model_handle_t* handle,
    const char* const* texts,
    size_t count,
    ethervox_embed_pooling_t pooling,
    bool normalise,
    ethervox_cancel_token_t* cancel_token,
    float* out_vectors
) {
    if (!handle || !texts || !out_vectors) {
        return ETHERVOX_ERROR_NULL_POINTER;
    }
    
    if (count == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
#if !LLAMA_AVAILABLE
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
    if (!handle->model || !handle->ctx) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Get model properties
    const struct llama_vocab* vocab = llama_model_get_vocab(handle->model);
    int n_embd = llama_model_n_embd(handle->model);
    int n_ctx = llama_n_ctx(handle->ctx);
    
    ETHERVOX_LOG_DEBUG("[Embeddings] Embedding %zu texts, dims=%d, pooling=%d, normalize=%d",
                      count, n_embd, (int)pooling, (int)normalise);
    
    // Map pooling strategy to llama pooling type
    enum llama_pooling_type llama_pooling;
    switch (pooling) {
        case ETHERVOX_EMBED_POOLING_MEAN:
            llama_pooling = LLAMA_POOLING_TYPE_MEAN;
            break;
        case ETHERVOX_EMBED_POOLING_CLS:
            llama_pooling = LLAMA_POOLING_TYPE_CLS;
            break;
        case ETHERVOX_EMBED_POOLING_LAST:
            llama_pooling = LLAMA_POOLING_TYPE_LAST;
            break;
        default:
            ETHERVOX_LOG_ERROR("[Embeddings] Invalid pooling type: %d", (int)pooling);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Process each text
    for (size_t i = 0; i < count; i++) {
        // Check cancellation
        if (cancel_token && ethervox_cancel_token_is_cancelled(cancel_token)) {
            ETHERVOX_LOG_INFO("[Embeddings] Cancelled at text %zu/%zu", i, count);
            return ETHERVOX_ERROR_TIMEOUT;  // Use timeout as cancellation indicator
        }
        
        const char* text = texts[i];
        if (!text) {
            ETHERVOX_LOG_ERROR("[Embeddings] NULL text at index %zu", i);
            return ETHERVOX_ERROR_NULL_POINTER;
        }
        
        // Tokenize text. Passing n_tokens_max=0 with a NULL buffer is llama_tokenize()'s own
        // "probe the required count" idiom: on a too-small buffer (0 always is, for non-empty
        // text) it returns the NEGATIVE of the token count that would have been produced, not a
        // failure - negate it back to get the real count. A genuine failure is INT32_MIN
        // (overflow) or a positive-required-count negation that still comes out non-positive,
        // i.e. this only ever happens for empty text (handled separately below).
        int n_tokens = -llama_tokenize(vocab, text, (int)strlen(text), NULL, 0, true, false);
        if (n_tokens < 0) {
            ETHERVOX_LOG_ERROR("[Embeddings] Tokenization failed for text %zu", i);
            return ETHERVOX_ERROR_FAILED;
        }
        
        // Truncate if too long. The buffer below is sized for the FULL token count: tokenising into a
        // buffer smaller than the text needs fails (llama_tokenize returns the negative count), so the
        // text is tokenised whole and only the first MAX_TEXT_TOKENS tokens are decoded.
        const int n_full = n_tokens;
        if (n_full > MAX_TEXT_TOKENS) {
            ETHERVOX_LOG_WARN("[Embeddings] Text %zu truncated from %d to %d tokens",
                            i, n_full, MAX_TEXT_TOKENS);
            n_tokens = MAX_TEXT_TOKENS;
        }
        
        if (n_tokens == 0) {
            ETHERVOX_LOG_WARN("[Embeddings] Empty text at index %zu", i);
            // Fill with zeros
            memset(out_vectors + i * n_embd, 0, n_embd * sizeof(float));
            continue;
        }
        
        // Allocate token buffer
        llama_token* tokens = (llama_token*)malloc(n_full * sizeof(llama_token));
        if (!tokens) {
            return ETHERVOX_ERROR_OUT_OF_MEMORY;
        }
        
        // Actually tokenize
        int n_tokenized = llama_tokenize(vocab, text, (int)strlen(text), tokens, n_full, true, false);
        if (n_tokenized != n_full) {
            free(tokens);
            ETHERVOX_LOG_ERROR("[Embeddings] Token count mismatch: expected %d, got %d", n_full, n_tokenized);
            return ETHERVOX_ERROR_FAILED;
        }
        
        // Create batch for this text
        struct llama_batch batch = llama_batch_get_one(tokens, n_tokens);
        
        // Decode to get embeddings
        int decode_result = llama_decode(handle->ctx, batch);
        if (decode_result != 0) {
            free(tokens);
            ETHERVOX_LOG_ERROR("[Embeddings] Decode failed for text %zu: %d", i, decode_result);
            return ETHERVOX_ERROR_FAILED;
        }
        
        // Get embeddings based on pooling type
        float* embeddings = NULL;
        if (llama_pooling == LLAMA_POOLING_TYPE_MEAN || 
            llama_pooling == LLAMA_POOLING_TYPE_CLS || 
            llama_pooling == LLAMA_POOLING_TYPE_LAST) {
            // Use sequence-based embedding (with pooling)
            embeddings = llama_get_embeddings_seq(handle->ctx, 0);
        } else {
            // Fallback to raw embeddings
            embeddings = llama_get_embeddings(handle->ctx);
        }
        
        if (!embeddings) {
            free(tokens);
            ETHERVOX_LOG_ERROR("[Embeddings] Failed to get embeddings for text %zu", i);
            return ETHERVOX_ERROR_FAILED;
        }
        
        // Copy embeddings to output
        float* out_ptr = out_vectors + i * n_embd;
        memcpy(out_ptr, embeddings, n_embd * sizeof(float));
        
        // Apply L2 normalization if requested
        if (normalise) {
            l2_normalize(out_ptr, n_embd);
        }
        
        free(tokens);
    }
    
    ETHERVOX_LOG_DEBUG("[Embeddings] Successfully embedded %zu texts", count);
    return ETHERVOX_SUCCESS;
#endif
}
