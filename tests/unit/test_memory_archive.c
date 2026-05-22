/**
 * @file test_memory_archive.c
 * @brief Unit tests for memory archiving functionality
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif
#define TEST_DIR "/tmp/ethervox_test_archive"
#define ARCHIVE_DIR TEST_DIR "/archive"

static void cleanup_test_dir(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_DIR);
    system(cmd);
}

static void create_test_session_file(const char* filename, const char* content) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", TEST_DIR, filename);
    
    FILE* f = fopen(filepath, "w");
    if (f) {
        fprintf(f, "%s\n", content);
        fclose(f);
    }
}

static int count_files_in_dir(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) return ETHERVOX_SUCCESS;
    
    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {  // Skip . and ..
            count++;
        }
    }
    closedir(dir);
    return count;
}

static void test_archive_basic(void) {
    printf("Test 1: Basic archiving...\n");
    
    cleanup_test_dir();
#ifdef _WIN32
    _mkdir(TEST_DIR);
#else
    mkdir(TEST_DIR, 0755);
#endif
    
    // Create test session files
    create_test_session_file("session_1.jsonl", "{\"id\":0,\"text\":\"test1\"}");
    create_test_session_file("session_2.jsonl", "{\"id\":1,\"text\":\"test2\"}");
    
    // Initialize memory store (this will create a NEW session file with timestamp)
    ethervox_memory_store_t store;
    if (ethervox_memory_init(&store, NULL, TEST_DIR) != 0) {
        printf("  ✗ Failed to initialize memory store\n");
        cleanup_test_dir();
        exit(1);
    }
    
    // The current session will have a timestamp-based name, different from session_1 and session_2
    
    // Archive old sessions
    uint32_t archived = 0;
    if (ethervox_memory_archive_sessions(&store, &archived) != 0) {
        printf("  ✗ Archive failed\n");
        ethervox_memory_cleanup(&store);
        cleanup_test_dir();
        exit(1);
    }
    
    // Verify results - should archive the 2 old files
    if (archived != 2) {
        printf("  ✗ Expected 2 archived files, got %u\n", archived);
        ethervox_memory_cleanup(&store);
        cleanup_test_dir();
        exit(1);
    }
    
    // Verify archive directory exists and contains files
    int archive_count = count_files_in_dir(ARCHIVE_DIR);
    if (archive_count != 2) {
        printf("  ✗ Expected 2 files in archive, found %d\n", archive_count);
        ethervox_memory_cleanup(&store);
        cleanup_test_dir();
        exit(1);
    }
    
    // Verify current session still exists (check that a timestamped session file exists)
    int main_count = count_files_in_dir(TEST_DIR);
    if (main_count < 1) {  // At least the current session should exist
        printf("  ✗ Current session file missing\n");
        ethervox_memory_cleanup(&store);
        cleanup_test_dir();
        exit(1);
    }
    
    printf("  ✓ Basic archiving works\n");
    
    ethervox_memory_cleanup(&store);
    cleanup_test_dir();
}

static void test_archive_empty(void) {
    printf("Test 2: Archive with no old files...\n");
    
    cleanup_test_dir();
#ifdef _WIN32
    _mkdir(TEST_DIR);
#else
    mkdir(TEST_DIR, 0755);
#endif
    
    // Initialize store - this creates a timestamped session file
    ethervox_memory_store_t store;
    if (ethervox_memory_init(&store, NULL, TEST_DIR) != 0) {
        printf("  ✗ Failed to initialize memory store\n");
        cleanup_test_dir();
        exit(1);
    }
    
    // Now archive - should find 0 files to archive (only current session exists)
    uint32_t archived = 0;
    if (ethervox_memory_archive_sessions(&store, &archived) != 0) {
        printf("  ✗ Archive failed\n");
        ethervox_memory_cleanup(&store);
        cleanup_test_dir();
        exit(1);
    }
    
    if (archived != 0) {
        printf("  ✗ Expected 0 archived files, got %u\n", archived);
        ethervox_memory_cleanup(&store);
        cleanup_test_dir();
        exit(1);
    }
    
    printf("  ✓ Empty archive handled correctly\n");
    
    ethervox_memory_cleanup(&store);
    cleanup_test_dir();
}

static void test_archive_no_storage(void) {
    printf("Test 3: Archive with no storage directory...\n");
    
    ethervox_memory_store_t store;
    if (ethervox_memory_init(&store, NULL, NULL) != 0) {
        printf("  ✗ Failed to initialize memory store\n");
        exit(1);
    }
    
    uint32_t archived = 0;
    if (ethervox_memory_archive_sessions(&store, &archived) != 0) {
        printf("  ✗ Should return success for no-op\n");
        ethervox_memory_cleanup(&store);
        exit(1);
    }
    
    if (archived != 0) {
        printf("  ✗ Expected 0 archived files, got %u\n", archived);
        ethervox_memory_cleanup(&store);
        exit(1);
    }
    
    printf("  ✓ No-storage case handled correctly\n");
    
    ethervox_memory_cleanup(&store);
}

static void test_archive_multiple_runs(void) {
    printf("Test 4: Multiple archive runs (idempotence)...\n");
    
    cleanup_test_dir();
#ifdef _WIN32
    _mkdir(TEST_DIR);
#else
    mkdir(TEST_DIR, 0755);
#endif
    
    create_test_session_file("session_1.jsonl", "{\"id\":0,\"text\":\"test1\"}");
    create_test_session_file("session_2.jsonl", "{\"id\":1,\"text\":\"test2\"}");
    
    // Initialize creates its own timestamped session
    ethervox_memory_store_t store;
    if (ethervox_memory_init(&store, NULL, TEST_DIR) != 0) {
        printf("  ✗ Failed to initialize memory store\n");
        cleanup_test_dir();
        exit(1);
    }
    
    // First archive - should archive session_1 and session_2
    uint32_t archived = 0;
    if (ethervox_memory_archive_sessions(&store, &archived) != 0 || archived != 2) {
        printf("  ✗ First archive failed or wrong count (got %u)\n", archived);
        ethervox_memory_cleanup(&store);
        cleanup_test_dir();
        exit(1);
    }
    
    // Second archive (should find nothing)
    archived = 999;  // Set to non-zero to verify it gets set correctly
    if (ethervox_memory_archive_sessions(&store, &archived) != 0) {
        printf("  ✗ Second archive failed\n");
        ethervox_memory_cleanup(&store);
        cleanup_test_dir();
        exit(1);
    }
    
    if (archived != 0) {
        printf("  ✗ Second archive should find 0 files, found %u\n", archived);
        ethervox_memory_cleanup(&store);
        cleanup_test_dir();
        exit(1);
    }
    
    printf("  ✓ Multiple runs are idempotent\n");
    
    ethervox_memory_cleanup(&store);
    cleanup_test_dir();
}

int main(void) {
    printf("\n━━━ Memory Archive Unit Tests ━━━\n\n");
    
    test_archive_basic();
    test_archive_empty();
    test_archive_no_storage();
    test_archive_multiple_runs();
    
    printf("\n━━━ All tests passed! ━━━\n");
    
    return ETHERVOX_SUCCESS;
}
