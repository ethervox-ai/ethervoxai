# LLM Test Cancellation and Model Detection

## Overview

The LLM test suite now includes:
1. **Ctrl+C cancellation** - Gracefully exit tests early
2. **Model availability detection** - Skip tests if no LLM is loaded

## Features

### 1. Ctrl+C Cancellation

Press **Ctrl+C** at any time during the test suite to gracefully cancel execution.

**Behavior:**
- Immediately stops the current test
- Writes cancellation notice to console and report file
- Marks cancelled tests as "skipped"
- Continues to summary and cleanup
- Removes signal handlers properly

**Visual Feedback:**
```
⚠ Test cancellation requested (Ctrl+C)...
Test was cancelled by user - marked as skipped
```

**In Test Report:**
```
⚠ Test cancelled by user (SIGINT)
Test cancelled after 15 queries
Test cancelled - partial results above
```

**Use Cases:**
- Long stress test taking too long
- Found issue early, want to investigate
- Testing other features, need to exit
- Model is slow/unresponsive

### 2. Model Availability Detection

Tests now verify an LLM model is loaded before running any tests.

**Detection Method:**
- Sends a simple test query to the governor
- Checks if response is successful
- If no model or error, skips all tests

**No Model Loaded:**
```
❌ No LLM model is loaded
ℹ Please load a model first using /load command
ℹ Example: /load models/granite-4.1-3b-Q6_K.gguf
ℹ Skipping all LLM-dependent tests

Test Summary:
Tests Passed:  0
Tests Failed:  0
Tests Skipped: 8
```

**Model Loaded:**
```
✓ LLM model is loaded and responsive
ℹ Model path: models/granite-4.1-3b-Q6_K.gguf
```

### 3. Individual Test Guards

Each test that requires an LLM includes guards:

**Stress Test Example:**
```c
// Verify governor is still valid
if (!governor) {
    LLM_TEST_FAIL("Governor instance is NULL");
    report_test_fail("No governor - test skipped");
    g_llm_tests_skipped++;
    return;
}
```

This prevents crashes if:
- Governor is cleaned up mid-test
- Model is unloaded during tests
- Memory corruption occurs

## Implementation Details

### Signal Handler

```c
static volatile sig_atomic_t g_test_cancelled = 0;

static void interrupt_handler(int sig) {
    g_test_cancelled = 1;
    printf("\n\n⚠ Test cancellation requested (Ctrl+C)...\n");
    
    if (g_test_report_file) {
        fprintf(g_test_report_file, "\n⚠ Test cancelled by user (SIGINT)\n");
        fflush(g_test_report_file);
    }
}
```

**Registered in:**
- `install_crash_handlers()` adds SIGINT handler
- `remove_crash_handlers()` restores default handler

### Cancellation Checks

**Main Test Loop:**
```c
while (time(NULL) < end_time && g_runtime_test_running && !g_test_cancelled) {
    // Check before each query
    if (g_test_cancelled) {
        LLM_TEST_INFO("Test cancelled by user");
        break;
    }
    
    // Execute query...
}
```

**Result Handling:**
```c
if (g_test_cancelled) {
    LLM_TEST_INFO("Test was cancelled by user - marked as skipped");
    g_llm_tests_skipped++;
} else if (/* normal pass/fail logic */) {
    // ...
}
```

### Model Detection

**Test Query:**
```c
bool model_loaded = false;
char* test_response = NULL;
char* test_error = NULL;

ethervox_governor_status_t test_status = ethervox_governor_execute(
    governor, "test", &test_response, &test_error, NULL,
    track_tool_progress, NULL, NULL
);

if (test_status == ETHERVOX_GOVERNOR_SUCCESS || 
    test_status == ETHERVOX_GOVERNOR_NEED_CLARIFICATION) {
    model_loaded = true;
}

// Clean up
if (test_response) free(test_response);
if (test_error) free(test_error);
```

**Early Exit:**
```c
if (!model_loaded) {
    LLM_TEST_FAIL("No LLM model is loaded");
    // ... user instructions ...
    g_llm_tests_skipped = 8;  // All 8 tests
    goto cleanup;
}
```

## Usage Examples

### Starting Without Model

```bash
$ ./build/ethervoxai
EthervoxAI> /testllm

╔═══════════════════════════════════════════════════════════════╗
║            ETHERVOXAI LLM TOOL USAGE TESTS                    ║
╚═══════════════════════════════════════════════════════════════╝

Test report: llm_test_report_1701234567.log
Crash detection enabled

=== Checking LLM Availability ===
❌ No LLM model is loaded
ℹ Please load a model first using /load command
ℹ Example: /load models/granite-4.1-3b-Q6_K.gguf
ℹ Skipping all LLM-dependent tests

═══════════════════════════════════════════════════════════════
                 LLM TEST SUMMARY
═══════════════════════════════════════════════════════════════

  Tests Passed:  0
  Tests Failed:  0
  Tests Skipped: 8
  Total Tests:   8
  Pass Rate:     0.0%
  Duration:      1 seconds
```

### Cancelling During Stress Test

```bash
$ ./build/ethervoxai
EthervoxAI> /load models/granite-4.1-3b-Q6_K.gguf
EthervoxAI> /testllm

... Tests 1-7 run normally ...

=== Test 8: Long Runtime Stress Test ===
ℹ Running stress test for 300 seconds (5 minutes)
ℹ Queries will be sent every 10 seconds
ℹ Press Ctrl+C to cancel test early
ℹ Query #1: "What is 123 + 456?"
✓ Success
ℹ Query #2: "Calculate 50% of 1000"
✓ Success
^C

⚠ Test cancellation requested (Ctrl+C)...
ℹ Test cancelled by user

=== Stress Test Results ===
Duration: 25 seconds (0.4 minutes)
Total queries: 2
Successful: 2 (100.0%)
Failed: 0 (0.0%)
ℹ Test was cancelled by user - marked as skipped

═══════════════════════════════════════════════════════════════
                 LLM TEST SUMMARY
═══════════════════════════════════════════════════════════════

  Tests Passed:  7
  Tests Failed:  0
  Tests Skipped: 1    ← Stress test cancelled
  Total Tests:   8
  Pass Rate:     87.5%
  Duration:      45 seconds

Test report saved to: llm_test_report_1701234567.log
```

## Test Report Format

### With Cancellation

```
EthervoxAI LLM Tool Usage Test Report
======================================

Timestamp: Mon Dec 2 12:34:56 2024
Model Path: models/granite-4.1-3b-Q6_K.gguf

=== Initialization ===
ℹ LLM model available
✓ Model ready

... Tests 1-7 ...

=== Test 8: Long Runtime Stress Test ===
ℹ Starting long runtime stress test
  Query #1: "What is 123 + 456?"
  ✓ Success
  Query #2: "Calculate 50% of 1000"
  ✓ Success

⚠ Test cancelled by user (SIGINT)
ℹ Test cancelled after 2 queries

Stress Test Summary:
  Duration: 25 seconds
  Total queries: 2
  Successful: 2 (100.0%)
  Failed: 0
  Crashed: 0
ℹ Test cancelled - partial results above

=== Test Summary ===
Tests Passed:  7
Tests Failed:  0
Tests Skipped: 1
Total Tests:   8
Pass Rate:     87.5%
Duration:      45 seconds
```

### No Model

```
EthervoxAI LLM Tool Usage Test Report
======================================

Timestamp: Mon Dec 2 12:30:00 2024
Model Path: N/A

=== Initialization ===
❌ No LLM model loaded
ℹ All tests skipped - no model available

=== Test Summary ===
Tests Passed:  0
Tests Failed:  0
Tests Skipped: 8
Total Tests:   8
Pass Rate:     0.0%
Duration:      1 seconds
```

## Error Handling

### Multiple Ctrl+C Presses

Only the first Ctrl+C is handled gracefully. Subsequent presses may force-quit the application (OS default behavior).

**Recommended:** Press Ctrl+C once and wait for cleanup to complete.

### Model Unloaded Mid-Test

Each test function checks if governor is still valid:

```c
if (!governor) {
    LLM_TEST_FAIL("Governor instance is NULL");
    g_llm_tests_skipped++;
    return;
}
```

This prevents crashes if model is somehow unloaded during test execution.

### Stress Test Cancellation During Query

The stress test checks for cancellation:
1. Before starting each query
2. After sleep intervals
3. In the main loop condition

This ensures prompt response to Ctrl+C even during long queries.

## Integration with Existing Features

### Works With Crash Detection

Cancellation and crash detection are independent:
- Crashes still generate crash reports
- Cancellation marks tests as skipped (not failed)
- Both write to the test report file
- Signal handlers don't conflict

### Works With File Reporting

All cancellation events are logged:
- Console output shows user-friendly message
- Report file gets structured entry
- Partial results are preserved
- Summary reflects skipped tests

### Works With Test Lifecycle

Cancellation triggers cleanup:
```c
cleanup:
    remove_crash_handlers();
    if (g_test_report_file) {
        fclose(g_test_report_file);
    }
```

Ensures:
- Signal handlers removed
- Files closed properly
- No resource leaks
- Clean exit

## Future Enhancements

Potential improvements:
- Timeout for model detection (avoid infinite hang)
- Better model state introspection (direct API check)
- Pause/resume capability (beyond just cancel)
- Save partial results for resume
- Configurable cancellation behavior
