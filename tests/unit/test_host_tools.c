/**
 * @file test_host_tools.c
 * @brief Tests for host-registered tools (C2.2)
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/host_tools.h"
#include "ethervox/tool_manifest.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

#define CHECK_SUCCESS(expr) do { \
    ethervox_result_t _r = (expr); \
    if (_r != ETHERVOX_SUCCESS) { \
        fprintf(stderr, "FAIL: %s:%d: %s returned %d\n", __FILE__, __LINE__, #expr, _r); \
        return 1; \
    } \
} while (0)

// Test callback that returns a simple JSON result
static ethervox_result_t test_callback_success(
    const char* arguments_json,
    void* user_data,
    char** out_result_json,
    char** out_error_message
) {
    (void)user_data;
    
    // Simple echo: return the arguments as result
    *out_result_json = strdup(arguments_json);
    *out_error_message = NULL;
    
    return ETHERVOX_SUCCESS;
}

// Test callback that returns an error
static ethervox_result_t test_callback_error(
    const char* arguments_json,
    void* user_data,
    char** out_result_json,
    char** out_error_message
) {
    (void)arguments_json;
    (void)user_data;
    
    *out_result_json = NULL;
    *out_error_message = strdup("Test error message");
    
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
}

// Test callback that violates the contract (returns both)
static ethervox_result_t test_callback_invalid_both(
    const char* arguments_json,
    void* user_data,
    char** out_result_json,
    char** out_error_message
) {
    (void)arguments_json;
    (void)user_data;
    
    *out_result_json = strdup("result");
    *out_error_message = strdup("error");
    
    return ETHERVOX_SUCCESS;
}

// Test callback that violates the contract (returns neither)
static ethervox_result_t test_callback_invalid_neither(
    const char* arguments_json,
    void* user_data,
    char** out_result_json,
    char** out_error_message
) {
    (void)arguments_json;
    (void)user_data;
    
    *out_result_json = NULL;
    *out_error_message = NULL;
    
    return ETHERVOX_SUCCESS;
}

static int test_register_and_clear(void) {
    printf("test_register_and_clear...\n");
    
    tool_manifest_registry_t registry = {0};
    
    ethervox_host_tool_t tool = {
        .name = "test_tool",
        .description = "A test tool",
        .parameters_schema_json = "{\"type\":\"object\"}",
        .is_mutating = false,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    
    // Register tool
    CHECK_SUCCESS(ethervox_tool_registry_register_host_tool(&registry, &tool));
    
    // Clear tools
    CHECK_SUCCESS(ethervox_tool_registry_clear_host_tools(&registry));
    
    printf("  PASS\n");
    return 0;
}

static int test_duplicate_registration(void) {
    printf("test_duplicate_registration...\n");
    
    tool_manifest_registry_t registry = {0};
    
    ethervox_host_tool_t tool = {
        .name = "duplicate_tool",
        .description = "A test tool",
        .parameters_schema_json = "{\"type\":\"object\"}",
        .is_mutating = false,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    
    // Register tool
    CHECK_SUCCESS(ethervox_tool_registry_register_host_tool(&registry, &tool));
    
    // Try to register again - should fail
    ethervox_result_t result = ethervox_tool_registry_register_host_tool(&registry, &tool);
    CHECK(result == ETHERVOX_ERROR_ALREADY_EXISTS);
    
    // Clean up
    ethervox_tool_registry_clear_host_tools(&registry);
    
    printf("  PASS\n");
    return 0;
}

static int test_timeout_setting(void) {
    printf("test_timeout_setting...\n");
    
    tool_manifest_registry_t registry = {0};
    
    // Set timeout
    CHECK_SUCCESS(ethervox_tool_registry_set_timeout(&registry, 5000));
    CHECK(registry.host_tool_timeout_ms == 5000);
    
    // Set to zero (no timeout)
    CHECK_SUCCESS(ethervox_tool_registry_set_timeout(&registry, 0));
    CHECK(registry.host_tool_timeout_ms == 0);
    
    printf("  PASS\n");
    return 0;
}

static int test_null_safety(void) {
    printf("test_null_safety...\n");
    
    tool_manifest_registry_t registry = {0};
    ethervox_host_tool_t tool = {
        .name = "test",
        .description = "Test",
        .parameters_schema_json = "{}",
        .is_mutating = false,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    
    // NULL registry
    CHECK(ethervox_tool_registry_register_host_tool(NULL, &tool) == ETHERVOX_ERROR_INVALID_ARGUMENT);
    
    // NULL tool
    CHECK(ethervox_tool_registry_register_host_tool(&registry, NULL) == ETHERVOX_ERROR_INVALID_ARGUMENT);
    
    // NULL timeout
    CHECK(ethervox_tool_registry_set_timeout(NULL, 100) == ETHERVOX_ERROR_INVALID_ARGUMENT);
    
    // NULL clear
    CHECK(ethervox_tool_registry_clear_host_tools(NULL) == ETHERVOX_ERROR_INVALID_ARGUMENT);
    
    // NULL string_free is safe
    ethervox_string_free(NULL);
    
    printf("  PASS\n");
    return 0;
}

static int test_invalid_tool_fields(void) {
    printf("test_invalid_tool_fields...\n");
    
    tool_manifest_registry_t registry = {0};
    
    // Missing name
    ethervox_host_tool_t tool1 = {
        .name = NULL,
        .description = "Test",
        .parameters_schema_json = "{}",
        .is_mutating = false,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    CHECK(ethervox_tool_registry_register_host_tool(&registry, &tool1) == ETHERVOX_ERROR_INVALID_ARGUMENT);
    
    // Missing description
    ethervox_host_tool_t tool2 = {
        .name = "test",
        .description = NULL,
        .parameters_schema_json = "{}",
        .is_mutating = false,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    CHECK(ethervox_tool_registry_register_host_tool(&registry, &tool2) == ETHERVOX_ERROR_INVALID_ARGUMENT);
    
    // Missing schema
    ethervox_host_tool_t tool3 = {
        .name = "test",
        .description = "Test",
        .parameters_schema_json = NULL,
        .is_mutating = false,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    CHECK(ethervox_tool_registry_register_host_tool(&registry, &tool3) == ETHERVOX_ERROR_INVALID_ARGUMENT);
    
    // Missing callback
    ethervox_host_tool_t tool4 = {
        .name = "test",
        .description = "Test",
        .parameters_schema_json = "{}",
        .is_mutating = false,
        .invoke = NULL,
        .user_data = NULL
    };
    CHECK(ethervox_tool_registry_register_host_tool(&registry, &tool4) == ETHERVOX_ERROR_INVALID_ARGUMENT);
    
    printf("  PASS\n");
    return 0;
}

static int test_mutating_flag(void) {
    printf("test_mutating_flag...\n");
    
    tool_manifest_registry_t registry = {0};
    
    // Register non-mutating tool
    ethervox_host_tool_t tool1 = {
        .name = "read_tool",
        .description = "Read-only tool",
        .parameters_schema_json = "{}",
        .is_mutating = false,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    CHECK_SUCCESS(ethervox_tool_registry_register_host_tool(&registry, &tool1));
    
    // Register mutating tool
    ethervox_host_tool_t tool2 = {
        .name = "write_tool",
        .description = "Mutating tool",
        .parameters_schema_json = "{}",
        .is_mutating = true,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    CHECK_SUCCESS(ethervox_tool_registry_register_host_tool(&registry, &tool2));
    
    // Verify flags
    CHECK(ethervox_host_tool_is_mutating(&registry, "read_tool") == false);
    CHECK(ethervox_host_tool_is_mutating(&registry, "write_tool") == true);
    CHECK(ethervox_host_tool_is_mutating(&registry, "nonexistent") == false);
    
    // Clean up
    ethervox_tool_registry_clear_host_tools(&registry);
    
    printf("  PASS\n");
    return 0;
}

static int test_mutating_refusal(void) {
    printf("test_mutating_refusal...\n");
    
    tool_manifest_registry_t registry = {0};
    
    // Register mutating tool
    ethervox_host_tool_t tool = {
        .name = "delete_file",
        .description = "Deletes a file",
        .parameters_schema_json = "{\"type\":\"object\"}",
        .is_mutating = true,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    CHECK_SUCCESS(ethervox_tool_registry_register_host_tool(&registry, &tool));
    
    // Attempt to invoke - should be refused
    char* result = NULL;
    char* error = NULL;
    ethervox_result_t invoke_result = ethervox_host_tool_invoke(
        &registry, "delete_file", "{\"path\":\"/tmp/test\"}", &result, &error
    );
    
    CHECK(invoke_result == ETHERVOX_ERROR_PERMISSION_DENIED);
    CHECK(result == NULL);
    CHECK(error == NULL);
    
    // Clean up
    ethervox_tool_registry_clear_host_tools(&registry);
    
    printf("  PASS\n");
    return 0;
}

static int test_invoke_success(void) {
    printf("test_invoke_success...\n");
    
    tool_manifest_registry_t registry = {0};
    
    // Register non-mutating tool
    ethervox_host_tool_t tool = {
        .name = "echo",
        .description = "Echoes input",
        .parameters_schema_json = "{\"type\":\"object\"}",
        .is_mutating = false,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    CHECK_SUCCESS(ethervox_tool_registry_register_host_tool(&registry, &tool));
    
    // Invoke tool
    char* result = NULL;
    char* error = NULL;
    ethervox_result_t invoke_result = ethervox_host_tool_invoke(
        &registry, "echo", "{\"message\":\"hello\"}", &result, &error
    );
    
    CHECK(invoke_result == ETHERVOX_SUCCESS);
    CHECK(result != NULL);
    CHECK(error == NULL);
    CHECK(strcmp(result, "{\"message\":\"hello\"}") == 0);
    
    // Clean up
    ethervox_string_free(result);
    ethervox_tool_registry_clear_host_tools(&registry);
    
    printf("  PASS\n");
    return 0;
}

static int test_invoke_error(void) {
    printf("test_invoke_error...\n");
    
    tool_manifest_registry_t registry = {0};
    
    // Register tool that returns error
    ethervox_host_tool_t tool = {
        .name = "failing_tool",
        .description = "Always fails",
        .parameters_schema_json = "{\"type\":\"object\"}",
        .is_mutating = false,
        .invoke = test_callback_error,
        .user_data = NULL
    };
    CHECK_SUCCESS(ethervox_tool_registry_register_host_tool(&registry, &tool));
    
    // Invoke tool
    char* result = NULL;
    char* error = NULL;
    ethervox_result_t invoke_result = ethervox_host_tool_invoke(
        &registry, "failing_tool", "{}", &result, &error
    );
    
    CHECK(invoke_result == ETHERVOX_ERROR_INVALID_ARGUMENT);
    CHECK(result == NULL);
    CHECK(error != NULL);
    CHECK(strcmp(error, "Test error message") == 0);
    
    // Clean up
    ethervox_string_free(error);
    ethervox_tool_registry_clear_host_tools(&registry);
    
    printf("  PASS\n");
    return 0;
}

static int test_string_free(void) {
    printf("test_string_free...\n");
    
    // Allocate and free
    char* str = strdup("test string");
    CHECK(str != NULL);
    ethervox_string_free(str);
    
    // NULL is safe
    ethervox_string_free(NULL);
    
    printf("  PASS\n");
    return 0;
}

static int test_exists(void) {
    printf("test_exists...\n");

    tool_manifest_registry_t registry = {0};

    // Not registered yet: exists() is false, distinct from is_mutating() also being false
    CHECK(ethervox_host_tool_exists(&registry, "maybe_tool") == false);
    CHECK(ethervox_host_tool_is_mutating(&registry, "maybe_tool") == false);

    ethervox_host_tool_t non_mutating = {
        .name = "maybe_tool",
        .description = "A non-mutating tool",
        .parameters_schema_json = "{\"type\":\"object\"}",
        .is_mutating = false,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    CHECK_SUCCESS(ethervox_tool_registry_register_host_tool(&registry, &non_mutating));

    // Registered and non-mutating: exists() true, is_mutating() false - the case the governor's
    // execute_tool_call_json() fallback must be able to tell apart from "not registered"
    CHECK(ethervox_host_tool_exists(&registry, "maybe_tool") == true);
    CHECK(ethervox_host_tool_is_mutating(&registry, "maybe_tool") == false);

    ethervox_host_tool_t mutating = {
        .name = "mutating_tool",
        .description = "A mutating tool",
        .parameters_schema_json = "{\"type\":\"object\"}",
        .is_mutating = true,
        .invoke = test_callback_success,
        .user_data = NULL
    };
    CHECK_SUCCESS(ethervox_tool_registry_register_host_tool(&registry, &mutating));
    CHECK(ethervox_host_tool_exists(&registry, "mutating_tool") == true);
    CHECK(ethervox_host_tool_is_mutating(&registry, "mutating_tool") == true);

    ethervox_tool_registry_clear_host_tools(&registry);

    printf("  PASS\n");
    return 0;
}

int main(void) {
    printf("Running host tools tests...\n\n");
    
    int failed = 0;
    
    failed += test_register_and_clear();
    failed += test_duplicate_registration();
    failed += test_timeout_setting();
    failed += test_null_safety();
    failed += test_invalid_tool_fields();
    failed += test_mutating_flag();
    failed += test_mutating_refusal();
    failed += test_invoke_success();
    failed += test_invoke_error();
    failed += test_string_free();
    failed += test_exists();
    
    printf("\n");
    if (failed == 0) {
        printf("All tests PASSED\n");
    } else {
        printf("%d test(s) FAILED\n", failed);
    }
    
    return failed;
}
