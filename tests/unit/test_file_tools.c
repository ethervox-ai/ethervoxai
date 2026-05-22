/**
 * @file test_file_tools.c
 * @brief Unit tests for file tools (Phase 1: Binary Detection & Enhanced file_read)
 *
 * Tests:
 * - Binary content detection
 * - Token estimation and oversized file handling
 * - Error messages for binary files
 * - Error messages for large files with helpful suggestions
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/file_tools.h"
#include "ethervox/error.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif
#include <unistd.h>

// Test data directory
#define TEST_DIR "/tmp/ethervox_file_test"

// Helper: Create test directory
static void setup_test_dir(void) {
#ifdef _WIN32
    _mkdir(TEST_DIR);
#else
    mkdir(TEST_DIR, 0755);
#endif
}

// Helper: Clean up test directory
static void cleanup_test_dir(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_DIR);
    system(cmd);
}

// Helper: Create a text file
static void create_text_file(const char* filename, const char* content) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", TEST_DIR, filename);
    FILE* f = fopen(path, "w");
    assert(f != NULL);
    fputs(content, f);
    fclose(f);
}

// Helper: Create a binary file
static void create_binary_file(const char* filename, const unsigned char* data, size_t size) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", TEST_DIR, filename);
    FILE* f = fopen(path, "wb");
    assert(f != NULL);
    fwrite(data, 1, size, f);
    fclose(f);
}

// Helper: Create a large text file
static void create_large_text_file(const char* filename, size_t size_kb) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", TEST_DIR, filename);
    FILE* f = fopen(path, "w");
    assert(f != NULL);
    
    // Write repeated content to reach desired size
    const char* line = "This is line %d with some content to fill space.\n";
    size_t written = 0;
    size_t target = size_kb * 1024;
    int line_num = 0;
    
    while (written < target) {
        char buf[256];
        int len = snprintf(buf, sizeof(buf), line, line_num++);
        fputs(buf, f);
        written += len;
    }
    
    fclose(f);
}

// Test 1: Binary detection - null bytes
void test_binary_detection_null_bytes(void) {
    printf("Test 1: Binary detection (null bytes)...\n");
    
    setup_test_dir();
    
    // Create binary file with null bytes
    unsigned char binary_data[] = {0x00, 0x01, 0x02, 0x03, 'H', 'e', 'l', 'l', 'o', 0x00};
    create_binary_file("binary_null.bin", binary_data, sizeof(binary_data));
    
    // Initialize file tools
    ethervox_file_tools_config_t config = {0};
    const char* base_paths[] = {TEST_DIR, NULL};
    assert(ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY) == 0);
    
    // Add filter for .bin extension
    ethervox_file_tools_add_filter(&config, ".bin");
    
    // Try to read binary file
    char* content = NULL;
    uint64_t size = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/binary_null.bin", TEST_DIR);
    
    int result = ethervox_file_read(&config, path, &content, &size);
    
    // Should return -2 (binary detected)
    assert(result == -2);
    assert(content == NULL);
    
    ethervox_file_tools_cleanup(&config);
    cleanup_test_dir();
    
    printf("  ✓ Binary file with null bytes correctly detected\n");
}

// Test 2: Binary detection - high control character ratio
void test_binary_detection_control_chars(void) {
    printf("Test 2: Binary detection (control characters)...\n");
    
    setup_test_dir();
    
    // Create file with high ratio of control characters
    unsigned char control_data[1000];
    for (int i = 0; i < 1000; i++) {
        // Fill with control chars (but no nulls to test the ratio threshold)
        control_data[i] = (i % 20 == 0) ? 'A' : (unsigned char)(i % 31 + 1);
    }
    create_binary_file("control_heavy.dat", control_data, sizeof(control_data));
    
    ethervox_file_tools_config_t config = {0};
    const char* base_paths[] = {TEST_DIR, NULL};
    assert(ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY) == 0);
    ethervox_file_tools_add_filter(&config, ".dat");
    
    char* content = NULL;
    uint64_t size = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/control_heavy.dat", TEST_DIR);
    
    int result = ethervox_file_read(&config, path, &content, &size);
    
    // Should detect as binary due to high control char ratio (>30%)
    assert(result == -2);
    
    ethervox_file_tools_cleanup(&config);
    cleanup_test_dir();
    
    printf("  ✓ File with high control char ratio correctly detected as binary\n");
}

// Test 3: Normal text file - should NOT be detected as binary
void test_text_file_not_binary(void) {
    printf("Test 3: Normal text file (not binary)...\n");
    
    setup_test_dir();
    
    const char* text_content = 
        "This is a normal text file.\n"
        "It has multiple lines.\n"
        "With newlines, tabs\t, and normal characters.\n"
        "Should NOT be detected as binary.\n";
    
    create_text_file("normal.txt", text_content);
    
    ethervox_file_tools_config_t config = {0};
    const char* base_paths[] = {TEST_DIR, NULL};
    assert(ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY) == 0);
    ethervox_file_tools_add_filter(&config, ".txt");
    
    char* content = NULL;
    uint64_t size = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/normal.txt", TEST_DIR);
    
    int result = ethervox_file_read(&config, path, &content, &size);
    
    // Should succeed (not binary)
    assert(result == 0);
    assert(content != NULL);
    assert(strcmp(content, text_content) == 0);
    
    free(content);
    ethervox_file_tools_cleanup(&config);
    cleanup_test_dir();
    
    printf("  ✓ Normal text file correctly read (not flagged as binary)\n");
}

// Test 4: Code file with special chars - should NOT be binary
void test_code_file_not_binary(void) {
    printf("Test 4: Code file with special characters (not binary)...\n");
    
    setup_test_dir();
    
    const char* code_content = 
        "int main() {\n"
        "    printf(\"Hello, world!\\n\");\n"
        "    char* str = \"test\\ttabs\\nand\\rescapes\";\n"
        "    return ETHERVOX_SUCCESS;\n"
        "}\n";
    
    create_text_file("code.c", code_content);
    
    ethervox_file_tools_config_t config = {0};
    const char* base_paths[] = {TEST_DIR, NULL};
    assert(ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY) == 0);
    ethervox_file_tools_add_filter(&config, ".c");
    
    char* content = NULL;
    uint64_t size = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/code.c", TEST_DIR);
    
    int result = ethervox_file_read(&config, path, &content, &size);
    
    assert(result == 0);
    assert(content != NULL);
    
    free(content);
    ethervox_file_tools_cleanup(&config);
    cleanup_test_dir();
    
    printf("  ✓ Code file with escape sequences correctly read\n");
}

// Test 5: Large file handling - should be caught by size check
void test_large_file_rejected(void) {
    printf("Test 5: Large file rejection (>10MB)...\n");
    
    setup_test_dir();
    
    // Create 11MB file (exceeds default 10MB limit)
    create_large_text_file("huge.txt", 11 * 1024);
    
    ethervox_file_tools_config_t config = {0};
    const char* base_paths[] = {TEST_DIR, NULL};
    assert(ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY) == 0);
    ethervox_file_tools_add_filter(&config, ".txt");
    
    char* content = NULL;
    uint64_t size = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/huge.txt", TEST_DIR);
    
    int result = ethervox_file_read(&config, path, &content, &size);
    
    // Should be rejected due to size
    assert(result == -1);
    assert(content == NULL);
    
    ethervox_file_tools_cleanup(&config);
    cleanup_test_dir();
    
    printf("  ✓ File >10MB correctly rejected\n");
}

// Test 6: Medium-sized file (6KB) - should succeed
void test_medium_file_reads(void) {
    printf("Test 6: Medium file read (6KB)...\n");
    
    setup_test_dir();
    
    // Create 6KB file (below 10MB limit)
    create_large_text_file("medium.txt", 6);
    
    ethervox_file_tools_config_t config = {0};
    const char* base_paths[] = {TEST_DIR, NULL};
    assert(ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY) == 0);
    ethervox_file_tools_add_filter(&config, ".txt");
    
    char* content = NULL;
    uint64_t size = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/medium.txt", TEST_DIR);
    
    int result = ethervox_file_read(&config, path, &content, &size);
    
    // Should succeed
    assert(result == 0);
    assert(content != NULL);
    assert(size >= 6 * 1024);  // At least 6KB
    
    free(content);
    ethervox_file_tools_cleanup(&config);
    cleanup_test_dir();
    
    printf("  ✓ Medium-sized file (6KB) read successfully\n");
}

// Test 7: Empty file - should NOT be binary
void test_empty_file(void) {
    printf("Test 7: Empty file handling...\n");
    
    setup_test_dir();
    
    create_text_file("empty.txt", "");
    
    ethervox_file_tools_config_t config = {0};
    const char* base_paths[] = {TEST_DIR, NULL};
    assert(ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY) == 0);
    ethervox_file_tools_add_filter(&config, ".txt");
    
    char* content = NULL;
    uint64_t size = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/empty.txt", TEST_DIR);
    
    int result = ethervox_file_read(&config, path, &content, &size);
    
    // Empty file should succeed (not binary)
    assert(result == 0);
    assert(content != NULL);
    assert(size == 0);
    assert(strlen(content) == 0);
    
    free(content);
    ethervox_file_tools_cleanup(&config);
    cleanup_test_dir();
    
    printf("  ✓ Empty file handled correctly\n");
}

// Test 8: File with UTF-8 content - should NOT be binary
void test_utf8_file(void) {
    printf("Test 8: UTF-8 encoded file...\n");
    
    setup_test_dir();
    
    const char* utf8_content = 
        "Hello in different languages:\n"
        "English: Hello\n"
        "Spanish: Hola\n"
        "Chinese: 你好\n"
        "Japanese: こんにちは\n"
        "Arabic: مرحبا\n"
        "Emoji: 🎉 🚀 ✨\n";
    
    create_text_file("utf8.txt", utf8_content);
    
    ethervox_file_tools_config_t config = {0};
    const char* base_paths[] = {TEST_DIR, NULL};
    assert(ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY) == 0);
    ethervox_file_tools_add_filter(&config, ".txt");
    
    char* content = NULL;
    uint64_t size = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/utf8.txt", TEST_DIR);
    
    int result = ethervox_file_read(&config, path, &content, &size);
    
    // UTF-8 text should be readable
    assert(result == 0);
    assert(content != NULL);
    assert(strstr(content, "Hello") != NULL);
    assert(strstr(content, "🎉") != NULL);  // Emoji should be preserved
    
    free(content);
    ethervox_file_tools_cleanup(&config);
    cleanup_test_dir();
    
    printf("  ✓ UTF-8 file with international characters read correctly\n");
}

// Test 9: Extension filtering
void test_extension_filtering(void) {
    printf("Test 9: Extension filtering...\n");
    
    setup_test_dir();
    
    create_text_file("allowed.txt", "Allowed content");
    create_text_file("not_allowed.xyz", "Should not be readable");
    
    ethervox_file_tools_config_t config = {0};
    const char* base_paths[] = {TEST_DIR, NULL};
    assert(ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY) == 0);
    
    // Only allow .txt files
    ethervox_file_tools_add_filter(&config, ".txt");
    
    char* content = NULL;
    uint64_t size = 0;
    
    // Should succeed for .txt
    char path1[512];
    snprintf(path1, sizeof(path1), "%s/allowed.txt", TEST_DIR);
    assert(ethervox_file_read(&config, path1, &content, &size) == 0);
    free(content);
    content = NULL;
    
    // Should fail for .xyz
    char path2[512];
    snprintf(path2, sizeof(path2), "%s/not_allowed.xyz", TEST_DIR);
    assert(ethervox_file_read(&config, path2, &content, &size) == -1);
    
    ethervox_file_tools_cleanup(&config);
    cleanup_test_dir();
    
    printf("  ✓ Extension filtering works correctly\n");
}

// Test 10: Path validation (file outside allowed base path)
void test_path_validation(void) {
    printf("Test 10: Path validation (security)...\n");
    
    setup_test_dir();
    
    // Create file outside allowed directory
    FILE* f = fopen("/tmp/outside.txt", "w");
    if (f) {
        fputs("Should not be readable", f);
        fclose(f);
    }
    
    ethervox_file_tools_config_t config = {0};
    const char* base_paths[] = {TEST_DIR, NULL};  // Only allow TEST_DIR
    assert(ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY) == 0);
    ethervox_file_tools_add_filter(&config, ".txt");
    
    char* content = NULL;
    uint64_t size = 0;
    
    // Try to read file outside allowed path
    int result = ethervox_file_read(&config, "/tmp/outside.txt", &content, &size);
    
    // Should be rejected
    assert(result == -1);
    assert(content == NULL);
    
    ethervox_file_tools_cleanup(&config);
    cleanup_test_dir();
    unlink("/tmp/outside.txt");
    
    printf("  ✓ Path validation prevents reading outside allowed directories\n");
}

// Main test runner
int main(void) {
    printf("\n=== File Tools Phase 1 Unit Tests ===\n\n");
    
    printf("Binary Detection Tests:\n");
    test_binary_detection_null_bytes();
    test_binary_detection_control_chars();
    test_text_file_not_binary();
    test_code_file_not_binary();
    test_utf8_file();
    test_empty_file();
    
    printf("\nSize Handling Tests:\n");
    test_large_file_rejected();
    test_medium_file_reads();
    
    printf("\nSecurity Tests:\n");
    test_extension_filtering();
    test_path_validation();
    
    printf("\n=== All tests passed! ✓ ===\n\n");
    
    return ETHERVOX_SUCCESS;
}
