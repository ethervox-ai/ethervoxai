/**
 * @file test_compute_tools.c
 * @brief Test program for compute tools
 *
 * Verifies that calculator and percentage tools work correctly.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/error.h"
#include "ethervox/compute_tools.h"
#include "ethervox/governor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_calculator(void) {
    printf("\n=== Testing Calculator ===\n");
    
    const ethervox_tool_t* calc = ethervox_tool_calculator();
    
    struct {
        const char* input;
        const char* expected_contains;
    } tests[] = {
        {"{\"expression\": \"5 + 5\"}", "10"},
        {"{\"expression\": \"47.50 * 0.15\"}", "7.125"},
        {"{\"expression\": \"100 / 4\"}", "25"},
        {"{\"expression\": \"2 ^ 8\"}", "256"},
        {"{\"expression\": \"sqrt(144)\"}", "12"},
        {"{\"expression\": \"(5 + 3) * 2\"}", "16"},
        {"{\"expression\": \"-10 + 5\"}", "-5"},
    };
    
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        char* result = NULL;
        char* error = NULL;
        
        int status = calc->execute(tests[i].input, &result, &error);
        
        if (status == 0 && result) {
            printf("[OK] %s -> %s\n", tests[i].input, result);
            
            if (!strstr(result, tests[i].expected_contains)) {
                printf("  WARNING: Expected result to contain '%s'\n", tests[i].expected_contains);
            }
            
            free(result);
        } else {
            printf("[FAIL] %s -> ERROR: %s\n", tests[i].input, error ? error : "unknown");
            if (error) free(error);
        }
    }
}

static void test_percentage(void) {
    printf("\n=== Testing Percentage Calculator ===\n");
    
    const ethervox_tool_t* percent = ethervox_tool_percentage();
    
    struct {
        const char* input;
        const char* expected_contains;
    } tests[] = {
        {"{\"value\": 47.50, \"percentage\": 15, \"operation\": \"of\"}", "7.13"},
        {"{\"value\": 100, \"percentage\": 20, \"operation\": \"of\"}", "20"},
        {"{\"value\": 100, \"percentage\": 50, \"operation\": \"increase\"}", "150"},
        {"{\"value\": 100, \"percentage\": 25, \"operation\": \"decrease\"}", "75"},
        {"{\"value\": 50, \"percentage\": 200, \"operation\": \"is_what_percent\"}", "25"},
    };
    
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        char* result = NULL;
        char* error = NULL;
        
        int status = percent->execute(tests[i].input, &result, &error);
        
        if (status == 0 && result) {
            printf("[OK] %s -> %s\n", tests[i].input, result);
            
            if (!strstr(result, tests[i].expected_contains)) {
                printf("  WARNING: Expected result to contain '%s'\n", tests[i].expected_contains);
            }
            
            free(result);
        } else {
            printf("[FAIL] %s -> ERROR: %s\n", tests[i].input, error ? error : "unknown");
            if (error) free(error);
        }
    }
}

static void test_registry(void) {
    printf("\n=== Testing Tool Registry ===\n");
    
    ethervox_tool_registry_t registry;
    if (ethervox_tool_registry_init(&registry, 8) != 0) {
        printf("[FAIL] Failed to initialize registry\n");
        return;
    }
    
    // Create a minimal chat template for testing
    const chat_template_t* template = chat_template_get(CHAT_TEMPLATE_CHATML, NULL);
    
    int count = ethervox_compute_tools_register_all(&registry);
    printf("[OK] Registered %d compute tools\n", count);
    
    // Test finding tools
    const ethervox_tool_t* calc = ethervox_tool_registry_find(&registry, "calculator_compute");
    if (calc) {
        printf("[OK] Found calculator_compute tool\n");
    } else {
        printf("[FAIL] Could not find calculator_compute tool\n");
    }
    
    const ethervox_tool_t* percent = ethervox_tool_registry_find(&registry, "percentage_calculate");
    if (percent) {
        printf("[OK] Found percentage_calculate tool\n");
    } else {
        printf("[FAIL] Could not find percentage_calculate tool\n");
    }
    
    // Test system prompt building
    char system_prompt[4096];
    if (ethervox_tool_registry_build_system_prompt(&registry, &template, system_prompt, sizeof(system_prompt), NULL) == 0) {
        printf("[OK] Built system prompt (%zu chars)\n", strlen(system_prompt));
        printf("\nSystem Prompt Preview:\n%s\n", system_prompt);
    } else {
        printf("[FAIL] Failed to build system prompt\n");
    }
    
    ethervox_tool_registry_cleanup(&registry);
    printf("[OK] Cleaned up registry\n");
}

int main(void) {
    printf("Ethervox Compute Tools Test\n");
    printf("============================\n");
    
    test_calculator();
    test_percentage();
    test_registry();
    
    printf("\n=== All Tests Complete ===\n");
    
    return 0;
}
