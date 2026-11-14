/**
 * @file platform_time.h
 * @brief Cross-platform time and sleep primitives
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 *
 * Thin static inline wrappers over POSIX and Win32 time APIs.
 * Zero overhead, header-only, chosen by CMake defines.
 */

#ifndef ETHERVOX_PLATFORM_TIME_H
#define ETHERVOX_PLATFORM_TIME_H

#include "ethervox/error.h"
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Monotonic time (for measuring durations, not affected by clock adjustments)
// ============================================================================

#ifdef _WIN32
#include <windows.h>

/**
 * Get monotonic time in milliseconds
 * 
 * Uses QueryPerformanceCounter on Windows, CLOCK_MONOTONIC on POSIX.
 * Suitable for measuring durations, not affected by system clock changes.
 */
static inline uint64_t ethervox_time_monotonic_ms(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000) / frequency.QuadPart);
}

#else
#include <sys/time.h>

static inline uint64_t ethervox_time_monotonic_ms(void) {
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)(ts.tv_sec * 1000) + (uint64_t)(ts.tv_nsec / 1000000);
    }
#endif
    // Fallback to gettimeofday (not strictly monotonic but widely available)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000) + (uint64_t)(tv.tv_usec / 1000);
}

#endif

// ============================================================================
// Wall clock time (real time, for timestamps)
// ============================================================================

/**
 * Get current Unix timestamp in seconds
 * 
 * Uses time() which is portable across all platforms.
 */
static inline time_t ethervox_time_now(void) {
    return time(NULL);
}

/**
 * Get current Unix timestamp in milliseconds
 */
static inline uint64_t ethervox_time_now_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    
    // Convert FILETIME to Unix timestamp
    // FILETIME is 100-nanosecond intervals since 1601-01-01
    // Unix timestamp is seconds since 1970-01-01
    uint64_t time = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    time -= 116444736000000000ULL;  // Difference between 1601 and 1970
    return time / 10000;  // Convert 100ns to ms
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000) + (uint64_t)(tv.tv_usec / 1000);
#endif
}

// ============================================================================
// Sleep
// ============================================================================

#ifdef _WIN32
#include <windows.h>

/**
 * Sleep for specified milliseconds
 */
static inline void ethervox_sleep_ms(uint32_t milliseconds) {
    Sleep(milliseconds);
}

/**
 * Sleep for specified microseconds
 * 
 * Note: Windows Sleep() has ~15ms granularity. For sub-millisecond precision,
 * consider using a busy-wait or high-resolution timer.
 */
static inline void ethervox_sleep_us(uint32_t microseconds) {
    // Windows Sleep() only supports milliseconds, round up
    uint32_t ms = (microseconds + 999) / 1000;
    Sleep(ms > 0 ? ms : 1);
}

#else
#include <unistd.h>

/**
 * Sleep for specified milliseconds
 */
static inline void ethervox_sleep_ms(uint32_t milliseconds) {
    usleep(milliseconds * 1000);
}

/**
 * Sleep for specified microseconds
 */
static inline void ethervox_sleep_us(uint32_t microseconds) {
    usleep(microseconds);
}

#endif

// ============================================================================
// Time formatting (for logging)
// ============================================================================

/**
 * Format time as ISO 8601 string: "2026-08-16T14:30:00Z"
 * 
 * @param timestamp Unix timestamp in seconds (use ethervox_time_now())
 * @param buffer Output buffer (must be at least 21 bytes)
 * @param buffer_size Size of output buffer
 * @return Number of characters written (excluding null terminator), or 0 on error
 */
static inline size_t ethervox_time_format_iso8601(
    time_t timestamp,
    char* buffer,
    size_t buffer_size
) {
    if (!buffer || buffer_size < 21) return 0;
    
    struct tm utc_time;
#ifdef _WIN32
    gmtime_s(&utc_time, &timestamp);
#else
    gmtime_r(&timestamp, &utc_time);
#endif
    
    return (size_t)strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", &utc_time);
}

#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_PLATFORM_TIME_H */
