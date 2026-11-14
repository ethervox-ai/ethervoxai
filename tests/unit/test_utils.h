/**
 * @file test_utils.h
 * @brief Common utilities for unit tests
 *
 * Provides CHECK() macro that works regardless of NDEBUG setting.
 * Tests in tests/unit/ are built with -DNDEBUG (Release mode), which
 * turns assert() into a no-op. CHECK() always evaluates its condition.
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_TEST_UTILS_H
#define ETHERVOX_TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>

/**
 * CHECK macro: always evaluates condition, exits on failure
 * 
 * Unlike assert(), this works regardless of NDEBUG setting.
 * Use this in all unit tests to ensure they actually validate conditions
 * even when built in Release mode.
 */
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "  ✗ CHECK failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

/**
 * RUN_TEST macro: standardized test execution with pass/fail reporting
 */
#define RUN_TEST(test_func) do { \
    printf("Running %s...\n", #test_func); \
    test_func(); \
    printf("  ✓ %s passed\n", #test_func); \
} while (0)

#endif // ETHERVOX_TEST_UTILS_H
