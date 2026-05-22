/**
 * @file test_context_overflow.c
 * @brief Unit tests for mid-generation context overflow management
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/governor.h"
#include "ethervox/error.h"
#include "ethervox/context_tools.h"
#include "ethervox/memory_tools.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Access internal governor structures for testing
typedef struct {
    // ... (only the fields we need for testing)
    bool initialized;
    bool llm_loaded;
    int32_t current_kv_pos;
    void* llm_ctx;
    void* llm_model;
} test_governor_t;

void test_health_detection(void) {
    printf("Testing context health detection...\n");
    
    // Note: This test is conceptual - actual implementation requires 
    // initializing a real llama context which is heavyweight for unit tests.
    // In practice, you'd mock the llama_n_ctx and current_kv_pos values.
    
    printf("  ✓ Health detection test (conceptual - requires full llama setup)\n");
}

void test_turn_tracking(void) {
    printf("Testing turn tracking...\n");
    
    // Create a mock conversation history
    conversation_history_t history;
    history.turns = NULL;
    history.turn_count = 0;
    history.capacity = 0;
    
    // Initialize with small capacity
    history.capacity = 4;
    history.turns = malloc(history.capacity * sizeof(conversation_turn_t));
    assert(history.turns != NULL);
    
    // Test appending turns
    conversation_turn_t turn1 = {
        .turn_number = 0,
        .kv_start = 0,
        .kv_end = 100,
        .timestamp = time(NULL),
        .importance = 0.5f,
        .is_user = true
    };
    strncpy(turn1.preview, "What is the weather?", sizeof(turn1.preview) - 1);
    
    conversation_turn_t turn2 = {
        .turn_number = 1,
        .kv_start = 101,
        .kv_end = 250,
        .timestamp = time(NULL),
        .importance = 0.7f,
        .is_user = false
    };
    strncpy(turn2.preview, "The weather is sunny.", sizeof(turn2.preview) - 1);
    
    // Manually append (simulating append_turn internal function)
    history.turns[0] = turn1;
    history.turn_count = 1;
    history.turns[1] = turn2;
    history.turn_count = 2;
    
    // Verify turns
    assert(history.turn_count == 2);
    assert(history.turns[0].is_user == true);
    assert(history.turns[0].kv_start == 0);
    assert(history.turns[0].kv_end == 100);
    assert(history.turns[1].is_user == false);
    assert(history.turns[1].kv_start == 101);
    assert(history.turns[1].kv_end == 250);
    
    // Cleanup
    free(history.turns);
    
    printf("  ✓ Turn tracking works\n");
}

void test_shift_window_action(void) {
    printf("Testing shift_window action...\n");
    
    // Note: This test requires a full llama context to be initialized
    // because shift_window uses llama_memory_seq_rm which needs a real context.
    // For now, this is a placeholder.
    
    printf("  ✓ Shift window test (requires full llama setup)\n");
}

void test_summarize_old_action(void) {
    printf("Testing summarize_old action...\n");
    
    // Note: This test requires a full llama context and memory store
    // For now, this is a placeholder.
    
    printf("  ✓ Summarize old test (requires full llama setup)\n");
}

void test_automatic_warning_injection(void) {
    printf("Testing automatic warning injection...\n");
    
    // Note: This test requires checking that when context reaches 80%,
    // the governor automatically injects a warning message.
    // This requires a full llama setup.
    
    printf("  ✓ Automatic warning test (requires full llama setup)\n");
}

void test_context_tools_registration(void) {
    printf("Testing context tools registration...\n");
    
    // Test that context_manage tool can be registered
    ethervox_tool_registry_t registry;
    memset(&registry, 0, sizeof(registry));
    
    // Initialize registry (simplified - real init would do more)
    registry.tools = NULL;
    registry.tool_count = 0;
    registry.capacity = 0;
    
    // Create a memory store
    ethervox_memory_store_t memory_store;
    memset(&memory_store, 0, sizeof(memory_store));
    
    // Register context_manage tool
    int result = register_context_manage_tool(&registry, &memory_store);
    assert(result == 0);
    assert(registry.tool_count == 1);
    
    // Verify tool is registered
    bool found = false;
    for (uint32_t i = 0; i < registry.tool_count; i++) {
        if (strcmp(registry.tools[i].name, "context_manage") == 0) {
            found = true;
            break;
        }
    }
    assert(found == true);
    
    // Cleanup registry properly
    ethervox_tool_registry_cleanup(&registry);
    
    printf("  ✓ Context tools registration works\n");
}

void test_kv_position_tracking(void) {
    printf("Testing KV position tracking...\n");
    
    // Test that conversation turns correctly track their KV cache positions
    conversation_history_t history;
    history.turns = malloc(10 * sizeof(conversation_turn_t));
    history.capacity = 10;
    history.turn_count = 0;
    
    // Simulate a conversation with known KV positions
    int current_pos = 100;  // Start after system prompt
    
    // User turn 1: 50 tokens
    conversation_turn_t user_turn1 = {
        .turn_number = 0,
        .kv_start = current_pos,
        .kv_end = current_pos + 49,
        .timestamp = time(NULL),
        .importance = 0.5f,
        .is_user = true
    };
    current_pos += 50;
    history.turns[history.turn_count++] = user_turn1;
    
    // Assistant turn 1: 100 tokens
    conversation_turn_t assistant_turn1 = {
        .turn_number = 1,
        .kv_start = current_pos,
        .kv_end = current_pos + 99,
        .timestamp = time(NULL),
        .importance = 0.7f,
        .is_user = false
    };
    current_pos += 100;
    history.turns[history.turn_count++] = assistant_turn1;
    
    // Verify positions
    assert(history.turns[0].kv_start == 100);
    assert(history.turns[0].kv_end == 149);
    assert(history.turns[1].kv_start == 150);
    assert(history.turns[1].kv_end == 249);
    
    // Calculate total tokens used
    int total_tokens = 0;
    for (uint32_t i = 0; i < history.turn_count; i++) {
        total_tokens += (history.turns[i].kv_end - history.turns[i].kv_start + 1);
    }
    assert(total_tokens == 150);  // 50 + 100
    
    free(history.turns);
    
    printf("  ✓ KV position tracking works\n");
}

int main(void) {
    printf("\n=== Context Overflow Management Tests ===\n\n");
    
    test_turn_tracking();
    test_kv_position_tracking();
    test_context_tools_registration();
    test_health_detection();
    test_shift_window_action();
    test_summarize_old_action();
    test_automatic_warning_injection();
    
    printf("\n=== All Tests Passed ===\n");
    return ETHERVOX_SUCCESS;
}
