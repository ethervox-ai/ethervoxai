/**
 * @file test_profile_conformance.c
 * @brief Conformance tests for ETHERVOX_PROFILE feature composition (TASK-C1.2)
 *
 * Verifies that each profile correctly configures its feature flags and that
 * the generated ethervox_features.h matches expectations.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include <stdio.h>
#include <stdlib.h>
#include "ethervox_features.h"

// Test macros (minimal test framework)
#define TEST(name) \
    static int test_##name(void); \
    static int test_##name##_run(void) { \
        printf("  Running: %s\n", #name); \
        int result = test_##name(); \
        if (result == 0) { \
            printf("    ✓ PASS\n"); \
        } else { \
            printf("    ✗ FAIL\n"); \
        } \
        return result; \
    } \
    static int test_##name(void)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "      ASSERTION FAILED: %s\n", msg); \
            return 1; \
        } \
    } while (0)

#define ASSERT_EQ(a, b, msg) ASSERT((a) == (b), msg)

// Profile-specific feature expectations
#ifdef ETHERVOX_PROFILE_EDGE
#define EXPECTED_HTTP 0
#define EXPECTED_DOWNLOADER 0
#define EXPECTED_BUG_REPORT 0
#define EXPECTED_WEATHER 0
#define EXPECTED_FILE_TOOLS 0
#define PROFILE_NAME "EDGE"
#elif defined(ETHERVOX_PROFILE_MOBILE)
#define EXPECTED_HTTP 1
#define EXPECTED_DOWNLOADER 1
#define EXPECTED_BUG_REPORT 1
#define EXPECTED_WEATHER 1
#define EXPECTED_FILE_TOOLS 1
#define PROFILE_NAME "MOBILE"
#elif defined(ETHERVOX_PROFILE_DESKTOP)
#define EXPECTED_HTTP 1
#define EXPECTED_DOWNLOADER 1
#define EXPECTED_BUG_REPORT 1
#define EXPECTED_WEATHER 1
#define EXPECTED_FILE_TOOLS 1
#define PROFILE_NAME "DESKTOP"
#elif defined(ETHERVOX_PROFILE_WORKSPACE)
#define EXPECTED_HTTP 0
#define EXPECTED_DOWNLOADER 0
#define EXPECTED_BUG_REPORT 0
#define EXPECTED_WEATHER 0
#define EXPECTED_FILE_TOOLS 0
#define PROFILE_NAME "WORKSPACE"
#else
#error "No ETHERVOX_PROFILE defined - must be EDGE, MOBILE, DESKTOP, or WORKSPACE"
#endif

// Test: Profile macro is defined
TEST(profile_macro_defined) {
#if defined(ETHERVOX_PROFILE_EDGE) || defined(ETHERVOX_PROFILE_MOBILE) || \
    defined(ETHERVOX_PROFILE_DESKTOP) || defined(ETHERVOX_PROFILE_WORKSPACE)
    return 0;
#else
    return 1;
#endif
}

// Test: HTTP feature matches profile expectation
TEST(http_feature_correct) {
    ASSERT_EQ(ETHERVOX_HAS_HTTP, EXPECTED_HTTP, "HTTP feature mismatch");
    return 0;
}

// Test: Downloader feature matches profile expectation
TEST(downloader_feature_correct) {
    ASSERT_EQ(ETHERVOX_HAS_DOWNLOADER, EXPECTED_DOWNLOADER, "Downloader feature mismatch");
    return 0;
}

// Test: Bug reporter feature matches profile expectation
TEST(bug_reporter_feature_correct) {
    ASSERT_EQ(ETHERVOX_HAS_BUG_REPORT, EXPECTED_BUG_REPORT, "Bug reporter feature mismatch");
    return 0;
}

// Test: Weather feature matches profile expectation
TEST(weather_feature_correct) {
    ASSERT_EQ(ETHERVOX_HAS_WEATHER, EXPECTED_WEATHER, "Weather feature mismatch");
    return 0;
}

// Test: File tools feature matches profile expectation
TEST(file_tools_feature_correct) {
    ASSERT_EQ(ETHERVOX_HAS_FILE_TOOLS, EXPECTED_FILE_TOOLS, "File tools feature mismatch");
    return 0;
}

// Test: WORKSPACE profile has no network features
#ifdef ETHERVOX_PROFILE_WORKSPACE
TEST(workspace_no_network) {
    ASSERT_EQ(ETHERVOX_HAS_HTTP, 0, "WORKSPACE must not have HTTP");
    ASSERT_EQ(ETHERVOX_HAS_DOWNLOADER, 0, "WORKSPACE must not have DOWNLOADER");
    ASSERT_EQ(ETHERVOX_HAS_WEATHER, 0, "WORKSPACE must not have WEATHER");
    return 0;
}
#endif

// Test: EDGE profile has minimal features
#ifdef ETHERVOX_PROFILE_EDGE
TEST(edge_minimal_features) {
    ASSERT_EQ(ETHERVOX_HAS_HTTP, 0, "EDGE must not have HTTP");
    ASSERT_EQ(ETHERVOX_HAS_DOWNLOADER, 0, "EDGE must not have DOWNLOADER");
    ASSERT_EQ(ETHERVOX_HAS_FILE_TOOLS, 0, "EDGE must not have FILE_TOOLS");
    return 0;
}
#endif

int main(void) {
    printf("ETHERVOX_PROFILE Conformance Tests (%s)\n", PROFILE_NAME);
    printf("=====================================\n\n");
    
    int failures = 0;
    
    failures += test_profile_macro_defined_run();
    failures += test_http_feature_correct_run();
    failures += test_downloader_feature_correct_run();
    failures += test_bug_reporter_feature_correct_run();
    failures += test_weather_feature_correct_run();
    failures += test_file_tools_feature_correct_run();
    
#ifdef ETHERVOX_PROFILE_WORKSPACE
    failures += test_workspace_no_network_run();
#endif
    
#ifdef ETHERVOX_PROFILE_EDGE
    failures += test_edge_minimal_features_run();
#endif
    
    printf("\n");
    if (failures == 0) {
        printf("✓ All conformance tests passed for %s profile\n", PROFILE_NAME);
        return EXIT_SUCCESS;
    } else {
        printf("✗ %d test(s) failed for %s profile\n", failures, PROFILE_NAME);
        return EXIT_FAILURE;
    }
}
