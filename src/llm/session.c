/**
 * @file session.c
 * @brief Session forking and sequence management implementation
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/session.h"
#include "ethervox/error.h"
#include "llama.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Platform-specific includes
#if defined(_WIN32)
    #include <windows.h>
    typedef CRITICAL_SECTION mutex_t;
    #define MUTEX_INIT(m) InitializeCriticalSection(&(m))
    #define MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
    #define MUTEX_LOCK(m) EnterCriticalSection(&(m))
    #define MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
#else
    #include <pthread.h>
    typedef pthread_mutex_t mutex_t;
    #define MUTEX_INIT(m) pthread_mutex_init(&(m), NULL)
    #define MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
    #define MUTEX_LOCK(m) pthread_mutex_lock(&(m))
    #define MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))
#endif

// File format constants
#define SESSION_MAGIC 0x53455356  // "SESV"
#define SESSION_VERSION 1

/**
 * Session structure
 */
struct ethervox_session {
    ethervox_model_handle_t* model_handle;  // Reference to model (not owned)
    struct llama_model* model;              // Direct reference for efficiency
    struct llama_context* ctx;              // Direct reference for efficiency
    llama_seq_id seq_id;                    // Allocated sequence ID
    int32_t prefill_pos;                    // Position where prefill ended (-1 = not prefilled)
    int32_t current_pos;                    // Current position in sequence
    uint32_t max_forks;                     // Max concurrent forks allowed
    uint32_t active_forks;                  // Number of active child sessions
    bool is_fork;                           // True if this is a forked session
    ethervox_session_t* parent;             // Parent session if forked (not owned)
    llama_token* tokens;                    // Token buffer for save/load (NULL if not configured)
    size_t tokens_capacity;                 // Capacity of token buffer
    size_t tokens_count;                    // Number of tokens in buffer
};

// Forward declarations of internal helpers
static ethervox_result_t allocate_sequence_id(struct llama_context* ctx, llama_seq_id* out);
static void free_sequence_id(struct llama_context* ctx, llama_seq_id seq_id);

/**
 * Allocate a sequence ID from the context
 *
 * Searches for an unused sequence ID in [0, n_seq_max). llama.cpp doesn't
 * provide an allocator, so we must track usage ourselves.
 *
 * Strategy: Use llama_memory_seq_pos_max to check if sequence is empty.
 * If pos_max returns -1, the sequence is unused.
 */
static ethervox_result_t allocate_sequence_id(struct llama_context* ctx, llama_seq_id* out) {
    ETHERVOX_CHECK_PTR(ctx);
    ETHERVOX_CHECK_PTR(out);

    uint32_t n_seq_max = llama_n_seq_max(ctx);
    llama_memory_t mem = llama_get_memory(ctx);

    // Search for an empty sequence
    for (llama_seq_id seq_id = 0; seq_id < (llama_seq_id)n_seq_max; ++seq_id) {
        llama_pos pos_max = llama_memory_seq_pos_max(mem, seq_id);
        if (pos_max == -1) {
            // Found unused sequence
            *out = seq_id;
            return ETHERVOX_SUCCESS;
        }
    }

    // No sequences available
    // Error returned below
    return ETHERVOX_ERROR_OUT_OF_MEMORY;
}

/**
 * Free a sequence ID
 *
 * Removes all tokens from the sequence, making it available for reuse.
 */
static void free_sequence_id(struct llama_context* ctx, llama_seq_id seq_id) {
    if (!ctx || seq_id < 0) {
        return;
    }

    llama_memory_t mem = llama_get_memory(ctx);
    // Remove all tokens from this sequence
    llama_memory_seq_rm(mem, seq_id, -1, -1);
}

/**
 * Create a session from a model handle
 */
ethervox_result_t ethervox_session_create(
    ethervox_model_handle_t* handle,
    const ethervox_session_config_t* config,
    ethervox_session_t** out
) {
    ETHERVOX_CHECK_PTR(handle);
    ETHERVOX_CHECK_PTR(out);

    // Get model and context from handle
    struct llama_model* model = ethervox_model_handle_get_model(handle);
    struct llama_context* ctx = ethervox_model_handle_get_context(handle);
    
    if (!model || !ctx) {
        // Error returned below
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Allocate session structure
    ethervox_session_t* session = (ethervox_session_t*)calloc(1, sizeof(ethervox_session_t));
    if (!session) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }

    // Store references
    session->model_handle = handle;
    session->model = model;
    session->ctx = ctx;
    session->seq_id = -1;
    session->prefill_pos = -1;
    session->current_pos = 0;
    session->is_fork = false;
    session->parent = NULL;
    session->active_forks = 0;

    // Apply config or defaults
    if (config) {
        session->max_forks = config->max_forks;
        if (config->save_prefill_tokens) {
            // Allocate token buffer
            session->tokens_capacity = 4096;  // Default buffer size
            session->tokens = (llama_token*)malloc(sizeof(llama_token) * session->tokens_capacity);
            if (!session->tokens) {
                free(session);
                return ETHERVOX_ERROR_OUT_OF_MEMORY;
            }
        }
    }

    *out = session;
    return ETHERVOX_SUCCESS;
}

/**
 * Destroy a session
 */
void ethervox_session_destroy(ethervox_session_t* session) {
    if (!session) {
        return;
    }

    // Free sequence ID if allocated
    if (session->seq_id >= 0 && session->ctx) {
        free_sequence_id(session->ctx, session->seq_id);
    }

    // Decrement parent's fork count if this is a fork
    if (session->is_fork && session->parent) {
        session->parent->active_forks--;
    }

    // Free token buffer
    free(session->tokens);

    // Free session structure
    free(session);
}

/**
 * Prefill a session with text
 */
ethervox_result_t ethervox_session_prefill(
    ethervox_session_t* session,
    const char* text,
    ethervox_progress_cb progress_cb,
    void* user_data
) {
    ETHERVOX_CHECK_PTR(session);
    ETHERVOX_CHECK_PTR(text);
    ETHERVOX_CHECK_PTR(session->model);
    ETHERVOX_CHECK_PTR(session->ctx);

    // Allocate sequence ID if not already allocated
    if (session->seq_id < 0) {
        ethervox_result_t result = allocate_sequence_id(session->ctx, &session->seq_id);
        ETHERVOX_CHECK(result);
    }

    // Tokenize text
    int n_tokens_max = strlen(text) * 2 + 16;  // Conservative estimate
    llama_token* tokens = (llama_token*)malloc(sizeof(llama_token) * n_tokens_max);
    if (!tokens) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }

    const struct llama_vocab* vocab = llama_model_get_vocab(session->model);
    int n_tokens = llama_tokenize(
        vocab,
        text,
        strlen(text),
        tokens,
        n_tokens_max,
        /*add_special*/ true,
        /*parse_special*/ false
    );

    if (n_tokens < 0) {
        free(tokens);
        // Error returned below
        return ETHERVOX_ERROR_FAILED;
    }

    // Save tokens if configured
    if (session->tokens) {
        // Ensure capacity
        if ((size_t)n_tokens > session->tokens_capacity) {
            size_t new_capacity = session->tokens_capacity * 2;
            while (new_capacity < (size_t)n_tokens) {
                new_capacity *= 2;
            }
            llama_token* new_tokens = (llama_token*)realloc(session->tokens, sizeof(llama_token) * new_capacity);
            if (!new_tokens) {
                free(tokens);
                return ETHERVOX_ERROR_OUT_OF_MEMORY;
            }
            session->tokens = new_tokens;
            session->tokens_capacity = new_capacity;
        }
        memcpy(session->tokens, tokens, sizeof(llama_token) * n_tokens);
        session->tokens_count = n_tokens;
    }

    // Decode tokens into KV cache
    // Create batch with proper seq_id support
    struct llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    
    // Fill batch with tokens
    for (int i = 0; i < n_tokens; ++i) {
        batch.token[i] = tokens[i];
        batch.pos[i] = session->current_pos + i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = session->seq_id;
        batch.logits[i] = false;  // Don't need logits for prefill
    }
    batch.n_tokens = n_tokens;

    // Decode
    int decode_result = llama_decode(session->ctx, batch);
    llama_batch_free(batch);
    
    if (decode_result != 0) {
        free(tokens);
        // Error returned below
        return ETHERVOX_ERROR_FAILED;
    }

    // Update positions
    session->current_pos += n_tokens;
    session->prefill_pos = session->current_pos;

    free(tokens);

    // Report progress
    if (progress_cb) {
        progress_cb(1.0f, user_data);
    }

    return ETHERVOX_SUCCESS;
}

/**
 * Fork a session
 */
ethervox_result_t ethervox_session_fork(
    ethervox_session_t* parent,
    ethervox_session_t** out_child
) {
    ETHERVOX_CHECK_PTR(parent);
    ETHERVOX_CHECK_PTR(out_child);
    ETHERVOX_CHECK_PTR(parent->ctx);

    // Check if parent has reached max forks
    if (parent->max_forks > 0 && parent->active_forks >= parent->max_forks) {
        // Error returned below
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }

    // Allocate child session
    ethervox_session_t* child = (ethervox_session_t*)calloc(1, sizeof(ethervox_session_t));
    if (!child) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }

    // Copy configuration from parent
    child->model_handle = parent->model_handle;
    child->model = parent->model;
    child->ctx = parent->ctx;
    child->prefill_pos = parent->prefill_pos;
    child->current_pos = parent->current_pos;
    child->max_forks = parent->max_forks;
    child->is_fork = true;
    child->parent = parent;

    // Allocate new sequence ID for child
    ethervox_result_t result = allocate_sequence_id(child->ctx, &child->seq_id);
    if (result != ETHERVOX_SUCCESS) {
        free(child);
        return result;
    }

    // Copy KV cache from parent to child
    llama_memory_t mem = llama_get_memory(parent->ctx);
    llama_memory_seq_cp(mem, parent->seq_id, child->seq_id, -1, -1);

    // Increment parent's fork count
    parent->active_forks++;

    *out_child = child;
    return ETHERVOX_SUCCESS;
}

/**
 * Reset session to prefill position
 */
ethervox_result_t ethervox_session_reset_to_prefill(
    ethervox_session_t* session
) {
    ETHERVOX_CHECK_PTR(session);
    ETHERVOX_CHECK_PTR(session->ctx);

    if (session->prefill_pos < 0) {
        // Error returned below
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Remove tokens after prefill position
    llama_memory_t mem = llama_get_memory(session->ctx);
    llama_memory_seq_rm(mem, session->seq_id, session->prefill_pos, -1);

    // Reset current position
    session->current_pos = session->prefill_pos;

    return ETHERVOX_SUCCESS;
}

/**
 * Save session state to file
 */
ethervox_result_t ethervox_session_save_state(
    ethervox_session_t* session,
    const char* path
) {
    ETHERVOX_CHECK_PTR(session);
    ETHERVOX_CHECK_PTR(path);
    ETHERVOX_CHECK_PTR(session->ctx);

    if (session->seq_id < 0) {
        // Error returned below
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Open file
    FILE* fp = fopen(path, "wb");
    if (!fp) {
        // Error returned below
        return ETHERVOX_ERROR_FILE_READ;
    }

    // Write header
    uint32_t magic = SESSION_MAGIC;
    uint32_t version = SESSION_VERSION;
    if (fwrite(&magic, sizeof(uint32_t), 1, fp) != 1 ||
        fwrite(&version, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_FILE_READ;
    }

    // Write token count and tokens
    uint32_t n_tokens = (uint32_t)session->tokens_count;
    if (fwrite(&n_tokens, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_FILE_READ;
    }

    if (n_tokens > 0 && session->tokens) {
        if (fwrite(session->tokens, sizeof(llama_token), n_tokens, fp) != n_tokens) {
            fclose(fp);
            // Error returned below
            return ETHERVOX_ERROR_FILE_READ;
        }
    }

    // Get sequence state size
    size_t state_size = llama_state_seq_get_size(session->ctx, session->seq_id);
    if (state_size == 0) {
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_FAILED;
    }

    // Write state size
    uint64_t state_size_64 = (uint64_t)state_size;
    if (fwrite(&state_size_64, sizeof(uint64_t), 1, fp) != 1) {
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_FILE_READ;
    }

    // Allocate buffer for state
    uint8_t* state_buffer = (uint8_t*)malloc(state_size);
    if (!state_buffer) {
        fclose(fp);
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }

    // Get sequence state
    size_t written = llama_state_seq_get_data(session->ctx, state_buffer, state_size, session->seq_id);
    if (written != state_size) {
        free(state_buffer);
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_FAILED;
    }

    // Write state to file
    if (fwrite(state_buffer, 1, state_size, fp) != state_size) {
        free(state_buffer);
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_FILE_READ;
    }

    free(state_buffer);
    fclose(fp);

    return ETHERVOX_SUCCESS;
}

/**
 * Load session state from file
 */
ethervox_result_t ethervox_session_load_state(
    ethervox_session_t* session,
    const char* path
) {
    ETHERVOX_CHECK_PTR(session);
    ETHERVOX_CHECK_PTR(path);
    ETHERVOX_CHECK_PTR(session->ctx);

    // Allocate sequence ID if not already allocated
    if (session->seq_id < 0) {
        ethervox_result_t result = allocate_sequence_id(session->ctx, &session->seq_id);
        ETHERVOX_CHECK(result);
    }

    // Open file
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        // Error returned below
        return ETHERVOX_ERROR_FILE_READ;
    }

    // Read and validate header
    uint32_t magic, version;
    if (fread(&magic, sizeof(uint32_t), 1, fp) != 1 ||
        fread(&version, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_FILE_READ;
    }

    if (magic != SESSION_MAGIC) {
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    if (version != SESSION_VERSION) {
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Read token count
    uint32_t n_tokens;
    if (fread(&n_tokens, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_FILE_READ;
    }

    // Read tokens if present
    if (n_tokens > 0) {
        // Ensure token buffer capacity
        if (session->tokens) {
            if (n_tokens > session->tokens_capacity) {
                llama_token* new_tokens = (llama_token*)realloc(session->tokens, sizeof(llama_token) * n_tokens);
                if (!new_tokens) {
                    fclose(fp);
                    return ETHERVOX_ERROR_OUT_OF_MEMORY;
                }
                session->tokens = new_tokens;
                session->tokens_capacity = n_tokens;
            }

            if (fread(session->tokens, sizeof(llama_token), n_tokens, fp) != n_tokens) {
                fclose(fp);
                // Error returned below
                return ETHERVOX_ERROR_FILE_READ;
            }
            session->tokens_count = n_tokens;
        } else {
            // Skip tokens if not configured to save them
            if (fseek(fp, sizeof(llama_token) * n_tokens, SEEK_CUR) != 0) {
                fclose(fp);
                // Error returned below
                return ETHERVOX_ERROR_FILE_READ;
            }
        }
    }

    // Read state size
    uint64_t state_size_64;
    if (fread(&state_size_64, sizeof(uint64_t), 1, fp) != 1) {
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_FILE_READ;
    }
    size_t state_size = (size_t)state_size_64;

    // Allocate buffer for state
    uint8_t* state_buffer = (uint8_t*)malloc(state_size);
    if (!state_buffer) {
        fclose(fp);
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }

    // Read state from file
    if (fread(state_buffer, 1, state_size, fp) != state_size) {
        free(state_buffer);
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_FILE_READ;
    }

    // Load state into sequence
    size_t bytes_read = llama_state_seq_set_data(session->ctx, state_buffer, state_size, session->seq_id);
    if (bytes_read == 0) {
        free(state_buffer);
        fclose(fp);
        // Error returned below
        return ETHERVOX_ERROR_FAILED;
    }

    free(state_buffer);
    fclose(fp);

    // Update session positions based on loaded state
    llama_memory_t mem = llama_get_memory(session->ctx);
    llama_pos pos_max = llama_memory_seq_pos_max(mem, session->seq_id);
    if (pos_max >= 0) {
        session->current_pos = pos_max + 1;
        // Assume the loaded state is the prefill
        session->prefill_pos = session->current_pos;
    }

    return ETHERVOX_SUCCESS;
}

/**
 * Get session statistics
 */
ethervox_result_t ethervox_session_get_stats(
    const ethervox_session_t* session,
    int32_t* out_current_pos,
    int32_t* out_prefill_pos
) {
    ETHERVOX_CHECK_PTR(session);

    if (out_current_pos) {
        *out_current_pos = session->current_pos;
    }

    if (out_prefill_pos) {
        *out_prefill_pos = session->prefill_pos;
    }

    return ETHERVOX_SUCCESS;
}

/**
 * Get session sequence ID
 */
int32_t ethervox_session_get_seq_id(
    const ethervox_session_t* session
) {
    return session ? session->seq_id : -1;
}
