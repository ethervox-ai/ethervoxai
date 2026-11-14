/**
 * @file cancel_token.c
 * @brief Cancellation token implementation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/cancel_token.h"

#include <stdatomic.h>
#include <stdlib.h>

/**
 * Cancellation token structure
 *
 * Uses C11 atomics for lock-free, thread-safe cancellation flag.
 * Overhead: single byte (bool) + padding.
 */
struct ethervox_cancel_token {
    atomic_bool cancelled;
};

ethervox_result_t ethervox_cancel_token_create(ethervox_cancel_token_t** out) {
    if (out == NULL) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    ethervox_cancel_token_t* token = (ethervox_cancel_token_t*)malloc(sizeof(ethervox_cancel_token_t));
    if (token == NULL) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }

    // Initialize to non-cancelled state
    atomic_init(&token->cancelled, false);

    *out = token;
    return ETHERVOX_SUCCESS;
}

void ethervox_cancel_token_cancel(ethervox_cancel_token_t* token) {
    if (token == NULL) {
        return;
    }

    // Thread-safe atomic store with sequential consistency
    atomic_store(&token->cancelled, true);
}

bool ethervox_cancel_token_is_cancelled(const ethervox_cancel_token_t* token) {
    if (token == NULL) {
        return false;
    }

    // Thread-safe atomic load with sequential consistency
    return atomic_load(&token->cancelled);
}

void ethervox_cancel_token_free(ethervox_cancel_token_t* token) {
    if (token == NULL) {
        return;
    }

    free(token);
}
