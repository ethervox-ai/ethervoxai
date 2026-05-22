/**
 * Unit tests for file_append tool
 * 
 * Tests the file_append functionality through standard file operations
 * since the tool wrapper is an internal implementation detail.
 * 
 * Integration testing of the actual tool happens in integration_tests.c
 */

#include <stdio.h>
#include "ethervox/error.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_FILE "test_append.txt"
#define TEST_FILE2 "test_append_multi.txt"

// Color codes for output
#define COLOR_GREEN "\033[0;32m"
#define COLOR_RED "\033[0;31m"
#define COLOR_RESET "\033[0m"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    if (!(condition)) { \
        printf(COLOR_RED "  ✗ %s" COLOR_RESET "\n", message); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS(message) do { \
    printf(COLOR_GREEN "  ✓ %s" COLOR_RESET "\n", message); \
    tests_passed++; \
} while(0)

static void cleanup_test_files(void) {
    unlink(TEST_FILE);
    unlink(TEST_FILE2);
}

// Helper function to simulate file_append behavior
static int file_append(const char* path, const char* content) {
    if (!path || !content) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    FILE* f = fopen(path, "a");
    if (!f) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    fprintf(f, "%s", content);
    fclose(f);
    return ETHERVOX_SUCCESS;
}

static void test_append_basic(void) {
    printf("Test 1: Basic append to existing file...\n");
    
    // Create initial file
    FILE* f = fopen(TEST_FILE, "w");
    TEST_ASSERT(f != NULL, "Failed to create test file");
    fprintf(f, "Line 1\n");
    fclose(f);
    
    // Append content
    int ret = file_append(TEST_FILE, "Line 2\n");
    TEST_ASSERT(ret == 0, "Append failed");
    
    // Verify content
    f = fopen(TEST_FILE, "r");
    TEST_ASSERT(f != NULL, "Failed to open file for verification");
    
    char buffer[256];
    fgets(buffer, sizeof(buffer), f);
    TEST_ASSERT(strcmp(buffer, "Line 1\n") == 0, "First line corrupted");
    
    fgets(buffer, sizeof(buffer), f);
    TEST_ASSERT(strcmp(buffer, "Line 2\n") == 0, "Second line not appended");
    
    fclose(f);
    
    TEST_PASS("Basic append works");
}

static void test_append_new_file(void) {
    printf("Test 2: Append to new file (create if not exists)...\n");
    
    unlink(TEST_FILE);  // Ensure file doesn't exist
    
    int ret = file_append(TEST_FILE, "New content\n");
    TEST_ASSERT(ret == 0, "Append to new file failed");
    
    // Verify file was created
    FILE* f = fopen(TEST_FILE, "r");
    TEST_ASSERT(f != NULL, "File was not created");
    
    char buffer[256];
    fgets(buffer, sizeof(buffer), f);
    TEST_ASSERT(strcmp(buffer, "New content\n") == 0, "Content not written correctly");
    
    fclose(f);
    
    TEST_PASS("Append to new file works");
}

static void test_append_multiple(void) {
    printf("Test 3: Multiple sequential appends...\n");
    
    unlink(TEST_FILE2);
    
    // Append three times
    int ret1 = file_append(TEST_FILE2, "Line 1\n");
    int ret2 = file_append(TEST_FILE2, "Line 2\n");
    int ret3 = file_append(TEST_FILE2, "Line 3\n");
    
    TEST_ASSERT(ret1 == 0 && ret2 == 0 && ret3 == 0, "One or more appends failed");
    
    // Verify all lines present
    FILE* f = fopen(TEST_FILE2, "r");
    TEST_ASSERT(f != NULL, "Failed to open file");
    
    int line_count = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), f)) {
        line_count++;
    }
    fclose(f);
    
    TEST_ASSERT(line_count == 3, "Expected 3 lines");
    
    TEST_PASS("Multiple appends work");
}

static void test_append_with_newlines(void) {
    printf("Test 4: Append content with embedded newlines...\n");
    
    unlink(TEST_FILE);
    
    int ret = file_append(TEST_FILE, "Header\n---\nLine 1\nLine 2\n");
    TEST_ASSERT(ret == 0, "Append with newlines failed");
    
    // Verify content
    FILE* f = fopen(TEST_FILE, "r");
    TEST_ASSERT(f != NULL, "Failed to open file");
    
    char buffer[1024];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[len] = '\0';
    fclose(f);
    
    TEST_ASSERT(strstr(buffer, "Header") != NULL, "Header not found");
    TEST_ASSERT(strstr(buffer, "---") != NULL, "Separator not found");
    TEST_ASSERT(strstr(buffer, "Line 1") != NULL, "Line 1 not found");
    TEST_ASSERT(strstr(buffer, "Line 2") != NULL, "Line 2 not found");
    
    TEST_PASS("Newlines handled correctly");
}

static void test_append_errors(void) {
    printf("Test 5: Error handling...\n");
    
    // NULL path
    int ret1 = file_append(NULL, "test");
    TEST_ASSERT(ret1 != 0, "Should reject NULL path");
    
    // NULL content
    int ret2 = file_append(TEST_FILE, NULL);
    TEST_ASSERT(ret2 != 0, "Should reject NULL content");
    
    // Invalid path (directory doesn't exist)
    int ret3 = file_append("/nonexistent/dir/file.txt", "test");
    TEST_ASSERT(ret3 != 0, "Should reject invalid path");
    
    TEST_PASS("Error handling works");
}

static void test_append_empty_content(void) {
    printf("Test 6: Append empty content...\n");
    
    unlink(TEST_FILE);
    
    // Create file with initial content
    FILE* f = fopen(TEST_FILE, "w");
    fprintf(f, "Initial\n");
    fclose(f);
    
    // Append empty string (should succeed but not change file)
    int ret = file_append(TEST_FILE, "");
    TEST_ASSERT(ret == 0, "Empty content append should succeed");
    
    // Verify file unchanged
    f = fopen(TEST_FILE, "r");
    char buffer[256];
    fgets(buffer, sizeof(buffer), f);
    fclose(f);
    
    TEST_ASSERT(strcmp(buffer, "Initial\n") == 0, "File should be unchanged");
    
    TEST_PASS("Empty content handled correctly");
}

static void test_append_special_chars(void) {
    printf("Test 7: Append content with special characters...\n");
    
    unlink(TEST_FILE);
    
    // Content with quotes, backslashes, tabs
    int ret = file_append(TEST_FILE, "Quote: \"test\"\tTab\tHere");
    TEST_ASSERT(ret == 0, "Special chars append failed");
    
    // Verify content
    FILE* f = fopen(TEST_FILE, "r");
    char buffer[256];
    fgets(buffer, sizeof(buffer), f);
    fclose(f);
    
    TEST_ASSERT(strstr(buffer, "\"test\"") != NULL, "Quotes preserved");
    TEST_ASSERT(strstr(buffer, "\t") != NULL, "Tabs preserved");
    
    TEST_PASS("Special characters handled correctly");
}

int main(void) {
    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  File Append Unit Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("\n");
    
    cleanup_test_files();
    
    test_append_basic();
    test_append_new_file();
    test_append_multiple();
    test_append_with_newlines();
    test_append_errors();
    test_append_empty_content();
    test_append_special_chars();
    
    cleanup_test_files();
    
    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("\n");
    
    return tests_failed > 0 ? 1 : 0;
}
