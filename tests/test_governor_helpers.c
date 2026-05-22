/**
 * @file test_governor_helpers.c
 * @brief Unit tests for governor helper functions
 *
 * This test suite validates each helper function independently.
 * Tests are focused, fast, and don't require a full Governor instance.
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "governor_helpers.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Test macros
#define TEST(name) \
    void test_##name(); \
    void run_test_##name() { \
        tests_run++; \
        printf("[TEST] %s...\n", #name); \
        test_##name(); \
    } \
    void test_##name()

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("  ✗ FAILED: %s\n", msg); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual, msg) \
    ASSERT_TRUE((expected) == (actual), msg)

#define ASSERT_STR_EQ(expected, actual, msg) \
    ASSERT_TRUE(strcmp((expected), (actual)) == 0, msg)

#define ASSERT_NULL(ptr, msg) \
    ASSERT_TRUE((ptr) == NULL, msg)

#define ASSERT_NOT_NULL(ptr, msg) \
    ASSERT_TRUE((ptr) != NULL, msg)

#define PASS() \
    do { \
        printf("  ✓ PASSED\n"); \
        tests_passed++; \
    } while(0)

// ============================================================================
// Buffer Management Tests
// ============================================================================

TEST(string_buffer_init_default) {
    string_buffer_t buffer;
    ASSERT_TRUE(string_buffer_init(&buffer, 0), "Init should succeed");
    ASSERT_NOT_NULL(buffer.data, "Data should be allocated");
    ASSERT_EQ(0, buffer.length, "Length should be 0");
    ASSERT_TRUE(buffer.capacity > 0, "Capacity should be positive");
    string_buffer_free(&buffer);
    PASS();
}

TEST(string_buffer_init_custom_size) {
    string_buffer_t buffer;
    ASSERT_TRUE(string_buffer_init(&buffer, 1024), "Init should succeed");
    ASSERT_EQ(1024, buffer.capacity, "Capacity should match requested");
    string_buffer_free(&buffer);
    PASS();
}

TEST(string_buffer_append_simple) {
    string_buffer_t buffer;
    string_buffer_init(&buffer, 0);
    
    ASSERT_TRUE(string_buffer_append(&buffer, "Hello"), "Append should succeed");
    ASSERT_STR_EQ("Hello", buffer.data, "Content should match");
    ASSERT_EQ(5, buffer.length, "Length should be 5");
    
    string_buffer_free(&buffer);
    PASS();
}

TEST(string_buffer_append_multiple) {
    string_buffer_t buffer;
    string_buffer_init(&buffer, 0);
    
    string_buffer_append(&buffer, "Hello");
    string_buffer_append(&buffer, " ");
    string_buffer_append(&buffer, "World");
    
    ASSERT_STR_EQ("Hello World", buffer.data, "Content should be concatenated");
    ASSERT_EQ(11, buffer.length, "Length should be 11");
    
    string_buffer_free(&buffer);
    PASS();
}

TEST(string_buffer_grow_automatically) {
    string_buffer_t buffer;
    string_buffer_init(&buffer, 16);  // Small capacity
    
    // Append more than 16 bytes
    string_buffer_append(&buffer, "This is a long string that will exceed the initial capacity");
    
    ASSERT_TRUE(buffer.capacity > 16, "Buffer should have grown");
    ASSERT_TRUE(strlen(buffer.data) > 16, "Content should be preserved");
    
    string_buffer_free(&buffer);
    PASS();
}

TEST(string_buffer_clear_preserves_capacity) {
    string_buffer_t buffer;
    string_buffer_init(&buffer, 0);
    
    string_buffer_append(&buffer, "Hello World");
    size_t original_capacity = buffer.capacity;
    
    string_buffer_clear(&buffer);
    
    ASSERT_EQ(0, buffer.length, "Length should be 0");
    ASSERT_STR_EQ("", buffer.data, "Data should be empty");
    ASSERT_EQ(original_capacity, buffer.capacity, "Capacity should be unchanged");
    
    string_buffer_free(&buffer);
    PASS();
}

TEST(string_buffer_transfer_ownership) {
    string_buffer_t buffer;
    string_buffer_init(&buffer, 0);
    
    string_buffer_append(&buffer, "Test");
    char* transferred = string_buffer_transfer(&buffer);
    
    ASSERT_NOT_NULL(transferred, "Transfer should succeed");
    ASSERT_STR_EQ("Test", transferred, "Content should match");
    ASSERT_NULL(buffer.data, "Buffer data should be NULL after transfer");
    ASSERT_EQ(0, buffer.length, "Buffer length should be 0");
    
    free(transferred);
    PASS();
}

// ============================================================================
// Token Processing Tests
// ============================================================================

TEST(clean_token_text_normal) {
    char output[256];
    ASSERT_TRUE(clean_token_text("Hello", output, sizeof(output)), "Should succeed");
    ASSERT_STR_EQ("Hello", output, "Normal text should pass through");
    PASS();
}

TEST(clean_token_text_filters_special_markers) {
    char output[256];
    
    clean_token_text("<|im_end|>", output, sizeof(output));
    ASSERT_STR_EQ("", output, "Should filter stop marker");
    
    clean_token_text("</s>", output, sizeof(output));
    ASSERT_STR_EQ("", output, "Should filter EOS");
    
    PASS();
}

// ============================================================================
// KV Cache Tests
// ============================================================================

TEST(kv_cache_has_room_check) {
    kv_cache_state_t state = {0};
    state.current_pos = 100;
    state.capacity = 2048;
    state.is_valid = true;
    
    ASSERT_TRUE(kv_cache_has_room(&state, 100), "Should have room for 100 tokens");
    ASSERT_TRUE(kv_cache_has_room(&state, 1948), "Should have room for 1948 tokens");
    ASSERT_TRUE(!kv_cache_has_room(&state, 1949), "Should not have room for 1949 tokens");
    
    PASS();
}

TEST(kv_cache_invalid_state) {
    kv_cache_state_t state = {0};
    state.is_valid = false;
    
    ASSERT_TRUE(!kv_cache_has_room(&state, 1), "Invalid state should return false");
    
    PASS();
}

// ============================================================================
// Sequence Detection Tests
// ============================================================================

TEST(check_stop_sequence_no_match) {
    const char* stop_seqs[] = {"<|im_end|>", "</s>", NULL};
    
    sequence_match_t result = check_stop_sequence("Hello", stop_seqs);
    ASSERT_EQ(SEQ_NO_MATCH, result, "Should not match");
    
    PASS();
}

TEST(check_stop_sequence_partial_match) {
    const char* stop_seqs[] = {"<|im_end|>", "</s>", NULL};
    
    ASSERT_EQ(SEQ_PARTIAL_MATCH, check_stop_sequence("<", stop_seqs), "Should partially match");
    ASSERT_EQ(SEQ_PARTIAL_MATCH, check_stop_sequence("<|", stop_seqs), "Should partially match");
    ASSERT_EQ(SEQ_PARTIAL_MATCH, check_stop_sequence("<|im", stop_seqs), "Should partially match");
    
    PASS();
}

TEST(check_stop_sequence_complete_match) {
    const char* stop_seqs[] = {"<|im_end|>", "</s>", NULL};
    
    ASSERT_EQ(SEQ_COMPLETE_MATCH, check_stop_sequence("<|im_end|>", stop_seqs), "Should completely match");
    ASSERT_EQ(SEQ_COMPLETE_MATCH, check_stop_sequence("</s>", stop_seqs), "Should completely match");
    
    PASS();
}

TEST(check_tool_call_xml_attribute) {
    ASSERT_EQ(SEQ_NO_MATCH, check_tool_call("Hello", false), "Should not match");
    ASSERT_EQ(SEQ_PARTIAL_MATCH, check_tool_call("<tool", false), "Should partially match");
    ASSERT_EQ(SEQ_PARTIAL_MATCH, check_tool_call("<tool_call name=\"test\"", false), "Should partially match");
    ASSERT_EQ(SEQ_COMPLETE_MATCH, check_tool_call("<tool_call name=\"test\" />", false), "Should completely match");
    
    PASS();
}

TEST(check_tool_call_json_in_xml) {
    ASSERT_EQ(SEQ_PARTIAL_MATCH, check_tool_call("<tool_call>\n{", true), "Should partially match");
    ASSERT_EQ(SEQ_COMPLETE_MATCH, check_tool_call("<tool_call>\n{}\n</tool_call>", true), "Should completely match");
    
    PASS();
}

TEST(extract_tool_name_xml) {
    char name[64];
    
    ASSERT_TRUE(extract_tool_name("<tool_call name=\"get_time\" />", name, sizeof(name)), "Should extract");
    ASSERT_STR_EQ("get_time", name, "Name should match");
    
    PASS();
}

TEST(extract_tool_name_json) {
    char name[64];
    
    ASSERT_TRUE(extract_tool_name("{\"name\": \"calculator\"}", name, sizeof(name)), "Should extract");
    ASSERT_STR_EQ("calculator", name, "Name should match");
    
    PASS();
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(error_context_set_and_get) {
    error_context_t ctx;
    error_context_clear(&ctx);
    
    ASSERT_TRUE(!error_context_has_error(&ctx), "Should not have error initially");
    
    SET_ERROR(&ctx, -1, "Test error", true);
    
    ASSERT_TRUE(error_context_has_error(&ctx), "Should have error");
    ASSERT_EQ(-1, ctx.code, "Code should match");
    ASSERT_TRUE(ctx.is_fatal, "Should be fatal");
    ASSERT_TRUE(strstr(ctx.message, "Test error") != NULL, "Message should contain error text");
    
    PASS();
}

TEST(error_context_clear) {
    error_context_t ctx;
    SET_ERROR(&ctx, -1, "Test", true);
    
    error_context_clear(&ctx);
    
    ASSERT_TRUE(!error_context_has_error(&ctx), "Should not have error after clear");
    ASSERT_EQ(0, ctx.code, "Code should be 0");
    
    PASS();
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST(stats_initialization) {
    generation_stats_t stats;
    stats_init(&stats);
    
    ASSERT_EQ(0, stats.tokens_generated, "Should be 0");
    ASSERT_EQ(0, stats.tool_calls_executed, "Should be 0");
    ASSERT_EQ(0.0, stats.first_token_time, "Should be 0.0");
    
    PASS();
}

TEST(stats_record_tokens) {
    generation_stats_t stats;
    stats_init(&stats);
    
    stats_record_token(&stats);
    stats_record_token(&stats);
    stats_record_token(&stats);
    
    ASSERT_EQ(3, stats.tokens_generated, "Should be 3");
    
    PASS();
}

TEST(stats_tokens_per_second) {
    generation_stats_t stats;
    stats_init(&stats);
    
    stats.tokens_generated = 100;
    stats.total_generation_time = 2.0;  // 2 seconds
    
    double tps = stats_get_tokens_per_second(&stats);
    ASSERT_TRUE(tps >= 49.9 && tps <= 50.1, "Should be ~50 tok/s");
    
    PASS();
}

// ============================================================================
// Memory Management Tests
// ============================================================================

TEST(safe_malloc_success) {
    error_context_t ctx;
    error_context_clear(&ctx);
    
    void* ptr = safe_malloc(100, &ctx);
    
    ASSERT_NOT_NULL(ptr, "Should allocate");
    ASSERT_TRUE(!error_context_has_error(&ctx), "Should not have error");
    
    free(ptr);
    PASS();
}

TEST(safe_free_null_safe) {
    void* ptr = NULL;
    safe_free(&ptr);  // Should not crash
    PASS();
}

TEST(safe_strdup_success) {
    error_context_t ctx;
    error_context_clear(&ctx);
    
    char* dup = safe_strdup("test", &ctx);
    
    ASSERT_NOT_NULL(dup, "Should duplicate");
    ASSERT_STR_EQ("test", dup, "Content should match");
    ASSERT_TRUE(!error_context_has_error(&ctx), "Should not have error");
    
    free(dup);
    PASS();
}

// ============================================================================
// Test Runner
// ============================================================================

int main(void) {
    printf("========================================\n");
    printf("Governor Helpers Unit Test Suite\n");
    printf("========================================\n");

    // Buffer tests
    printf("\n=== Buffer Management ===\n");
    run_test_string_buffer_init_default();
    run_test_string_buffer_init_custom_size();
    run_test_string_buffer_append_simple();
    run_test_string_buffer_append_multiple();
    run_test_string_buffer_grow_automatically();
    run_test_string_buffer_clear_preserves_capacity();
    run_test_string_buffer_transfer_ownership();

    // Token tests
    printf("\n=== Token Processing ===\n");
    run_test_clean_token_text_normal();
    run_test_clean_token_text_filters_special_markers();

    // KV cache tests
    printf("\n=== KV Cache Management ===\n");
    run_test_kv_cache_has_room_check();
    run_test_kv_cache_invalid_state();

    // Sequence detection tests
    printf("\n=== Sequence Detection ===\n");
    run_test_check_stop_sequence_no_match();
    run_test_check_stop_sequence_partial_match();
    run_test_check_stop_sequence_complete_match();
    run_test_check_tool_call_xml_attribute();
    run_test_check_tool_call_json_in_xml();
    run_test_extract_tool_name_xml();
    run_test_extract_tool_name_json();

    // Error handling tests
    printf("\n=== Error Handling ===\n");
    run_test_error_context_set_and_get();
    run_test_error_context_clear();

    // Statistics tests
    printf("\n=== Statistics ===\n");
    run_test_stats_initialization();
    run_test_stats_record_tokens();
    run_test_stats_tokens_per_second();

    // Memory tests
    printf("\n=== Memory Management ===\n");
    run_test_safe_malloc_success();
    run_test_safe_free_null_safe();
    run_test_safe_strdup_success();

    // Summary
    printf("\n========================================\n");
    printf("Test Results\n");
    printf("========================================\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: %d (%.1f%%)\n", tests_passed, 100.0 * tests_passed / tests_run);
    printf("Failed: %d (%.1f%%)\n", tests_failed, 100.0 * tests_failed / tests_run);
    printf("========================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
