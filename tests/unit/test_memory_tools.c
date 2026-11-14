/**
 * @file test_memory_tools.c
 * @brief Unit tests for memory tools plugin
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include "test_utils.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_init_cleanup(void) {
    printf("Testing init/cleanup...\n");
    
    ethervox_memory_store_t store;
    ethervox_result_t result = ethervox_memory_init(&store, "test-session", "/tmp");
    CHECK(ethervox_is_success(result));
    CHECK(store.is_initialized);
    CHECK(strcmp(store.session_id, "test-session") == 0);
    
    ethervox_memory_cleanup(&store);
    CHECK(!store.is_initialized);
    
    printf("  ✓ Init/cleanup works\n");
}

void test_store_retrieve(void) {
    printf("Testing store and retrieve...\n");
    
    ethervox_memory_store_t store;
    ethervox_result_t result = ethervox_memory_init(&store, NULL, "/tmp");
    CHECK(ethervox_is_success(result));
    
    // Store a memory
    const char* tags[] = {"test", "example"};
    uint64_t memory_id;
    
    result = ethervox_memory_store_add(&store, "Test message",
                                          tags, 2, 0.8f, true, &memory_id);
    CHECK(ethervox_is_success(result));
    CHECK(memory_id == 0);  // First memory
    CHECK(store.entry_count == 1);
    
    // Retrieve by ID
    const ethervox_memory_entry_t* entry;
    result = ethervox_memory_get_by_id(&store, memory_id, &entry);
    CHECK(ethervox_is_success(result));
    CHECK(strcmp(entry->text, "Test message") == 0);
    CHECK(entry->importance == 0.8f);
    CHECK(entry->is_user_message == true);
    CHECK(entry->tag_count == 2);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Store and retrieve works\n");
}

void test_search(void) {
    printf("Testing search...\n");
    
    ethervox_memory_store_t store;
    ethervox_result_t result = ethervox_memory_init(&store, NULL, "/tmp");
    CHECK(ethervox_is_success(result));
    
    // Store multiple memories
    const char* tags1[] = {"build", "error"};
    const char* tags2[] = {"build", "success"};
    const char* tags3[] = {"setup", "config"};
    
    uint64_t id;
    result = ethervox_memory_store_add(&store, "Build failed with error", tags1, 2, 0.9f, true, &id);
    CHECK(ethervox_is_success(result));
    result = ethervox_memory_store_add(&store, "Build succeeded", tags2, 2, 0.7f, false, &id);
    CHECK(ethervox_is_success(result));
    result = ethervox_memory_store_add(&store, "Setup configuration", tags3, 2, 0.5f, true, &id);
    CHECK(ethervox_is_success(result));
    
    // Search by text
    ethervox_memory_search_result_t* results = NULL;
    uint32_t count = 0;
    
    result = ethervox_memory_search(&store, "build error", NULL, 0, 10, &results, &count);
    CHECK(ethervox_is_success(result));
    CHECK(count > 0);
    CHECK(strstr(results[0].entry.text, "error") != NULL);
    free(results);
    
    // Search by tag
    const char* filter[] = {"setup"};
    result = ethervox_memory_search(&store, NULL, filter, 1, 10, &results, &count);
    CHECK(ethervox_is_success(result));
    CHECK(count == 1);
    CHECK(strcmp(results[0].entry.text, "Setup configuration") == 0);
    free(results);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Search works\n");
}

void test_export_import(void) {
    printf("Testing export/import...\n");
    
    ethervox_memory_store_t store;
    ethervox_result_t result = ethervox_memory_init(&store, NULL, "/tmp");
    CHECK(ethervox_is_success(result));
    
    // Store some data
    const char* tags[] = {"test"};
    uint64_t id;
    result = ethervox_memory_store_add(&store, "Test export", tags, 1, 0.8f, true, &id);
    CHECK(ethervox_is_success(result));
    
    // Export to JSON
    uint64_t bytes;
    result = ethervox_memory_export(&store, "/tmp/test_export.json", "json", &bytes);
    CHECK(ethervox_is_success(result));
    CHECK(bytes > 0);
    
    // Export to Markdown
    result = ethervox_memory_export(&store, "/tmp/test_export.md", "markdown", &bytes);
    CHECK(ethervox_is_success(result));
    CHECK(bytes > 0);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Export works\n");
}

void test_forget(void) {
    printf("Testing forget/pruning...\n");
    
    ethervox_memory_store_t store;
    ethervox_result_t result = ethervox_memory_init(&store, NULL, "/tmp");
    CHECK(ethervox_is_success(result));
    
    // Store memories with different importance
    const char* tags[] = {"test"};
    uint64_t id;
    
    result = ethervox_memory_store_add(&store, "High importance", tags, 1, 0.9f, true, &id);
    CHECK(ethervox_is_success(result));
    result = ethervox_memory_store_add(&store, "Medium importance", tags, 1, 0.6f, true, &id);
    CHECK(ethervox_is_success(result));
    result = ethervox_memory_store_add(&store, "Low importance", tags, 1, 0.2f, true, &id);
    CHECK(ethervox_is_success(result));
    
    CHECK(store.entry_count == 3);
    
    // Prune low importance (< 0.5)
    uint32_t pruned;
    result = ethervox_memory_forget(&store, 0, 0.5f, &pruned);
    CHECK(ethervox_is_success(result));
    
    CHECK(pruned == 1);  // Only one entry below 0.5
    CHECK(store.entry_count == 2);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Forget works\n");
}

void test_summarize(void) {
    printf("Testing summarize...\n");
    
    ethervox_memory_store_t store;
    ethervox_result_t result = ethervox_memory_init(&store, NULL, "/tmp");
    CHECK(ethervox_is_success(result));
    
    // Store conversation
    const char* tags[] = {"conversation"};
    uint64_t id;
    
    result = ethervox_memory_store_add(&store, "User asks question", tags, 1, 0.8f, true, &id);
    CHECK(ethervox_is_success(result));
    result = ethervox_memory_store_add(&store, "Assistant responds", tags, 1, 0.8f, false, &id);
    CHECK(ethervox_is_success(result));
    
    // Generate summary
    char* summary = NULL;
    result = ethervox_memory_summarize(&store, 10, NULL, &summary, NULL, NULL);
    CHECK(ethervox_is_success(result));
    CHECK(summary != NULL);
    CHECK(strlen(summary) > 0);
    
    free(summary);
    ethervox_memory_cleanup(&store);
    printf("  ✓ Summarize works\n");
}

int main(void) {
    printf("=== Memory Tools Unit Tests ===\n\n");
    
    test_init_cleanup();
    test_store_retrieve();
    test_search();
    test_export_import();
    test_forget();
    test_summarize();
    
    printf("\n=== All tests passed! ===\n");
    return ETHERVOX_SUCCESS;
}
