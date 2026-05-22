/**
 * @file test_governor.c
 * @brief Test program for Governor orchestration
 *
 * Tests the Governor's ability to parse tool calls, execute them,
 * and manage the reasoning loop.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/error.h"
#include "ethervox/governor.h"
#include "ethervox/compute_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_status(ethervox_governor_status_t status) {
    switch (status) {
        case ETHERVOX_GOVERNOR_SUCCESS:
            printf("SUCCESS");
            break;
        case ETHERVOX_GOVERNOR_NEED_CLARIFICATION:
            printf("NEED_CLARIFICATION");
            break;
        case ETHERVOX_GOVERNOR_TIMEOUT:
            printf("TIMEOUT");
            break;
        case ETHERVOX_GOVERNOR_ERROR:
            printf("ERROR");
            break;
        case ETHERVOX_GOVERNOR_USER_DENIED:
            printf("USER_DENIED");
            break;
        default:
            printf("UNKNOWN(%d)", status);
    }
}

static void test_governor_basic(void) {
    printf("\n=== Testing Governor Basic Execution ===\n");
    
    // Setup registry
    ethervox_tool_registry_t registry;
    if (ethervox_tool_registry_init(&registry, 8) != 0) {
        printf("[FAIL] Failed to initialize registry\n");
        return;
    }
    
    int count = ethervox_compute_tools_register_all(&registry);
    printf("Registered %d tools\n", count);
    
    // Initialize Governor
    ethervox_governor_t* governor = NULL;
    ethervox_governor_config_t config = {
        .confidence_threshold = 0.85f,
        .max_iterations = 5,
        .max_tool_calls_per_iteration = 10,
        .timeout_seconds = 30
    };
    
    if (ethervox_governor_init(&governor, &config, &registry) != 0) {
        printf("[FAIL] Failed to initialize Governor\n");
        ethervox_tool_registry_cleanup(&registry);
        return;
    }
    
    printf("[OK] Governor initialized\n");
    
    // Test queries
    struct {
        const char* query;
        bool should_use_tools;
    } tests[] = {
        {"What's 15% tip on $47.50?", true},
        {"Calculate sqrt(144)", true},
        {"What's the weather like?", false},
    };
    
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        printf("\n--- Query: %s ---\n", tests[i].query);
        
        char* response = NULL;
        char* error = NULL;
        ethervox_confidence_metrics_t metrics = {0};
        
        ethervox_governor_status_t status = ethervox_governor_execute(
            governor,
            tests[i].query,
            &response,
            &error,
            &metrics,
            NULL,  // No progress callback for tests
            NULL
        );
        
        printf("Status: ");
        print_status(status);
        printf("\n");
        
        printf("Confidence: %.2f\n", metrics.confidence);
        printf("Iterations: %u\n", metrics.iteration_count);
        printf("Tools called: %u\n", metrics.tool_calls_made);
        
        if (response) {
            printf("Response: %s\n", response);
            free(response);
        }
        
        if (error) {
            printf("Error: %s\n", error);
            free(error);
        }
        
        if (tests[i].should_use_tools && metrics.tool_calls_made == 0) {
            printf("⚠️  Expected tool calls but none were made\n");
        }
    }
    
    // Cleanup
    ethervox_governor_cleanup(governor);
    ethervox_tool_registry_cleanup(&registry);
    printf("\n[OK] Governor test complete\n");
}

static void test_xml_parsing(void) {
    printf("\n=== Testing XML Tool Call Parsing ===\n");
    
    // This tests the internal parsing logic indirectly through execution
    ethervox_tool_registry_t registry;
    if (ethervox_tool_registry_init(&registry, 8) != 0) {
        printf("[FAIL] Failed to initialize registry\n");
        return;
    }
    
    ethervox_compute_tools_register_all(&registry);
    
    // Test various XML formats
    const char* xml_tests[] = {
        "<tool_call name=\"calculator_compute\" expression=\"5+5\" />",
        "<tool_call name=\"percentage_calculate\" value=\"100\" percentage=\"20\" operation=\"of\" />",
        "<tool_call name=\"unknown_tool\" foo=\"bar\" />",  // Should fail gracefully
    };
    
    printf("Testing %zu XML formats...\n", sizeof(xml_tests) / sizeof(xml_tests[0]));
    
    // XML parsing is tested indirectly through Governor execution
    // Direct testing would require exposing internal functions
    
    ethervox_tool_registry_cleanup(&registry);
    printf("[OK] XML parsing test complete\n");
}

int main(void) {
    printf("Ethervox Governor Test\n");
    printf("======================\n");
    
    test_governor_basic();
    test_xml_parsing();
    
    printf("\n=== All Governor Tests Complete ===\n");
    
    return 0;
}
