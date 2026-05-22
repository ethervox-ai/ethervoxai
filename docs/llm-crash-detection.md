# LLM Crash Detection & Stress Testing

## Overview

The LLM test suite includes comprehensive crash detection and long-runtime stress testing to validate the stability and reliability of the LLM subsystem under sustained load.

## Features

### 1. Crash Detection System

**Signal Handlers**: Intercepts and handles 5 types of crashes:
- `SIGSEGV` - Segmentation fault (invalid memory access)
- `SIGABRT` - Abort signal (assertion failures)
- `SIGFPE` - Floating-point exception
- `SIGILL` - Illegal instruction
- `SIGBUS` - Bus error (alignment issues)

**Recovery Mechanism**: Uses `setjmp`/`longjmp` to recover from crashes and continue testing:
```c
static jmp_buf g_crash_recovery_point;
static volatile sig_atomic_t g_crash_occurred;
```

**Crash Reports**: Generates two types of reports when a crash occurs:

1. **Main Test Report Entry**: Records crash in the main test log
2. **Detailed Crash Report**: Separate timestamped file with full crash details
   - Format: `llm_test_crash_YYYYMMDDHHMMSS.log`
   - Includes: Signal name, test context, timestamp, recovery status

### 2. File-Based Reporting

**Test Report File**: All test results are logged to a timestamped file:
- Format: `llm_test_report_<timestamp>.log`
- Location: Current working directory
- Content: Test headers, pass/fail status, detailed info, summary statistics

**Functions**:
```c
void write_to_report(const char* format, ...);
void report_test_header(const char* test_name);
void report_test_pass(const char* msg);
void report_test_fail(const char* msg);
void report_test_info(const char* format, ...);
```

### 3. Long Runtime Stress Test (Test 8)

**Purpose**: Validate LLM stability under sustained load over extended periods

**Parameters**:
- Duration: 5 minutes (300 seconds)
- Query Interval: 10 seconds
- Total Queries: ~30 queries
- Pass Criteria: >90% success rate AND zero crashes

**Query Rotation**: 8 diverse queries to stress different code paths:
1. Calculator (basic arithmetic)
2. Memory operations (add/search)
3. Tag-based search
4. Multi-tool orchestration
5. Complex reasoning
6. Error correction
7. Context recall
8. Data extraction

**Progress Tracking**:
- Updates every 30 seconds
- Displays: time elapsed, queries completed, success rate
- Tracks: successful/failed/crashed query counts separately

**Recovery**: Each query is protected by crash recovery:
```c
if (setjmp(g_crash_recovery_point) == 0) {
    // Execute query
} else {
    // Crash occurred - increment counter and continue
    crashed_queries++;
}
```

## Usage

### Running Tests

```bash
# Start EthervoxAI
./build/ethervoxai

# Run LLM tests (includes all 8 tests)
/testllm
```

### Output Files

1. **Test Report**: `llm_test_report_<timestamp>.log`
   - Complete test execution log
   - All test results and statistics
   - Summary with pass rates and duration

2. **Crash Reports** (if crashes occur): `llm_test_crash_<timestamp>.log`
   - Signal type and name
   - Test context when crash occurred
   - Timestamp and recovery status
   - Debugging recommendations

### Interpreting Results

**Successful Test Suite**:
```
Tests Passed:  8
Tests Failed:  0
Pass Rate:     100.0%
Duration:      ~310 seconds (includes 5-min stress test)
```

**Stress Test Success Criteria**:
- Success rate > 90%
- Zero crashes detected
- All queries complete within timeout

**Warning Signs**:
- Crashes during stress test (check crash reports)
- Success rate < 90% (model or prompt issues)
- Tests fail to complete (hangs or deadlocks)

## Implementation Details

### Global State

```c
// Crash recovery
static jmp_buf g_crash_recovery_point;
static volatile sig_atomic_t g_crash_occurred;
static char g_crash_signal_name[64];
static char g_current_test_name[128];

// File reporting
static FILE* g_test_report_file = NULL;
static char g_test_report_path[512];
```

### Test Lifecycle

1. **Initialization**:
   - Generate report filename with timestamp
   - Open report file for writing
   - Install crash handlers
   - Write test header

2. **Execution**:
   - Run Tests 1-7 (tool usage validation)
   - Run Test 8 (5-minute stress test)
   - Each test writes to both console and file

3. **Cleanup**:
   - Write summary statistics to file
   - Close report file
   - Remove crash handlers
   - Display report location

### Crash Handler Flow

```
Signal Received
    ↓
crash_handler(int sig)
    ↓
Set g_crash_occurred flag
    ↓
Write to main test report
    ↓
Generate separate crash report file
    ↓
longjmp to recovery point
    ↓
Test continues
```

## Testing the Crash Detection

To manually test crash detection (for development):

```c
// Add to test function temporarily:
LLM_TEST_INFO("Triggering intentional crash...");
volatile int* null_ptr = NULL;
*null_ptr = 42;  // Causes SIGSEGV
```

Expected behavior:
- Crash is caught by signal handler
- Crash report generated
- Test continues with next query
- Summary shows crashed query count

## Troubleshooting

**Test report not created**:
- Check file permissions in current directory
- Verify g_test_report_file opened successfully
- Look for initialization errors in console output

**Crash reports missing**:
- Check if crash handlers installed (look for "Crash detection enabled")
- Verify signal handlers registered correctly
- Check for competing signal handlers in codebase

**Stress test hangs**:
- Check governor timeout settings
- Look for deadlocks in tool execution
- Verify model is responsive (try individual queries first)

**Low success rate**:
- Review model quality (try Phi-3.5 or Llama-3.2)
- Check system prompt in tool_registry.c
- Verify tool descriptions are clear
- Ensure model is properly loaded

## Performance Considerations

**Memory**: Stress test monitors for memory leaks over 5 minutes
**CPU**: Sustained load tests CPU throttling and thermal management
**I/O**: File reporting has minimal overhead (~100 bytes per test)
**Time**: Total test suite runtime is ~6-7 minutes (mostly stress test)

## Future Enhancements

Potential improvements:
- Configurable stress test duration
- Memory leak detection (RSS monitoring)
- Network stress tests (for remote models)
- Concurrent query stress testing
- Performance profiling integration
- Custom query sets via config file
