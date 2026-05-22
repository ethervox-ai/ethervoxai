/**
 * @file test_tool_manifest.c
 * @brief Unit tests for Tool Manifest System
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/tool_manifest.h"
#include "ethervox/error.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test CRC32 implementation
void test_crc32(void) {
    printf("[TEST] CRC32 checksum... ");
    
    const char* test_data = "Hello, World!";
    uint32_t crc = ethervox_tool_crc32((const uint8_t*)test_data, strlen(test_data));
    
    // Known CRC32 for "Hello, World!" = 0xEC4AC3D0
    assert(crc == 0xEC4AC3D0);
    
    printf("✓ PASS\n");
}

// Test minimal system prompt generation
void test_minimal_system_prompt(void) {
    printf("[TEST] Minimal system prompt generation... ");
    
    // Create a mock manifest registry
    tool_manifest_registry_t registry = {0};
    registry.tools_available = false;  // LLM-only mode
    registry.fallback_level = 2;
    
    char prompt[4096];
    int len = ethervox_tool_build_minimal_system_prompt(&registry, prompt, sizeof(prompt), 255);
    
    assert(len > 0);
    assert(len < 1000);  // Should be compact
    assert(strstr(prompt, "/help") != NULL);  // Should have basic commands
    assert(strstr(prompt, "/quit") != NULL);
    
    printf("✓ PASS (%d bytes)\n", len);
}

// Test fallback level names
void test_fallback_level_names(void) {
    printf("[TEST] Fallback level names... ");
    
    const char* level0 = ethervox_tool_fallback_level_name(0);
    const char* level1 = ethervox_tool_fallback_level_name(1);
    const char* level2 = ethervox_tool_fallback_level_name(2);
    const char* level3 = ethervox_tool_fallback_level_name(3);
    
    assert(strstr(level0, "Optimal") != NULL || strstr(level0, "JSON") != NULL);
    assert(strstr(level1, "Good") != NULL || strstr(level1, "Binary") != NULL);
    assert(strstr(level2, "LLM-only") != NULL);
    assert(strstr(level3, "Emergency") != NULL);
    
    printf("✓ PASS\n");
}

// Test manifest export/import roundtrip
void test_manifest_roundtrip(void) {
    printf("[TEST] Manifest export/import roundtrip... ");
    
    // Create a simple tool registry
    ethervox_tool_registry_t registry;
    ethervox_tool_registry_init(&registry, 4);
    
    // Add test tools
    ethervox_tool_t tool1 = {
        .name = "test_tool_1",
        .description = "First test tool",
        .execute = NULL,
        .is_deterministic = true,
        .requires_confirmation = false
    };
    
    ethervox_tool_t tool2 = {
        .name = "test_tool_2",
        .description = "Second test tool",
        .execute = NULL,
        .is_deterministic = false,
        .requires_confirmation = true
    };
    
    ethervox_tool_registry_add(&registry, &tool1);
    ethervox_tool_registry_add(&registry, &tool2);
    
    // Export to manifest
    const char* test_path = "/tmp/test_tools.bin";
    int export_result = ethervox_tool_registry_export_manifest(&registry, test_path);
    assert(export_result == 0);
    
    // Import manifest
    tool_manifest_registry_t manifest;
    int import_result = ethervox_tool_manifest_init(&manifest, test_path);
    assert(import_result == 0);
    assert(manifest.tools_available == true);
    assert(manifest.header.tool_count == 2);
    
    // Verify tool names
    const tool_index_entry_t* entry1 = ethervox_tool_get_index(&manifest, "test_tool_1");
    const tool_index_entry_t* entry2 = ethervox_tool_get_index(&manifest, "test_tool_2");
    
    assert(entry1 != NULL);
    assert(entry2 != NULL);
    assert(strcmp(entry1->name, "test_tool_1") == 0);
    assert(strcmp(entry2->name, "test_tool_2") == 0);
    
    // Cleanup
    ethervox_tool_manifest_cleanup(&manifest);
    ethervox_tool_registry_cleanup(&registry);
    remove(test_path);
    
    printf("✓ PASS\n");
}

// Test token count estimation
void test_token_count_estimation(void) {
    printf("[TEST] Token count estimation... ");
    
    // Create mock manifest with 31 tools (like production)
    tool_manifest_registry_t registry = {0};
    registry.tools_available = true;
    registry.fallback_level = 1;  // Binary one-liners
    
    // Allocate mock index
    registry.header.tool_count = 31;
    registry.index = (tool_index_entry_t*)calloc(31, sizeof(tool_index_entry_t));
    
    for (uint32_t i = 0; i < 31; i++) {
        snprintf(registry.index[i].name, TOOL_NAME_MAX, "tool_%u", i);
        snprintf(registry.index[i].one_line, TOOL_DESC_MAX, 
                "This is test tool number %u for demonstration", i);
        registry.index[i].enabled = 1;
        registry.index[i].priority = 5;
    }
    
    // Generate minimal prompt
    char prompt[8192];
    int len = ethervox_tool_build_minimal_system_prompt(&registry, prompt, sizeof(prompt), 255);
    
    // Estimate tokens (~0.75 tokens per word in English)
    int word_count = 0;
    bool in_word = false;
    for (int i = 0; i < len; i++) {
        if (prompt[i] == ' ' || prompt[i] == '\n' || prompt[i] == '\t') {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            word_count++;
        }
    }
    
    int estimated_tokens = (int)(word_count * 0.75);
    
    printf("✓ PASS (estimated %d tokens, goal: <600)\n", estimated_tokens);
    assert(estimated_tokens < 600);  // Should be much less than 15,000!
    
    free(registry.index);
}

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf(" Tool Manifest System - Unit Tests\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    test_crc32();
    test_fallback_level_names();
    test_minimal_system_prompt();
    test_token_count_estimation();
    test_manifest_roundtrip();
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf(" ✓ ALL TESTS PASSED\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    return ETHERVOX_SUCCESS;
}
