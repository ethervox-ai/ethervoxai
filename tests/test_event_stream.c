/**
 * @file test_event_stream.c
 * @brief Tests for event stream and UTF-8 validation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/event_stream.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_LOG(...) printf("[TEST] " __VA_ARGS__); printf("\n")
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); return 1; } } while(0)

// ============================================================================
// UTF-8 Validation Tests
// ============================================================================

static int test_validate_null(void) {
    TEST_LOG("Testing validate with NULL");
    
    size_t valid_len = 999;
    bool result = ethervox_validate_utf8(NULL, &valid_len);
    
    CHECK(result == true, "NULL should be valid");
    CHECK(valid_len == 0, "NULL should have length 0");
    
    TEST_LOG("✓ Validate NULL");
    return 0;
}

static int test_validate_ascii(void) {
    TEST_LOG("Testing validate with ASCII");
    
    const char* ascii = "Hello, world!";
    size_t valid_len = 0;
    bool result = ethervox_validate_utf8(ascii, &valid_len);
    
    CHECK(result == true, "ASCII should be valid");
    CHECK(valid_len == strlen(ascii), "Full string should be valid");
    
    TEST_LOG("✓ Validate ASCII");
    return 0;
}

static int test_validate_emoji(void) {
    TEST_LOG("Testing validate with emoji");
    
    // "Hello 👋" - 4-byte emoji
    const char* emoji = "Hello \xF0\x9F\x91\x8B";
    size_t valid_len = 0;
    bool result = ethervox_validate_utf8(emoji, &valid_len);
    
    CHECK(result == true, "Complete emoji should be valid");
    CHECK(valid_len == strlen(emoji), "Full string should be valid");
    
    TEST_LOG("✓ Validate complete emoji");
    return 0;
}

static int test_validate_incomplete_2byte(void) {
    TEST_LOG("Testing validate with incomplete 2-byte sequence");
    
    // "Hello £" where £ is incomplete (missing second byte)
    char incomplete[10];
    strcpy(incomplete, "Hello ");
    incomplete[6] = (char)0xC2;  // Start of 2-byte sequence
    incomplete[7] = '\0';
    
    size_t valid_len = 0;
    bool result = ethervox_validate_utf8(incomplete, &valid_len);
    
    CHECK(result == false, "Incomplete sequence should be invalid");
    CHECK(valid_len == 6, "Valid length should be up to incomplete byte");
    
    TEST_LOG("✓ Validate incomplete 2-byte");
    return 0;
}

static int test_validate_incomplete_3byte(void) {
    TEST_LOG("Testing validate with incomplete 3-byte sequence");
    
    // "Hi €" where € is incomplete (missing last byte)
    char incomplete[10];
    strcpy(incomplete, "Hi ");
    incomplete[3] = (char)0xE2;  // Start of 3-byte sequence (€)
    incomplete[4] = (char)0x82;  // Second byte
    incomplete[5] = '\0';        // Missing third byte
    
    size_t valid_len = 0;
    bool result = ethervox_validate_utf8(incomplete, &valid_len);
    
    CHECK(result == false, "Incomplete 3-byte should be invalid");
    CHECK(valid_len == 3, "Valid length should stop before incomplete sequence");
    
    TEST_LOG("✓ Validate incomplete 3-byte");
    return 0;
}

static int test_validate_incomplete_4byte(void) {
    TEST_LOG("Testing validate with incomplete 4-byte emoji");
    
    // "Test 👋" where emoji is incomplete (missing last byte)
    char incomplete[15];
    strcpy(incomplete, "Test ");
    incomplete[5] = (char)0xF0;  // Start of 4-byte sequence
    incomplete[6] = (char)0x9F;  // Second byte
    incomplete[7] = (char)0x91;  // Third byte
    incomplete[8] = '\0';        // Missing fourth byte
    
    size_t valid_len = 0;
    bool result = ethervox_validate_utf8(incomplete, &valid_len);
    
    CHECK(result == false, "Incomplete emoji should be invalid");
    CHECK(valid_len == 5, "Valid length should stop before incomplete emoji");
    
    TEST_LOG("✓ Validate incomplete 4-byte emoji");
    return 0;
}

static int test_validate_invalid_continuation(void) {
    TEST_LOG("Testing validate with invalid continuation byte");
    
    // 2-byte sequence with invalid second byte
    char invalid[10];
    strcpy(invalid, "Hi ");
    invalid[3] = (char)0xC2;  // Start of 2-byte
    invalid[4] = (char)0x20;  // Invalid continuation (should be 10xxxxxx)
    invalid[5] = '\0';
    
    size_t valid_len = 0;
    bool result = ethervox_validate_utf8(invalid, &valid_len);
    
    CHECK(result == false, "Invalid continuation should be invalid");
    CHECK(valid_len == 3, "Valid length should stop before invalid byte");
    
    TEST_LOG("✓ Validate invalid continuation");
    return 0;
}

static int test_validate_mixed_content(void) {
    TEST_LOG("Testing validate with mixed ASCII, 2-byte, 3-byte, 4-byte");
    
    // "Hello £€👋!" - ASCII, 2-byte, 3-byte, 4-byte, ASCII
    const char* mixed = "Hello \xC2\xA3\xE2\x82\xAC\xF0\x9F\x91\x8B!";
    size_t valid_len = 0;
    bool result = ethervox_validate_utf8(mixed, &valid_len);
    
    CHECK(result == true, "Mixed content should be valid");
    CHECK(valid_len == strlen(mixed), "Full string should be valid");
    
    TEST_LOG("✓ Validate mixed content");
    return 0;
}

// ============================================================================
// Safe String Creation Tests
// ============================================================================

static int test_create_safe_null(void) {
    TEST_LOG("Testing create_safe with NULL");
    
    size_t len = 999;
    char* result = ethervox_create_safe_utf8(NULL, &len);
    
    CHECK(result == NULL, "NULL input should return NULL");
    CHECK(len == 0, "Length should be 0");
    
    TEST_LOG("✓ Create safe NULL");
    return 0;
}

static int test_create_safe_valid(void) {
    TEST_LOG("Testing create_safe with valid string");
    
    const char* input = "Hello, world!";
    size_t len = 0;
    char* result = ethervox_create_safe_utf8(input, &len);
    
    CHECK(result != NULL, "Valid input should return non-NULL");
    CHECK(strcmp(result, input) == 0, "Result should match input");
    CHECK(len == strlen(input), "Length should match");
    
    free(result);
    TEST_LOG("✓ Create safe valid string");
    return 0;
}

static int test_create_safe_truncate_emoji(void) {
    TEST_LOG("Testing create_safe truncates incomplete emoji");
    
    // "Test 👋" with incomplete emoji
    char incomplete[15];
    strcpy(incomplete, "Test ");
    incomplete[5] = (char)0xF0;  // Start of emoji
    incomplete[6] = (char)0x9F;
    incomplete[7] = (char)0x91;
    incomplete[8] = '\0';        // Incomplete
    
    size_t len = 0;
    char* result = ethervox_create_safe_utf8(incomplete, &len);
    
    CHECK(result != NULL, "Should return truncated string");
    CHECK(strcmp(result, "Test ") == 0, "Should truncate to 'Test '");
    CHECK(len == 5, "Length should be 5");
    
    free(result);
    TEST_LOG("✓ Create safe truncates emoji");
    return 0;
}

static int test_create_safe_completely_invalid(void) {
    TEST_LOG("Testing create_safe with completely invalid UTF-8");
    
    // String starting with invalid byte
    char invalid[10];
    invalid[0] = (char)0xFF;  // Invalid start byte
    invalid[1] = (char)0xFE;
    invalid[2] = '\0';
    
    size_t len = 999;
    char* result = ethervox_create_safe_utf8(invalid, &len);
    
    CHECK(result == NULL, "Completely invalid should return NULL");
    CHECK(len == 0, "Length should be 0");
    
    TEST_LOG("✓ Create safe completely invalid");
    return 0;
}

// ============================================================================
// Event Structure Tests
// ============================================================================

static int test_event_sizes(void) {
    TEST_LOG("Testing event structure sizes");
    
    // Verify event structure is not too large
    size_t event_size = sizeof(ethervox_event_t);
    CHECK(event_size < 256, "Event structure should be reasonably sized");
    
    TEST_LOG("Event size: %zu bytes", event_size);
    TEST_LOG("✓ Event sizes");
    return 0;
}

static int test_event_token(void) {
    TEST_LOG("Testing token event");
    
    ethervox_event_t event;
    event.type = ETHERVOX_EVENT_TOKEN;
    event.token.text = "Hello";
    event.token.token_id = 12345;
    
    CHECK(event.type == ETHERVOX_EVENT_TOKEN, "Type should be TOKEN");
    CHECK(strcmp(event.token.text, "Hello") == 0, "Text should match");
    CHECK(event.token.token_id == 12345, "Token ID should match");
    
    TEST_LOG("✓ Token event");
    return 0;
}

static int test_event_finished(void) {
    TEST_LOG("Testing finished event");
    
    ethervox_event_t event;
    event.type = ETHERVOX_EVENT_FINISHED;
    event.finished.finish_reason = "stop";
    
    CHECK(event.type == ETHERVOX_EVENT_FINISHED, "Type should be FINISHED");
    CHECK(strcmp(event.finished.finish_reason, "stop") == 0, "Reason should match");
    
    TEST_LOG("✓ Finished event");
    return 0;
}

static int test_event_usage(void) {
    TEST_LOG("Testing usage event");
    
    ethervox_event_t event;
    event.type = ETHERVOX_EVENT_USAGE;
    event.usage.prompt_tokens = 100;
    event.usage.completion_tokens = 50;
    event.usage.total_tokens = 150;
    event.usage.elapsed_ms = 1234.5;
    
    CHECK(event.type == ETHERVOX_EVENT_USAGE, "Type should be USAGE");
    CHECK(event.usage.prompt_tokens == 100, "Prompt tokens should match");
    CHECK(event.usage.completion_tokens == 50, "Completion tokens should match");
    CHECK(event.usage.total_tokens == 150, "Total tokens should match");
    CHECK(event.usage.elapsed_ms > 1234.0 && event.usage.elapsed_ms < 1235.0, "Elapsed should match");
    
    TEST_LOG("✓ Usage event");
    return 0;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    printf("====== Event Stream Tests ======\n");
    
    int failed = 0;
    
    // UTF-8 validation tests
    failed += test_validate_null();
    failed += test_validate_ascii();
    failed += test_validate_emoji();
    failed += test_validate_incomplete_2byte();
    failed += test_validate_incomplete_3byte();
    failed += test_validate_incomplete_4byte();
    failed += test_validate_invalid_continuation();
    failed += test_validate_mixed_content();
    
    // Safe string creation tests
    failed += test_create_safe_null();
    failed += test_create_safe_valid();
    failed += test_create_safe_truncate_emoji();
    failed += test_create_safe_completely_invalid();
    
    // Event structure tests
    failed += test_event_sizes();
    failed += test_event_token();
    failed += test_event_finished();
    failed += test_event_usage();
    
    printf("\n");
    if (failed == 0) {
        printf("====== All Tests Passed ======\n");
        return 0;
    } else {
        printf("====== %d Tests Failed ======\n", failed);
        return 1;
    }
}
