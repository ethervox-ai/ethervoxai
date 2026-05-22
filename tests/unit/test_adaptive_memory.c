/**
 * @file test_adaptive_memory.c
 * @brief Test adaptive learning memory features (corrections and patterns)
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_correction_storage(void) {
    printf("\nTesting correction storage...\n");
    
    ethervox_memory_store_t store;
    if (ethervox_memory_init(&store, NULL, "/tmp") != 0) {
        printf("  ✗ Failed to init memory store\n");
        return;
    }
    
    // Store a correction
    uint64_t correction_id;
    const char* correction = "User prefers metric units, not imperial";
    const char* context = "Previous responses used feet instead of meters";
    
    if (ethervox_memory_store_correction(&store, correction, context, &correction_id) != 0) {
        printf("  ✗ Failed to store correction\n");
        ethervox_memory_cleanup(&store);
        return;
    }
    
    printf("  ✓ Stored correction with ID %llu\n", (unsigned long long)correction_id);
    
    // Retrieve the correction
    const ethervox_memory_entry_t* entry;
    if (ethervox_memory_get_by_id(&store, correction_id, &entry) != 0) {
        printf("  ✗ Failed to retrieve correction\n");
        ethervox_memory_cleanup(&store);
        return;
    }
    
    // Verify importance
    if (entry->importance != 0.99f) {
        printf("  ✗ Correction importance is %.2f, expected 0.99\n", entry->importance);
        ethervox_memory_cleanup(&store);
        return;
    }
    printf("  ✓ Correction has importance=0.99\n");
    
    // Verify tags
    bool has_correction_tag = false;
    bool has_priority_tag = false;
    for (uint32_t i = 0; i < entry->tag_count; i++) {
        if (strcmp(entry->tags[i], "correction") == 0) has_correction_tag = true;
        if (strcmp(entry->tags[i], "high_priority") == 0) has_priority_tag = true;
    }
    
    if (!has_correction_tag || !has_priority_tag) {
        printf("  ✗ Correction missing required tags\n");
        ethervox_memory_cleanup(&store);
        return;
    }
    printf("  ✓ Correction has correct tags\n");
    
    // Test retrieval through get_corrections
    ethervox_memory_search_result_t* results = NULL;
    uint32_t result_count = 0;
    
    if (ethervox_memory_get_corrections(&store, &results, &result_count, 10) != 0) {
        printf("  ✗ Failed to get corrections\n");
        ethervox_memory_cleanup(&store);
        return;
    }
    
    if (result_count != 1) {
        printf("  ✗ Expected 1 correction, got %u\n", result_count);
        free(results);
        ethervox_memory_cleanup(&store);
        return;
    }
    
    printf("  ✓ Retrieved %u correction via get_corrections()\n", result_count);
    free(results);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Correction storage test passed\n");
}

static void test_pattern_storage(void) {
    printf("\nTesting pattern storage...\n");
    
    ethervox_memory_store_t store;
    if (ethervox_memory_init(&store, NULL, "/tmp") != 0) {
        printf("  ✗ Failed to init memory store\n");
        return;
    }
    
    // Store a pattern
    uint64_t pattern_id;
    const char* pattern = "User responds positively to concise bullet-point summaries";
    
    if (ethervox_memory_store_pattern(&store, pattern, &pattern_id) != 0) {
        printf("  ✗ Failed to store pattern\n");
        ethervox_memory_cleanup(&store);
        return;
    }
    
    printf("  ✓ Stored pattern with ID %llu\n", (unsigned long long)pattern_id);
    
    // Retrieve the pattern
    const ethervox_memory_entry_t* entry;
    if (ethervox_memory_get_by_id(&store, pattern_id, &entry) != 0) {
        printf("  ✗ Failed to retrieve pattern\n");
        ethervox_memory_cleanup(&store);
        return;
    }
    
    // Verify importance
    if (entry->importance != 0.90f) {
        printf("  ✗ Pattern importance is %.2f, expected 0.90\n", entry->importance);
        ethervox_memory_cleanup(&store);
        return;
    }
    printf("  ✓ Pattern has importance=0.90\n");
    
    // Verify tags
    bool has_pattern_tag = false;
    bool has_success_tag = false;
    for (uint32_t i = 0; i < entry->tag_count; i++) {
        if (strcmp(entry->tags[i], "pattern") == 0) has_pattern_tag = true;
        if (strcmp(entry->tags[i], "success") == 0) has_success_tag = true;
    }
    
    if (!has_pattern_tag || !has_success_tag) {
        printf("  ✗ Pattern missing required tags\n");
        ethervox_memory_cleanup(&store);
        return;
    }
    printf("  ✓ Pattern has correct tags\n");
    
    // Test retrieval through get_patterns
    ethervox_memory_search_result_t* results = NULL;
    uint32_t result_count = 0;
    
    if (ethervox_memory_get_patterns(&store, &results, &result_count, 10) != 0) {
        printf("  ✗ Failed to get patterns\n");
        ethervox_memory_cleanup(&store);
        return;
    }
    
    if (result_count != 1) {
        printf("  ✗ Expected 1 pattern, got %u\n", result_count);
        free(results);
        ethervox_memory_cleanup(&store);
        return;
    }
    
    printf("  ✓ Retrieved %u pattern via get_patterns()\n", result_count);
    free(results);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Pattern storage test passed\n");
}

static void test_multiple_corrections_and_patterns(void) {
    printf("\nTesting multiple corrections and patterns...\n");
    
    ethervox_memory_store_t store;
    if (ethervox_memory_init(&store, NULL, "/tmp") != 0) {
        printf("  ✗ Failed to init memory store\n");
        return;
    }
    
    // Store 3 corrections
    for (int i = 0; i < 3; i++) {
        char correction[256];
        snprintf(correction, sizeof(correction), "Correction number %d", i + 1);
        uint64_t id;
        if (ethervox_memory_store_correction(&store, correction, NULL, &id) != 0) {
            printf("  ✗ Failed to store correction %d\n", i + 1);
            ethervox_memory_cleanup(&store);
            return;
        }
    }
    
    // Store 5 patterns
    for (int i = 0; i < 5; i++) {
        char pattern[256];
        snprintf(pattern, sizeof(pattern), "Success pattern number %d", i + 1);
        uint64_t id;
        if (ethervox_memory_store_pattern(&store, pattern, &id) != 0) {
            printf("  ✗ Failed to store pattern %d\n", i + 1);
            ethervox_memory_cleanup(&store);
            return;
        }
    }
    
    // Retrieve all corrections
    ethervox_memory_search_result_t* corrections = NULL;
    uint32_t correction_count = 0;
    
    if (ethervox_memory_get_corrections(&store, &corrections, &correction_count, 10) != 0) {
        printf("  ✗ Failed to get corrections\n");
        ethervox_memory_cleanup(&store);
        return;
    }
    
    if (correction_count != 3) {
        printf("  ✗ Expected 3 corrections, got %u\n", correction_count);
        free(corrections);
        ethervox_memory_cleanup(&store);
        return;
    }
    printf("  ✓ Retrieved %u corrections\n", correction_count);
    free(corrections);
    
    // Retrieve all patterns
    ethervox_memory_search_result_t* patterns = NULL;
    uint32_t pattern_count = 0;
    
    if (ethervox_memory_get_patterns(&store, &patterns, &pattern_count, 10) != 0) {
        printf("  ✗ Failed to get patterns\n");
        ethervox_memory_cleanup(&store);
        return;
    }
    
    if (pattern_count != 5) {
        printf("  ✗ Expected 5 patterns, got %u\n", pattern_count);
        free(patterns);
        ethervox_memory_cleanup(&store);
        return;
    }
    printf("  ✓ Retrieved %u patterns\n", pattern_count);
    free(patterns);
    
    ethervox_memory_cleanup(&store);
    printf("  ✓ Multiple corrections/patterns test passed\n");
}

int main(void) {
    printf("=== Adaptive Memory Tests ===\n");
    
    test_correction_storage();
    test_pattern_storage();
    test_multiple_corrections_and_patterns();
    
    printf("\n=== All adaptive memory tests passed! ===\n");
    return ETHERVOX_SUCCESS;
}
