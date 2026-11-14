/**
 * @file session.h
 * @brief Session forking and sequence management for batch throughput
 *
 * Enables prefilling a shared prompt once, then forking it N times to avoid
 * re-prefilling. Classifying 300 documents currently re-prefills the same
 * system prompt 300 times — this API eliminates that cost.
 *
 * Target: ≥4× throughput vs naive re-prefill path.
 *
 * Workflow:
 *   1. Create session with model handle
 *   2. Prefill with common prompt (e.g., "Classify this document:")
 *   3. Fork the session 300 times → each fork inherits the KV cache
 *   4. Each fork appends document content and generates
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_SESSION_H
#define ETHERVOX_SESSION_H

#include "ethervox/error.h"
#include "ethervox/model_pool.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque handle to a session
 * 
 * A session owns a sequence ID within a model's context and provides
 * prefill, fork, and reset operations. Each session must be destroyed
 * when no longer needed to free its sequence ID.
 */
typedef struct ethervox_session ethervox_session_t;

/**
 * Session configuration
 */
typedef struct {
    uint32_t max_forks;        /**< Maximum number of concurrent forks (0 = use model's n_seq_max) */
    bool kv_unified;           /**< Use unified KV buffer (true = better for shared prefix, measure both) */
    bool save_prefill_tokens;  /**< Save tokens for state save/load (adds overhead) */
} ethervox_session_config_t;

/**
 * Create a session from a model handle
 *
 * Allocates a sequence ID within the model's context. The session owns this
 * sequence until destroyed. The number of concurrent sessions (including forks)
 * is limited by the model's n_seq_max, configured at context creation.
 *
 * Thread-safety: Can be called concurrently on different model handles.
 * A model handle's inference_mutex serializes operations on that model.
 *
 * @param handle Model handle from model pool
 * @param config Session configuration (NULL = defaults)
 * @param out Receives session handle (caller must destroy)
 * @return ETHERVOX_SUCCESS, ETHERVOX_ERROR_OUT_OF_MEMORY if no sequence IDs available, or error
 */
ethervox_result_t ethervox_session_create(
    ethervox_model_handle_t* handle,
    const ethervox_session_config_t* config,
    ethervox_session_t** out
);

/**
 * Prefill a session with text
 *
 * Encodes and prefills the given text into the session's KV cache. Records
 * the position where prefill ends, which becomes the reset point for
 * reset_to_prefill().
 *
 * Can be called multiple times to extend the prefill. Each call updates
 * the recorded prefill position.
 *
 * @param session Session to prefill
 * @param text UTF-8 text to prefill
 * @param progress_cb Optional progress callback
 * @param user_data User data for progress callback
 * @return ETHERVOX_SUCCESS or error
 */
ethervox_result_t ethervox_session_prefill(
    ethervox_session_t* session,
    const char* text,
    ethervox_progress_cb progress_cb,
    void* user_data
);

/**
 * Fork a session
 *
 * Creates a child session that inherits the parent's KV cache via
 * llama_memory_seq_cp. The child gets a new sequence ID and can generate
 * independently without affecting the parent.
 *
 * CRITICAL PROPERTY: After forking, generating in the child MUST NOT modify
 * the parent. This is asserted during development.
 *
 * Forking costs memory: each fork holds KV for its sequence. The pool's
 * budget accounting (C2.1) must see this, coordinated with C3.4.
 *
 * @param parent Parent session to fork from
 * @param out_child Receives child session (caller must destroy)
 * @return ETHERVOX_SUCCESS, ETHERVOX_ERROR_OUT_OF_MEMORY if no sequence IDs available, or error
 */
ethervox_result_t ethervox_session_fork(
    ethervox_session_t* parent,
    ethervox_session_t** out_child
);

/**
 * Reset session to prefill position
 *
 * Truncates the session's KV cache to the position where prefill ended,
 * removing all generated tokens. Uses llama_memory_seq_rm to remove tokens
 * after the prefill boundary.
 *
 * This does NOT re-prefill. It truncates, which is much faster.
 *
 * @param session Session to reset
 * @return ETHERVOX_SUCCESS or error
 */
ethervox_result_t ethervox_session_reset_to_prefill(
    ethervox_session_t* session
);

/**
 * Save session state to file
 *
 * Saves the session's sequence state to a file using llama_state_seq_save_file.
 * The file includes a versioned header to detect format mismatches.
 *
 * Format (uint32_t little-endian):
 *   - Magic: 0x53455356 ("SESV")
 *   - Version: 1
 *   - n_tokens: token count
 *   - tokens: llama_token[n_tokens]
 *   - state_size: size of sequence state
 *   - state: raw sequence state from llama.cpp
 *
 * @param session Session to save
 * @param path File path to write
 * @return ETHERVOX_SUCCESS or error
 */
ethervox_result_t ethervox_session_save_state(
    ethervox_session_t* session,
    const char* path
);

/**
 * Load session state from file
 *
 * Loads a saved session state into this session's sequence. Validates the
 * file header to detect version mismatches.
 *
 * On version mismatch, returns ETHERVOX_ERROR_INVALID_ARGUMENT with a clear
 * error message via ethervox_get_last_error_message().
 *
 * @param session Session to load into (existing state is replaced)
 * @param path File path to read
 * @return ETHERVOX_SUCCESS, ETHERVOX_ERROR_INVALID_ARGUMENT if version mismatch, or error
 */
ethervox_result_t ethervox_session_load_state(
    ethervox_session_t* session,
    const char* path
);

/**
 * Destroy a session
 *
 * Frees the session's sequence ID, making it available for reuse. The session
 * handle becomes invalid after this call.
 *
 * NULL is safe.
 *
 * @param session Session to destroy
 */
void ethervox_session_destroy(ethervox_session_t* session);

/**
 * Get session statistics
 *
 * Returns current position and prefill boundary for debugging.
 *
 * @param session Session to query
 * @param out_current_pos Receives current token position (can be NULL)
 * @param out_prefill_pos Receives prefill boundary position (can be NULL)
 * @return ETHERVOX_SUCCESS or error
 */
ethervox_result_t ethervox_session_get_stats(
    const ethervox_session_t* session,
    int32_t* out_current_pos,
    int32_t* out_prefill_pos
);

/**
 * Get session sequence ID
 *
 * Returns the llama.cpp sequence ID allocated to this session.
 * Useful for direct llama.cpp operations.
 *
 * @param session Session to query
 * @return Sequence ID, or -1 if not allocated
 */
int32_t ethervox_session_get_seq_id(
    const ethervox_session_t* session
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_SESSION_H
