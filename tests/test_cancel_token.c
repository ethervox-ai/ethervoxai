/**
 * @file test_cancel_token.c
 * @brief Tests for cancellation token
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/cancel_token.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define TEST_LOG(...) printf("[TEST] " __VA_ARGS__); printf("\n")
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); return 1; } } while(0)

// ============================================================================
// Basic API Tests
// ============================================================================

static int test_create_and_free(void) {
    TEST_LOG("Testing create and free");
    
    ethervox_cancel_token_t* token = NULL;
    ethervox_result_t result = ethervox_cancel_token_create(&token);
    
    CHECK(result == ETHERVOX_SUCCESS, "create should succeed");
    CHECK(token != NULL, "token should not be NULL");
    CHECK(!ethervox_cancel_token_is_cancelled(token), "new token should not be cancelled");
    
    ethervox_cancel_token_free(token);
    TEST_LOG("✓ Create and free");
    return 0;
}

static int test_create_null_out(void) {
    TEST_LOG("Testing create with NULL output");
    
    ethervox_result_t result = ethervox_cancel_token_create(NULL);
    CHECK(result == ETHERVOX_ERROR_INVALID_ARGUMENT, "create with NULL should fail");
    
    TEST_LOG("✓ Create with NULL output");
    return 0;
}

static int test_free_null(void) {
    TEST_LOG("Testing free with NULL");
    
    // Should not crash
    ethervox_cancel_token_free(NULL);
    
    TEST_LOG("✓ Free NULL");
    return 0;
}

static int test_cancel_null(void) {
    TEST_LOG("Testing cancel with NULL");
    
    // Should not crash
    ethervox_cancel_token_cancel(NULL);
    
    TEST_LOG("✓ Cancel NULL");
    return 0;
}

static int test_is_cancelled_null(void) {
    TEST_LOG("Testing is_cancelled with NULL");
    
    bool result = ethervox_cancel_token_is_cancelled(NULL);
    CHECK(result == false, "is_cancelled(NULL) should return false");
    
    TEST_LOG("✓ is_cancelled NULL");
    return 0;
}

// ============================================================================
// Cancellation Tests
// ============================================================================

static int test_cancel_basic(void) {
    TEST_LOG("Testing basic cancellation");
    
    ethervox_cancel_token_t* token = NULL;
    ethervox_result_t result = ethervox_cancel_token_create(&token);
    CHECK(result == ETHERVOX_SUCCESS, "create should succeed");
    
    CHECK(!ethervox_cancel_token_is_cancelled(token), "token should not be cancelled initially");
    
    ethervox_cancel_token_cancel(token);
    CHECK(ethervox_cancel_token_is_cancelled(token), "token should be cancelled after cancel()");
    
    // Cancel again - should be idempotent
    ethervox_cancel_token_cancel(token);
    CHECK(ethervox_cancel_token_is_cancelled(token), "token should still be cancelled");
    
    ethervox_cancel_token_free(token);
    TEST_LOG("✓ Basic cancellation");
    return 0;
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

typedef struct {
    ethervox_cancel_token_t* token;
    volatile bool thread_started;
    volatile bool thread_cancelled;
} cancel_thread_data_t;

static void* cancel_thread_func(void* arg) {
    cancel_thread_data_t* data = (cancel_thread_data_t*)arg;
    
    data->thread_started = true;
    
    // Wait a bit to ensure main thread is checking
    usleep(50000);  // 50ms
    
    // Cancel the token
    ethervox_cancel_token_cancel(data->token);
    data->thread_cancelled = true;
    
    return NULL;
}

static int test_cancel_from_another_thread(void) {
    TEST_LOG("Testing cancellation from another thread");
    
    ethervox_cancel_token_t* token = NULL;
    ethervox_result_t result = ethervox_cancel_token_create(&token);
    CHECK(result == ETHERVOX_SUCCESS, "create should succeed");
    
    cancel_thread_data_t data = {
        .token = token,
        .thread_started = false,
        .thread_cancelled = false
    };
    
    pthread_t thread;
    int ret = pthread_create(&thread, NULL, cancel_thread_func, &data);
    CHECK(ret == 0, "thread creation should succeed");
    
    // Wait for thread to start
    while (!data.thread_started) {
        usleep(1000);
    }
    
    // Poll for cancellation (simulating a long operation)
    bool detected_cancel = false;
    for (int i = 0; i < 200; i++) {  // 200ms total
        if (ethervox_cancel_token_is_cancelled(token)) {
            detected_cancel = true;
            break;
        }
        usleep(1000);  // 1ms
    }
    
    pthread_join(thread, NULL);
    
    CHECK(detected_cancel, "should detect cancellation from other thread");
    CHECK(data.thread_cancelled, "thread should have cancelled");
    CHECK(ethervox_cancel_token_is_cancelled(token), "token should be cancelled");
    
    ethervox_cancel_token_free(token);
    TEST_LOG("✓ Cancel from another thread");
    return 0;
}

static void* check_thread_func(void* arg) {
    ethervox_cancel_token_t* token = (ethervox_cancel_token_t*)arg;
    
    // Check from this thread - should work (read-only)
    volatile bool is_cancelled = ethervox_cancel_token_is_cancelled(token);
    (void)is_cancelled;  // Suppress unused warning
    
    return NULL;
}

static int test_concurrent_checks(void) {
    TEST_LOG("Testing concurrent is_cancelled checks");
    
    ethervox_cancel_token_t* token = NULL;
    ethervox_result_t result = ethervox_cancel_token_create(&token);
    CHECK(result == ETHERVOX_SUCCESS, "create should succeed");
    
    // Create multiple threads that all check the token
    const int num_threads = 10;
    pthread_t threads[num_threads];
    
    for (int i = 0; i < num_threads; i++) {
        int ret = pthread_create(&threads[i], NULL, check_thread_func, token);
        CHECK(ret == 0, "thread creation should succeed");
    }
    
    // Also check from main thread
    for (int i = 0; i < 100; i++) {
        ethervox_cancel_token_is_cancelled(token);
    }
    
    // Join all threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    ethervox_cancel_token_free(token);
    TEST_LOG("✓ Concurrent checks");
    return 0;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    printf("====== Cancel Token Tests ======\n");
    
    int failed = 0;
    
    failed += test_create_and_free();
    failed += test_create_null_out();
    failed += test_free_null();
    failed += test_cancel_null();
    failed += test_is_cancelled_null();
    failed += test_cancel_basic();
    failed += test_cancel_from_another_thread();
    failed += test_concurrent_checks();
    
    printf("\n");
    if (failed == 0) {
        printf("====== All Tests Passed ======\n");
        return 0;
    } else {
        printf("====== %d Tests Failed ======\n", failed);
        return 1;
    }
}
