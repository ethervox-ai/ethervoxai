/**
 * @file test_chat_templates.c
 * @brief Golden tests for chat template formatting
 *
 * Verifies that all supported chat templates produce correct output
 * byte-for-byte against expected formats from official specifications.
 *
 * Tests GRANITE_3, GRANITE_4, CHATML (Qwen), and LLAMA3 templates
 * with system messages, user messages, assistant messages, tool results,
 * and multi-turn conversations.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/chat_template.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s\n", msg); \
        return 1; \
    } \
} while(0)

#define BUFFER_SIZE 4096

// ============================================================================
// GRANITE_3 Template Tests
// ============================================================================

static int test_granite3_system() {
    printf("--- Test: GRANITE_3 System Message ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_GRANITE_3, NULL);
    CHECK(tmpl != NULL, "Template should not be NULL");
    CHECK(tmpl->type == CHAT_TEMPLATE_GRANITE_3, "Template type should be GRANITE_3");
    
    char output[BUFFER_SIZE];
    ethervox_result_t result = chat_template_format_system(
        tmpl, "You are a helpful assistant.", output, sizeof(output));
    
    CHECK(result == ETHERVOX_SUCCESS, "Format should succeed");
    
    const char* expected = "<|start_of_role|>system<|end_of_role|>You are a helpful assistant.<|end_of_text|>\n";
    CHECK(strcmp(output, expected) == 0, "Output should match expected format");
    
    printf("PASS: GRANITE_3 system message\n");
    printf("Output: %s", output);
    return 0;
}

static int test_granite3_user() {
    printf("\n--- Test: GRANITE_3 User Message ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_GRANITE_3, NULL);
    char output[BUFFER_SIZE];
    
    ethervox_result_t result = chat_template_format_user(
        tmpl, "What is 2+2?", output, sizeof(output));
    
    CHECK(result == ETHERVOX_SUCCESS, "Format should succeed");
    
    const char* expected = "<|start_of_role|>user<|end_of_role|>What is 2+2?<|end_of_text|>\n";
    CHECK(strcmp(output, expected) == 0, "Output should match expected format");
    
    printf("PASS: GRANITE_3 user message\n");
    printf("Output: %s", output);
    return 0;
}

static int test_granite3_assistant_start() {
    printf("\n--- Test: GRANITE_3 Assistant Start ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_GRANITE_3, NULL);
    char output[BUFFER_SIZE];
    
    ethervox_result_t result = chat_template_format_assistant_start(tmpl, output, sizeof(output));
    
    CHECK(result == ETHERVOX_SUCCESS, "Format should succeed");
    
    const char* expected = "<|start_of_role|>assistant<|end_of_role|>";
    CHECK(strcmp(output, expected) == 0, "Output should match expected format");
    
    printf("PASS: GRANITE_3 assistant start\n");
    printf("Output: %s\n", output);
    return 0;
}

static int test_granite3_tool_result() {
    printf("\n--- Test: GRANITE_3 Tool Result ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_GRANITE_3, NULL);
    char output[BUFFER_SIZE];
    
    const char* tool_result = "{\"result\": 4}";
    ethervox_result_t result = chat_template_format_tool_result(tmpl, tool_result, output, sizeof(output));
    
    CHECK(result == ETHERVOX_SUCCESS, "Format should succeed");
    
    const char* expected = "<|start_of_role|>user<|end_of_role|><tool_result>{\"result\": 4}</tool_result><|end_of_text|>\n<|start_of_role|>assistant<|end_of_role|>";
    CHECK(strcmp(output, expected) == 0, "Output should match expected format");
    
    printf("PASS: GRANITE_3 tool result\n");
    printf("Output: %s", output);
    return 0;
}

// ============================================================================
// GRANITE_4 Template Tests
// ============================================================================

static int test_granite4_system() {
    printf("\n--- Test: GRANITE_4 System Message ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_GRANITE_4, NULL);
    CHECK(tmpl != NULL, "Template should not be NULL");
    CHECK(tmpl->type == CHAT_TEMPLATE_GRANITE_4, "Template type should be GRANITE_4");
    
    char output[BUFFER_SIZE];
    ethervox_result_t result = chat_template_format_system(
        tmpl, "You are a helpful assistant.", output, sizeof(output));
    
    CHECK(result == ETHERVOX_SUCCESS, "Format should succeed");
    
    const char* expected = "<|start_of_role|>system<|end_of_role|>You are a helpful assistant.<|end_of_text|>\n";
    CHECK(strcmp(output, expected) == 0, "Output should match expected format");
    
    printf("PASS: GRANITE_4 system message\n");
    printf("Output: %s", output);
    return 0;
}

static int test_granite4_conversation() {
    printf("\n--- Test: GRANITE_4 Multi-turn Conversation ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_GRANITE, NULL);  // Test backward compat alias
    CHECK(tmpl->type == CHAT_TEMPLATE_GRANITE_4, "GRANITE alias should map to GRANITE_4");
    
    char output[BUFFER_SIZE];
    char* pos = output;
    size_t remaining = sizeof(output);
    
    // System message
    ethervox_result_t result = chat_template_format_system(
        tmpl, "You are a helpful assistant.", pos, remaining);
    CHECK(result >= 0, "System format should succeed");
    size_t len = strlen(pos);
    pos += len;
    remaining -= len;
    
    // User message
    result = chat_template_format_user(tmpl, "What is the capital of France?", pos, remaining);
    CHECK(result >= 0, "User format should succeed");
    len = strlen(pos);
    pos += len;
    remaining -= len;
    
    // Assistant start
    result = chat_template_format_assistant_start(tmpl, pos, remaining);
    CHECK(result >= 0, "Assistant start should succeed");
    len = strlen(pos);
    pos += len;
    remaining -= len;
    
    // Simulated assistant response
    const char* assistant_msg = "The capital of France is Paris.<|end_of_text|>\n";
    strncpy(pos, assistant_msg, remaining);
    pos += strlen(assistant_msg);
    remaining -= strlen(assistant_msg);
    
    const char* expected = 
        "<|start_of_role|>system<|end_of_role|>You are a helpful assistant.<|end_of_text|>\n"
        "<|start_of_role|>user<|end_of_role|>What is the capital of France?<|end_of_text|>\n"
        "<|start_of_role|>assistant<|end_of_role|>The capital of France is Paris.<|end_of_text|>\n";
    
    CHECK(strcmp(output, expected) == 0, "Conversation should match expected format");
    
    printf("PASS: GRANITE_4 multi-turn conversation\n");
    printf("Output:\n%s", output);
    return 0;
}

// ============================================================================
// CHATML/Qwen Template Tests
// ============================================================================

static int test_chatml_system() {
    printf("\n--- Test: CHATML System Message ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_CHATML, NULL);
    CHECK(tmpl != NULL, "Template should not be NULL");
    // CHATML is an alias for QWEN
    CHECK(tmpl->type == CHAT_TEMPLATE_QWEN || tmpl->type == CHAT_TEMPLATE_CHATML, 
          "Template type should be QWEN/CHATML");
    
    char output[BUFFER_SIZE];
    ethervox_result_t result = chat_template_format_system(
        tmpl, "You are a helpful assistant.", output, sizeof(output));
    
    CHECK(result == ETHERVOX_SUCCESS, "Format should succeed");
    
    const char* expected = "<|im_start|>system\nYou are a helpful assistant.<|im_end|>\n";
    CHECK(strcmp(output, expected) == 0, "Output should match expected ChatML format");
    
    printf("PASS: CHATML system message\n");
    printf("Output: %s", output);
    return 0;
}

static int test_chatml_conversation() {
    printf("\n--- Test: CHATML Multi-turn Conversation ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_QWEN, NULL);
    
    char output[BUFFER_SIZE];
    char* pos = output;
    size_t remaining = sizeof(output);
    
    // System message
    ethervox_result_t result = chat_template_format_system(
        tmpl, "You are a helpful assistant.", pos, remaining);
    CHECK(result >= 0, "System format should succeed");
    size_t len = strlen(pos);
    pos += len;
    remaining -= len;
    
    // User message
    result = chat_template_format_user(tmpl, "Hello!", pos, remaining);
    CHECK(result >= 0, "User format should succeed");
    len = strlen(pos);
    pos += len;
    remaining -= len;
    
    // Assistant start
    result = chat_template_format_assistant_start(tmpl, pos, remaining);
    CHECK(result >= 0, "Assistant start should succeed");
    len = strlen(pos);
    pos += len;
    remaining -= len;
    
    // Simulated assistant response
    const char* assistant_msg = "Hi! How can I help you today?<|im_end|>\n";
    strncpy(pos, assistant_msg, remaining);
    
    const char* expected = 
        "<|im_start|>system\nYou are a helpful assistant.<|im_end|>\n"
        "<|im_start|>user\nHello!<|im_end|>\n"
        "<|im_start|>assistant\nHi! How can I help you today?<|im_end|>\n";
    
    CHECK(strcmp(output, expected) == 0, "Conversation should match expected ChatML format");
    
    printf("PASS: CHATML multi-turn conversation\n");
    printf("Output:\n%s", output);
    return 0;
}

static int test_chatml_tool_result() {
    printf("\n--- Test: CHATML Tool Result ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_CHATML, NULL);
    char output[BUFFER_SIZE];
    
    const char* tool_result = "<calculator_result>4</calculator_result>";
    ethervox_result_t result = chat_template_format_tool_result(tmpl, tool_result, output, sizeof(output));
    
    CHECK(result == ETHERVOX_SUCCESS, "Format should succeed");
    
    const char* expected = "<|im_start|>user\n<tool_result><calculator_result>4</calculator_result></tool_result><|im_end|>\n<|im_start|>assistant\n";
    CHECK(strcmp(output, expected) == 0, "Output should match expected format");
    
    printf("PASS: CHATML tool result\n");
    printf("Output: %s", output);
    return 0;
}

// ============================================================================
// LLAMA3 Template Tests
// ============================================================================

static int test_llama3_system() {
    printf("\n--- Test: LLAMA3 System Message ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_LLAMA3, NULL);
    CHECK(tmpl != NULL, "Template should not be NULL");
    CHECK(tmpl->type == CHAT_TEMPLATE_LLAMA3, "Template type should be LLAMA3");
    
    char output[BUFFER_SIZE];
    ethervox_result_t result = chat_template_format_system(
        tmpl, "You are a helpful assistant.", output, sizeof(output));
    
    CHECK(result == ETHERVOX_SUCCESS, "Format should succeed");
    
    const char* expected = "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\nYou are a helpful assistant.<|eot_id|>";
    CHECK(strcmp(output, expected) == 0, "Output should match expected Llama3 format");
    
    printf("PASS: LLAMA3 system message\n");
    printf("Output: %s\n", output);
    return 0;
}

static int test_llama3_conversation() {
    printf("\n--- Test: LLAMA3 Multi-turn Conversation ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_LLAMA3, NULL);
    
    char output[BUFFER_SIZE];
    char* pos = output;
    size_t remaining = sizeof(output);
    
    // System message
    ethervox_result_t result = chat_template_format_system(
        tmpl, "You are a helpful assistant.", pos, remaining);
    CHECK(result >= 0, "System format should succeed");
    size_t len = strlen(pos);
    pos += len;
    remaining -= len;
    
    // User message
    result = chat_template_format_user(tmpl, "What is AI?", pos, remaining);
    CHECK(result >= 0, "User format should succeed");
    len = strlen(pos);
    pos += len;
    remaining -= len;
    
    // Assistant start
    result = chat_template_format_assistant_start(tmpl, pos, remaining);
    CHECK(result >= 0, "Assistant start should succeed");
    len = strlen(pos);
    pos += len;
    remaining -= len;
    
    // Simulated assistant response
    const char* assistant_msg = "AI stands for Artificial Intelligence.<|eot_id|>";
    strncpy(pos, assistant_msg, remaining);
    
    const char* expected = 
        "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\nYou are a helpful assistant.<|eot_id|>"
        "<|start_header_id|>user<|end_header_id|>\n\nWhat is AI?<|eot_id|>"
        "<|start_header_id|>assistant<|end_header_id|>\n\nAI stands for Artificial Intelligence.<|eot_id|>";
    
    CHECK(strcmp(output, expected) == 0, "Conversation should match expected Llama3 format");
    
    printf("PASS: LLAMA3 multi-turn conversation\n");
    printf("Output:\n%s\n", output);
    return 0;
}

static int test_llama3_tool_result() {
    printf("\n--- Test: LLAMA3 Tool Result ---\n");
    
    const chat_template_t* tmpl = chat_template_get(CHAT_TEMPLATE_LLAMA3, NULL);
    char output[BUFFER_SIZE];
    
    const char* tool_result = "{\"temperature\": 72, \"condition\": \"sunny\"}";
    ethervox_result_t result = chat_template_format_tool_result(tmpl, tool_result, output, sizeof(output));
    
    CHECK(result == ETHERVOX_SUCCESS, "Format should succeed");
    
    const char* expected = "<|start_header_id|>user<|end_header_id|>\n\n<tool_result>{\"temperature\": 72, \"condition\": \"sunny\"}</tool_result><|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n";
    CHECK(strcmp(output, expected) == 0, "Output should match expected format");
    
    printf("PASS: LLAMA3 tool result\n");
    printf("Output: %s\n", output);
    return 0;
}

// ============================================================================
// Stop Sequence Tests
// ============================================================================

static int test_stop_sequences() {
    printf("\n--- Test: Stop Sequences ---\n");
    
    // Test GRANITE_3 stop sequences
    const chat_template_t* granite3 = chat_template_get(CHAT_TEMPLATE_GRANITE_3, NULL);
    CHECK(chat_template_has_stop_sequence(granite3, "Some text<|end_of_text|>"), 
          "Should detect GRANITE_3 stop sequence");
    CHECK(!chat_template_has_stop_sequence(granite3, "Normal text without stops"), 
          "Should not detect stop in normal text");
    
    // Test CHATML stop sequences
    const chat_template_t* chatml = chat_template_get(CHAT_TEMPLATE_CHATML, NULL);
    CHECK(chat_template_has_stop_sequence(chatml, "Response<|im_end|>"), 
          "Should detect ChatML stop sequence");
    
    // Test LLAMA3 stop sequences
    const chat_template_t* llama3 = chat_template_get(CHAT_TEMPLATE_LLAMA3, NULL);
    CHECK(chat_template_has_stop_sequence(llama3, "Response<|eot_id|>"), 
          "Should detect Llama3 stop sequence");
    
    printf("PASS: Stop sequence detection\n");
    return 0;
}

// ============================================================================
// Tool Format Tests
// ============================================================================

static int test_tool_formats() {
    printf("\n--- Test: Tool Formats ---\n");
    
    const chat_template_t* granite3 = chat_template_get(CHAT_TEMPLATE_GRANITE_3, NULL);
    CHECK(chat_template_get_tool_format(granite3) == TOOL_FORMAT_JSON_IN_XML,
          "GRANITE_3 should use JSON_IN_XML format");
    
    const chat_template_t* granite4 = chat_template_get(CHAT_TEMPLATE_GRANITE_4, NULL);
    CHECK(chat_template_get_tool_format(granite4) == TOOL_FORMAT_JSON_IN_XML,
          "GRANITE_4 should use JSON_IN_XML format");
    
    const chat_template_t* chatml = chat_template_get(CHAT_TEMPLATE_CHATML, NULL);
    CHECK(chat_template_get_tool_format(chatml) == TOOL_FORMAT_XML_ATTR,
          "CHATML should use XML_ATTR format");
    
    const chat_template_t* llama3 = chat_template_get(CHAT_TEMPLATE_LLAMA3, NULL);
    CHECK(chat_template_get_tool_format(llama3) == TOOL_FORMAT_XML_ATTR,
          "LLAMA3 should use XML_ATTR format");
    
    printf("PASS: Tool format detection\n");
    return 0;
}

// ============================================================================
// Auto-detection Tests
// ============================================================================

static int test_auto_detection() {
    printf("\n--- Test: Template Auto-detection ---\n");
    
    chat_template_type_t type;
    
    // Test Granite 3 detection
    type = chat_template_detect("models/granite-3.0-8b-instruct.gguf");
    CHECK(type == CHAT_TEMPLATE_GRANITE_3, "Should detect GRANITE_3 from filename");
    
    // Test Granite 4 detection
    type = chat_template_detect("models/granite-4.0-1b.gguf");
    CHECK(type == CHAT_TEMPLATE_GRANITE_4, "Should detect GRANITE_4 from filename");
    
    type = chat_template_detect("models/granite-8b.gguf");  // Unversioned defaults to 4
    CHECK(type == CHAT_TEMPLATE_GRANITE_4, "Unversioned granite should default to GRANITE_4");
    
    // Test Qwen/ChatML detection
    type = chat_template_detect("models/qwen2.5-7b.gguf");
    CHECK(type == CHAT_TEMPLATE_QWEN, "Should detect QWEN from filename");
    
    // Test Llama3 detection
    type = chat_template_detect("models/llama-3-8b.gguf");
    CHECK(type == CHAT_TEMPLATE_LLAMA3, "Should detect LLAMA3 from filename");
    
    printf("PASS: Template auto-detection\n");
    return 0;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    printf("=== Chat Template Golden Tests ===\n\n");
    
    int result = 0;
    
    // GRANITE_3 tests
    result |= test_granite3_system();
    result |= test_granite3_user();
    result |= test_granite3_assistant_start();
    result |= test_granite3_tool_result();
    
    // GRANITE_4 tests
    result |= test_granite4_system();
    result |= test_granite4_conversation();
    
    // CHATML/Qwen tests
    result |= test_chatml_system();
    result |= test_chatml_conversation();
    result |= test_chatml_tool_result();
    
    // LLAMA3 tests
    result |= test_llama3_system();
    result |= test_llama3_conversation();
    result |= test_llama3_tool_result();
    
    // Cross-cutting tests
    result |= test_stop_sequences();
    result |= test_tool_formats();
    result |= test_auto_detection();
    
    if (result == 0) {
        printf("\n✅ All chat template tests passed!\n");
    } else {
        printf("\n❌ Some tests failed\n");
    }
    
    return result;
}
