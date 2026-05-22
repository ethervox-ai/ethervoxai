/**
 * @file memory_example_memonly.c
 * @brief Test memory tools in memory-only mode (no file persistence)
 *
 * This demonstrates how the memory plugin works on platforms without
 * file system access or when persistence is disabled.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Memory-Only Mode Test (No File Persistence) ===\n\n");
    
    ethervox_memory_store_t memory_store;
    
    // Initialize with NULL storage_dir = memory-only mode
    if (ethervox_memory_init(&memory_store, "test_session", NULL) != 0) {
        fprintf(stderr, "Failed to initialize memory store\n");
        return 1;
    }
    
    printf("Memory store initialized in memory-only mode\n");
    printf("Session ID: %s\n\n", memory_store.session_id);
    
    // Store some test data
    const char* messages[] = {
        "Android app started",
        "User asked about weather",
        "Fetched weather data"
    };
    
    const char* tags1[] = {"system", "startup"};
    const char* tags2[] = {"user", "weather", "query"};
    const char* tags3[] = {"assistant", "weather", "data"};
    
    uint64_t id;
    ethervox_memory_store_add(&memory_store, messages[0], tags1, 2, 0.5f, false, &id);
    ethervox_memory_store_add(&memory_store, messages[1], tags2, 3, 0.9f, true, &id);
    ethervox_memory_store_add(&memory_store, messages[2], tags3, 3, 0.8f, false, &id);
    
    printf("Stored 3 memories (no files written)\n\n");
    
    // Search by tag
    printf("=== Searching for tag='weather' ===\n");
    ethervox_memory_search_result_t* results = NULL;
    uint32_t result_count = 0;
    
    const char* filter[] = {"weather"};
    ethervox_memory_search(&memory_store, NULL, filter, 1, 10, &results, &result_count);
    
    for (uint32_t i = 0; i < result_count; i++) {
        printf("  [%.2f] %s\n", results[i].relevance, results[i].entry.text);
    }
    free(results);
    
    printf("\n=== Statistics ===\n");
    printf("Total memories: %llu\n", (unsigned long long)memory_store.total_memories_stored);
    printf("Total searches: %llu\n", (unsigned long long)memory_store.total_searches);
    printf("Persistence: DISABLED (memory-only mode)\n");
    
    ethervox_memory_cleanup(&memory_store);
    
    printf("\n✅ Memory-only mode works correctly\n");
    printf("This is ideal for:\n");
    printf("  - Android testing without file permissions\n");
    printf("  - Privacy-focused apps\n");
    printf("  - Embedded devices with limited storage\n");
    
    return 0;
}
