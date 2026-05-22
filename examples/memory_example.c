/**
 * @file memory_example.c
 * @brief Example usage of conversation memory tools with Governor
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== EthervoxAI Memory Tools Example ===\n\n");
    
    // Initialize memory store
    ethervox_memory_store_t memory_store;
    if (ethervox_memory_init(&memory_store, NULL, "./memory_data") != 0) {
        fprintf(stderr, "Failed to initialize memory store\n");
        return 1;
    }
    
    printf("Memory store initialized: session_id=%s\n", memory_store.session_id);
    printf("Storage location: %s\n\n", memory_store.storage_filepath);
    
    // Add some conversation memories
    const char* user_msgs[] = {
        "Can you help me install build tools for macOS?",
        "I need to compile EthervoxAI with llama.cpp support",
        "What dependencies do I need?",
        "The build is failing with C++ header errors"
    };
    
    const char* assistant_msgs[] = {
        "I'll help you install the necessary build tools using Homebrew",
        "You'll need cmake, llama.cpp, and OpenMP support",
        "Install cmake, ninja, pkg-config, libomp, openssl, and curl",
        "This might be a broken Command Line Tools installation"
    };
    
    const char* tags[][5] = {
        {"setup", "macos", "tools", NULL},
        {"build", "llama.cpp", "compilation", NULL},
        {"dependencies", "brew", "installation", NULL},
        {"troubleshooting", "error", "c++", "headers", NULL}
    };
    
    float importance[] = {0.8f, 0.9f, 0.7f, 0.95f};
    
    printf("Storing conversation turns...\n");
    for (int i = 0; i < 4; i++) {
        uint64_t user_id, assistant_id;
        
        // Count tags
        uint32_t tag_count = 0;
        while (tags[i][tag_count] != NULL) tag_count++;
        
        // Store user message
        ethervox_memory_store_add(&memory_store, user_msgs[i],
                                 tags[i], tag_count,
                                 importance[i], true, &user_id);
        
        // Store assistant message
        ethervox_memory_store_add(&memory_store, assistant_msgs[i],
                                 tags[i], tag_count,
                                 importance[i], false, &assistant_id);
        
        printf("  Turn %d stored (IDs: %llu, %llu)\n", i + 1, (unsigned long long)user_id, (unsigned long long)assistant_id);
    }
    
    printf("\nTotal memories stored: %llu\n\n", (unsigned long long)memory_store.total_memories_stored);
    
    // Search for specific topics
    printf("=== Search: 'llama.cpp compilation' ===\n");
    ethervox_memory_search_result_t* results = NULL;
    uint32_t result_count = 0;
    
    printf("About to call ethervox_memory_search...\n");
    fflush(stdout);
    
    ethervox_result_t search_ret = ethervox_memory_search(&memory_store, "llama.cpp compilation",
                          NULL, 0, 5, &results, &result_count);
    
    printf("Search returned: %d, result_count=%u\n", search_ret, result_count);
    fflush(stdout);
    
    for (uint32_t i = 0; i < result_count; i++) {
        printf("  [%.2f] %s: %s\n",
               results[i].relevance,
               results[i].entry.is_user_message ? "User" : "Assistant",
               results[i].entry.text);
    }
    free(results);
    
    // Search by tags
    printf("\n=== Search: tag='troubleshooting' ===\n");
    const char* tag_filter[] = {"troubleshooting"};
    
    ethervox_memory_search(&memory_store, NULL, tag_filter, 1, 5,
                          &results, &result_count);
    
    for (uint32_t i = 0; i < result_count; i++) {
        printf("  %s: %s\n",
               results[i].entry.is_user_message ? "User" : "Assistant",
               results[i].entry.text);
    }
    free(results);
    
    // Generate summary
    printf("\n=== Summary of last 4 turns ===\n");
    char* summary = NULL;
    char** key_points = NULL;
    uint32_t kp_count = 0;
    
    ethervox_memory_summarize(&memory_store, 4, NULL,
                             &summary, &key_points, &kp_count);
    
    printf("%s\n", summary);
    
    if (kp_count > 0) {
        printf("\nKey points:\n");
        for (uint32_t i = 0; i < kp_count; i++) {
            printf("  - %s\n", key_points[i]);
            free(key_points[i]);
        }
        free(key_points);
    }
    free(summary);
    
    // Export to JSON
    printf("\n=== Exporting session ===\n");
    uint64_t bytes_written = 0;
    
    ethervox_memory_export(&memory_store, "conversation_export.json",
                          "json", &bytes_written);
    printf("  JSON export: %llu bytes written to conversation_export.json\n", (unsigned long long)bytes_written);
    
    ethervox_memory_export(&memory_store, "conversation_export.md",
                          "markdown", &bytes_written);
    printf("  Markdown export: %llu bytes written to conversation_export.md\n", (unsigned long long)bytes_written);
    
    // Test forget functionality
    printf("\n=== Testing memory pruning ===\n");
    printf("Before pruning: %u entries\n", memory_store.entry_count);
    
    uint32_t pruned = 0;
    ethervox_memory_forget(&memory_store, 0, 0.75f, &pruned);  // Remove importance < 0.75
    
    printf("After pruning (importance < 0.75): %u entries (%u pruned)\n",
           memory_store.entry_count, pruned);
    
    // Cleanup
    printf("\n=== Statistics ===\n");
    printf("Total memories stored: %llu\n", (unsigned long long)memory_store.total_memories_stored);
    printf("Total searches: %llu\n", (unsigned long long)memory_store.total_searches);
    printf("Total exports: %llu\n", (unsigned long long)memory_store.total_exports);
    
    ethervox_memory_cleanup(&memory_store);
    
    printf("\n=== Example complete ===\n");
    printf("Check the following files:\n");
    printf("  - %s (append-only log)\n", memory_store.storage_filepath);
    printf("  - conversation_export.json\n");
    printf("  - conversation_export.md\n");
    
    return 0;
}
