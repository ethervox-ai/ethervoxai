// SPDX-License-Identifier: CC-BY-NC-SA-4.0
#include "ethervox/logging.h"
#include "ethervox/error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test_func) do { \
    printf("Running %s...\n", #test_func); \
    tests_run++; \
    test_func(); \
    tests_passed++; \
    printf("  ✓ %s passed\n", #test_func); \
} while(0)

// =============================================================================
// Basic Error Code Tests
// =============================================================================

static void test_error_string_all_codes(void) {
    // Test all general errors
    assert(strcmp(ethervox_error_string(ETHERVOX_SUCCESS), "Success") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_GENERIC), "Generic error") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_NULL_POINTER), "NULL pointer") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_INVALID_ARGUMENT), "Invalid argument") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_OUT_OF_MEMORY), "Out of memory") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_NOT_INITIALIZED), "Not initialized") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_ALREADY_INITIALIZED), "Already initialized") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_TIMEOUT), "Timeout") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_NOT_SUPPORTED), "Not supported") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_BUFFER_TOO_SMALL), "Buffer too small") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_NOT_IMPLEMENTED), "Not implemented") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_FAILED), "Operation failed") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_NOT_FOUND), "Not found") == 0);
    
    // Test platform errors
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_PLATFORM_INIT), "Platform initialization failed") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_HAL_NOT_FOUND), "HAL not found") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_GPIO_FAILURE), "GPIO operation failed") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_HARDWARE_NOT_AVAILABLE), "Hardware not available") == 0);
    
    // Test audio errors
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_AUDIO_INIT), "Audio initialization failed") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_AUDIO_DEVICE_NOT_FOUND), "Audio device not found") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_AUDIO_FORMAT_UNSUPPORTED), "Audio format unsupported") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_AUDIO_BUFFER_OVERFLOW), "Audio buffer overflow") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_AUDIO_BUFFER_UNDERFLOW), "Audio buffer underflow") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_AUDIO_DEVICE_BUSY), "Audio device busy") == 0);
    
    // Test STT errors
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_STT_INIT), "STT initialization failed") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_STT_MODEL_NOT_FOUND), "STT model not found") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_STT_PROCESSING), "STT processing failed") == 0);
    
    // Test wake word errors
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_WAKEWORD_INIT), "Wake word initialization failed") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_WAKEWORD_MODEL_NOT_FOUND), "Wake word model not found") == 0);
    
    // Test plugin errors
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_PLUGIN_NOT_FOUND), "Plugin not found") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_PLUGIN_INIT), "Plugin initialization failed") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_PLUGIN_EXECUTION), "Plugin execution failed") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_PLUGIN_MAX_REACHED), "Maximum plugins reached") == 0);
    
    // Test network errors
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_NETWORK), "Network error") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_API_CALL), "API call failed") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_API_RESPONSE), "Invalid API response") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_API_RATE_LIMIT), "API rate limit exceeded") == 0);
    
    // Test file I/O errors
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_FILE_NOT_FOUND), "File not found") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_FILE_READ), "File read error") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_FILE_WRITE), "File write error") == 0);
    assert(strcmp(ethervox_error_string(ETHERVOX_ERROR_FILE_PERMISSION), "File permission denied") == 0);
    
    // Test unknown error code
    const char* unknown = ethervox_error_string((ethervox_result_t)(-9999));
    assert(strcmp(unknown, "Unknown error") == 0);
}

static void test_is_success_error(void) {
    // Test success case
    assert(ethervox_is_success(ETHERVOX_SUCCESS) == true);
    assert(ethervox_is_error(ETHERVOX_SUCCESS) == false);
    
    // Test various error codes
    assert(ethervox_is_success(ETHERVOX_ERROR_NULL_POINTER) == false);
    assert(ethervox_is_error(ETHERVOX_ERROR_NULL_POINTER) == true);
    
    assert(ethervox_is_success(ETHERVOX_ERROR_AUDIO_INIT) == false);
    assert(ethervox_is_error(ETHERVOX_ERROR_AUDIO_INIT) == true);
    
    assert(ethervox_is_success(ETHERVOX_ERROR_FILE_NOT_FOUND) == false);
    assert(ethervox_is_error(ETHERVOX_ERROR_FILE_NOT_FOUND) == true);
}

// =============================================================================
// Error Context Tests
// =============================================================================

static void test_error_context_basic(void) {
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
    assert(ctx->timestamp_ms > 0);  // Should have a timestamp
}

static void test_error_context_clear(void) {
    // Set an error
    ethervox_error_set_context(
        ETHERVOX_ERROR_AUDIO_INIT,
        "Audio failed",
        __FILE__,
        __LINE__,
        __func__
    );
    
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_ERROR_AUDIO_INIT);
    
    // Clear it
    ethervox_error_clear();
    ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_SUCCESS);
    assert(ctx->message == NULL);
    assert(ctx->file == NULL);
    assert(ctx->line == 0);
    assert(ctx->function == NULL);
    assert(ctx->timestamp_ms == 0);
}

static void test_error_context_null_message(void) {
    ethervox_error_clear();
    
    // NULL message should be allowed
    ethervox_error_set_context(
        ETHERVOX_ERROR_GENERIC,
        NULL,
        __FILE__,
        __LINE__,
        __func__
    );
    
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_ERROR_GENERIC);
    assert(ctx->message == NULL);
    assert(ctx->file != NULL);
}

static void test_error_context_overwrite(void) {
    ethervox_error_clear();
    
    // Set first error
    ethervox_error_set_context(
        ETHERVOX_ERROR_AUDIO_INIT,
        "First error",
        "file1.c",
        100,
        "func1"
    );
    
    // Set second error (should overwrite)
    ethervox_error_set_context(
        ETHERVOX_ERROR_STT_INIT,
        "Second error",
        "file2.c",
        200,
        "func2"
    );
    
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_ERROR_STT_INIT);
    assert(strcmp(ctx->message, "Second error") == 0);
    assert(strcmp(ctx->file, "file2.c") == 0);
    assert(ctx->line == 200);
}

// =============================================================================
// Macro Tests
// =============================================================================

static ethervox_result_t helper_check_ptr(const char* ptr) {
    ETHERVOX_CHECK_PTR(ptr);
    return ETHERVOX_SUCCESS;
}

static void test_check_ptr_macro(void) {
    ethervox_error_clear();
    
    // Valid pointer should succeed
    ethervox_result_t result = helper_check_ptr("valid");
    assert(ethervox_is_success(result));
    
    // NULL pointer should fail
    result = helper_check_ptr(NULL);
    assert(result == ETHERVOX_ERROR_NULL_POINTER);
    
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_ERROR_NULL_POINTER);
    assert(ctx->message != NULL);
    assert(strstr(ctx->message, "NULL") != NULL);
}

static ethervox_result_t helper_return_error(void) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_TIMEOUT, "Operation timed out");
}

static void test_return_error_macro(void) {
    ethervox_error_clear();
    
    ethervox_result_t result = helper_return_error();
    assert(result == ETHERVOX_ERROR_TIMEOUT);
    
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_ERROR_TIMEOUT);
    assert(strcmp(ctx->message, "Operation timed out") == 0);
}

static ethervox_result_t helper_level3(void) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_FILE_NOT_FOUND, "File missing");
}

static ethervox_result_t helper_level2(void) {
    ETHERVOX_CHECK(helper_level3());
    return ETHERVOX_SUCCESS;
}

static ethervox_result_t helper_level1(void) {
    ETHERVOX_CHECK(helper_level2());
    return ETHERVOX_SUCCESS;
}

static void test_check_propagation(void) {
    ethervox_error_clear();
    
    ethervox_result_t result = helper_level1();
    assert(result == ETHERVOX_ERROR_FILE_NOT_FOUND);
    
    // Context should be from level3 where error was set
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_ERROR_FILE_NOT_FOUND);
    assert(strcmp(ctx->message, "File missing") == 0);
}

static ethervox_result_t helper_early_success(void) {
    ETHERVOX_CHECK(helper_check_ptr("valid"));
    ETHERVOX_CHECK(helper_check_ptr("also valid"));
    return ETHERVOX_SUCCESS;
}

static void test_check_success_cases(void) {
    ethervox_error_clear();
    
    ethervox_result_t result = helper_early_success();
    assert(ethervox_is_success(result));
}

// =============================================================================
// Error Propagation Chains
// =============================================================================

static ethervox_result_t init_audio_simulation(bool should_fail) {
    if (should_fail) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Audio hardware not found");
    }
    return ETHERVOX_SUCCESS;
}

static ethervox_result_t init_stt_simulation(bool should_fail) {
    if (should_fail) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_STT_MODEL_NOT_FOUND, "STT model missing");
    }
    return ETHERVOX_SUCCESS;
}

static ethervox_result_t init_pipeline_simulation(bool audio_fail, bool stt_fail) {
    ETHERVOX_CHECK(init_audio_simulation(audio_fail));
    ETHERVOX_CHECK(init_stt_simulation(stt_fail));
    return ETHERVOX_SUCCESS;
}

static void test_pipeline_error_propagation(void) {
    ethervox_error_clear();
    
    // Success case
    ethervox_result_t result = init_pipeline_simulation(false, false);
    assert(ethervox_is_success(result));
    
    // Audio failure
    ethervox_error_clear();
    result = init_pipeline_simulation(true, false);
    assert(result == ETHERVOX_ERROR_AUDIO_INIT);
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_ERROR_AUDIO_INIT);
    
    // STT failure (audio succeeds)
    ethervox_error_clear();
    result = init_pipeline_simulation(false, true);
    assert(result == ETHERVOX_ERROR_STT_MODEL_NOT_FOUND);
    ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_ERROR_STT_MODEL_NOT_FOUND);
    
    // Audio failure should prevent STT from running
    ethervox_error_clear();
    result = init_pipeline_simulation(true, true);
    assert(result == ETHERVOX_ERROR_AUDIO_INIT);  // Should fail at audio step
}

// =============================================================================
// Edge Cases and Stress Tests
// =============================================================================

static void test_multiple_errors_sequence(void) {
    // Test that errors can be set and cleared multiple times
    for (int i = 0; i < 100; i++) {
        ethervox_error_set_context(
            ETHERVOX_ERROR_GENERIC,
            "Test iteration",
            __FILE__,
            i,
            __func__
        );
        
        const ethervox_error_context_t* ctx = ethervox_error_get_context();
        assert(ctx->line == i);
        
        ethervox_error_clear();
        ctx = ethervox_error_get_context();
        assert(ctx->code == ETHERVOX_SUCCESS);
    }
}

static void test_long_error_message(void) {
    ethervox_error_clear();
    
    const char* long_message = 
        "This is a very long error message that contains a lot of text. "
        "It should still be stored correctly in the error context without "
        "causing any issues. The error handling system should be able to "
        "handle messages of arbitrary length without crashing or truncating.";
    
    ethervox_error_set_context(
        ETHERVOX_ERROR_GENERIC,
        long_message,
        __FILE__,
        __LINE__,
        __func__
    );
    
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx->message == long_message);  // Should store pointer as-is
}

static void test_error_set_macro(void) {
    ethervox_error_clear();
    
    ETHERVOX_ERROR_SET(ETHERVOX_ERROR_BUFFER_TOO_SMALL, "Buffer insufficient");
    
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx->code == ETHERVOX_ERROR_BUFFER_TOO_SMALL);
    assert(strcmp(ctx->message, "Buffer insufficient") == 0);
    assert(ctx->file != NULL);
    assert(ctx->line > 0);
    assert(ctx->function != NULL);
}

// =============================================================================
// Logging Integration Tests
// =============================================================================

static void test_logging_integration(void) {
    ethervox_log_set_level(ETHERVOX_LOG_LEVEL_DEBUG);
    assert(ethervox_log_get_level() == ETHERVOX_LOG_LEVEL_DEBUG);
    
    // Test logging at various levels
    ETHERVOX_LOG_DEBUG("Debug message test");
    ETHERVOX_LOG_INFO("Info message test");
    ETHERVOX_LOG_WARN("Warning message test");
    ETHERVOX_LOG_ERROR("Error message test");
    
    // Test logging with error context
    ethervox_error_set_context(
        ETHERVOX_ERROR_AUDIO_INIT,
        "Test audio error",
        __FILE__,
        __LINE__,
        __func__
    );
    ethervox_log_error_context(ethervox_error_get_context());
}

// =============================================================================
// Real-World Scenario Tests
// =============================================================================

static ethervox_result_t validate_config(const char* config_path) {
    ETHERVOX_CHECK_PTR(config_path);
    
    if (strlen(config_path) == 0) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_INVALID_ARGUMENT, "Empty config path");
    }
    
    return ETHERVOX_SUCCESS;
}

static ethervox_result_t load_model(const char* model_path) {
    ETHERVOX_CHECK_PTR(model_path);
    
    // Simulate model loading
    if (strcmp(model_path, "invalid.model") == 0) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_FILE_NOT_FOUND, "Model file not found");
    }
    
    return ETHERVOX_SUCCESS;
}

static ethervox_result_t init_system(const char* config_path, const char* model_path) {
    ETHERVOX_CHECK(validate_config(config_path));
    ETHERVOX_CHECK(load_model(model_path));
    return ETHERVOX_SUCCESS;
}

static void test_realistic_initialization(void) {
    ethervox_error_clear();
    
    // Success case
    ethervox_result_t result = init_system("config.json", "model.bin");
    assert(ethervox_is_success(result));
    
    // NULL config
    ethervox_error_clear();
    result = init_system(NULL, "model.bin");
    assert(result == ETHERVOX_ERROR_NULL_POINTER);
    
    // Empty config path
    ethervox_error_clear();
    result = init_system("", "model.bin");
    assert(result == ETHERVOX_ERROR_INVALID_ARGUMENT);
    
    // Invalid model
    ethervox_error_clear();
    result = init_system("config.json", "invalid.model");
    assert(result == ETHERVOX_ERROR_FILE_NOT_FOUND);
}

// =============================================================================
// Main Test Runner
// =============================================================================

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║       EthervoxAI Comprehensive Error Handling Tests          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("📋 Basic Error Code Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(test_error_string_all_codes);
    RUN_TEST(test_is_success_error);
    printf("\n");
    
    printf("📝 Error Context Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(test_error_context_basic);
    RUN_TEST(test_error_context_clear);
    RUN_TEST(test_error_context_null_message);
    RUN_TEST(test_error_context_overwrite);
    printf("\n");
    
    printf("🔧 Macro Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(test_check_ptr_macro);
    RUN_TEST(test_return_error_macro);
    RUN_TEST(test_check_propagation);
    RUN_TEST(test_check_success_cases);
    RUN_TEST(test_error_set_macro);
    printf("\n");
    
    printf("🔗 Error Propagation Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(test_pipeline_error_propagation);
    printf("\n");
    
    printf("⚡ Edge Cases and Stress Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(test_multiple_errors_sequence);
    RUN_TEST(test_long_error_message);
    printf("\n");
    
    printf("📊 Logging Integration\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(test_logging_integration);
    printf("\n");
    
    printf("🌐 Real-World Scenarios\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(test_realistic_initialization);
    printf("\n");
    
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                       Test Summary                            ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║  Total Tests Run:    %-3d                                     ║\n", tests_run);
    printf("║  Tests Passed:       %-3d                                     ║\n", tests_passed);
    printf("║  Tests Failed:       %-3d                                     ║\n", tests_run - tests_passed);
    printf("║  Success Rate:       %.1f%%                                  ║\n", 
           (tests_run > 0) ? (100.0 * tests_passed / tests_run) : 0.0);
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    if (tests_passed == tests_run) {
        printf("✅ All tests passed! Error handling is ready for production use.\n\n");
        return ETHERVOX_SUCCESS;
    } else {
        printf("❌ Some tests failed. Please review the failures above.\n\n");
        return ETHERVOX_SUCCESS;
    }
}
