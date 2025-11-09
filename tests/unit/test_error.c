// SPDX-License-Identifier: CC-BY-NC-SA-4.0
#include "ethervox/error.h"
#include "ethervox/logging.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_error_string(void) {
    printf("Testing error_string...\n");
    assert(strcmp(ethervox_error_string(ETHERVOX_SUCCESS), "Success") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_NULL_POINTER), "NULL pointer") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_AUDIO_INIT), "Audio initialization failed") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_PLUGIN_NOT_FOUND), "Plugin not found") == 0);
    printf("  ✓ Error string conversion works\n");
}

static void test_is_success_error(void) {
    printf("Testing is_success/is_error...\n");
    assert(ethervox_is_success(ETHERVOX_SUCCESS) == true);
    assert(ethervox_is_error(ETHERVOX_SUCCESS) == false);
    assert(ethervox_is_success(ETHERVOX_ERROR_NULL_POINTER) == false);
    assert(ethervox_is_error(ETHERVOX_ERROR_NULL_POINTER) == true);
    printf("  ✓ Success/error checking works\n");
}

static void test_error_context(void) {
    printf("Testing error context...\n");
    ethervox_error_clear();
    
    ethervox_error_set_context(
        ETHERVOX_ERROR_INVALID_ARGUMENT,
        "Test error message",
        "test_error.c",
        42,
        "test_function"
    );
    
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx != NULL);
    assert(ctx->code == ETHERVOX_ERROR_INVALID_ARGUMENT);
    assert(strcmp(ctx->message, "Test error message") == 0);
    assert(strcmp(ctx->file, "test_error.c") == 0);
    assert(ctx->line == 42);
    assert(strcmp(ctx->function, "test_function") == 0);
    
    ethervox_error_clear();
    ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_SUCCESS);
    
    printf("  ✓ Error context works\n");
}

static ethervox_result_t test_check_ptr(const char* ptr) {
    ETHERVOX_CHECK_PTR(ptr);
    return ETHERVOX_SUCCESS;
}

static ethervox_result_t test_check_propagation(const char* ptr) {
    ETHERVOX_CHECK(test_check_ptr(ptr));
    return ETHERVOX_SUCCESS;
}

static void test_macros(void) {
    printf("Testing error macros...\n");
    
    ethervox_result_t result = test_check_ptr("valid");
    assert(ethervox_is_success(result));
    
    result = test_check_ptr(NULL);
    assert(result == ETHERVOX_ERROR_NULL_POINTER);
    
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_ERROR_NULL_POINTER);
    
    ethervox_error_clear();
    result = test_check_propagation(NULL);
    assert(result == ETHERVOX_ERROR_NULL_POINTER);
    
    printf("  ✓ Error macros work\n");
}

static void test_logging(void) {
    printf("Testing logging...\n");
    
    ethervox_log_set_level(ETHERVOX_LOG_LEVEL_DEBUG);
    assert(ethervox_log_get_level() == ETHERVOX_LOG_LEVEL_DEBUG);
    
    ETHERVOX_LOG_DEBUG("Debug message test");
    ETHERVOX_LOG_INFO("Info message test");
    ETHERVOX_LOG_WARN("Warning message test");
    ETHERVOX_LOG_ERROR("Error message test");
    
    ethervox_error_set_context(
        ETHERVOX_ERROR_AUDIO_INIT,
        "Test audio error",
        __FILE__,
        __LINE__,
        __func__
    );
    ethervox_log_error_context(ethervox_error_get_context());
    
    printf("  ✓ Logging works\n");
}

int main(void) {
    printf("\n=== Running Error Handling Tests ===\n\n");
    
    test_error_string();
    test_is_success_error();
    test_error_context();
    test_macros();
    test_logging();
    
    printf("\n=== All Tests Passed ===\n\n");
    return 0;
}
