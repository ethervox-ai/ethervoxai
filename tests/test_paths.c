/**
 * @file test_paths.c
 * @brief Test path configuration API
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ethervox/paths.h"
#include "ethervox/error.h"

// Test status tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "❌ FAIL: %s\n", message); \
        tests_failed++; \
        return; \
    } \
    tests_passed++; \
} while(0)

#define TEST_LOG(format, ...) printf("  ℹ️  " format "\n", ##__VA_ARGS__)

/**
 * Test: Validate rejects NULL paths
 */
static void test_validate_null() {
    printf("\n🧪 Test: Validate rejects NULL\n");
    
    ethervox_result_t result = ethervox_paths_validate(NULL);
    TEST_ASSERT(result == ETHERVOX_ERROR_INVALID_ARGUMENT, 
                "Validate should reject NULL paths");
}

/**
 * Test: Validate rejects empty paths
 */
static void test_validate_empty() {
    printf("\n🧪 Test: Validate rejects empty paths\n");
    
    ethervox_paths_t paths = {
        .data_dir = "",
        .cache_dir = "/tmp",
        .models_dir = "/tmp",
        .temp_dir = "/tmp"
    };
    
    ethervox_result_t result = ethervox_paths_validate(&paths);
    TEST_ASSERT(result == ETHERVOX_ERROR_INVALID_ARGUMENT,
                "Validate should reject empty data_dir");
}

/**
 * Test: Validate rejects relative paths
 */
static void test_validate_relative() {
    printf("\n🧪 Test: Validate rejects relative paths\n");
    
    ethervox_paths_t paths = {
        .data_dir = "data",  // Relative path
        .cache_dir = "/tmp",
        .models_dir = "/tmp",
        .temp_dir = "/tmp"
    };
    
    ethervox_result_t result = ethervox_paths_validate(&paths);
    TEST_ASSERT(result == ETHERVOX_ERROR_INVALID_ARGUMENT,
                "Validate should reject relative paths");
}

/**
 * Test: Get default paths
 */
static void test_get_default() {
    printf("\n🧪 Test: Get default paths\n");
    
    ethervox_paths_t paths;
    char buffer[2048];
    
    ethervox_result_t result = ethervox_paths_get_default(&paths, buffer, sizeof(buffer));
    
#if defined(__APPLE__) || defined(__linux__)
    // macOS and Linux should succeed
    TEST_ASSERT(result == ETHERVOX_SUCCESS, "Get default should succeed on macOS/Linux");
    TEST_ASSERT(paths.data_dir != NULL, "data_dir should be set");
    TEST_ASSERT(paths.cache_dir != NULL, "cache_dir should be set");
    TEST_ASSERT(paths.models_dir != NULL, "models_dir should be set");
    TEST_ASSERT(paths.temp_dir != NULL, "temp_dir should be set");
    
    TEST_LOG("data_dir: %s", paths.data_dir);
    TEST_LOG("cache_dir: %s", paths.cache_dir);
    TEST_LOG("models_dir: %s", paths.models_dir);
    TEST_LOG("temp_dir: %s", paths.temp_dir);
    
    // Verify paths are absolute
    TEST_ASSERT(paths.data_dir[0] == '/', "data_dir should be absolute");
    TEST_ASSERT(paths.cache_dir[0] == '/', "cache_dir should be absolute");
    TEST_ASSERT(paths.models_dir[0] == '/', "models_dir should be absolute");
    TEST_ASSERT(paths.temp_dir[0] == '/', "temp_dir should be absolute");
    
#elif defined(ANDROID) || defined(__ANDROID__) || defined(TARGET_OS_IPHONE)
    // Android/iOS should return NOT_IMPLEMENTED (no defaults)
    TEST_ASSERT(result == ETHERVOX_ERROR_NOT_IMPLEMENTED,
                "Get default should return NOT_IMPLEMENTED on Android/iOS");
#endif
}

/**
 * Test: Path join
 */
// On Windows the join inserts '\\'; normalise so the same expectations hold on every platform.
static void normalize_seps(char* path) {
#ifdef _WIN32
    for (char* p = path; *p; p++) {
        if (*p == '\\') *p = '/';
    }
#else
    (void)path;
#endif
}

static void test_path_join() {
    printf("\n🧪 Test: Path join\n");
    
    char out[256];
    ethervox_result_t result;
    
    // Join with trailing separator
    result = ethervox_path_join("/tmp/", "file.txt", out, sizeof(out));
    TEST_ASSERT(result == ETHERVOX_SUCCESS, "Join should succeed");
    normalize_seps(out);
    TEST_ASSERT(strcmp(out, "/tmp/file.txt") == 0, "Should handle trailing separator");
    
    // Join without trailing separator
    result = ethervox_path_join("/tmp", "file.txt", out, sizeof(out));
    TEST_ASSERT(result == ETHERVOX_SUCCESS, "Join should succeed");
    normalize_seps(out);
    TEST_ASSERT(strcmp(out, "/tmp/file.txt") == 0, "Should add separator");
    
    // Join with nested path
    result = ethervox_path_join("/tmp", "foo/bar.txt", out, sizeof(out));
    TEST_ASSERT(result == ETHERVOX_SUCCESS, "Join should succeed");
    normalize_seps(out);
    TEST_ASSERT(strcmp(out, "/tmp/foo/bar.txt") == 0, "Should handle nested paths");
    
    TEST_LOG("Path join output: %s", out);
}

/**
 * Test: Path join buffer overflow
 */
static void test_path_join_overflow() {
    printf("\n🧪 Test: Path join buffer overflow\n");
    
    char out[10];  // Too small
    
    ethervox_result_t result = ethervox_path_join("/tmp", "very_long_filename.txt", 
                                                  out, sizeof(out));
    TEST_ASSERT(result == ETHERVOX_ERROR_INVALID_ARGUMENT,
                "Join should reject buffer overflow");
}

/**
 * Test: Ensure directory
 */
static void test_ensure_directory() {
    printf("\n🧪 Test: Ensure directory\n");
    
    // Create a test directory in /tmp
    const char* test_dir = "/tmp/ethervox_test_dir";
    
    // Remove if exists (ignore errors)
    system("rm -rf /tmp/ethervox_test_dir 2>/dev/null");
    
    ethervox_result_t result = ethervox_ensure_directory(test_dir);
    TEST_ASSERT(result == ETHERVOX_SUCCESS, "Ensure directory should succeed");
    
    // Calling again should also succeed (idempotent)
    result = ethervox_ensure_directory(test_dir);
    TEST_ASSERT(result == ETHERVOX_SUCCESS, "Ensure directory should be idempotent");
    
    // Clean up
    system("rm -rf /tmp/ethervox_test_dir 2>/dev/null");
    
    TEST_LOG("Directory creation successful");
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("═══════════════════════════════════════════════════════\n");
    printf("  Path Configuration Tests\n");
    printf("═══════════════════════════════════════════════════════\n");
    
    // Run all tests
    test_validate_null();
    test_validate_empty();
    test_validate_relative();
    test_get_default();
    test_path_join();
    test_path_join_overflow();
    test_ensure_directory();
    
    // Print summary
    printf("\n═══════════════════════════════════════════════════════\n");
    printf("  Test Results\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("  ✅ Passed: %d\n", tests_passed);
    printf("  ❌ Failed: %d\n", tests_failed);
    printf("═══════════════════════════════════════════════════════\n");
    
    if (tests_failed > 0) {
        printf("\n⚠️  Some tests failed. Review the output above.\n");
        return 1;
    } else {
        printf("\n✨ All tests passed!\n");
        return 0;
    }
}
