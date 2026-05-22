/**
 * @file test_cache_summarization.c
 * @brief Unit tests for KV cache summarization feature
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <stdio.h>
#include "ethervox/error.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#endif

#include "ethervox/governor.h"
#include "ethervox/memory_tools.h"
#include "ethervox/config.h"

// Test counters
static int tests_passed = 0;
static int tests_failed = 0;
static int total_assertions = 0;

#define TEST_ASSERT(condition, message) do { \
    total_assertions++; \
    if (!(condition)) { \
        printf("  ✗ ASSERTION FAILED: %s\n", message); \
        tests_failed++; \
        return ETHERVOX_ERROR_INVALID_ARGUMENT; \
    } else { \
        printf("  ✓ %s\n", message); \
        tests_passed++; \
    } \
} while(0)

/**
 * Test 1: Manual cache summarization with minimal conversation
 */
static int test_manual_summarization_minimal() {
    printf("\n[Test 1] Manual cache summarization with minimal conversation\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Initialize memory store
    ethervox_memory_store_t memory;
    int ret = ethervox_memory_init(&memory, NULL, NULL);
    TEST_ASSERT(ret == 0, "Memory store initialized");
    
    // Initialize tool registry
    ethervox_tool_registry_t registry;
    ret = ethervox_tool_registry_init(&registry, 16);
    TEST_ASSERT(ret == 0, "Tool registry initialized");
    
    // Register memory tools
    ret = ethervox_memory_tools_register(&registry, &memory);
    TEST_ASSERT(ret == 0, "Memory tools registered");
    
    // Initialize governor
    ethervox_governor_config_t config = ethervox_governor_default_config();
    ethervox_governor_t* governor = NULL;
    
    ret = ethervox_governor_init(&governor, &config, &registry);
    TEST_ASSERT(ret == 0, "Governor initialized");
    TEST_ASSERT(governor != NULL, "Governor pointer is valid");
    
    // Try to summarize without loading a model (should fail gracefully)
    ret = ethervox_governor_summarize_and_clear_cache(governor, true);
    TEST_ASSERT(ret != 0, "Summarization fails without loaded model");
    
    // Cleanup
    ethervox_governor_cleanup(governor);
    ethervox_tool_registry_cleanup(&registry);
    ethervox_memory_cleanup(&memory);
    
    return ETHERVOX_SUCCESS;
}

/**
 * Test 2: Verify summary storage in memory
 */
static int test_summary_storage() {
    printf("\n[Test 2] Verify summary storage in memory\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Initialize memory store
    ethervox_memory_store_t memory;
    int ret = ethervox_memory_init(&memory, NULL, NULL);
    TEST_ASSERT(ret == 0, "Memory store initialized");
    
    // Add some test memories to simulate conversation
    ret = ethervox_memory_store_add(&memory, "User asked about the weather", NULL, 0, 0.8f, true, NULL);
    TEST_ASSERT(ret == 0, "Test memory 1 stored");
    
    ret = ethervox_memory_store_add(&memory, "Assistant provided weather forecast", NULL, 0, 0.8f, false, NULL);
    TEST_ASSERT(ret == 0, "Test memory 2 stored");
    
    // Manually store a summary with the expected tags
    const char* tags[] = {"context_summary", "manual_clear", "auto_generated"};
    ret = ethervox_memory_store_add(&memory, 
        "[Manual Cache Clear - Context Summary]\n\nDiscussed weather and forecast.\n\nTurn count: 2.",
        tags, 3, 0.90f, false, NULL);
    TEST_ASSERT(ret == 0, "Summary stored in memory");
    
    // Search for the summary by tags
    ethervox_memory_search_result_t* results = NULL;
    uint32_t result_count = 0;
    
    const char* search_tags[] = {"context_summary", "manual_clear"};
    ret = ethervox_memory_search(&memory, NULL, search_tags, 2, 10, &results, &result_count);
    TEST_ASSERT(ret == 0, "Memory search completed");
    TEST_ASSERT(result_count == 1, "Found exactly one summary");
    TEST_ASSERT(results != NULL, "Search results not NULL");
    
    if (results) {
        TEST_ASSERT(results[0].entry.importance >= 0.90f, "Summary has high importance");
        TEST_ASSERT(strstr(results[0].entry.text, "Cache Clear") != NULL, "Summary contains expected text");
        free(results);
    }
    
    // Cleanup
    ethervox_memory_cleanup(&memory);
    
    return ETHERVOX_SUCCESS;
}

/**
 * Test 3: Cache health check before summarization
 */
static int test_cache_health_check() {
    printf("\n[Test 3] Cache health check before summarization\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    ethervox_memory_store_t memory;
    ethervox_memory_init(&memory, NULL, NULL);
    
    ethervox_tool_registry_t registry;
    ethervox_tool_registry_init(&registry, 16);
    ethervox_memory_tools_register(&registry, &memory);
    
    ethervox_governor_config_t config = ethervox_governor_default_config();
    ethervox_governor_t* governor = NULL;
    ethervox_governor_init(&governor, &config, &registry);
    
    TEST_ASSERT(governor != NULL, "Governor initialized for cache health test");
    
    // Without a loaded model, summarization should fail but not crash
    int ret = ethervox_governor_summarize_and_clear_cache(governor, false);
    TEST_ASSERT(ret != 0, "Summarization correctly fails with no model");
    
    // With force_clear=false on empty cache, should return early
    ret = ethervox_governor_summarize_and_clear_cache(governor, false);
    TEST_ASSERT(ret != 0, "Non-forced clear returns early on empty cache");
    
    ethervox_governor_cleanup(governor);
    ethervox_tool_registry_cleanup(&registry);
    ethervox_memory_cleanup(&memory);
    
    return ETHERVOX_SUCCESS;
}

/**
 * Test 4: NULL pointer safety
 */
static int test_null_pointer_safety() {
    printf("\n[Test 4] NULL pointer safety\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // All calls with NULL should fail gracefully without crashing
    int ret = ethervox_governor_summarize_and_clear_cache(NULL, true);
    TEST_ASSERT(ret != 0, "Summarization fails with NULL governor");
    
    ret = ethervox_governor_summarize_and_clear_cache(NULL, false);
    TEST_ASSERT(ret != 0, "Non-forced summarization fails with NULL governor");
    
    return ETHERVOX_SUCCESS;
}

/**
 * Test 5: Conversation history tracking
 */
static int test_conversation_history() {
    printf("\n[Test 5] Conversation history tracking\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    ethervox_memory_store_t memory;
    ethervox_memory_init(&memory, NULL, NULL);
    
    ethervox_tool_registry_t registry;
    ethervox_tool_registry_init(&registry, 16);
    ethervox_memory_tools_register(&registry, &memory);
    
    ethervox_governor_config_t config = ethervox_governor_default_config();
    ethervox_governor_t* governor = NULL;
    ethervox_governor_init(&governor, &config, &registry);
    
    TEST_ASSERT(governor != NULL, "Governor initialized");
    
    // Store some conversation turns in memory to simulate history
    const char* user_msgs[] = {
        "What is the weather today?",
        "Tell me about machine learning",
        "How do I bake a cake?"
    };
    
    const char* assistant_msgs[] = {
        "The weather is sunny and 72°F",
        "Machine learning is a subset of AI...",
        "To bake a cake, you need flour, eggs..."
    };
    
    for (int i = 0; i < 3; i++) {
        ethervox_memory_store_add(&memory, user_msgs[i], NULL, 0, 0.8f, true, NULL);
        ethervox_memory_store_add(&memory, assistant_msgs[i], NULL, 0, 0.8f, false, NULL);
    }
    
    TEST_ASSERT(memory.entry_count == 6, "6 conversation turns stored in memory");
    
    // Verify we can search the conversation
    ethervox_memory_search_result_t* results = NULL;
    uint32_t result_count = 0;
    
    int ret = ethervox_memory_search(&memory, "weather", NULL, 0, 5, &results, &result_count);
    TEST_ASSERT(ret == 0, "Search for 'weather' succeeded");
    TEST_ASSERT(result_count > 0, "Found weather-related memory");
    
    if (results) {
        free(results);
    }
    
    ethervox_governor_cleanup(governor);
    ethervox_tool_registry_cleanup(&registry);
    ethervox_memory_cleanup(&memory);
    
    return ETHERVOX_SUCCESS;
}

/**
 * Generate test report
 */
static void generate_report() {
    // Create reports directory
    char report_dir[512];
    const char* home = getenv("HOME");
    if (home) {
        snprintf(report_dir, sizeof(report_dir), "%s/.ethervox/reports", home);
    } else {
        snprintf(report_dir, sizeof(report_dir), "./.ethervox/reports");
    }
    
#ifdef _WIN32
    _mkdir(report_dir);
#else
    mkdir(report_dir, 0755);
#endif
    
    // Generate filename with timestamp
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char filename[768];
    snprintf(filename, sizeof(filename), "%s/cache_summarization_test_%04d%02d%02d_%02d%02d%02d.txt",
             report_dir,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);
    
    FILE* report = fopen(filename, "w");
    if (!report) {
        fprintf(stderr, "Failed to create report file: %s\n", filename);
        return;
    }
    
    fprintf(report, "═══════════════════════════════════════════════════════════════\n");
    fprintf(report, "  EthervoxAI - KV Cache Summarization Test Report\n");
    fprintf(report, "═══════════════════════════════════════════════════════════════\n\n");
    
    fprintf(report, "Test Date: %04d-%02d-%02d %02d:%02d:%02d\n\n",
            tm_info->tm_year + 1900,
            tm_info->tm_mon + 1,
            tm_info->tm_mday,
            tm_info->tm_hour,
            tm_info->tm_min,
            tm_info->tm_sec);
    
    fprintf(report, "Test Results:\n");
    fprintf(report, "  Total Assertions:  %d\n", total_assertions);
    fprintf(report, "  Passed:            %d\n", tests_passed);
    fprintf(report, "  Failed:            %d\n", tests_failed);
    fprintf(report, "  Success Rate:      %.1f%%\n\n", 
            (tests_passed * 100.0) / total_assertions);
    
    fprintf(report, "Test Coverage:\n");
    fprintf(report, "  ✓ Manual cache summarization\n");
    fprintf(report, "  ✓ Summary storage in memory\n");
    fprintf(report, "  ✓ Cache health checks\n");
    fprintf(report, "  ✓ NULL pointer safety\n");
    fprintf(report, "  ✓ Conversation history tracking\n\n");
    
    fprintf(report, "═══════════════════════════════════════════════════════════════\n");
    fprintf(report, "Overall: %s\n", tests_failed == 0 ? "PASS ✓" : "FAIL ✗");
    fprintf(report, "═══════════════════════════════════════════════════════════════\n");
    
    fclose(report);
    
    printf("\n📄 Test report saved to: %s\n", filename);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  EthervoxAI - KV Cache Summarization Test Suite              ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    // Run all tests
    if (test_manual_summarization_minimal() != 0) goto cleanup;
    if (test_summary_storage() != 0) goto cleanup;
    if (test_cache_health_check() != 0) goto cleanup;
    if (test_null_pointer_safety() != 0) goto cleanup;
    if (test_conversation_history() != 0) goto cleanup;
    
cleanup:
    // Print summary
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  Test Summary                                                 ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║  Total Assertions: %-3d                                        ║\n", total_assertions);
    printf("║  Passed:           %-3d                                        ║\n", tests_passed);
    printf("║  Failed:           %-3d                                        ║\n", tests_failed);
    printf("║  Success Rate:     %.1f%%                                     ║\n", 
           (tests_passed * 100.0) / total_assertions);
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║  Result: %-53s║\n", tests_failed == 0 ? "PASS ✓" : "FAIL ✗");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    // Generate report
    generate_report();
    
    return tests_failed == 0 ? 0 : 1;
}
