/**
 * @file test_secret_mode.c
 * @brief Unit tests for privacy/secret mode functionality
 *
 * Tests that secret mode prevents memory logging while maintaining
 * normal operation of the LLM and tool execution.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include "ethervox/error.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_privacy_mode_toggle(void) {
    printf("Testing privacy mode toggle...\n");
    
    // Initially should be disabled (normal mode)
    assert(ethervox_memory_get_privacy_mode() == false);
    
    // Enable secret mode
    ethervox_memory_set_privacy_mode(true);
    assert(ethervox_memory_get_privacy_mode() == true);
    
    // Disable secret mode
    ethervox_memory_set_privacy_mode(false);
    assert(ethervox_memory_get_privacy_mode() == false);
    
    printf("  ✓ Privacy mode toggle works\n");
}

void test_secret_mode_prevents_logging(void) {
    printf("Testing secret mode prevents memory logging...\n");
    
    ethervox_memory_store_t store;
    ethervox_memory_init(&store, "test-secret", "/tmp");
    
    // Store a memory in normal mode
    const char* tags[] = {"normal", "test"};
    uint64_t memory_id;
    
    ethervox_memory_set_privacy_mode(false);
    int result = ethervox_memory_store_add(&store, "Normal mode message",
                                          tags, 2, 0.8f, true, &memory_id);
    assert(result == 0);
    assert(store.entry_count == 1);
    
    // Enable secret mode
    ethervox_memory_set_privacy_mode(true);
    
    // Try to store a memory in secret mode
    result = ethervox_memory_store_add(&store, "Secret mode message",
                                      tags, 2, 0.8f, true, &memory_id);
    
    // Should succeed (to not disrupt LLM flow) but not actually store
    assert(result == 0);
    
    // Memory count should still be 1 (secret message not stored)
    assert(store.entry_count == 1);
    
    // Verify only the normal mode message is present
    const ethervox_memory_entry_t* entry;
    assert(ethervox_memory_get_by_id(&store, 0, &entry) == 0);
    assert(strcmp(entry->text, "Normal mode message") == 0);
    
    // Restore normal mode
    ethervox_memory_set_privacy_mode(false);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Secret mode prevents logging\n");
}

void test_secret_mode_state_restoration(void) {
    printf("Testing secret mode state restoration...\n");
    
    ethervox_memory_store_t store;
    ethervox_memory_init(&store, "test-restore", "/tmp");
    
    const char* tags[] = {"test"};
    uint64_t id;
    
    // Save original state (should be false)
    bool original_state = ethervox_memory_get_privacy_mode();
    assert(original_state == false);
    
    // Enable secret mode temporarily
    ethervox_memory_set_privacy_mode(true);
    
    // Add memory in secret mode (should not be stored)
    ethervox_memory_store_add(&store, "Secret message", tags, 1, 0.5f, true, &id);
    assert(store.entry_count == 0);
    
    // Restore original state
    ethervox_memory_set_privacy_mode(original_state);
    assert(ethervox_memory_get_privacy_mode() == false);
    
    // Now add in normal mode (should be stored)
    ethervox_memory_store_add(&store, "Normal message", tags, 1, 0.5f, true, &id);
    assert(store.entry_count == 1);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Secret mode state restoration works\n");
}

void test_secret_mode_search_behavior(void) {
    printf("Testing secret mode search behavior...\n");
    
    ethervox_memory_store_t store;
    ethervox_memory_init(&store, "test-search", "/tmp");
    
    const char* tags[] = {"search-test"};
    uint64_t id;
    
    // Add some memories in normal mode
    ethervox_memory_set_privacy_mode(false);
    ethervox_memory_store_add(&store, "Normal memory 1", tags, 1, 0.8f, true, &id);
    ethervox_memory_store_add(&store, "Normal memory 2", tags, 1, 0.7f, true, &id);
    assert(store.entry_count == 2);
    
    // Enable secret mode
    ethervox_memory_set_privacy_mode(true);
    
    // Search should still work in secret mode (reads existing memories)
    ethervox_memory_search_result_t* results = NULL;
    uint32_t count = 0;
    
    ethervox_memory_search(&store, "Normal", NULL, 0, 10, &results, &count);
    assert(count == 2);
    free(results);
    
    // Try to add a memory in secret mode
    ethervox_memory_store_add(&store, "Secret search test", tags, 1, 0.9f, true, &id);
    
    // Memory count should still be 2 (secret one not added)
    assert(store.entry_count == 2);
    
    // Search again - should only find the original 2
    ethervox_memory_search(&store, "memory", NULL, 0, 10, &results, &count);
    assert(count == 2);
    free(results);
    
    // Restore normal mode
    ethervox_memory_set_privacy_mode(false);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Secret mode search behavior correct\n");
}

void test_secret_mode_tool_execution(void) {
    printf("Testing secret mode with tool execution...\n");
    
    ethervox_memory_store_t store;
    ethervox_memory_init(&store, "test-tool", "/tmp");
    
    // Register memory tools
    void* registry = NULL;  // Placeholder - would be real registry in integration test
    
    // Enable secret mode
    ethervox_memory_set_privacy_mode(true);
    
    // Simulate tool execution in secret mode
    const char* tags[] = {"tool-test"};
    uint64_t id;
    
    int result = ethervox_memory_store_add(&store, "Tool result in secret mode",
                                          tags, 1, 0.5f, false, &id);
    
    // Should succeed (tool doesn't fail) but not actually store
    assert(result == 0);
    assert(store.entry_count == 0);
    
    // Disable secret mode
    ethervox_memory_set_privacy_mode(false);
    
    // Now tool execution should store
    result = ethervox_memory_store_add(&store, "Tool result in normal mode",
                                      tags, 1, 0.5f, false, &id);
    assert(result == 0);
    assert(store.entry_count == 1);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Secret mode tool execution works\n");
}

void test_secret_mode_multiple_toggles(void) {
    printf("Testing multiple secret mode toggles...\n");
    
    ethervox_memory_store_t store;
    ethervox_memory_init(&store, "test-toggles", "/tmp");
    
    const char* tags[] = {"toggle-test"};
    uint64_t id;
    
    // Test rapid toggling
    for (int i = 0; i < 10; i++) {
        bool enable = (i % 2 == 0);
        ethervox_memory_set_privacy_mode(enable);
        assert(ethervox_memory_get_privacy_mode() == enable);
        
        // Try to store
        char msg[64];
        snprintf(msg, sizeof(msg), "Message %d", i);
        ethervox_memory_store_add(&store, msg, tags, 1, 0.5f, true, &id);
    }
    
    // Should have 5 entries (odd iterations had secret mode off)
    assert(store.entry_count == 5);
    
    // Verify correct messages were stored (odd indices: 1, 3, 5, 7, 9)
    for (uint32_t i = 0; i < store.entry_count; i++) {
        const ethervox_memory_entry_t* entry;
        ethervox_memory_get_by_id(&store, i, &entry);
        
        // Extract number from "Message N"
        int msg_num = 0;
        sscanf(entry->text, "Message %d", &msg_num);
        
        // Should be odd numbers only
        assert(msg_num % 2 == 1);
    }
    
    // Ensure we're in normal mode at the end
    ethervox_memory_set_privacy_mode(false);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Multiple toggles work correctly\n");
}

int main(void) {
    printf("\n=== Secret Mode Unit Tests ===\n\n");
    
    test_privacy_mode_toggle();
    test_secret_mode_prevents_logging();
    test_secret_mode_state_restoration();
    test_secret_mode_search_behavior();
    test_secret_mode_tool_execution();
    test_secret_mode_multiple_toggles();
    
    printf("\n✅ All secret mode tests passed!\n\n");
    return ETHERVOX_SUCCESS;
}
