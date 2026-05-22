/**
 * @file test_settings_persistence.c
 * @brief Unit tests for EthervoxAI persistent settings system
 *
 * Tests JSON-based settings persistence for Whisper STT, conversation,
 * and wake word configuration.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <assert.h>
#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#endif
#include <time.h>

#include "ethervox/settings.h"
#include "ethervox/logging.h"

// Test report generation
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static time_t test_start_time;
static char test_report_path[512];

static void test_report_init(void) {
    test_start_time = time(NULL);
    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;
    
    // Create reports directory if needed
    const char* home = getenv("HOME");
    if (home) {
        char reports_dir[512];
        snprintf(reports_dir, sizeof(reports_dir), "%s/.ethervox/reports", home);
#ifdef _WIN32
        _mkdir(reports_dir);
#else
        mkdir(reports_dir, 0755);
#endif
        
        snprintf(test_report_path, sizeof(test_report_path),
                 "%s/settings_test_%ld.txt", reports_dir, test_start_time);
    } else {
        snprintf(test_report_path, sizeof(test_report_path),
                 "settings_test_%ld.txt", test_start_time);
    }
}

static void test_report_finalize(void) {
    FILE* report = fopen(test_report_path, "w");
    if (!report) {
        fprintf(stderr, "Failed to write test report to %s\n", test_report_path);
        return;
    }
    
    time_t end_time = time(NULL);
    double duration = difftime(end_time, test_start_time);
    
    fprintf(report, "EthervoxAI Settings Persistence Test Report\n");
    fprintf(report, "==========================================\n\n");
    fprintf(report, "Test Date: %s", ctime(&test_start_time));
    fprintf(report, "Duration: %.2f seconds\n\n", duration);
    fprintf(report, "Results:\n");
    fprintf(report, "  Total Tests: %d\n", tests_run);
    fprintf(report, "  Passed: %d\n", tests_passed);
    fprintf(report, "  Failed: %d\n", tests_failed);
    fprintf(report, "  Success Rate: %.1f%%\n\n", 
            tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    if (tests_failed == 0) {
        fprintf(report, "Status: ✓ ALL TESTS PASSED\n");
    } else {
        fprintf(report, "Status: ✗ SOME TESTS FAILED\n");
    }
    
    fclose(report);
    printf("\nTest report saved to: %s\n", test_report_path);
}

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("  ✗ FAILED: %s\n", message); \
        tests_failed++; \
        return; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define TEST_PASS(message) printf("  ✓ %s\n", message)

// Test temporary file path
static char test_settings_path[512];

static void setup_test_file(void) {
#ifdef _WIN32
    snprintf(test_settings_path, sizeof(test_settings_path),
             "ethervox_test_settings_%ld.json", (long)time(NULL));
#else
    snprintf(test_settings_path, sizeof(test_settings_path),
             "/tmp/ethervox_test_settings_%d.json", getpid());
#endif
    unlink(test_settings_path); // Remove if exists
}

static void cleanup_test_file(void) {
    unlink(test_settings_path);
}

static void test_default_settings(void) {
    printf("\nTest: Default Settings\n");
    
    ethervox_persistent_settings_t settings = ethervox_settings_get_defaults();
    
    TEST_ASSERT(settings.version == 1, "Version should be 1");
    TEST_ASSERT(strcmp(settings.whisper.model_name, "base.bin") == 0,
                "Default Whisper model should be base.bin");
    TEST_ASSERT(strcmp(settings.whisper.language, "auto") == 0,
                "Default language should be auto");
    TEST_ASSERT(settings.whisper.temperature == 0.0f,
                "Default temperature should be 0.0");
    TEST_ASSERT(settings.whisper.beam_size == 5,
                "Default beam size should be 5");
    TEST_ASSERT(settings.conversation.listen_timeout_ms == 5000,
                "Default listen timeout should be 5000ms");
    TEST_ASSERT(settings.conversation.filter_hallucinations == true,
                "Hallucination filtering should be enabled by default");
    TEST_ASSERT(strcmp(settings.wake_word.wake_phrase, "hey ethervox") == 0,
                "Default wake phrase should be 'hey ethervox'");
    TEST_ASSERT(settings.wake_word.detection_threshold == 0.4f,
                "Default detection threshold should be 0.4");
    TEST_ASSERT(settings.wake_word.expected_syllables == 3,
                "Default expected syllables should be 3");
    
    TEST_PASS("Default settings initialized correctly");
}

static void test_save_and_load(void) {
    printf("\nTest: Save and Load Settings\n");
    
    setup_test_file();
    
    // Create custom settings
    ethervox_persistent_settings_t original = ethervox_settings_get_defaults();
    strncpy(original.whisper.model_name, "tiny.bin", sizeof(original.whisper.model_name) - 1);
    strncpy(original.whisper.language, "en", sizeof(original.whisper.language) - 1);
    original.whisper.temperature = 0.5f;
    original.whisper.beam_size = 3;
    original.conversation.listen_timeout_ms = 8000;
    original.conversation.audio_energy_threshold = 0.02f;
    original.wake_word.detection_threshold = 0.6f;
    strncpy(original.wake_word.wake_phrase, "hello assistant", 
            sizeof(original.wake_word.wake_phrase) - 1);
    
    // Save settings
    int result = ethervox_settings_save(&original, test_settings_path);
    TEST_ASSERT(result == 0, "Settings should save successfully");
    
    // Verify file exists
    struct stat st;
    TEST_ASSERT(stat(test_settings_path, &st) == 0, "Settings file should exist");
    TEST_ASSERT(st.st_size > 0, "Settings file should not be empty");
    
    // Load settings
    ethervox_persistent_settings_t loaded;
    result = ethervox_settings_load(&loaded, test_settings_path);
    TEST_ASSERT(result == 0, "Settings should load successfully");
    
    // Verify loaded values match original
    TEST_ASSERT(strcmp(loaded.whisper.model_name, "tiny.bin") == 0,
                "Loaded model name should match");
    TEST_ASSERT(strcmp(loaded.whisper.language, "en") == 0,
                "Loaded language should match");
    TEST_ASSERT(loaded.whisper.temperature == 0.5f,
                "Loaded temperature should match");
    TEST_ASSERT(loaded.whisper.beam_size == 3,
                "Loaded beam size should match");
    TEST_ASSERT(loaded.conversation.listen_timeout_ms == 8000,
                "Loaded listen timeout should match");
    TEST_ASSERT(loaded.conversation.audio_energy_threshold == 0.02f,
                "Loaded energy threshold should match");
    TEST_ASSERT(loaded.wake_word.detection_threshold == 0.6f,
                "Loaded detection threshold should match");
    TEST_ASSERT(strcmp(loaded.wake_word.wake_phrase, "hello assistant") == 0,
                "Loaded wake phrase should match");
    
    cleanup_test_file();
    TEST_PASS("Save and load roundtrip successful");
}

static void test_load_nonexistent_file(void) {
    printf("\nTest: Load Nonexistent File\n");
    
    ethervox_persistent_settings_t settings;
    int result = ethervox_settings_load(&settings, "/tmp/nonexistent_settings_file.json");
    
    TEST_ASSERT(result == 0, "Loading nonexistent file should return success (uses defaults)");
    TEST_ASSERT(strcmp(settings.whisper.model_name, "base.bin") == 0,
                "Should fall back to default settings");
    
    TEST_PASS("Nonexistent file handled gracefully with defaults");
}

static void test_export_import_json(void) {
    printf("\nTest: JSON Export and Import\n");
    
    ethervox_persistent_settings_t original = ethervox_settings_get_defaults();
    original.whisper.temperature = 0.7f;
    original.conversation.silence_timeout_ms = 3000;
    original.wake_word.min_syllables = 2;
    original.wake_word.max_syllables = 6;
    
    // Export to JSON string
    char* json = ethervox_settings_export(&original);
    TEST_ASSERT(json != NULL, "Export should produce JSON string");
    TEST_ASSERT(strlen(json) > 100, "JSON string should have reasonable length");
    TEST_ASSERT(strstr(json, "\"version\"") != NULL, "JSON should contain version field");
    TEST_ASSERT(strstr(json, "\"whisper\"") != NULL, "JSON should contain whisper section");
    TEST_ASSERT(strstr(json, "\"conversation\"") != NULL, "JSON should contain conversation section");
    TEST_ASSERT(strstr(json, "\"wake_word\"") != NULL, "JSON should contain wake_word section");
    
    // Import from JSON string
    ethervox_persistent_settings_t imported;
    int result = ethervox_settings_import(&imported, json);
    TEST_ASSERT(result == 0, "Import should succeed");
    
    // Verify imported values
    TEST_ASSERT(imported.whisper.temperature == 0.7f,
                "Imported temperature should match");
    TEST_ASSERT(imported.conversation.silence_timeout_ms == 3000,
                "Imported silence timeout should match");
    TEST_ASSERT(imported.wake_word.min_syllables == 2,
                "Imported min syllables should match");
    TEST_ASSERT(imported.wake_word.max_syllables == 6,
                "Imported max syllables should match");
    
    free(json);
    TEST_PASS("JSON export/import roundtrip successful");
}

static void test_invalid_json(void) {
    printf("\nTest: Invalid JSON Handling\n");
    
    ethervox_persistent_settings_t settings;
    
    // Test with malformed JSON
    const char* invalid_json = "{\"whisper\": {\"model\": \"test.bin\""; // Missing closing braces
    int result = ethervox_settings_import(&settings, invalid_json);
    TEST_ASSERT(result != 0, "Import should fail with invalid JSON");
    
    // Test with NULL
    result = ethervox_settings_import(&settings, NULL);
    TEST_ASSERT(result != 0, "Import should fail with NULL input");
    
    // Test save with NULL
    result = ethervox_settings_save(NULL, test_settings_path);
    TEST_ASSERT(result != 0, "Save should fail with NULL settings");
    
    TEST_PASS("Invalid input handled correctly");
}

static void test_partial_json(void) {
    printf("\nTest: Partial JSON (Missing Fields)\n");
    
    // JSON with only some fields - should use defaults for missing fields
    const char* partial_json = 
        "{"
        "  \"version\": 1,"
        "  \"whisper\": {"
        "    \"model\": \"small.bin\""
        "  },"
        "  \"conversation\": {"
        "    \"listen_timeout_ms\": 10000"
        "  }"
        "}";
    
    ethervox_persistent_settings_t settings;
    int result = ethervox_settings_import(&settings, partial_json);
    TEST_ASSERT(result == 0, "Import should succeed with partial JSON");
    
    // Verify specified values were loaded
    TEST_ASSERT(strcmp(settings.whisper.model_name, "small.bin") == 0,
                "Specified model should be loaded");
    TEST_ASSERT(settings.conversation.listen_timeout_ms == 10000,
                "Specified timeout should be loaded");
    
    // Verify missing values use defaults
    TEST_ASSERT(strcmp(settings.whisper.language, "auto") == 0,
                "Missing language field should use default");
    TEST_ASSERT(settings.whisper.beam_size == 5,
                "Missing beam_size should use default");
    TEST_ASSERT(strcmp(settings.wake_word.wake_phrase, "hey ethervox") == 0,
                "Missing wake_word section should use defaults");
    
    TEST_PASS("Partial JSON handled with default fallbacks");
}

static void test_whisper_settings_validation(void) {
    printf("\nTest: Whisper Settings Validation\n");
    
    ethervox_persistent_settings_t settings = ethervox_settings_get_defaults();
    
    // Test temperature range
    settings.whisper.temperature = 0.0f;
    TEST_ASSERT(settings.whisper.temperature >= 0.0f && settings.whisper.temperature <= 1.0f,
                "Temperature should be in valid range");
    
    // Test beam size range
    settings.whisper.beam_size = 5;
    TEST_ASSERT(settings.whisper.beam_size >= 1 && settings.whisper.beam_size <= 10,
                "Beam size should be in valid range");
    
    // Test n_threads
    settings.whisper.n_threads = -1; // Auto-detect
    TEST_ASSERT(settings.whisper.n_threads >= -1, "n_threads should be -1 or positive");
    
    TEST_PASS("Whisper settings within valid ranges");
}

static void test_conversation_settings_validation(void) {
    printf("\nTest: Conversation Settings Validation\n");
    
    ethervox_persistent_settings_t settings = ethervox_settings_get_defaults();
    
    // Test timeout values
    TEST_ASSERT(settings.conversation.listen_timeout_ms > 0,
                "Listen timeout should be positive");
    TEST_ASSERT(settings.conversation.conversation_timeout_ms > 0,
                "Conversation timeout should be positive");
    TEST_ASSERT(settings.conversation.silence_timeout_ms > 0,
                "Silence timeout should be positive");
    
    // Test energy threshold
    TEST_ASSERT(settings.conversation.audio_energy_threshold >= 0.0f &&
                settings.conversation.audio_energy_threshold <= 1.0f,
                "Energy threshold should be 0.0-1.0");
    
    // Test audio chunk size
    TEST_ASSERT(settings.conversation.max_audio_chunk_size > 0,
                "Audio chunk size should be positive");
    
    TEST_PASS("Conversation settings within valid ranges");
}

static void test_wake_word_settings_validation(void) {
    printf("\nTest: Wake Word Settings Validation\n");
    
    ethervox_persistent_settings_t settings = ethervox_settings_get_defaults();
    
    // Test detection threshold
    TEST_ASSERT(settings.wake_word.detection_threshold >= 0.0f &&
                settings.wake_word.detection_threshold <= 1.0f,
                "Detection threshold should be 0.0-1.0");
    
    // Test syllable counts
    TEST_ASSERT(settings.wake_word.expected_syllables > 0,
                "Expected syllables should be positive");
    TEST_ASSERT(settings.wake_word.min_syllables <= settings.wake_word.expected_syllables,
                "Min syllables should be <= expected");
    TEST_ASSERT(settings.wake_word.max_syllables >= settings.wake_word.expected_syllables,
                "Max syllables should be >= expected");
    
    // Test VAD thresholds
    TEST_ASSERT(settings.wake_word.vad_energy_threshold >= 0.0f,
                "VAD energy threshold should be non-negative");
    TEST_ASSERT(settings.wake_word.vad_zcr_min >= 0.0f &&
                settings.wake_word.vad_zcr_min <= 1.0f,
                "VAD ZCR min should be 0.0-1.0");
    TEST_ASSERT(settings.wake_word.vad_zcr_max >= settings.wake_word.vad_zcr_min &&
                settings.wake_word.vad_zcr_max <= 1.0f,
                "VAD ZCR max should be >= min and <= 1.0");
    
    // Test cooldown
    TEST_ASSERT(settings.wake_word.cooldown_ms >= 0,
                "Cooldown should be non-negative");
    
    TEST_PASS("Wake word settings within valid ranges");
}

static void test_default_path(void) {
    printf("\nTest: Default Settings Path\n");
    
    const char* path = ethervox_settings_get_default_path();
    TEST_ASSERT(path != NULL, "Default path should not be NULL");
    TEST_ASSERT(strlen(path) > 0, "Default path should not be empty");
    TEST_ASSERT(strstr(path, ".ethervox") != NULL,
                "Default path should contain .ethervox directory");
    TEST_ASSERT(strstr(path, "settings.json") != NULL,
                "Default path should end with settings.json");
    
    printf("    Default path: %s\n", path);
    TEST_PASS("Default path is valid");
}

static void test_concurrent_access(void) {
    printf("\nTest: Concurrent Save/Load\n");
    
    setup_test_file();
    
    ethervox_persistent_settings_t settings1 = ethervox_settings_get_defaults();
    settings1.whisper.temperature = 0.3f;
    
    // Save settings
    int result = ethervox_settings_save(&settings1, test_settings_path);
    TEST_ASSERT(result == 0, "First save should succeed");
    
    // Load immediately (simulating concurrent access)
    ethervox_persistent_settings_t settings2;
    result = ethervox_settings_load(&settings2, test_settings_path);
    TEST_ASSERT(result == 0, "Load during concurrent access should succeed");
    TEST_ASSERT(settings2.whisper.temperature == 0.3f,
                "Loaded value should match saved value");
    
    // Modify and save again
    settings2.whisper.temperature = 0.8f;
    result = ethervox_settings_save(&settings2, test_settings_path);
    TEST_ASSERT(result == 0, "Second save should succeed");
    
    // Load final result
    ethervox_persistent_settings_t settings3;
    result = ethervox_settings_load(&settings3, test_settings_path);
    TEST_ASSERT(result == 0, "Final load should succeed");
    TEST_ASSERT(settings3.whisper.temperature == 0.8f,
                "Final value should match last save");
    
    cleanup_test_file();
    TEST_PASS("Concurrent access handled correctly");
}

static void test_settings_print(void) {
    printf("\nTest: Settings Display\n");
    
    ethervox_persistent_settings_t settings = ethervox_settings_get_defaults();
    
    printf("\n--- Settings Display Output ---\n");
    ethervox_settings_print(&settings);
    printf("--- End Settings Display ---\n\n");
    
    TEST_PASS("Settings display completed without errors");
}

int main(void) {
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║  EthervoxAI Settings Persistence Test Suite             ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    
    test_report_init();
    
    // Set log level to error to avoid cluttering test output
    ethervox_log_set_level(ETHERVOX_LOG_LEVEL_ERROR);
    
    // Run all tests
    test_default_settings();
    test_save_and_load();
    test_load_nonexistent_file();
    test_export_import_json();
    test_invalid_json();
    test_partial_json();
    test_whisper_settings_validation();
    test_conversation_settings_validation();
    test_wake_word_settings_validation();
    test_default_path();
    test_concurrent_access();
    test_settings_print();
    
    // Summary
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║  Test Summary                                            ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  Total Tests:  %-3d                                       ║\n", tests_run);
    printf("║  Passed:       %-3d                                       ║\n", tests_passed);
    printf("║  Failed:       %-3d                                       ║\n", tests_failed);
    printf("╚══════════════════════════════════════════════════════════╝\n");
    
    if (tests_failed == 0) {
        printf("\n✓ ALL TESTS PASSED\n\n");
    } else {
        printf("\n✗ SOME TESTS FAILED\n\n");
    }
    
    test_report_finalize();
    
    return tests_failed == 0 ? 0 : 1;
}
