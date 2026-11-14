/**
 * @file platform_thread.h
 * @brief Cross-platform threading primitives
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 *
 * Thin static inline wrappers over POSIX pthread and Win32 APIs.
 * Zero overhead, header-only, chosen by CMake defines.
 */

#ifndef ETHERVOX_PLATFORM_THREAD_H
#define ETHERVOX_PLATFORM_THREAD_H

#include "ethervox/error.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Mutex
// ============================================================================

#ifdef _WIN32
// Windows implementation
#include <windows.h>

typedef CRITICAL_SECTION ethervox_mutex_t;

static inline ethervox_result_t ethervox_mutex_init(ethervox_mutex_t* mutex) {
    if (!mutex) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    InitializeCriticalSection(mutex);
    return ETHERVOX_SUCCESS;
}

static inline void ethervox_mutex_destroy(ethervox_mutex_t* mutex) {
    if (mutex) {
        DeleteCriticalSection(mutex);
    }
}

static inline ethervox_result_t ethervox_mutex_lock(ethervox_mutex_t* mutex) {
    if (!mutex) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    EnterCriticalSection(mutex);
    return ETHERVOX_SUCCESS;
}

static inline ethervox_result_t ethervox_mutex_unlock(ethervox_mutex_t* mutex) {
    if (!mutex) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    LeaveCriticalSection(mutex);
    return ETHERVOX_SUCCESS;
}

#else
// POSIX implementation
#include <pthread.h>
#include <errno.h>

typedef pthread_mutex_t ethervox_mutex_t;

static inline ethervox_result_t ethervox_mutex_init(ethervox_mutex_t* mutex) {
    if (!mutex) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    int result = pthread_mutex_init(mutex, NULL);
    return (result == 0) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

static inline void ethervox_mutex_destroy(ethervox_mutex_t* mutex) {
    if (mutex) {
        pthread_mutex_destroy(mutex);
    }
}

static inline ethervox_result_t ethervox_mutex_lock(ethervox_mutex_t* mutex) {
    if (!mutex) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    int result = pthread_mutex_lock(mutex);
    return (result == 0) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

static inline ethervox_result_t ethervox_mutex_unlock(ethervox_mutex_t* mutex) {
    if (!mutex) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    int result = pthread_mutex_unlock(mutex);
    return (result == 0) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

#endif

// ============================================================================
// Thread
// ============================================================================

#ifdef _WIN32
// Windows thread implementation
typedef HANDLE ethervox_thread_t;
typedef DWORD (WINAPI *ethervox_thread_func_t)(void*);

static inline ethervox_result_t ethervox_thread_create(
    ethervox_thread_t* thread,
    ethervox_thread_func_t func,
    void* arg
) {
    if (!thread || !func) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    *thread = CreateThread(NULL, 0, func, arg, 0, NULL);
    return (*thread != NULL) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

static inline ethervox_result_t ethervox_thread_join(ethervox_thread_t thread) {
    DWORD result = WaitForSingleObject(thread, INFINITE);
    if (result == WAIT_OBJECT_0) {
        CloseHandle(thread);
        return ETHERVOX_SUCCESS;
    }
    return ETHERVOX_ERROR_FAILED;
}

static inline ethervox_result_t ethervox_thread_detach(ethervox_thread_t thread) {
    CloseHandle(thread);
    return ETHERVOX_SUCCESS;
}

#else
// POSIX thread implementation
typedef pthread_t ethervox_thread_t;
typedef void* (*ethervox_thread_func_t)(void*);

static inline ethervox_result_t ethervox_thread_create(
    ethervox_thread_t* thread,
    ethervox_thread_func_t func,
    void* arg
) {
    if (!thread || !func) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    int result = pthread_create(thread, NULL, func, arg);
    return (result == 0) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

static inline ethervox_result_t ethervox_thread_join(ethervox_thread_t thread) {
    int result = pthread_join(thread, NULL);
    return (result == 0) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

static inline ethervox_result_t ethervox_thread_detach(ethervox_thread_t thread) {
    int result = pthread_detach(thread);
    return (result == 0) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

#endif

// ============================================================================
// Condition Variable
// ============================================================================

#ifdef _WIN32
// Windows condition variable
typedef CONDITION_VARIABLE ethervox_cond_t;

static inline ethervox_result_t ethervox_cond_init(ethervox_cond_t* cond) {
    if (!cond) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    InitializeConditionVariable(cond);
    return ETHERVOX_SUCCESS;
}

static inline void ethervox_cond_destroy(ethervox_cond_t* cond) {
    // Windows condition variables don't need destruction
    (void)cond;
}

static inline ethervox_result_t ethervox_cond_wait(
    ethervox_cond_t* cond,
    ethervox_mutex_t* mutex
) {
    if (!cond || !mutex) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    BOOL result = SleepConditionVariableCS(cond, mutex, INFINITE);
    return result ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

static inline ethervox_result_t ethervox_cond_signal(ethervox_cond_t* cond) {
    if (!cond) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    WakeConditionVariable(cond);
    return ETHERVOX_SUCCESS;
}

static inline ethervox_result_t ethervox_cond_broadcast(ethervox_cond_t* cond) {
    if (!cond) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    WakeAllConditionVariable(cond);
    return ETHERVOX_SUCCESS;
}

#else
// POSIX condition variable
typedef pthread_cond_t ethervox_cond_t;

static inline ethervox_result_t ethervox_cond_init(ethervox_cond_t* cond) {
    if (!cond) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    int result = pthread_cond_init(cond, NULL);
    return (result == 0) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

static inline void ethervox_cond_destroy(ethervox_cond_t* cond) {
    if (cond) {
        pthread_cond_destroy(cond);
    }
}

static inline ethervox_result_t ethervox_cond_wait(
    ethervox_cond_t* cond,
    ethervox_mutex_t* mutex
) {
    if (!cond || !mutex) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    int result = pthread_cond_wait(cond, mutex);
    return (result == 0) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

static inline ethervox_result_t ethervox_cond_signal(ethervox_cond_t* cond) {
    if (!cond) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    int result = pthread_cond_signal(cond);
    return (result == 0) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

static inline ethervox_result_t ethervox_cond_broadcast(ethervox_cond_t* cond) {
    if (!cond) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    int result = pthread_cond_broadcast(cond);
    return (result == 0) ? ETHERVOX_SUCCESS : ETHERVOX_ERROR_FAILED;
}

#endif

// ============================================================================
// Static mutex initializer (for global mutexes)
// ============================================================================

#ifdef _WIN32
// Windows: CRITICAL_SECTION cannot be statically initialized safely
// Use a runtime initialization approach with a global once-init pattern
#define ETHERVOX_MUTEX_INITIALIZER {0}
#else
// POSIX: pthread_mutex_t can be statically initialized
#define ETHERVOX_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif

#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_PLATFORM_THREAD_H */
