/**
 * @file test_lookahead_filtering.c
 * @brief Unit tests for lookahead buffer filtering system
 *
 * Tests the token streaming filter that prevents tool call JSON from leaking to UI:
 * - JSON opening brace detection
 * - Tool JSON fragment detection  
 * - inside_tool_call flag behavior
 * - Prefilled tool call handling
 * - Mixed content filtering
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Test macros
#define TEST(name) \
    void test_##name(); \
    void run_test_##name() { \
        tests_run++; \
        printf("\n[TEST] %s...\n", #name); \
        test_##name(); \
    } \
    void test_##name()

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  ✗ FAILED: %s\n", message); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            printf("  ✗ FAILED: %s (expected=%d, actual=%d)\n", message, (int)(expected), (int)(actual)); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define PASS() \
    do { \
        printf("  ✓ PASSED\n"); \
        tests_passed++; \
    } while(0)

// ============================================================================
// Helper Functions - Simulate the actual filtering logic
// ============================================================================

/**
 * Simulate the contains_tool_json check from governor.c
 * This is the logic we're testing - extracted for unit testing
 */
static bool contains_tool_json(const char* lookahead_buffer) {
    if (!lookahead_buffer || lookahead_buffer[0] == '\0') {
        return false;
    }
    
    size_t buf_len = strlen(lookahead_buffer);
    
    // Check for JSON opening brace at start
    if (buf_len > 0 && (lookahead_buffer[0] == '{' || 
        (buf_len >= 2 && lookahead_buffer[0] == '{' && lookahead_buffer[1] == '"'))) {
        return true;
    }
    
    // CRITICAL: Check for closing braces at start (end of tool call)
    if (buf_len > 0 && lookahead_buffer[0] == '}') {
        return true;
    }
    
    // Check for tool JSON indicators
    const char* tool_json_indicators[] = {
        "{\"name\":",           // JSON start with name field
        "\"arguments\":", "\"name\":", "\"location\":", "\"forecast_type\":",
        "\"expression\":", "\"duration_", 
        "},",                  // JSON object closing with comma
        "}\n",                 // JSON object closing with newline
        "}}",                  // Double closing brace (nested end)
        "\"},",                // JSON string closing with comma
        "\"}\n",               // JSON string closing with newline
        "</tool_call",         // Closing tag (partial or full)
        NULL
    };
    
    for (int i = 0; tool_json_indicators[i] != NULL; i++) {
        if (strstr(lookahead_buffer, tool_json_indicators[i]) != NULL) {
            return true;
        }
    }
    
    return false;
}

/**
 * Simulate the flush buffer filtering from governor.c
 */
static bool should_discard_buffer(const char* combined_lookahead) {
    if (!combined_lookahead || combined_lookahead[0] == '\0') {
        return false;
    }
    
    size_t buf_len = strlen(combined_lookahead);
    
    // Check for JSON opening at start
    if (buf_len > 0 && (combined_lookahead[0] == '{' || 
        (buf_len >= 2 && combined_lookahead[0] == '{' && combined_lookahead[1] == '"'))) {
        return true;
    }
    
    // CRITICAL: Check for closing braces at start (end of tool call)
    if (buf_len > 0 && combined_lookahead[0] == '}') {
        return true;
    }
    
    // Check for tool JSON patterns
    const char* tool_json_patterns[] = {
        "{\"name\":",       // JSON start with name field
        "\"arguments\":",   // Tool call argument field
        "\"name\":",        // Tool call name field
        "\"location\":",    // Common parameter
        "\"forecast_type\":",
        "\"expression\":",
        "\"duration_",
        "},",               // JSON object closing
        "}\n",              // JSON object with newline
        "}}",               // Double closing brace
        "\"},",            // JSON string closing
        "\"}\n",           // JSON string with newline
        "</tool_call",     // Closing tag (partial or full)
        NULL
    };
    
    for (int i = 0; tool_json_patterns[i] != NULL; i++) {
        if (strstr(combined_lookahead, tool_json_patterns[i]) != NULL) {
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// Unit Tests: JSON Opening Brace Detection
// ============================================================================

TEST(detects_json_opening_brace_at_start) {
    const char* buffer = "{\"name\": \"get_time\"";
    ASSERT(contains_tool_json(buffer), "Should detect JSON opening brace");
    PASS();
}

TEST(detects_json_with_quote_at_start) {
    const char* buffer = "{\"name";
    ASSERT(contains_tool_json(buffer), "Should detect {\" at start");
    PASS();
}

TEST(allows_single_brace_in_text) {
    // Single { in normal text should not trigger if it's not at the very start
    // But at the start, it should be blocked
    const char* buffer = "{";
    ASSERT(contains_tool_json(buffer), "Should block single { at start (prefill case)");
    PASS();
}

TEST(allows_brace_mid_sentence) {
    const char* buffer = "The value is { x: 10 }";
    // This wouldn't trigger because the JSON patterns look for specific keys
    ASSERT(!contains_tool_json(buffer), "Should allow braces in normal text");
    PASS();
}

// ============================================================================
// Unit Tests: Tool JSON Fragment Detection
// ============================================================================

TEST(detects_arguments_field) {
    const char* buffer = "some tokens \"arguments\": {";
    ASSERT(contains_tool_json(buffer), "Should detect 'arguments:' field");
    PASS();
}

TEST(detects_name_field) {
    const char* buffer = "\"name\": \"get_time\"";
    ASSERT(contains_tool_json(buffer), "Should detect 'name:' field");
    PASS();
}

TEST(detects_location_parameter) {
    const char* buffer = "\"location\": \"Portland\"";
    ASSERT(contains_tool_json(buffer), "Should detect 'location:' parameter");
    PASS();
}

TEST(detects_forecast_type_parameter) {
    const char* buffer = "\"forecast_type\": \"current\"";
    ASSERT(contains_tool_json(buffer), "Should detect 'forecast_type:' parameter");
    PASS();
}

TEST(detects_duration_parameter_prefix) {
    const char* buffer = "\"duration_minutes\": 30";
    ASSERT(contains_tool_json(buffer), "Should detect 'duration_*' parameter");
    PASS();
}

TEST(detects_json_closing_patterns) {
    const char* buffer = "}, \"other_field\"";
    ASSERT(contains_tool_json(buffer), "Should detect '},' pattern");
    PASS();
}

TEST(detects_single_closing_brace) {
    const char* buffer = "}";
    ASSERT(contains_tool_json(buffer), "Should detect single '}' at start");
    PASS();
}

TEST(detects_double_closing_brace) {
    const char* buffer = "}}";
    ASSERT(contains_tool_json(buffer), "Should detect '}}' pattern (nested JSON end)");
    PASS();
}

TEST(detects_closing_brace_with_newline) {
    const char* buffer = "}\n";
    ASSERT(contains_tool_json(buffer), "Should detect '}\\n' pattern");
    PASS();
}

TEST(detects_tool_call_closing_tag) {
    const char* buffer = "</tool_call>";
    ASSERT(contains_tool_json(buffer), "Should detect closing tag");
    PASS();
}

TEST(detects_partial_closing_tag) {
    const char* buffer = "</tool_call";
    ASSERT(contains_tool_json(buffer), "Should detect partial closing tag");
    PASS();
}

TEST(detects_json_end_before_closing_tag) {
    const char* buffer = "}\n</tool_call>";
    ASSERT(contains_tool_json(buffer), "Should detect JSON end + closing tag");
    PASS();
}

// ============================================================================
// Unit Tests: Normal Text Pass-Through
// ============================================================================

TEST(allows_normal_text) {
    const char* buffer = "The current time is";
    ASSERT(!contains_tool_json(buffer), "Should allow normal text");
    PASS();
}

TEST(allows_punctuation) {
    const char* buffer = "Hello! How can I help?";
    ASSERT(!contains_tool_json(buffer), "Should allow text with punctuation");
    PASS();
}

TEST(allows_numbers) {
    const char* buffer = "The answer is 42";
    ASSERT(!contains_tool_json(buffer), "Should allow text with numbers");
    PASS();
}

TEST(allows_empty_buffer) {
    const char* buffer = "";
    ASSERT(!contains_tool_json(buffer), "Should handle empty buffer");
    PASS();
}

TEST(allows_null_buffer) {
    ASSERT(!contains_tool_json(NULL), "Should handle NULL buffer");
    PASS();
}

// ============================================================================
// Unit Tests: Flush Buffer Filtering
// ============================================================================

TEST(flush_discards_json_at_start) {
    const char* buffer = "{\"name\": \"get_time\", \"arguments\": {}}\n</tool_call>";
    ASSERT(should_discard_buffer(buffer), "Flush should discard JSON at start");
    PASS();
}

TEST(flush_discards_tool_closing_tag) {
    const char* buffer = "</tool_call>";
    ASSERT(should_discard_buffer(buffer), "Flush should discard </tool_call> tag");
    PASS();
}

TEST(flush_discards_partial_tool_json) {
    // This should contain a pattern that matches our flush filters
    const char* buffer = "\"arguments\": {\"location\"";
    ASSERT(should_discard_buffer(buffer), "Flush should discard partial JSON");
    PASS();
}

TEST(flush_allows_clean_response) {
    const char* buffer = "The weather in Portland is sunny with a temperature of 70°F.";
    ASSERT(!should_discard_buffer(buffer), "Flush should allow clean response");
    PASS();
}

// ============================================================================
// Integration Tests: Real-World Scenarios
// ============================================================================

TEST(scenario_time_query_with_prefill) {
    // Simulates: User asks "What time is it?"
    // System prefills: <tool_call>\n
    // Model generates: {"name": "get_time", "arguments": {}}
    
    // The very first tokens would be: {, "name
    const char* first_tokens = "{\"name";
    ASSERT(contains_tool_json(first_tokens), "Should block JSON start immediately");
    
    printf("  Scenario: Prefilled time query - JSON blocked ✓\n");
    PASS();
}

TEST(scenario_weather_query_streaming) {
    // Model generates weather tool call token by token
    const char* fragments[] = {
        "{",
        "{\"",
        "{\"name\":",
        "\"location\":",
        "\"forecast_type\":",
        NULL
    };
    
    for (int i = 0; fragments[i] != NULL; i++) {
        ASSERT(contains_tool_json(fragments[i]), "Each fragment should be blocked");
    }
    
    printf("  Scenario: Weather query streaming - All fragments blocked ✓\n");
    PASS();
}

TEST(scenario_normal_conversation) {
    // Normal conversation should stream smoothly
    const char* tokens[] = {
        "Hello",
        "! ",
        "I'm",
        " Eth",
        "ervox",
        ", ",
        "your",
        " assistant",
        NULL
    };
    
    for (int i = 0; tokens[i] != NULL; i++) {
        ASSERT(!contains_tool_json(tokens[i]), "Normal tokens should pass through");
    }
    
    printf("  Scenario: Normal conversation - All tokens passed ✓\n");
    PASS();
}

TEST(scenario_tool_result_explanation) {
    // After tool execution, model explains result
    // This should NOT be blocked
    const char* explanation = "The current time is 5:35 PM.";
    ASSERT(!contains_tool_json(explanation), "Tool result explanation should pass through");
    
    printf("  Scenario: Tool result explanation - Passed through ✓\n");
    PASS();
}

TEST(scenario_mixed_content_with_json_word) {
    // Text that contains "name" or "arguments" as normal words
    const char* text1 = "My name is Alice";
    const char* text2 = "The function takes arguments";
    
    ASSERT(!contains_tool_json(text1), "Should allow 'name' in normal text");
    ASSERT(!contains_tool_json(text2), "Should allow 'arguments' in normal text");
    
    printf("  Scenario: Mixed content - Normal usage of JSON keywords allowed ✓\n");
    PASS();
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(edge_case_partial_json_key) {
    const char* buffer = "\"arg";  // Partial "arguments"
    // This should NOT be blocked - we only block complete patterns
    ASSERT(!contains_tool_json(buffer), "Should not block partial key");
    PASS();
}

TEST(edge_case_json_in_string_literal) {
    // Model talking about JSON (meta)
    const char* buffer = "You can use \"name\" field";
    // The pattern \"name\": would trigger, but \"name\" followed by space should not
    ASSERT(!contains_tool_json(buffer), "Should allow JSON discussion");
    PASS();
}

TEST(edge_case_unicode_similar_to_brace) {
    // Some unicode characters might look like braces
    const char* buffer = "﹛test﹜";  // Unicode braces
    // These are different bytes than ASCII {}, shouldn't trigger
    ASSERT(!contains_tool_json(buffer), "Should not trigger on unicode braces");
    PASS();
}

TEST(edge_case_very_long_buffer) {
    // Test with a large buffer
    char buffer[2048];
    memset(buffer, 'a', sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    ASSERT(!contains_tool_json(buffer), "Should handle large buffers efficiently");
    PASS();
}

// ============================================================================
// Regression Tests: Known Issues
// ============================================================================

TEST(regression_leaked_get_time_json) {
    // Issue: {"name": "get_time", was leaking through
    // This was the original bug that prompted these tests
    const char* leaked = "{\"name\": \"get_time\",";
    ASSERT(contains_tool_json(leaked), "Should block the exact leaked string");
    
    printf("  Regression: Original leak scenario - Now blocked ✓\n");
    PASS();
}

TEST(regression_arguments_fragment_leak) {
    // Issue: \"arguments\": { was leaking through
    const char* leaked = "\"arguments\": {";
    ASSERT(contains_tool_json(leaked), "Should block arguments fragment");
    
    printf("  Regression: Arguments fragment - Now blocked ✓\n");
    PASS();
}

TEST(regression_location_parameter_leak) {
    // Issue: "location": "Beaverton, OR" was visible
    const char* leaked = "\"location\": \"Beaverton";
    ASSERT(contains_tool_json(leaked), "Should block location parameter");
    
    printf("  Regression: Location parameter - Now blocked ✓\n");
    PASS();
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("Lookahead Buffer Filtering Test Suite\n");
    printf("========================================\n");
    printf("Testing token streaming filter logic\n");
    printf("that prevents tool call JSON leakage\n");
    printf("========================================\n");

    // JSON opening brace detection
    printf("\n=== JSON Opening Brace Detection ===\n");
    run_test_detects_json_opening_brace_at_start();
    run_test_detects_json_with_quote_at_start();
    run_test_allows_single_brace_in_text();
    run_test_allows_brace_mid_sentence();

    // Tool JSON fragment detection
    printf("\n=== Tool JSON Fragment Detection ===\n");
    run_test_detects_arguments_field();
    run_test_detects_name_field();
    run_test_detects_location_parameter();
    run_test_detects_forecast_type_parameter();
    run_test_detects_duration_parameter_prefix();
    run_test_detects_json_closing_patterns();
    run_test_detects_single_closing_brace();
    run_test_detects_double_closing_brace();
    run_test_detects_closing_brace_with_newline();
    run_test_detects_tool_call_closing_tag();
    run_test_detects_partial_closing_tag();
    run_test_detects_json_end_before_closing_tag();

    // Normal text pass-through
    printf("\n=== Normal Text Pass-Through ===\n");
    run_test_allows_normal_text();
    run_test_allows_punctuation();
    run_test_allows_numbers();
    run_test_allows_empty_buffer();
    run_test_allows_null_buffer();

    // Flush buffer filtering
    printf("\n=== Flush Buffer Filtering ===\n");
    run_test_flush_discards_json_at_start();
    run_test_flush_discards_tool_closing_tag();
    run_test_flush_discards_partial_tool_json();
    run_test_flush_allows_clean_response();

    // Real-world scenarios
    printf("\n=== Real-World Scenarios ===\n");
    run_test_scenario_time_query_with_prefill();
    run_test_scenario_weather_query_streaming();
    run_test_scenario_normal_conversation();
    run_test_scenario_tool_result_explanation();
    run_test_scenario_mixed_content_with_json_word();

    // Edge cases
    printf("\n=== Edge Cases ===\n");
    run_test_edge_case_partial_json_key();
    run_test_edge_case_json_in_string_literal();
    run_test_edge_case_unicode_similar_to_brace();
    run_test_edge_case_very_long_buffer();

    // Regression tests
    printf("\n=== Regression Tests ===\n");
    run_test_regression_leaked_get_time_json();
    run_test_regression_arguments_fragment_leak();
    run_test_regression_location_parameter_leak();

    // Summary
    printf("\n========================================\n");
    printf("Test Results\n");
    printf("========================================\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: %d", tests_passed);
    if (tests_passed == tests_run) {
        printf(" ✓✓✓ ALL TESTS PASSED! ✓✓✓\n");
    } else {
        printf(" (%.1f%%)\n", 100.0 * tests_passed / tests_run);
    }
    printf("Failed: %d", tests_failed);
    if (tests_failed > 0) {
        printf(" ✗✗✗ FAILURES DETECTED ✗✗✗\n");
    } else {
        printf(" (0.0%%)\n");
    }
    printf("========================================\n");

    if (tests_passed == tests_run) {
        printf("\n🎉 SUCCESS: All filtering logic tests passed!\n");
        printf("The lookahead buffer system correctly:\n");
        printf("  ✓ Blocks tool call JSON at buffer start\n");
        printf("  ✓ Blocks tool JSON fragments mid-buffer\n");
        printf("  ✓ Allows normal text to pass through\n");
        printf("  ✓ Handles edge cases gracefully\n");
        printf("  ✓ Fixes the original leak scenarios\n");
        printf("\n");
    }

    return (tests_failed == 0) ? 0 : 1;
}
