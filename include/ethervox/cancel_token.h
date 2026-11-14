/**
 * @file cancel_token.h
 * @brief Cancellation token for long-running operations
 *
 * Provides thread-safe cancellation for generation, prefill, embedding,
 * vision encoding, and model loading operations. Checked between token
 * steps and batch chunks for sub-200ms cancellation latency.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_CANCEL_TOKEN_H
#define ETHERVOX_CANCEL_TOKEN_H

#include <stdbool.h>
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque cancellation token
 *
 * Thread-safe: cancel() may be called from any thread while the operation
 * is running. The operation checks is_cancelled() periodically and stops
 * gracefully within 200ms when cancelled.
 */
typedef struct ethervox_cancel_token ethervox_cancel_token_t;

/**
 * Create a new cancellation token
 *
 * The token is created in the non-cancelled state.
 *
 * @param out Output: Created token (caller must free with ethervox_cancel_token_free)
 * @return ETHERVOX_SUCCESS on success, ETHERVOX_ERROR_OUT_OF_MEMORY on allocation failure
 */
ethervox_result_t ethervox_cancel_token_create(ethervox_cancel_token_t** out);

/**
 * Cancel an operation
 *
 * Thread-safe: may be called from any thread. Setting the cancelled flag
 * is atomic. The operation will detect cancellation within 200ms and stop
 * gracefully.
 *
 * Calling cancel on an already-cancelled token is a no-op.
 *
 * @param token Token to cancel (must not be NULL)
 */
void ethervox_cancel_token_cancel(ethervox_cancel_token_t* token);

/**
 * Check if the token has been cancelled
 *
 * Thread-safe: may be called from any thread. Reading the cancelled flag
 * is atomic.
 *
 * @param token Token to check (must not be NULL)
 * @return true if cancelled, false otherwise
 */
bool ethervox_cancel_token_is_cancelled(const ethervox_cancel_token_t* token);

/**
 * Free a cancellation token
 *
 * Not thread-safe: caller must ensure no other thread is accessing the token.
 * Passing NULL is a no-op.
 *
 * @param token Token to free (may be NULL)
 */
void ethervox_cancel_token_free(ethervox_cancel_token_t* token);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_CANCEL_TOKEN_H
