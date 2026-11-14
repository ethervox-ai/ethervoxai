/**
 * @file test_logging_metrics.c
 * @brief Test suite for structured logging and metrics (C4.3)
 *
 * Tests:
 * 1. Structured logging callback receives all log entries
 * 2. Subsystem tagging works correctly
 * 3. Structured fields are passed through
 * 4. Metrics snapshot captures counters correctly
 * 5. Metrics reset preserves current state
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test state
static int g_callback_count = 0;
static ethervox_log_level_t g_last_level = ETHERVOX_LOG_LEVEL_OFF;
static ethervox_log_subsystem_t g_last_subsystem = ETHERVOX_SUBSYSTEM_UNKNOWN;
static char g_last_message[256] = {0};
static int g_last_field_count = 0;

static void test_callback(const ethervox_log_entry_t* entry, void* user_data) {
    g_callback_count++;
    g_last_level = entry->level;
    g_last_subsystem = entry->subsystem;
    g_last_field_count = entry->field_count;
    strncpy(g_last_message, entry->message, sizeof(g_last_message) - 1);
    
    // Verify user_data is passed through
    int* flag = (int*)user_data;
    if (flag) {
        (*flag)++;
    }
}

static void reset_test_state(void) {
    g_callback_count = 0;
    g_last_level = ETHERVOX_LOG_LEVEL_OFF;
    g_last_subsystem = ETHERVOX_SUBSYSTEM_UNKNOWN;
    g_last_message[0] = '\0';
    g_last_field_count = 0;
}

static void print_test_header(const char* name) {
    printf("\n=== %s ===\n", name);
}

/**
 * Test 1: Callback receives log entries
 */
static bool test_callback_invocation(void) {
    print_test_header("Test 1: Callback Receives Log Entries");
    
    reset_test_state();
    int user_flag = 0;
    
    // Set callback
    ethervox_log_set_callback(test_callback, &user_flag);
    
    // Log a message
    ETHERVOX_LOG_INFO("Test message %d", 42);
    
    // Verify callback was invoked
    if (g_callback_count != 1) {
        fprintf(stderr, "FAIL: Expected 1 callback, got %d\n", g_callback_count);
        return false;
    }
    
    if (g_last_level != ETHERVOX_LOG_LEVEL_INFO) {
        fprintf(stderr, "FAIL: Expected INFO level, got %d\n", g_last_level);
        return false;
    }
    
    if (strstr(g_last_message, "Test message 42") == NULL) {
        fprintf(stderr, "FAIL: Message mismatch: '%s'\n", g_last_message);
        return false;
    }
    
    if (user_flag != 1) {
        fprintf(stderr, "FAIL: User data not passed (flag=%d)\n", user_flag);
        return false;
    }
    
    // Clear callback
    ethervox_log_set_callback(NULL, NULL);
    
    printf("PASS: Callback invoked with correct data\n");
    return true;
}

/**
 * Test 2: Subsystem tagging
 */
static bool test_subsystem_tagging(void) {
    print_test_header("Test 2: Subsystem Tagging");
    
    reset_test_state();
    ethervox_log_set_callback(test_callback, NULL);
    
    // Log with specific subsystem
    ethervox_log_ex(ETHERVOX_LOG_LEVEL_DEBUG, ETHERVOX_SUBSYSTEM_MODEL_POOL,
                    __FILE__, __LINE__, __func__, "Model pool message");
    
    if (g_last_subsystem != ETHERVOX_SUBSYSTEM_MODEL_POOL) {
        fprintf(stderr, "FAIL: Expected MODEL_POOL subsystem, got %d\n", g_last_subsystem);
        ethervox_log_set_callback(NULL, NULL);
        return false;
    }
    
    ethervox_log_set_callback(NULL, NULL);
    printf("PASS: Subsystem correctly tagged\n");
    return true;
}

/**
 * Test 3: Structured fields
 */
static bool test_structured_fields(void) {
    print_test_header("Test 3: Structured Fields");
    
    reset_test_state();
    ethervox_log_set_callback(test_callback, NULL);
    
    // Log with fields
    ethervox_log_field_t fields[] = {
        { "model_id", "abc123" },
        { "tokens", "42" },
        { "latency_ms", "150" }
    };
    
    ethervox_log_fields(ETHERVOX_LOG_LEVEL_INFO, ETHERVOX_SUBSYSTEM_LLM,
                       __FILE__, __LINE__, __func__,
                       fields, 3, "Generation complete");
    
    if (g_last_field_count != 3) {
        fprintf(stderr, "FAIL: Expected 3 fields, got %d\n", g_last_field_count);
        ethervox_log_set_callback(NULL, NULL);
        return false;
    }
    
    ethervox_log_set_callback(NULL, NULL);
    printf("PASS: Fields passed to callback\n");
    return true;
}

/**
 * Test 4: Metrics snapshot
 */
static bool test_metrics_snapshot(void) {
    print_test_header("Test 4: Metrics Snapshot");
    
    // Get initial snapshot
    ethervox_metrics_t metrics1;
    ethervox_result_t result = ethervox_metrics_snapshot(&metrics1);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: metrics_snapshot failed: %s\n", ethervox_error_string(result));
        return false;
    }
    
    // Record some metrics (using internal functions for testing)
    extern void ethervox_metrics_record_generation(bool success, uint32_t tokens, uint64_t time_ms);
    extern void ethervox_metrics_record_structured_gen(float confidence);
    
    ethervox_metrics_record_generation(true, 100, 500);
    ethervox_metrics_record_generation(true, 50, 250);
    ethervox_metrics_record_structured_gen(0.85f);
    ethervox_metrics_record_structured_gen(0.92f);
    
    // Get second snapshot
    ethervox_metrics_t metrics2;
    result = ethervox_metrics_snapshot(&metrics2);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: second snapshot failed\n");
        return false;
    }
    
    // Verify counters increased
    if (metrics2.generations_total != metrics1.generations_total + 2) {
        fprintf(stderr, "FAIL: generations_total not incremented correctly\n");
        return false;
    }
    
    if (metrics2.tokens_generated_total != metrics1.tokens_generated_total + 150) {
        fprintf(stderr, "FAIL: tokens_generated_total not incremented\n");
        return false;
    }
    
    if (metrics2.structured_gens_total != metrics1.structured_gens_total + 2) {
        fprintf(stderr, "FAIL: structured_gens_total not incremented\n");
        return false;
    }
    
    // Verify average confidence
    float expected_avg = (0.85f + 0.92f) / 2.0f;
    float diff = metrics2.avg_confidence - expected_avg;
    if (diff < 0) diff = -diff;
    
    if (diff > 0.01f) {
        fprintf(stderr, "FAIL: avg_confidence incorrect (got %.3f, expected ~%.3f)\n",
                metrics2.avg_confidence, expected_avg);
        return false;
    }
    
    printf("PASS: Metrics snapshot captured correctly\n");
    printf("  Generations: %llu (+%llu)\n",
           (unsigned long long)metrics2.generations_total,
           (unsigned long long)(metrics2.generations_total - metrics1.generations_total));
    printf("  Tokens: %llu (+%llu)\n",
           (unsigned long long)metrics2.tokens_generated_total,
           (unsigned long long)(metrics2.tokens_generated_total - metrics1.tokens_generated_total));
    printf("  Avg confidence: %.3f\n", metrics2.avg_confidence);
    return true;
}

/**
 * Test 5: Metrics reset
 */
static bool test_metrics_reset(void) {
    print_test_header("Test 5: Metrics Reset");
    
    // Get snapshot before reset
    ethervox_metrics_t before;
    ethervox_metrics_snapshot(&before);
    
    // Reset
    ethervox_result_t result = ethervox_metrics_reset();
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "FAIL: metrics_reset failed\n");
        return false;
    }
    
    // Get snapshot after reset
    ethervox_metrics_t after;
    ethervox_metrics_snapshot(&after);
    
    // Verify cumulative counters are reset
    if (after.generations_total != 0) {
        fprintf(stderr, "FAIL: generations_total not reset (got %llu)\n",
                (unsigned long long)after.generations_total);
        return false;
    }
    
    if (after.tokens_generated_total != 0) {
        fprintf(stderr, "FAIL: tokens_generated_total not reset\n");
        return false;
    }
    
    printf("PASS: Metrics reset successfully\n");
    return true;
}

int main(void) {
    printf("STRUCTURED LOGGING AND METRICS TESTS (C4.3)\n");
    printf("=========================================\n");
    
    // Set log level to capture all messages
    ethervox_log_set_level(ETHERVOX_LOG_LEVEL_TRACE);
    
    int passed = 0;
    int total = 5;
    
    if (test_callback_invocation()) passed++;
    if (test_subsystem_tagging()) passed++;
    if (test_structured_fields()) passed++;
    if (test_metrics_snapshot()) passed++;
    if (test_metrics_reset()) passed++;
    
    printf("\n=========================================\n");
    printf("RESULTS: %d passed, %d failed\n", passed, total - passed);
    printf("=========================================\n");
    
    return (passed == total) ? 0 : 1;
}
