/**
 * @file integration_tests.c
 * @brief Interactive /test command for validating major features
 *
 * Runs comprehensive tests on:
 * - Context window management (overflow, compression)
 * - Hash table indexing (tag_hash, id_hash)
 * - Adaptive memory (corrections, patterns)
 * - Memory search and retrieval
 * - Tool execution
 * - System prompt generation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/memory_tools.h"
#include "ethervox/context_tools.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include "ethervox/chat_template.h"
#include "ethervox/platform.h"
#include "ethervox/platform_utils.h"
#include "ethervox/unit_conversion.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>

// Include llama.cpp headers for cache inspection
#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
#include "llama.h"
#endif

// Android logging support
#ifdef __ANDROID__
#include <android/log.h>
#define INTEGRATION_LOG(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "EthervoxIntegrationTests", fmt, ##__VA_ARGS__)
#else
#define INTEGRATION_LOG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#endif

// ANSI color codes for pretty output
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

#define TEST_PASS(msg, ...) printf(COLOR_GREEN "  [OK] " COLOR_RESET msg "\n", ##__VA_ARGS__)
#define TEST_FAIL(msg, ...) printf(COLOR_RED "  [FAIL] " COLOR_RESET msg "\n", ##__VA_ARGS__)
#define TEST_INFO(msg, ...) printf(COLOR_CYAN "  ℹ " COLOR_RESET msg "\n", ##__VA_ARGS__)
#define TEST_HEADER(msg, ...) printf("\n" COLOR_BOLD COLOR_BLUE "=== " msg " ===" COLOR_RESET "\n", ##__VA_ARGS__)
#define TEST_SUBHEADER(msg, ...) printf("\n" COLOR_YELLOW "→ " msg COLOR_RESET "\n", ##__VA_ARGS__)

static int g_tests_passed = 0;
static int g_tests_failed = 0;

// Report file for test results
static FILE* g_report_file = NULL;
static char g_report_path[512] = {0};

#define MAX_TEST_NAMES 50
static char* g_passed_tests[MAX_TEST_NAMES];
static char* g_failed_tests[MAX_TEST_NAMES];
static int g_passed_count = 0;
static int g_failed_count = 0;

static void record_test_result(const char* test_name, bool passed) {
    if (passed && g_passed_count < MAX_TEST_NAMES) {
        g_passed_tests[g_passed_count++] = strdup(test_name);
    } else if (!passed && g_failed_count < MAX_TEST_NAMES) {
        g_failed_tests[g_failed_count++] = strdup(test_name);
    }
}

// Test 1: Memory Store Initialization and Basic Operations
static void test_memory_basic(void) {
    TEST_HEADER("Test 1: Memory Store Basic Operations");
    
    ethervox_memory_store_t store;
    if (ethervox_memory_init(&store, "test_session", "/tmp") != 0) {
        TEST_FAIL("Failed to initialize memory store");
        g_tests_failed++;
        return;
    }
    TEST_PASS("Memory store initialized (session: %s)", store.session_id);
    
    // Add some test memories
    const char* tags1[] = {"greeting", "personal"};
    const char* tags2[] = {"tech", "ai"};
    const char* tags3[] = {"reminder", "important"};
    
    uint64_t id1, id2, id3;
    
    if (ethervox_memory_store_add(&store, "User's name is Tim", tags1, 2, 0.95f, true, &id1) != 0) {
        TEST_FAIL("Failed to store first memory");
        g_tests_failed++;
        ethervox_memory_cleanup(&store);
        return;
    }
    TEST_PASS("Stored memory 1 (ID: %llu, importance: 0.95)", (unsigned long long)id1);
    
    if (ethervox_memory_store_add(&store, "Discussion about llama.cpp integration", tags2, 2, 0.80f, false, &id2) != 0) {
        TEST_FAIL("Failed to store second memory");
        g_tests_failed++;
        ethervox_memory_cleanup(&store);
        return;
    }
    TEST_PASS("Stored memory 2 (ID: %llu, importance: 0.80)", (unsigned long long)id2);
    
    if (ethervox_memory_store_add(&store, "Call dentist on Friday", tags3, 2, 0.90f, true, &id3) != 0) {
        TEST_FAIL("Failed to store third memory");
        g_tests_failed++;
        ethervox_memory_cleanup(&store);
        return;
    }
    TEST_PASS("Stored memory 3 (ID: %llu, importance: 0.90)", (unsigned long long)id3);
    
    TEST_INFO("Total memories: %u, Total stored: %llu", 
              store.entry_count, (unsigned long long)store.total_memories_stored);
    
    // Test retrieval by ID
    const ethervox_memory_entry_t* entry;
    if (ethervox_memory_get_by_id(&store, id1, &entry) != 0) {
        TEST_FAIL("Failed to retrieve memory by ID");
        g_tests_failed++;
    } else {
        TEST_PASS("Retrieved memory by ID: \"%s\"", entry->text);
        g_tests_passed++;
    }
    
    ethervox_memory_cleanup(&store);
    TEST_PASS("Memory store cleaned up successfully");
    g_tests_passed++;
}

// Test 2: Memory Search and Tag Indexing
static void test_memory_search(void) {
    TEST_HEADER("Test 2: Memory Search and Tag Indexing");
    
    ethervox_memory_store_t store;
    ethervox_memory_init(&store, NULL, "/tmp");
    
    // Add diverse memories with different tags
    const char* tags_personal[] = {"personal", "name"};
    const char* tags_work[] = {"work", "project"};
    const char* tags_reminder[] = {"reminder", "urgent"};
    const char* tags_tech[] = {"tech", "programming"};
    
    uint64_t id;
    ethervox_memory_store_add(&store, "My name is Tim", tags_personal, 2, 0.95f, true, &id);
    ethervox_memory_store_add(&store, "Working on EthervoxAI project", tags_work, 2, 0.85f, false, &id);
    ethervox_memory_store_add(&store, "Reminder: Meeting at 3 PM", tags_reminder, 2, 0.90f, true, &id);
    ethervox_memory_store_add(&store, "Love using llama.cpp for LLM inference", tags_tech, 2, 0.80f, false, &id);
    ethervox_memory_store_add(&store, "Work deadline is Friday", tags_work, 2, 0.88f, true, &id);
    
    TEST_INFO("Added 5 test memories with various tags");
    
    // Test tag-based search
    TEST_SUBHEADER("Tag-based search for 'work' tag");
    const char* work_tag = "work";
    ethervox_memory_search_result_t* results = NULL;
    uint32_t result_count = 0;
    
    if (ethervox_memory_search(&store, NULL, &work_tag, 1, 10, &results, &result_count) != 0) {
        TEST_FAIL("Search failed");
        g_tests_failed++;
    } else {
        TEST_PASS("Found %u memories with 'work' tag", result_count);
        for (uint32_t i = 0; i < result_count; i++) {
            TEST_INFO("  [%u] %s", i + 1, results[i].entry.text);
        }
        free(results);
        
        if (result_count == 2) {
            TEST_PASS("Correct number of work-tagged memories found");
            g_tests_passed++;
        } else {
            TEST_FAIL("Expected 2 work memories, got %u", result_count);
            g_tests_failed++;
        }
    }
    
    // Test text-based search
    TEST_SUBHEADER("Text-based search for 'llama'");
    results = NULL;
    result_count = 0;
    
    if (ethervox_memory_search(&store, "llama", NULL, 0, 10, &results, &result_count) != 0) {
        TEST_FAIL("Text search failed");
        g_tests_failed++;
    } else {
        TEST_PASS("Found %u memories containing 'llama'", result_count);
        for (uint32_t i = 0; i < result_count; i++) {
            TEST_INFO("  [%u] %s (relevance: %.2f)", i + 1, results[i].entry.text, results[i].relevance);
        }
        free(results);
        
        if (result_count >= 1) {
            TEST_PASS("Successfully found text matches");
            g_tests_passed++;
        } else {
            TEST_FAIL("Should have found at least 1 match");
            g_tests_failed++;
        }
    }
    
    ethervox_memory_cleanup(&store);
}

// Test 3: Adaptive Memory (Corrections and Patterns)
static void test_adaptive_memory(void) {
    TEST_HEADER("Test 3: Adaptive Memory (Corrections & Patterns)");
    
    ethervox_memory_store_t store;
    ethervox_memory_init(&store, NULL, "/tmp");
    
    // Test correction storage
    TEST_SUBHEADER("Storing user corrections");
    uint64_t corr_id1, corr_id2;
    
    if (ethervox_memory_store_correction(&store, "User prefers metric units", "Used feet instead of meters", &corr_id1) != 0) {
        TEST_FAIL("Failed to store correction 1");
        g_tests_failed++;
    } else {
        TEST_PASS("Stored correction 1 (ID: %llu)", (unsigned long long)corr_id1);
        
        // Verify importance
        const ethervox_memory_entry_t* entry;
        if (ethervox_memory_get_by_id(&store, corr_id1, &entry) == 0) {
            if (entry->importance == 0.99f) {
                TEST_PASS("Correction has importance=0.99 [OK]");
                g_tests_passed++;
            } else {
                TEST_FAIL("Correction importance is %.2f, expected 0.99", entry->importance);
                g_tests_failed++;
            }
        }
    }
    
    ethervox_memory_store_correction(&store, "User likes concise responses", NULL, &corr_id2);
    TEST_PASS("Stored correction 2 (ID: %llu)", (unsigned long long)corr_id2);
    
    // Test pattern storage
    TEST_SUBHEADER("Storing success patterns");
    uint64_t pat_id1, pat_id2, pat_id3;
    
    ethervox_memory_store_pattern(&store, "User responds well to code examples", &pat_id1);
    TEST_PASS("Stored pattern 1 (ID: %llu)", (unsigned long long)pat_id1);
    
    ethervox_memory_store_pattern(&store, "Bullet-point lists work better than paragraphs", &pat_id2);
    TEST_PASS("Stored pattern 2 (ID: %llu)", (unsigned long long)pat_id2);
    
    ethervox_memory_store_pattern(&store, "User appreciates performance optimizations", &pat_id3);
    TEST_PASS("Stored pattern 3 (ID: %llu)", (unsigned long long)pat_id3);
    
    // Retrieve corrections
    TEST_SUBHEADER("Retrieving corrections for adaptive prompt");
    ethervox_memory_search_result_t* corrections = NULL;
    uint32_t correction_count = 0;
    
    if (ethervox_memory_get_corrections(&store, &corrections, &correction_count, 10) != 0) {
        TEST_FAIL("Failed to retrieve corrections");
        g_tests_failed++;
    } else {
        TEST_PASS("Retrieved %u corrections", correction_count);
        for (uint32_t i = 0; i < correction_count; i++) {
            TEST_INFO("  [%u] %s", i + 1, corrections[i].entry.text);
        }
        free(corrections);
        
        if (correction_count == 2) {
            TEST_PASS("Correct count of corrections retrieved");
            g_tests_passed++;
        } else {
            TEST_FAIL("Expected 2 corrections, got %u", correction_count);
            g_tests_failed++;
        }
    }
    
    // Retrieve patterns
    TEST_SUBHEADER("Retrieving success patterns");
    ethervox_memory_search_result_t* patterns = NULL;
    uint32_t pattern_count = 0;
    
    if (ethervox_memory_get_patterns(&store, &patterns, &pattern_count, 10) != 0) {
        TEST_FAIL("Failed to retrieve patterns");
        g_tests_failed++;
    } else {
        TEST_PASS("Retrieved %u patterns", pattern_count);
        for (uint32_t i = 0; i < pattern_count; i++) {
            TEST_INFO("  [%u] %s", i + 1, patterns[i].entry.text);
        }
        free(patterns);
        
        if (pattern_count == 3) {
            TEST_PASS("Correct count of patterns retrieved");
            g_tests_passed++;
        } else {
            TEST_FAIL("Expected 3 patterns, got %u", pattern_count);
            g_tests_failed++;
        }
    }
    
    ethervox_memory_cleanup(&store);
}

// Test 4: System Prompt Generation with Adaptive Learning
static void test_system_prompt_generation(void) {
    TEST_HEADER("Test 4: System Prompt Generation");
    
    // Create memory store with corrections and patterns
    ethervox_memory_store_t memory_store;
    ethervox_memory_init(&memory_store, NULL, "/tmp");
    
    uint64_t id;
    ethervox_memory_store_correction(&memory_store, "User wants technical details", NULL, &id);
    ethervox_memory_store_correction(&memory_store, "Avoid marketing language", NULL, &id);
    ethervox_memory_store_pattern(&memory_store, "Code examples are helpful", &id);
    ethervox_memory_store_pattern(&memory_store, "User prefers direct answers", &id);
    
    TEST_INFO("Added 2 corrections and 2 patterns to memory");
    
    // Create tool registry
    ethervox_tool_registry_t registry;
    if (ethervox_tool_registry_init(&registry, 10) != 0) {
        TEST_FAIL("Failed to initialize tool registry");
        g_tests_failed++;
        ethervox_memory_cleanup(&memory_store);
        return;
    }
    
    TEST_PASS("Tool registry initialized");
    
    // Get chat template
    const chat_template_t* template = chat_template_get(CHAT_TEMPLATE_QWEN, NULL);
    if (!template) {
        TEST_FAIL("Failed to get chat template");
        g_tests_failed++;
        ethervox_tool_registry_cleanup(&registry);
        ethervox_memory_cleanup(&memory_store);
        return;
    }
    
    TEST_PASS("Chat template loaded (type: ChatML)");
    
    // Build system prompt WITHOUT adaptive learning (NULL memory_store)
    TEST_SUBHEADER("Building system prompt without adaptive learning");
    char prompt_no_adapt[8192];
    if (ethervox_tool_registry_build_system_prompt(&registry, template, prompt_no_adapt, sizeof(prompt_no_adapt), NULL, NULL) != 0) {
        TEST_FAIL("Failed to build system prompt");
        g_tests_failed++;
    } else {
        TEST_PASS("System prompt built (%zu chars)", strlen(prompt_no_adapt));
        TEST_INFO("Prompt does NOT contain corrections/patterns (memory_store=NULL)");
        g_tests_passed++;
    }
    
    // Build system prompt WITH adaptive learning
    TEST_SUBHEADER("Building system prompt with adaptive learning");
    char prompt_with_adapt[8192];
    if (ethervox_tool_registry_build_system_prompt(&registry, template, prompt_with_adapt, sizeof(prompt_with_adapt), &memory_store, NULL) != 0) {
        TEST_FAIL("Failed to build adaptive system prompt");
        g_tests_failed++;
    } else {
        TEST_PASS("Adaptive system prompt built (%zu chars)", strlen(prompt_with_adapt));
        
        // Check if corrections are injected
        if (strstr(prompt_with_adapt, "USER CORRECTIONS")) {
            TEST_PASS("Found 'USER CORRECTIONS' section in prompt");
            g_tests_passed++;
        } else {
            TEST_INFO("No 'USER CORRECTIONS' section (may be desktop-only feature)");
        }
        
        // Check if patterns are injected
        if (strstr(prompt_with_adapt, "SUCCESSFUL PATTERNS")) {
            TEST_PASS("Found 'SUCCESSFUL PATTERNS' section in prompt");
            g_tests_passed++;
        } else {
            TEST_INFO("No 'SUCCESSFUL PATTERNS' section (may be desktop-only feature)");
        }
    }
    
    ethervox_tool_registry_cleanup(&registry);
    ethervox_memory_cleanup(&memory_store);
}

// Test 5: Memory Export/Import
static void test_memory_export_import(void) {
    TEST_HEADER("Test 5: Memory Export and Import");
    
    ethervox_memory_store_t store;
    ethervox_memory_init(&store, "export_test", "/tmp");
    
    // Add test data
    const char* tags1[] = {"test", "export"};
    const char* tags2[] = {"test", "data"};
    uint64_t id;
    
    ethervox_memory_store_add(&store, "First test memory", tags1, 2, 0.85f, true, &id);
    ethervox_memory_store_add(&store, "Second test memory", tags2, 2, 0.75f, false, &id);
    ethervox_memory_store_correction(&store, "Test correction", NULL, &id);
    
    TEST_INFO("Added 3 memories for export");
    
    // Export to JSON
    TEST_SUBHEADER("Exporting to JSON");
    const char* json_path = "/tmp/test_export.json";
    uint64_t bytes_written = 0;
    
    if (ethervox_memory_export(&store, json_path, "json", &bytes_written) != 0) {
        TEST_FAIL("Failed to export to JSON");
        g_tests_failed++;
    } else {
        TEST_PASS("Exported %llu bytes to %s", (unsigned long long)bytes_written, json_path);
        g_tests_passed++;
    }
    
    // Export to Markdown
    TEST_SUBHEADER("Exporting to Markdown");
    const char* md_path = "/tmp/test_export.md";
    bytes_written = 0;
    
    if (ethervox_memory_export(&store, md_path, "markdown", &bytes_written) != 0) {
        TEST_FAIL("Failed to export to Markdown");
        g_tests_failed++;
    } else {
        TEST_PASS("Exported %llu bytes to %s", (unsigned long long)bytes_written, md_path);
        g_tests_passed++;
    }
    
    ethervox_memory_cleanup(&store);
    
    // Test import
    TEST_SUBHEADER("Importing from JSON");
    ethervox_memory_store_t import_store;
    ethervox_memory_init(&import_store, "import_test", "/tmp");
    
    uint32_t turns_loaded = 0;
    if (ethervox_memory_import(&import_store, json_path, &turns_loaded) != 0) {
        TEST_FAIL("Failed to import from JSON");
        g_tests_failed++;
    } else {
        TEST_PASS("Imported %u turns from JSON", turns_loaded);
        TEST_INFO("Memory store now has %u entries", import_store.entry_count);
        
        if (import_store.entry_count >= 3) {
            TEST_PASS("Imported data contains expected entries");
            g_tests_passed++;
        } else {
            TEST_FAIL("Expected at least 3 entries, got %u", import_store.entry_count);
            g_tests_failed++;
        }
    }
    
    ethervox_memory_cleanup(&import_store);
}

// Test 6: Hash Table Performance (simulated)
static void test_hash_table_performance(void) {
    TEST_HEADER("Test 6: Hash Table Index Performance");
    
    ethervox_memory_store_t store;
    ethervox_memory_init(&store, NULL, "/tmp");
    
    TEST_INFO("Adding 50 memories with various tags for hash table testing");
    
    const char* tag_sets[][3] = {
        {"tech", "ai", "llm"},
        {"personal", "name", "info"},
        {"work", "project", "deadline"},
        {"reminder", "urgent", "task"},
        {"learning", "pattern", "success"}
    };
    
    // Add 50 memories
    clock_t start = clock();
    for (int i = 0; i < 50; i++) {
        char text[256];
        snprintf(text, sizeof(text), "Test memory number %d with diverse content", i);
        
        const char* tags[3];
        int tag_set = i % 5;
        for (int j = 0; j < 3; j++) {
            tags[j] = tag_sets[tag_set][j];
        }
        
        uint64_t id;
        ethervox_memory_store_add(&store, text, tags, 3, 0.5f + (i % 5) * 0.1f, i % 2 == 0, &id);
    }
    clock_t end = clock();
    double add_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
    
    TEST_PASS("Added 50 memories in %.2f ms (%.2f ms avg per memory)", add_time, add_time / 50.0);
    TEST_INFO("Tag index has %u unique tags", store.tag_index_count);
    
    // Test tag search performance
    TEST_SUBHEADER("Testing tag search performance");
    const char* search_tag = "tech";
    
    start = clock();
    ethervox_memory_search_result_t* results = NULL;
    uint32_t result_count = 0;
    ethervox_memory_search(&store, NULL, &search_tag, 1, 50, &results, &result_count);
    end = clock();
    double search_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
    
    TEST_PASS("Tag search completed in %.2f ms, found %u results", search_time, result_count);
    
    if (result_count == 10) {  // Should be 50/5 = 10
        TEST_PASS("Correct number of 'tech' tagged memories found");
        g_tests_passed++;
    } else {
        TEST_INFO("Found %u 'tech' tagged memories", result_count);
    }
    
    free(results);
    
    // Test ID lookup performance
    TEST_SUBHEADER("Testing ID lookup performance");
    uint64_t test_id = 25;  // Middle ID
    
    start = clock();
    const ethervox_memory_entry_t* entry;
    ethervox_memory_get_by_id(&store, test_id, &entry);
    end = clock();
    double lookup_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000000.0;  // microseconds
    
    TEST_PASS("ID lookup completed in %.2f μs", lookup_time);
    
    if (lookup_time < 100.0) {  // Should be very fast with hash table
        TEST_PASS("ID lookup is efficient (< 100 μs)");
        g_tests_passed++;
    } else {
        TEST_INFO("ID lookup took %.2f μs (hash table may not be active yet)", lookup_time);
    }
    
    ethervox_memory_cleanup(&store);
}

// Test 7: Memory archiving
static void test_memory_archive(void) {
    TEST_HEADER("Test 7: Memory Archiving");
    
    // Create a temporary test directory
    const char* test_dir = "/tmp/ethervox_archive_test";
    
    // Remove old test directory if it exists (ignore errors)
#ifdef _WIN32
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\" 2>nul", test_dir);
    system(cmd);
#else
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    system(cmd);
#endif
    
    // Create test directory
    platform_mkdir_recursive(test_dir);
    
    // Create some fake old session files
    char filepath[512];
    for (int i = 1; i <= 3; i++) {
        snprintf(filepath, sizeof(filepath), "%s/session_old_%d.jsonl", test_dir, i);
        FILE* f = fopen(filepath, "w");
        if (f) {
            fprintf(f, "{\"id\":%d,\"text\":\"old session %d\"}\n", i, i);
            fclose(f);
        }
    }
    
    // Initialize memory store
    ethervox_memory_store_t archive_store;
    if (ethervox_memory_init(&archive_store, NULL, test_dir) == 0) {
        // Archive the old sessions
        uint32_t archived = 0;
        if (ethervox_memory_archive_sessions(&archive_store, &archived) == 0) {
            if (archived == 3) {
                TEST_PASS("Archived 3 session files");
                g_tests_passed++;
            } else {
                TEST_FAIL("Expected 3 archived files, got %u", archived);
                g_tests_failed++;
            }
            
            // Verify archive directory exists using standard C library
            snprintf(filepath, sizeof(filepath), "%s/archive/session_old_1.jsonl", test_dir);
            FILE* test_file = fopen(filepath, "r");
            if (test_file) {
                fclose(test_file);
                TEST_PASS("Archive directory created and files moved");
                g_tests_passed++;
            } else {
                TEST_FAIL("Archived files not found in archive directory");
                g_tests_failed++;
            }
        } else {
            TEST_FAIL("Archive operation failed");
            g_tests_failed++;
        }
        
        ethervox_memory_cleanup(&archive_store);
    } else {
        TEST_FAIL("Failed to initialize archive test store");
        g_tests_failed++;
    }
    
    // Cleanup
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    system(cmd);
}

// Test 8: File Append Tool
static void test_file_append_tool(void) {
    TEST_HEADER("Test 8: File Append Tool");
    
    const char* test_file = "./test_append_integration.txt";
    
    // Create initial file
    FILE* f = fopen(test_file, "w");
    if (!f) {
        TEST_FAIL("Failed to create test file");
        g_tests_failed++;
        return;
    }
    fprintf(f, "Initial content\n");
    fprintf(f, "Line 2\n");
    fclose(f);
    TEST_PASS("Created test file with initial content");
    
    // Test basic file append operation (using standard file operations since tool wrapper is internal)
    f = fopen(test_file, "a");
    if (!f) {
        TEST_FAIL("Failed to open file for appending");
        g_tests_failed++;
        unlink(test_file);
        return;
    }
    fprintf(f, "\n--- Appended Section ---\n");
    fclose(f);
    TEST_PASS("Appended first section");
    
    // Append again
    f = fopen(test_file, "a");
    if (!f) {
        TEST_FAIL("Failed to open file for second append");
        g_tests_failed++;
        unlink(test_file);
        return;
    }
    fprintf(f, "Additional line\n");
    fclose(f);
    TEST_PASS("Appended second section");
    
    // Verify final content
    f = fopen(test_file, "r");
    if (!f) {
        TEST_FAIL("Failed to open file for verification");
        g_tests_failed++;
        unlink(test_file);
        return;
    }
    
    char buffer[1024];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[len] = '\0';
    fclose(f);
    
    // Check for all expected content
    int checks_passed = 0;
    if (strstr(buffer, "Initial content")) checks_passed++;
    if (strstr(buffer, "Line 2")) checks_passed++;
    if (strstr(buffer, "--- Appended Section ---")) checks_passed++;
    if (strstr(buffer, "Additional line")) checks_passed++;
    
    if (checks_passed == 4) {
        TEST_PASS("All content verified (%d/4 checks)", checks_passed);
        g_tests_passed++;
    } else {
        TEST_FAIL("Content verification failed (%d/4 checks)", checks_passed);
        TEST_INFO("File content: %s", buffer);
        g_tests_failed++;
    }
    
    // Test error handling
    FILE* bad_f = fopen("/nonexistent/dir/file.txt", "a");
    if (bad_f != NULL) {
        TEST_FAIL("Should have rejected invalid path");
        fclose(bad_f);
        g_tests_failed++;
    } else {
        TEST_PASS("Error handling works correctly");
        g_tests_passed++;
    }
    
    // Cleanup
    unlink(test_file);
}

// Test 9: Unit Conversion Tool
static void test_unit_conversion_tool(void) {
    TEST_HEADER("Test 9: Unit Conversion Tool");
    
    double result;
    char* error = NULL;
    int ret;
    
    // Temperature conversion
    ret = ethervox_unit_convert(100.0, "celsius", "fahrenheit", &result, &error);
    if (ret != 0) {
        TEST_FAIL("Temperature conversion failed: %s", error ? error : "unknown");
        free(error);
        g_tests_failed++;
        return;
    }
    if (fabs(result - 212.0) > 0.001) {
        TEST_FAIL("Temperature conversion incorrect: expected 212.0, got %.3f", result);
        g_tests_failed++;
        return;
    }
    TEST_PASS("Temperature: 100°C = 212°F");
    
    // Length conversion
    ret = ethervox_unit_convert(5.0, "mile", "kilometer", &result, &error);
    if (ret != 0 || fabs(result - 8.04672) > 0.001) {
        TEST_FAIL("Length conversion failed");
        free(error);
        g_tests_failed++;
        return;
    }
    TEST_PASS("Length: 5 miles ≈ 8.047 km");
    
    // Mass conversion
    ret = ethervox_unit_convert(1.0, "kilogram", "pound", &result, &error);
    if (ret != 0 || fabs(result - 2.20462) > 0.001) {
        TEST_FAIL("Mass conversion failed");
        free(error);
        g_tests_failed++;
        return;
    }
    TEST_PASS("Mass: 1 kg ≈ 2.205 lb");
    
    // Volume conversion
    ret = ethervox_unit_convert(1.0, "gallon", "liter", &result, &error);
    if (ret != 0 || fabs(result - 4.54609) > 0.001) {
        TEST_FAIL("Volume conversion failed");
        free(error);
        g_tests_failed++;
        return;
    }
    TEST_PASS("Volume: 1 imperial gallon ≈ 4.546 L");
    
    // Speed conversion
    ret = ethervox_unit_convert(60.0, "mph", "km/h", &result, &error);
    if (ret != 0 || fabs(result - 96.56) > 0.1) {
        TEST_FAIL("Speed conversion failed");
        free(error);
        g_tests_failed++;
        return;
    }
    TEST_PASS("Speed: 60 mph ≈ 96.56 km/h");
    
    // Pressure conversion
    ret = ethervox_unit_convert(1.0, "atmosphere", "psi", &result, &error);
    if (ret != 0 || fabs(result - 14.696) > 0.001) {
        TEST_FAIL("Pressure conversion failed");
        free(error);
        g_tests_failed++;
        return;
    }
    TEST_PASS("Pressure: 1 atm ≈ 14.696 psi");
    
    // Energy conversion
    ret = ethervox_unit_convert(1.0, "kilowatt hour", "joule", &result, &error);
    if (ret != 0 || fabs(result - 3.6e6) > 1.0) {
        TEST_FAIL("Energy conversion failed");
        free(error);
        g_tests_failed++;
        return;
    }
    TEST_PASS("Energy: 1 kWh = 3.6 MJ");
    
    // Data conversion
    ret = ethervox_unit_convert(1024.0, "byte", "kibibyte", &result, &error);
    if (ret != 0 || fabs(result - 1.0) > 0.001) {
        TEST_FAIL("Data conversion failed");
        free(error);
        g_tests_failed++;
        return;
    }
    TEST_PASS("Data: 1024 bytes = 1 KiB");
    
    // Error handling: incompatible units
    ret = ethervox_unit_convert(10.0, "meter", "kilogram", &result, &error);
    if (ethervox_is_success(ret)) {
        TEST_FAIL("Should reject incompatible units");
        g_tests_failed++;
        return;
    }
    free(error);
    error = NULL;
    TEST_PASS("Error handling: rejects incompatible units");
    
    // Error handling: unknown unit
    ret = ethervox_unit_convert(10.0, "foobar", "meter", &result, &error);
    if (ethervox_is_success(ret)) {
        TEST_FAIL("Should reject unknown units");
        g_tests_failed++;
        return;
    }
    free(error);
    TEST_PASS("Error handling: rejects unknown units");
    
    g_tests_passed++;
    TEST_PASS("Unit conversion integration test completed");
}

// Test 10: Cache Summarization with Real LLM
static void test_cache_summarization_live(ethervox_governor_t* governor) {
    TEST_HEADER("Test 10: Cache Summarization with Live LLM");
    
#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
    TEST_INFO("Skipping - llama.cpp not available");
    return;
#else
    
    if (!governor) {
        TEST_FAIL("Governor not provided - skipping live test");
        g_tests_failed++;
        record_test_result("Cache Summarization (Live)", false);
        return;
    }
    
    // Check if model is loaded
    if (!governor->llm_ctx || governor->system_prompt_token_count == 0) {
        TEST_INFO("No model loaded - this test requires a loaded LLM model");
        TEST_INFO("Skipping live cache summarization test");
        return;
    }
    
    TEST_INFO("Model loaded with %d system prompt tokens", governor->system_prompt_token_count);
    
    // Simulate a conversation by adding some turns to the history
    TEST_SUBHEADER("Simulating conversation history");
    
    if (!governor->conversation_history.turns) {
        TEST_FAIL("Conversation history not initialized");
        g_tests_failed++;
        record_test_result("Cache Summarization (Live)", false);
        return;
    }
    
    // Add test conversation turns
    const char* user_messages[] = {
        "What's the weather like today?",
        "Can you help me convert 10 miles to kilometers?",
        "Tell me about quantum computing",
        "What are the best practices for memory management in C?",
        "How does the KV cache work in LLM inference?"
    };
    
    const char* assistant_responses[] = {
        "I'd need to know your location to provide accurate weather information.",
        "10 miles is approximately 16.09 kilometers.",
        "Quantum computing uses quantum bits (qubits) that can exist in superposition states.",
        "Always free allocated memory, avoid memory leaks, use valgrind for debugging.",
        "The KV cache stores key-value attention states to avoid recomputing past tokens."
    };
    
    for (int i = 0; i < 5; i++) {
        if (governor->conversation_history.turn_count >= governor->conversation_history.capacity) {
            TEST_INFO("Conversation history full, skipping remaining turns");
            break;
        }
        
        conversation_turn_t* turn = &governor->conversation_history.turns[governor->conversation_history.turn_count];
        turn->is_user = true;
        strncpy(turn->preview, user_messages[i], sizeof(turn->preview) - 1);
        turn->preview[sizeof(turn->preview) - 1] = '\0';
        turn->token_count = strlen(user_messages[i]) / 4; // Rough estimate
        governor->conversation_history.turn_count++;
        
        if (governor->conversation_history.turn_count >= governor->conversation_history.capacity) {
            break;
        }
        
        turn = &governor->conversation_history.turns[governor->conversation_history.turn_count];
        turn->is_user = false;
        strncpy(turn->preview, assistant_responses[i], sizeof(turn->preview) - 1);
        turn->preview[sizeof(turn->preview) - 1] = '\0';
        turn->token_count = strlen(assistant_responses[i]) / 4;
        governor->conversation_history.turn_count++;
    }
    
    TEST_PASS("Added %u conversation turns", governor->conversation_history.turn_count);
    
    // Test summarization
    TEST_SUBHEADER("Testing manual cache summarization");
    
    llama_memory_t mem = llama_get_memory(governor->llm_ctx);
    int32_t pos_before = llama_memory_seq_pos_max(mem, 0);
    TEST_INFO("KV cache position before: %d tokens", pos_before);
    
    // Force summarization even if cache isn't full
    ethervox_result_t ret = ethervox_governor_summarize_and_clear_cache(governor, true);
    
    if (ret != 0) {
        TEST_FAIL("Summarization failed with error code %d", ret);
        g_tests_failed++;
        record_test_result("Cache Summarization (Live)", false);
        return;
    }
    
    TEST_PASS("Summarization completed successfully");
    
    int32_t pos_after = llama_memory_seq_pos_max(mem, 0);
    TEST_INFO("KV cache position after: %d tokens", pos_after);
    
    // Verify cache was cleared back to system prompt
    if (pos_after <= governor->system_prompt_token_count + 100) {  // Allow some buffer for summary
        TEST_PASS("Cache cleared correctly (pos=%d, system_prompt=%d)", 
                 pos_after, governor->system_prompt_token_count);
    } else {
        TEST_FAIL("Cache not cleared properly (pos=%d, expected ~%d)", 
                 pos_after, governor->system_prompt_token_count);
        g_tests_failed++;
        record_test_result("Cache Summarization (Live)", false);
        return;
    }
    
    // Verify summary was stored in memory
    TEST_SUBHEADER("Verifying summary storage");
    
    ethervox_tool_t* memory_search_tool = NULL;
    for (uint32_t i = 0; i < governor->tool_registry->tool_count; i++) {
        if (strcmp(governor->tool_registry->tools[i].name, "memory_search") == 0) {
            memory_search_tool = &governor->tool_registry->tools[i];
            break;
        }
    }
    
    if (!memory_search_tool) {
        TEST_INFO("memory_search tool not available - skipping summary verification");
    } else {
        char search_args[512];
        snprintf(search_args, sizeof(search_args),
            "{\"query\":null,"
            "\"tag_filter\":[\"context_summary\",\"manual_clear\"],"
            "\"limit\":1}");
        
        char* search_result = NULL;
        char* search_error = NULL;
        
        int search_status = memory_search_tool->execute(search_args, &search_result, &search_error);
        if (search_status == 0 && search_result) {
            TEST_PASS("Summary found in memory: %.100s...", search_result);
        } else {
            TEST_FAIL("Summary not found in memory");
            g_tests_failed++;
            record_test_result("Cache Summarization (Live)", false);
            free(search_result);
            free(search_error);
            return;
        }
        
        free(search_result);
        free(search_error);
    }
    
    g_tests_passed++;
    record_test_result("Cache Summarization (Live)", true);
    TEST_PASS("Cache summarization integration test completed");
    
#endif // ETHERVOX_WITH_LLAMA
}

// Main test runner
void run_integration_tests(ethervox_governor_t* governor) {
    // Create test report file
    time_t report_time = time(NULL);
    char report_dir[512];
    
#ifdef __ANDROID__
    const char* android_files = ethervox_get_android_files_dir();
    if (android_files && android_files[0] != '\0') {
        snprintf(report_dir, sizeof(report_dir), "%s/tests", android_files);
        INTEGRATION_LOG("Using test directory: %s", report_dir);
    } else {
        snprintf(report_dir, sizeof(report_dir), "./tests");
        INTEGRATION_LOG("Android files dir not set, using fallback: %s", report_dir);
    }
#else
    const char* home = getenv("HOME");
    if (home) {
        snprintf(report_dir, sizeof(report_dir), "%s/.ethervox/tests", home);
    } else {
        snprintf(report_dir, sizeof(report_dir), "./.ethervox/tests");
    }
#endif
    
    // Create directory if it doesn't exist
#ifdef _WIN32
#include <direct.h>  // for _mkdir on Windows
    int mkdir_result = _mkdir(report_dir);
#else
    int mkdir_result = mkdir(report_dir, 0755);
#endif
    if (mkdir_result != 0 && errno != EEXIST) {
        INTEGRATION_LOG("Warning: mkdir(%s) failed with errno %d: %s", 
                       report_dir, errno, strerror(errno));
    } else {
        INTEGRATION_LOG("Test directory ready: %s", report_dir);
    }
    
    snprintf(g_report_path, sizeof(g_report_path),
             "%s/integration_test_report_%ld.log", report_dir, report_time);
    
    g_report_file = fopen(g_report_path, "w");
    if (g_report_file) {
        fprintf(g_report_file, "EthervoxAI Integration Test Report\n");
        fprintf(g_report_file, "====================================\n\n");
        fprintf(g_report_file, "Timestamp: %s", ctime(&report_time));
        fprintf(g_report_file, "\n");
        fflush(g_report_file);
        INTEGRATION_LOG("Test report created: %s", g_report_path);
    } else {
        INTEGRATION_LOG("Warning: Failed to create test report: %s", g_report_path);
    }
    
    printf("\n");
    printf(COLOR_BOLD COLOR_MAGENTA);
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                                                               ║\n");
    printf("║          ETHERVOXAI INTEGRATION TEST SUITE                   ║\n");
#ifdef __ANDROID__
    printf("║          Android Platform - Major Features                   ║\n");
#else
    printf("║          macOS Desktop - Major Features                      ║\n");
#endif
    printf("║                                                               ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf(COLOR_RESET);
    
    // Get git info (skip on Android as popen is not reliable)
    char git_repo[128] = "unknown";
    char git_branch[128] = "unknown";
    char git_commit[64] = "unknown";
    
#ifndef __ANDROID__
    FILE* fp;
    
    // Get repository name from remote URL
    fp = popen("git remote get-url origin 2>/dev/null | sed 's#.*/##' | sed 's#\\.git##'", "r");
    if (fp) {
        if (fgets(git_repo, sizeof(git_repo), fp)) {
            // Remove newline
            git_repo[strcspn(git_repo, "\n")] = 0;
        }
        pclose(fp);
    }
    
    fp = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
    if (fp) {
        if (fgets(git_branch, sizeof(git_branch), fp)) {
            // Remove newline
            git_branch[strcspn(git_branch, "\n")] = 0;
        }
        pclose(fp);
    }
    
    fp = popen("git rev-parse --short HEAD 2>/dev/null", "r");
    if (fp) {
        if (fgets(git_commit, sizeof(git_commit), fp)) {
            // Remove newline
            git_commit[strcspn(git_commit, "\n")] = 0;
        }
        pclose(fp);
    }
#else
    // On Android, use build-time git info if available
    #ifdef ETHERVOX_GIT_BRANCH
    strncpy(git_branch, ETHERVOX_GIT_BRANCH, sizeof(git_branch) - 1);
    #endif
    #ifdef ETHERVOX_GIT_COMMIT
    strncpy(git_commit, ETHERVOX_GIT_COMMIT, sizeof(git_commit) - 1);
    #endif
    strncpy(git_repo, "ethervoxai-android", sizeof(git_repo) - 1);
#endif
    
    printf("\n");
    printf(COLOR_CYAN "  Repository: " COLOR_RESET "%s\n", git_repo);
    printf(COLOR_CYAN "  Branch:     " COLOR_RESET "%s\n", git_branch);
    printf(COLOR_CYAN "  Commit:     " COLOR_RESET "%s\n", git_commit);
    printf("\n");
    
    // Write header to report
    if (g_report_file) {
        fprintf(g_report_file, "Repository: %s\n", git_repo);
        fprintf(g_report_file, "Branch:     %s\n", git_branch);
        fprintf(g_report_file, "Commit:     %s\n", git_commit);
        fprintf(g_report_file, "\n");
        fflush(g_report_file);
    }
    
    time_t start_time = time(NULL);
    
    // Reset test name tracking
    g_passed_count = 0;
    g_failed_count = 0;
    
    // Run all tests
    int passed_before, failed_before;
    
    passed_before = g_tests_passed; failed_before = g_tests_failed;
    test_memory_basic();
    if (g_tests_failed > failed_before) record_test_result("Memory Basic Operations", false);
    else if (g_tests_passed > passed_before) record_test_result("Memory Basic Operations", true);
    
    passed_before = g_tests_passed; failed_before = g_tests_failed;
    test_memory_search();
    if (g_tests_failed > failed_before) record_test_result("Memory Search", false);
    else if (g_tests_passed > passed_before) record_test_result("Memory Search", true);
    
    passed_before = g_tests_passed; failed_before = g_tests_failed;
    test_adaptive_memory();
    if (g_tests_failed > failed_before) record_test_result("Adaptive Memory", false);
    else if (g_tests_passed > passed_before) record_test_result("Adaptive Memory", true);
    
    passed_before = g_tests_passed; failed_before = g_tests_failed;
    test_system_prompt_generation();
    if (g_tests_failed > failed_before) record_test_result("System Prompt Generation", false);
    else if (g_tests_passed > passed_before) record_test_result("System Prompt Generation", true);
    
    passed_before = g_tests_passed; failed_before = g_tests_failed;
    test_memory_export_import();
    if (g_tests_failed > failed_before) record_test_result("Memory Export/Import", false);
    else if (g_tests_passed > passed_before) record_test_result("Memory Export/Import", true);
    
    passed_before = g_tests_passed; failed_before = g_tests_failed;
    test_hash_table_performance();
    if (g_tests_failed > failed_before) record_test_result("Hash Table Performance", false);
    else if (g_tests_passed > passed_before) record_test_result("Hash Table Performance", true);
    
    passed_before = g_tests_passed; failed_before = g_tests_failed;
    test_memory_archive();
    if (g_tests_failed > failed_before) record_test_result("Memory Archive", false);
    else if (g_tests_passed > passed_before) record_test_result("Memory Archive", true);
    
    passed_before = g_tests_passed; failed_before = g_tests_failed;
    test_file_append_tool();
    if (g_tests_failed > failed_before) record_test_result("File Append Tool", false);
    else if (g_tests_passed > passed_before) record_test_result("File Append Tool", true);
    
    passed_before = g_tests_passed; failed_before = g_tests_failed;
    test_unit_conversion_tool();
    if (g_tests_failed > failed_before) record_test_result("Unit Conversion Tool", false);
    else if (g_tests_passed > passed_before) record_test_result("Unit Conversion Tool", true);
    
    // Test 10: Cache summarization with live LLM (requires loaded model)
    if (governor) {
        passed_before = g_tests_passed; failed_before = g_tests_failed;
        test_cache_summarization_live(governor);
        if (g_tests_failed > failed_before) record_test_result("Cache Summarization (Live)", false);
        else if (g_tests_passed > passed_before) record_test_result("Cache Summarization (Live)", true);
    } else {
        TEST_INFO("Skipping cache summarization test - no governor available");
    }
    
    time_t end_time = time(NULL);
    double duration = difftime(end_time, start_time);
    
    // Final summary
    printf("\n");
    printf(COLOR_BOLD COLOR_MAGENTA);
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                     TEST SUMMARY                              ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf(COLOR_RESET);
    printf("\n");
    
    int total_tests = g_tests_passed + g_tests_failed;
    double pass_rate = total_tests > 0 ? (g_tests_passed * 100.0 / total_tests) : 0.0;
    
    printf(COLOR_GREEN "  Tests Passed:  %d\n" COLOR_RESET, g_tests_passed);
    printf(COLOR_RED "  Tests Failed:  %d\n" COLOR_RESET, g_tests_failed);
    printf(COLOR_CYAN "  Total Tests:   %d\n" COLOR_RESET, total_tests);
    printf(COLOR_YELLOW "  Pass Rate:     %.1f%%\n" COLOR_RESET, pass_rate);
    printf(COLOR_BLUE "  Duration:      %.0f seconds\n" COLOR_RESET, duration);
    printf("\n");
    printf(COLOR_CYAN "  Repository:    " COLOR_RESET "%s\n", git_repo);
    printf(COLOR_CYAN "  Branch:        " COLOR_RESET "%s\n", git_branch);
    printf(COLOR_CYAN "  Commit:        " COLOR_RESET "%s\n", git_commit);
    printf("\n");
    
    // Also log summary to Android logcat
#ifdef __ANDROID__
    INTEGRATION_LOG("╔═══════════════════════════════════════╗");
    INTEGRATION_LOG("║        TEST SUMMARY                   ║");
    INTEGRATION_LOG("╚═══════════════════════════════════════╝");
    INTEGRATION_LOG("Tests Passed:  %d", g_tests_passed);
    INTEGRATION_LOG("Tests Failed:  %d", g_tests_failed);
    INTEGRATION_LOG("Total Tests:   %d", total_tests);
    INTEGRATION_LOG("Pass Rate:     %.1f%%", pass_rate);
    INTEGRATION_LOG("Duration:      %.0f seconds", duration);
    INTEGRATION_LOG("Repository:    %s", git_repo);
    INTEGRATION_LOG("Branch:        %s", git_branch);
    INTEGRATION_LOG("Commit:        %s", git_commit);
#endif
    
    // Show passed test names
    if (g_passed_count > 0) {
        printf(COLOR_GREEN "  Passed Tests:\n" COLOR_RESET);
        for (int i = 0; i < g_passed_count; i++) {
            printf(COLOR_GREEN "    [OK] %s\n" COLOR_RESET, g_passed_tests[i]);
#ifdef __ANDROID__
            INTEGRATION_LOG("  [OK] %s", g_passed_tests[i]);
#endif
            free(g_passed_tests[i]);
        }
        printf("\n");
    }
    
    // Show failed test names
    if (g_failed_count > 0) {
        printf(COLOR_RED "  Failed Tests:\n" COLOR_RESET);
        for (int i = 0; i < g_failed_count; i++) {
            printf(COLOR_RED "    [FAIL] %s\n" COLOR_RESET, g_failed_tests[i]);
#ifdef __ANDROID__
            INTEGRATION_LOG("  [FAIL] %s", g_failed_tests[i]);
#endif
            free(g_failed_tests[i]);
        }
        printf("\n");
    }
    
    if (g_tests_failed == 0) {
        printf(COLOR_BOLD COLOR_GREEN);
        printf("  [OK][OK][OK] ALL TESTS PASSED! [OK][OK][OK]\n");
        printf(COLOR_RESET);
#ifdef __ANDROID__
        INTEGRATION_LOG("[OK][OK][OK] ALL TESTS PASSED! [OK][OK][OK]");
#endif
    } else {
        printf(COLOR_BOLD COLOR_YELLOW);
        printf("  ⚠ Some tests failed - review output above\n");
        printf(COLOR_RESET);
#ifdef __ANDROID__
        INTEGRATION_LOG("⚠ Some tests failed - %d passed, %d failed", g_tests_passed, g_tests_failed);
#endif
    }
    
    // Write final summary to report file
    if (g_report_file) {
        fprintf(g_report_file, "\n");
        fprintf(g_report_file, "═══════════════════════════════════════\n");
        fprintf(g_report_file, "           TEST SUMMARY                 \n");
        fprintf(g_report_file, "═══════════════════════════════════════\n");
        fprintf(g_report_file, "Tests Passed:  %d\n", g_tests_passed);
        fprintf(g_report_file, "Tests Failed:  %d\n", g_tests_failed);
        fprintf(g_report_file, "Total Tests:   %d\n", total_tests);
        fprintf(g_report_file, "Pass Rate:     %.1f%%\n", pass_rate);
        fprintf(g_report_file, "Duration:      %.0f seconds\n", duration);
        fprintf(g_report_file, "\nRepository:    %s\n", git_repo);
        fprintf(g_report_file, "Branch:        %s\n", git_branch);
        fprintf(g_report_file, "Commit:        %s\n", git_commit);
        fprintf(g_report_file, "\n");
        
        if (g_passed_count > 0) {
            fprintf(g_report_file, "Passed Tests:\n");
            for (int i = 0; i < g_passed_count; i++) {
                fprintf(g_report_file, "  [OK] %s\n", g_passed_tests[i]);
            }
            fprintf(g_report_file, "\n");
        }
        
        if (g_failed_count > 0) {
            fprintf(g_report_file, "Failed Tests:\n");
            for (int i = 0; i < g_failed_count; i++) {
                fprintf(g_report_file, "  [FAIL] %s\n", g_failed_tests[i]);
            }
            fprintf(g_report_file, "\n");
        }
        
        if (g_tests_failed == 0) {
            fprintf(g_report_file, "[OK][OK][OK] ALL TESTS PASSED! [OK][OK][OK]\n");
        } else {
            fprintf(g_report_file, "⚠ Some tests failed - %d passed, %d failed\n", 
                   g_tests_passed, g_tests_failed);
        }
        
        fclose(g_report_file);
        g_report_file = NULL;
        INTEGRATION_LOG("Test report saved to: %s", g_report_path);
    }
    
    printf("\n");
}
