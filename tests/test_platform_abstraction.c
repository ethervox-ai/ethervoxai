/**
 * @file test_platform_abstraction.c
 * @brief Test suite for platform abstraction headers (C4.1)
 *
 * Tests that all platform headers compile and basic operations work.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/platform_thread.h"
#include "ethervox/platform_time.h"
#include "ethervox/platform_directory.h"
#include "ethervox/platform_fs.h"
#include "ethervox/platform_mem.h"
#include <stdio.h>
#include <string.h>

static void print_test_header(const char* name) {
    printf("\n=== %s ===\n", name);
}

/**
 * Test 1: Mutex operations
 */
static bool test_mutex(void) {
    print_test_header("Test 1: Mutex Operations");
    
    ethervox_mutex_t mutex;
    ethervox_result_t result = ethervox_mutex_init(&mutex);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: mutex_init failed\n");
        return false;
    }
    
    result = ethervox_mutex_lock(&mutex);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: mutex_lock failed\n");
        ethervox_mutex_destroy(&mutex);
        return false;
    }
    
    result = ethervox_mutex_unlock(&mutex);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: mutex_unlock failed\n");
        ethervox_mutex_destroy(&mutex);
        return false;
    }
    
    ethervox_mutex_destroy(&mutex);
    printf("PASS: Mutex init/lock/unlock/destroy\n");
    return true;
}

/**
 * Test 2: Time operations
 */
static bool test_time(void) {
    print_test_header("Test 2: Time Operations");
    
    // Monotonic time
    uint64_t t1 = ethervox_time_monotonic_ms();
    ethervox_sleep_ms(10);
    uint64_t t2 = ethervox_time_monotonic_ms();
    
    if (t2 <= t1) {
        fprintf(stderr, "FAIL: monotonic time not increasing\n");
        return false;
    }
    
    printf("Monotonic time delta: %llu ms\n", (unsigned long long)(t2 - t1));
    
    // Wall clock time
    time_t now = ethervox_time_now();
    uint64_t now_ms = ethervox_time_now_ms();
    
    printf("Wall clock: %lld (Unix timestamp)\n", (long long)now);
    printf("Wall clock ms: %llu\n", (unsigned long long)now_ms);
    
    // ISO 8601 formatting
    char iso_buffer[32];
    size_t written = ethervox_time_format_iso8601(now, iso_buffer, sizeof(iso_buffer));
    if (written == 0) {
        fprintf(stderr, "FAIL: ISO 8601 formatting failed\n");
        return false;
    }
    
    printf("ISO 8601: %s\n", iso_buffer);
    printf("PASS: Time operations\n");
    return true;
}

/**
 * Test 3: File system operations
 */
static bool test_fs(void) {
    print_test_header("Test 3: File System Operations");
    
    const char* test_file = "/tmp/ethervox_test_file.txt";
    const char* test_file2 = "/tmp/ethervox_test_file2.txt";
    
    // Create test file
    FILE* f = fopen(test_file, "w");
    if (!f) {
        fprintf(stderr, "FAIL: Could not create test file\n");
        return false;
    }
    fprintf(f, "Hello, Platform Abstraction!\n");
    fclose(f);
    
    // Test existence
    if (!ethervox_fs_exists(test_file)) {
        fprintf(stderr, "FAIL: File exists check failed\n");
        return false;
    }
    
    // Test is_file
    if (!ethervox_fs_is_file(test_file)) {
        fprintf(stderr, "FAIL: is_file check failed\n");
        return false;
    }
    
    // Test file size
    uint64_t size;
    ethervox_result_t result = ethervox_fs_file_size(test_file, &size);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: file_size failed\n");
        return false;
    }
    printf("File size: %llu bytes\n", (unsigned long long)size);
    
    // Test copy
    result = ethervox_fs_copy(test_file, test_file2);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: file copy failed\n");
        ethervox_fs_delete(test_file);
        return false;
    }
    
    // Test rename
    const char* test_file3 = "/tmp/ethervox_test_file3.txt";
    result = ethervox_fs_rename(test_file2, test_file3);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: file rename failed\n");
        ethervox_fs_delete(test_file);
        ethervox_fs_delete(test_file2);
        return false;
    }
    
    // Cleanup
    ethervox_fs_delete(test_file);
    ethervox_fs_delete(test_file3);
    
    printf("PASS: File system operations\n");
    return true;
}

/**
 * Test 4: Directory operations
 */
static bool test_directory(void) {
    print_test_header("Test 4: Directory Operations");
    
    const char* test_dir = "/tmp/ethervox_test_dir";
    
    // Create directory
    ethervox_result_t result = ethervox_dir_create(test_dir);
    if (result != ETHERVOX_SUCCESS && result != ETHERVOX_ERROR_FILE_EXISTS) {
        fprintf(stderr, "FAIL: dir_create failed\n");
        return false;
    }
    
    // Test is_dir
    if (!ethervox_fs_is_dir(test_dir)) {
        fprintf(stderr, "FAIL: is_dir check failed\n");
        ethervox_dir_remove(test_dir);
        return false;
    }
    
    // Create test files in directory
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/test1.txt", test_dir);
    FILE* f = fopen(file_path, "w");
    if (f) {
        fprintf(f, "test\n");
        fclose(f);
    }
    
    // List directory
    ethervox_dir_t dir;
    result = ethervox_dir_open(test_dir, &dir);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: dir_open failed\n");
        ethervox_dir_remove(test_dir);
        return false;
    }
    
    printf("Directory contents:\n");
    char entry_name[256];
    bool is_dir;
    int entry_count = 0;
    
    while (ethervox_dir_read(&dir, entry_name, sizeof(entry_name), &is_dir) == ETHERVOX_SUCCESS) {
        printf("  %s %s\n", entry_name, is_dir ? "[DIR]" : "");
        entry_count++;
    }
    
    ethervox_dir_close(&dir);
    
    if (entry_count == 0) {
        fprintf(stderr, "FAIL: No entries found in directory\n");
        ethervox_fs_delete(file_path);
        ethervox_dir_remove(test_dir);
        return false;
    }
    
    // Cleanup
    ethervox_fs_delete(file_path);
    ethervox_dir_remove(test_dir);
    
    printf("PASS: Directory operations (%d entries found)\n", entry_count);
    return true;
}

/**
 * Test 5: Memory operations
 */
static bool test_memory(void) {
    print_test_header("Test 5: Memory Operations");
    
    // Test aligned allocation
    void* aligned = ethervox_mem_aligned_alloc(1024, 64);
    if (!aligned) {
        fprintf(stderr, "FAIL: aligned_alloc failed\n");
        return false;
    }
    
    // Check alignment
    if (((uintptr_t)aligned % 64) != 0) {
        fprintf(stderr, "FAIL: Memory not aligned to 64 bytes\n");
        ethervox_mem_aligned_free(aligned);
        return false;
    }
    
    printf("Aligned memory: %p (aligned to 64 bytes)\n", aligned);
    ethervox_mem_aligned_free(aligned);
    
    // Test memory info
    uint64_t total_mb, available_mb;
    ethervox_result_t result = ethervox_mem_get_info(&total_mb, &available_mb);
    if (result == ETHERVOX_SUCCESS) {
        printf("System memory: %llu MB total, %llu MB available\n",
               (unsigned long long)total_mb,
               (unsigned long long)available_mb);
    } else {
        printf("Memory info not available on this platform\n");
    }
    
    printf("PASS: Memory operations\n");
    return true;
}

int main(void) {
    printf("PLATFORM ABSTRACTION LAYER TESTS (C4.1)\n");
    printf("=========================================\n");
    
    int passed = 0;
    int total = 5;
    
    if (test_mutex()) passed++;
    if (test_time()) passed++;
    if (test_fs()) passed++;
    if (test_directory()) passed++;
    if (test_memory()) passed++;
    
    printf("\n=========================================\n");
    printf("RESULTS: %d passed, %d failed\n", passed, total - passed);
    printf("=========================================\n");
    
    return (passed == total) ? 0 : 1;
}
