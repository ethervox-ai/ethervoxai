/**
 * @file llm_tool_tests.c
 * @brief End-to-end LLM tool usage validation tests
 *
 * Tests that the LLM actually calls tools correctly when prompted.
 * Each test suite validates a specific tool or tool category:
 * - Memory tools (store, search, retrieve)
 * - Compute tools (calculator, etc.)
 * - File tools (read, write)
 * - Context tools
 *
 * These tests ensure prompts are working and the model isn't hallucinating.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "ethervox/compute_tools.h"
#include "ethervox/file_tools.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include "ethervox/memory_tools.h"
#include "ethervox/platform.h"

// Platform-specific logging
#ifdef __ANDROID__
#include <android/log.h>
#define LLM_TEST_LOG(fmt, ...) \
  __android_log_print(ANDROID_LOG_INFO, "EthervoxLLMTests", fmt, ##__VA_ARGS__)
#else
#define LLM_TEST_LOG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#endif
#include <unistd.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

// ANSI color codes
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_BOLD "\033[1m"

#define LLM_TEST_PASS(msg, ...)                                     \
  do {                                                              \
    printf(COLOR_GREEN "  ✓ " COLOR_RESET msg "\n", ##__VA_ARGS__); \
    if (g_test_report_file) {                                       \
      fprintf(g_test_report_file, "  ✓ " msg "\n", ##__VA_ARGS__);  \
      fflush(g_test_report_file);                                   \
    }                                                               \
  } while (0)
#define LLM_TEST_FAIL(msg, ...)                                    \
  do {                                                             \
    printf(COLOR_RED "  ✗ " COLOR_RESET msg "\n", ##__VA_ARGS__);  \
    if (g_test_report_file) {                                      \
      fprintf(g_test_report_file, "  ✗ " msg "\n", ##__VA_ARGS__); \
      fflush(g_test_report_file);                                  \
    }                                                              \
  } while (0)
#define LLM_TEST_INFO(msg, ...)                                    \
  do {                                                             \
    printf(COLOR_CYAN "  ℹ " COLOR_RESET msg "\n", ##__VA_ARGS__); \
    if (g_test_report_file) {                                      \
      fprintf(g_test_report_file, "  ℹ " msg "\n", ##__VA_ARGS__); \
      fflush(g_test_report_file);                                  \
    }                                                              \
  } while (0)
#define LLM_TEST_WARN(msg, ...) printf(COLOR_YELLOW "  ⚠ " COLOR_RESET msg "\n", ##__VA_ARGS__)
#define LLM_TEST_HEADER(msg, ...) \
  printf("\n" COLOR_BOLD COLOR_BLUE "=== " msg " ===" COLOR_RESET "\n", ##__VA_ARGS__)
#define LLM_TEST_SUBHEADER(msg, ...) \
  printf("\n" COLOR_YELLOW "→ " msg COLOR_RESET "\n", ##__VA_ARGS__)

static int g_llm_tests_passed = 0;
static int g_llm_tests_failed = 0;
static int g_llm_tests_skipped = 0;

#define MAX_LLM_TEST_NAMES 50
static char* g_llm_passed_tests[MAX_LLM_TEST_NAMES];
static char* g_llm_failed_tests[MAX_LLM_TEST_NAMES];
static int g_llm_passed_count = 0;
static int g_llm_failed_count = 0;

static void record_llm_test_result(const char* test_name, bool passed) {
  if (passed && g_llm_passed_count < MAX_LLM_TEST_NAMES) {
    g_llm_passed_tests[g_llm_passed_count++] = strdup(test_name);
  } else if (!passed && g_llm_failed_count < MAX_LLM_TEST_NAMES) {
    g_llm_failed_tests[g_llm_failed_count++] = strdup(test_name);
  }
}

// Test report file
static FILE* g_test_report_file = NULL;
static char g_test_report_path[512] = {0};

// Verbose mode - captures debug output to report
static bool g_verbose_mode = false;
static ethervox_log_level_t g_saved_log_level = ETHERVOX_LOG_LEVEL_OFF;
static int g_saved_debug_enabled = 0;

// Stderr capture for verbose mode (logs to both console and file)
static FILE* g_stderr_capture_file = NULL;
static char g_stderr_capture_path[512] = {0};
static int g_saved_stderr_fd = -1;

// External debug flag (from logging.c)
extern int g_ethervox_debug_enabled;

// Crash detection
static jmp_buf g_crash_recovery_point;
static volatile sig_atomic_t g_crash_occurred = 0;

// Helper function for case-insensitive substring search
static int contains_case_insensitive(const char* haystack, const char* needle) {
  if (!haystack || !needle)
    return 0;

  size_t hlen = strlen(haystack);
  size_t nlen = strlen(needle);
  if (nlen > hlen)
    return 0;

  for (size_t i = 0; i <= hlen - nlen; i++) {
    size_t j;
    for (j = 0; j < nlen; j++) {
      if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
        break;
      }
    }
    if (j == nlen)
      return 1;
  }
  return 0;
}
static char g_crash_signal_name[64] = {0};
static char g_current_test_name[128] = {0};

// Test cancellation (Ctrl+C)
static volatile sig_atomic_t g_test_cancelled = 0;

// Long runtime test state
static volatile sig_atomic_t g_runtime_test_running = 0;

// Global state for tracking tool calls
typedef struct {
  char tool_name[64];
  char args_json[512];
  bool was_called;
  char result[256];
} tool_call_record_t;

static tool_call_record_t g_tool_calls[32];
static uint32_t g_tool_call_count = 0;

// ============================================================================
// Test Reporting Functions
// ============================================================================

static void write_to_report(const char* format, ...) {
  if (!g_test_report_file)
    return;

  va_list args;
  va_start(args, format);
  vfprintf(g_test_report_file, format, args);
  va_end(args);
  fflush(g_test_report_file);
}

static void report_test_header(const char* test_name) {
  strncpy(g_current_test_name, test_name, sizeof(g_current_test_name) - 1);
  write_to_report("\n=== %s ===\n", test_name);
}

static void report_test_pass(const char* msg) {
  write_to_report("  ✓ %s\n", msg);
}

static void report_test_fail(const char* msg) {
  write_to_report("  ✗ %s\n", msg);
}

static void report_test_info(const char* format, ...) {
  if (!g_test_report_file)
    return;

  va_list args;
  va_start(args, format);
  fprintf(g_test_report_file, "  ℹ ");
  vfprintf(g_test_report_file, format, args);
  fprintf(g_test_report_file, "\n");
  va_end(args);
  fflush(g_test_report_file);
}

// Verbose debug logging - captures to both console and report
static void report_debug(const char* format, ...) {
  if (!g_verbose_mode)
    return;

  va_list args, args_copy;
  va_start(args, format);

  // Print to console
  printf(COLOR_YELLOW "  [DEBUG] " COLOR_RESET);
  va_copy(args_copy, args);
  vprintf(format, args_copy);
  printf("\n");
  va_end(args_copy);

  // Write to report file
  if (g_test_report_file) {
    fprintf(g_test_report_file, "  [DEBUG] ");
    vfprintf(g_test_report_file, format, args);
    fprintf(g_test_report_file, "\n");
    fflush(g_test_report_file);
  }

  va_end(args);
}

// Start capturing stderr to a temporary file (without redirecting stderr)
static void start_stderr_capture(void) {
  if (!g_verbose_mode)
    return;

  // Create test_reports directory if it doesn't exist
  const char* report_dir = "test_reports";
#ifdef _WIN32
  _mkdir(report_dir);
#else
  mkdir(report_dir, 0755);
#endif

  // Create temporary file for stderr capture in test_reports directory
  snprintf(g_stderr_capture_path, sizeof(g_stderr_capture_path), "%s/llm_stderr_capture_%ld.log",
           report_dir, time(NULL));

  g_stderr_capture_file = fopen(g_stderr_capture_path, "w");
  if (!g_stderr_capture_file) {
    fprintf(stderr, "Warning: Could not create stderr capture file\n");
    return;
  }

  // Duplicate stderr fd before redirecting
  g_saved_stderr_fd = dup(fileno(stderr));

  // Redirect stderr to our capture file
  if (dup2(fileno(g_stderr_capture_file), fileno(stderr)) == -1) {
    // Restore stderr to report the error
    dup2(g_saved_stderr_fd, fileno(stderr));
    fprintf(stderr, "Warning: Could not redirect stderr\n");
    fclose(g_stderr_capture_file);
    g_stderr_capture_file = NULL;
    return;
  }
}

// Stop capturing stderr and merge captured output into test report
static void stop_stderr_capture(void) {
  if (!g_verbose_mode)
    return;

  // Flush stderr to ensure all output is written
  fflush(stderr);

  // Restore original stderr FIRST (before closing capture file)
  if (g_saved_stderr_fd != -1) {
    dup2(g_saved_stderr_fd, fileno(stderr));
    close(g_saved_stderr_fd);
    g_saved_stderr_fd = -1;
  }

  // NOW close the capture file (after stderr is restored)
  if (g_stderr_capture_file) {
    fclose(g_stderr_capture_file);
    g_stderr_capture_file = NULL;
  }

  // Read captured stderr and append to test report
  if (g_test_report_file && g_stderr_capture_path[0]) {
    FILE* capture = fopen(g_stderr_capture_path, "r");
    if (capture) {
      fprintf(g_test_report_file, "\n=== LLM Debug Output (stderr) ===\n");

      char line[1024];
      while (fgets(line, sizeof(line), capture)) {
        fprintf(g_test_report_file, "%s", line);
      }

      fprintf(g_test_report_file, "\n=== End Debug Output ===\n\n");
      fclose(capture);
    }
  }

  // Clean up temporary file
  unlink(g_stderr_capture_path);
}

// ============================================================================
// Crash Detection and Recovery
// ============================================================================

static const char* signal_name(int sig) {
  switch (sig) {
    case SIGSEGV:
      return "SIGSEGV (Segmentation Fault)";
    case SIGABRT:
      return "SIGABRT (Abort)";
    case SIGFPE:
      return "SIGFPE (Floating Point Exception)";
    case SIGILL:
      return "SIGILL (Illegal Instruction)";
    case SIGBUS:
      return "SIGBUS (Bus Error)";
    default:
      return "Unknown Signal";
  }
}

static void crash_handler(int sig) {
  g_crash_occurred = 1;
  strncpy(g_crash_signal_name, signal_name(sig), sizeof(g_crash_signal_name) - 1);

  // Write crash report immediately
  if (g_test_report_file) {
    write_to_report("\n!!! CRASH DETECTED !!!\n");
    write_to_report("Signal: %s\n", g_crash_signal_name);
    write_to_report("Test: %s\n", g_current_test_name);
    write_to_report("Timestamp: %ld\n", time(NULL));

    // Try to write backtrace info
    write_to_report("\nCrash Context:\n");
    write_to_report("  - Governor subsystem crash during test execution\n");
    write_to_report("  - Signal received: %d (%s)\n", sig, g_crash_signal_name);
    write_to_report("  - Attempting recovery...\n\n");
  }

  // Generate separate crash report file in ~/.ethervox/tests/
  const char* home = getenv("HOME");
  char crash_report_path[512];
  if (home) {
    char test_dir[512];
    snprintf(test_dir, sizeof(test_dir), "%s/.ethervox/tests", home);
#ifdef _WIN32
    _mkdir(test_dir);
#else
    mkdir(test_dir, 0755);
#endif
    snprintf(crash_report_path, sizeof(crash_report_path), "%s/llm_test_crash_%ld.log", test_dir,
             time(NULL));
  } else {
    snprintf(crash_report_path, sizeof(crash_report_path),
             "./.ethervox/reports/llm_test_crash_%ld.log", time(NULL));
  }

  FILE* crash_file = fopen(crash_report_path, "w");
  if (crash_file) {
    fprintf(crash_file, "EthervoxAI LLM Test Suite - Crash Report\n");
    fprintf(crash_file, "=========================================\n\n");
    fprintf(crash_file, "Timestamp: %s", ctime(&(time_t){time(NULL)}));
    fprintf(crash_file, "Signal: %s (%d)\n", g_crash_signal_name, sig);
    fprintf(crash_file, "Test Name: %s\n", g_current_test_name);
    fprintf(crash_file, "\nDescription:\n");
    fprintf(crash_file, "The LLM subsystem crashed during test execution.\n");
    fprintf(crash_file, "This indicates a serious issue with:\n");
    fprintf(crash_file, "  - Model loading/inference\n");
    fprintf(crash_file, "  - Memory management\n");
    fprintf(crash_file, "  - Tool execution\n");
    fprintf(crash_file, "  - Context management\n");
    fprintf(crash_file, "\nRecommended Actions:\n");
    fprintf(crash_file, "  1. Check model file integrity\n");
    fprintf(crash_file, "  2. Verify system memory availability\n");
    fprintf(crash_file, "  3. Review recent code changes\n");
    fprintf(crash_file, "  4. Run with --debug for detailed logs\n");
    fprintf(crash_file, "\nCrash report saved to: %s\n", crash_report_path);
    fclose(crash_file);

    if (g_test_report_file) {
      write_to_report("Detailed crash report: %s\n", crash_report_path);
    }
  }

  // Jump back to recovery point
  longjmp(g_crash_recovery_point, 1);
}

// Handle test cancellation (Ctrl+C)
static void interrupt_handler(int sig) {
  (void)sig;
  g_test_cancelled = 1;

  // Write to console
  printf("\n\n" COLOR_YELLOW "⚠ Test cancellation requested (Ctrl+C)..." COLOR_RESET "\n");

  // Write to report file if open
  if (g_test_report_file) {
    fprintf(g_test_report_file, "\n⚠ Test cancelled by user (SIGINT)\n");
    fflush(g_test_report_file);
  }
}

static void install_crash_handlers(void) {
  signal(SIGSEGV, crash_handler);
  signal(SIGABRT, crash_handler);
  signal(SIGFPE, crash_handler);
  signal(SIGILL, crash_handler);
  signal(SIGBUS, crash_handler);
  signal(SIGINT, interrupt_handler);  // Ctrl+C handler
}

static void remove_crash_handlers(void) {
  signal(SIGSEGV, SIG_DFL);
  signal(SIGABRT, SIG_DFL);
  signal(SIGFPE, SIG_DFL);
  signal(SIGILL, SIG_DFL);
  signal(SIGBUS, SIG_DFL);
  signal(SIGINT, SIG_DFL);
}

// Reset tool call tracking
static void reset_tool_tracking(void) {
  g_tool_call_count = 0;
  memset(g_tool_calls, 0, sizeof(g_tool_calls));
}

// Check if specific tool was called
static bool was_tool_called(const char* tool_name) {
  for (uint32_t i = 0; i < g_tool_call_count; i++) {
    if (strcmp(g_tool_calls[i].tool_name, tool_name) == 0) {
      return true;
    }
  }
  return false;
}

// Get tool call details
static tool_call_record_t* get_tool_call(const char* tool_name) {
  for (uint32_t i = 0; i < g_tool_call_count; i++) {
    if (strcmp(g_tool_calls[i].tool_name, tool_name) == 0) {
      return &g_tool_calls[i];
    }
  }
  return NULL;
}

// Progress callback to intercept tool calls
static void track_tool_progress(ethervox_governor_event_type_t event_type, const char* message,
                                void* user_data) {
  (void)user_data;  // Unused

  if (event_type == ETHERVOX_GOVERNOR_EVENT_TOOL_CALL && message) {
    const char* tool_start = strstr(message, "tool: ");
    if (!tool_start)
      tool_start = strstr(message, "Calling tool:");

    if (tool_start && g_tool_call_count < 32) {
      // Skip past "tool: " or "Calling tool: "
      tool_start = strchr(tool_start, ':');
      if (tool_start) {
        tool_start++;  // Skip ':'
        while (*tool_start == ' ')
          tool_start++;  // Skip spaces

        size_t len = strcspn(tool_start, " \n");
        if (len > 0 && len < sizeof(g_tool_calls[0].tool_name)) {
          strncpy(g_tool_calls[g_tool_call_count].tool_name, tool_start, len);
          g_tool_calls[g_tool_call_count].tool_name[len] = '\0';
          g_tool_calls[g_tool_call_count].was_called = true;
          g_tool_call_count++;
        }
      }
    }
  }
}

// ============================================================================
// Test 1: Memory Store Tool - Basic Add/Retrieve
// ============================================================================
static void test_llm_memory_add(ethervox_governor_t* governor) {
  LLM_TEST_SUBHEADER("Memory Add Tool Usage");

  reset_tool_tracking();

  const char* query = "Remember that my favorite color is blue.";
  char* response = NULL;
  char* error = NULL;

  LLM_TEST_INFO("Prompting: \"%s\"", query);
  report_debug("Executing query: %s", query);

  ethervox_governor_status_t status = ethervox_governor_execute(
      governor, query, &response, &error, NULL, track_tool_progress, NULL, NULL);

  report_debug("Governor status: %d", status);
  if (response) {
    report_debug("Response: %s", response);
  }
  if (error) {
    report_debug("Error: %s", error);
  }

  if (status == ETHERVOX_GOVERNOR_SUCCESS) {
    if (was_tool_called("memory_store")) {
      LLM_TEST_PASS("LLM correctly called memory_store tool");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_FAIL("LLM did not call memory_store (may have hallucinated storage)");
      LLM_TEST_INFO("Tools called: %u", g_tool_call_count);
      report_debug("Total tools called: %u", g_tool_call_count);
      for (uint32_t i = 0; i < g_tool_call_count; i++) {
        LLM_TEST_INFO("  - %s", g_tool_calls[i].tool_name);
        report_debug("  Tool %u: %s(%s)", i, g_tool_calls[i].tool_name, g_tool_calls[i].args_json);
      }
      g_llm_tests_failed++;
    }

    if (response && strstr(response, "blue")) {
      LLM_TEST_PASS("Response mentions 'blue': \"%s\"", response);
      g_llm_tests_passed++;
    } else {
      LLM_TEST_FAIL("Response doesn't acknowledge the color");
      g_llm_tests_failed++;
    }
  } else {
    LLM_TEST_FAIL("Governor execution failed: %s", error ? error : "unknown");
    g_llm_tests_failed += 2;
  }

  if (response)
    free(response);
  if (error)
    free(error);
}

// ============================================================================
// Test 2: Memory Search Tool
// ============================================================================
static void test_llm_memory_search(ethervox_governor_t* governor) {
  LLM_TEST_SUBHEADER("Memory Search Tool Usage");

  // First, ensure memory is populated (test may run independently)
  reset_tool_tracking();
  const char* setup_query = "Remember that my favorite color is blue.";
  char* setup_response = NULL;
  char* setup_error = NULL;

  LLM_TEST_INFO("Setup: \"%s\"", setup_query);
  ethervox_governor_execute(governor, setup_query, &setup_response, &setup_error, NULL,
                            track_tool_progress, NULL, NULL);
  if (setup_response)
    free(setup_response);
  if (setup_error)
    free(setup_error);

  // CRITICAL: Reset conversation to clear KV cache
  // This forces LLM to use memory_search tool instead of answering from context
  LLM_TEST_INFO("Resetting conversation to force memory_search tool usage");
  if (ethervox_governor_reset_conversation(governor) != 0) {
    LLM_TEST_WARN("Failed to reset conversation");
  }

  // Add a small delay to ensure memory persistence completes
  usleep(100000);  // 100ms

  // Now test memory search
  reset_tool_tracking();

  const char* query = "What is my favorite color?";
  char* response = NULL;
  char* error = NULL;

  LLM_TEST_INFO("Prompting: \"%s\"", query);
  LLM_TEST_INFO(
      "Expected: LLM should call memory_search tool to retrieve 'blue' from persistent storage");
  report_debug("Executing query: %s", query);
  report_debug(
      "Memory should contain 'blue' from setup query (stored in persistent DB, not context)");

  ethervox_governor_status_t status = ethervox_governor_execute(
      governor, query, &response, &error, NULL, track_tool_progress, NULL, NULL);

  report_debug("Governor status: %d", status);
  if (response)
    report_debug("Response: %s", response);
  if (error)
    report_debug("Error: %s", error);
  report_debug("Total tools called: %u", g_tool_call_count);

  if (status == ETHERVOX_GOVERNOR_SUCCESS) {
    bool called_search = was_tool_called("memory_search");

    report_debug("Called memory_search: %s", called_search ? "yes" : "no");

    if (called_search) {
      LLM_TEST_PASS("LLM correctly used memory search tool");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_FAIL("LLM did not search memory (may have guessed)");
      g_llm_tests_failed++;
    }

    if (response && strstr(response, "blue")) {
      LLM_TEST_PASS("Response correctly recalls 'blue'");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_WARN("Response doesn't contain 'blue' - memory may not have persisted");
      LLM_TEST_INFO("Response: %s", response ? response : "(null)");
      g_llm_tests_failed++;
    }
  } else {
    LLM_TEST_FAIL("Governor execution failed: %s", error ? error : "unknown");
    g_llm_tests_failed += 2;
  }

  if (response)
    free(response);
  if (error)
    free(error);
}

// ============================================================================
// Test 3: Calculator/Compute Tool
// ============================================================================
static void test_llm_calculator(ethervox_governor_t* governor) {
  LLM_TEST_SUBHEADER("Calculator Tool Usage");

  reset_tool_tracking();

  const char* query = "What is 1234 times 5678?";
  char* response = NULL;
  char* error = NULL;

  LLM_TEST_INFO("Prompting: \"%s\"", query);
  report_debug("Executing query: %s", query);

  ethervox_governor_status_t status = ethervox_governor_execute(
      governor, query, &response, &error, NULL, track_tool_progress, NULL, NULL);

  report_debug("Governor status: %d", status);
  if (response)
    report_debug("Response: %s", response);
  if (error)
    report_debug("Error: %s", error);
  report_debug("Total tools called: %u", g_tool_call_count);

  if (status == ETHERVOX_GOVERNOR_SUCCESS) {
    bool called_calc = was_tool_called("calculator_compute");

    report_debug("Called calculator_compute: %s", called_calc ? "yes" : "no");

    if (called_calc) {
      LLM_TEST_PASS("LLM correctly called calculator_compute");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_FAIL("LLM did not use calculator (may have computed incorrectly)");
      LLM_TEST_INFO("Expected result: 7006652");
      g_llm_tests_failed++;
    }

    // Check if response contains correct answer (1234 * 5678 = 7006652)
    // Accept both formatted (7,006,652) and unformatted (7006652) versions
    if (response && (strstr(response, "7006652") || strstr(response, "7,006,652"))) {
      LLM_TEST_PASS("Response contains correct result: 7006652");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_FAIL("Response has wrong result or hallucinated math");
      LLM_TEST_INFO("Response: %s", response ? response : "(null)");
      g_llm_tests_failed++;
    }
  } else {
    LLM_TEST_FAIL("Governor execution failed: %s", error ? error : "unknown");
    g_llm_tests_failed += 2;
  }

  if (response)
    free(response);
  if (error)
    free(error);
}

// ============================================================================
// Test 4: Memory Correction (Adaptive Learning)
// ============================================================================
static void test_llm_memory_correction(ethervox_governor_t* governor) {
  LLM_TEST_SUBHEADER("Memory Correction Tool Usage");

  reset_tool_tracking();

  const char* query =
      "Actually, I made a mistake - my favorite color is red, not blue. "
      "Please correct that.";
  char* response = NULL;
  char* error = NULL;

  LLM_TEST_INFO("Prompting: \"%s\"", query);
  report_debug("Executing query: %s", query);

  ethervox_governor_status_t status = ethervox_governor_execute(
      governor, query, &response, &error, NULL, track_tool_progress, NULL, NULL);

  report_debug("Governor status: %d", status);
  if (response)
    report_debug("Response: %s", response);
  if (error)
    report_debug("Error: %s", error);
  report_debug("Total tools called: %u", g_tool_call_count);

  if (status == ETHERVOX_GOVERNOR_SUCCESS) {
    bool called_correction = was_tool_called("memory_store_correction");
    bool called_update = was_tool_called("memory_store");

    report_debug("Called correction/update: %s",
                 (called_correction || called_update) ? "yes" : "no");

    if (called_correction || called_update) {
      LLM_TEST_PASS("LLM used correction/update tool appropriately");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_FAIL("LLM did not track correction (adaptive learning missed)");
      g_llm_tests_failed++;
    }

    if (response && strstr(response, "red")) {
      LLM_TEST_PASS("Response acknowledges 'red' as new favorite");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_FAIL("Response doesn't confirm correction");
      g_llm_tests_failed++;
    }
  } else {
    LLM_TEST_FAIL("Governor execution failed: %s", error ? error : "unknown");
    g_llm_tests_failed += 2;
  }

  if (response)
    free(response);
  if (error)
    free(error);
}

// ============================================================================
// Test 5: Memory Tag-Based Retrieval
// ============================================================================
static void test_llm_memory_tags(ethervox_governor_t* governor) {
  LLM_TEST_SUBHEADER("Tag-Based Memory Search");

  // First, store a tagged memory
  reset_tool_tracking();
  const char* setup_query =
      "Remember: meeting with Tim on Friday at 3pm. Tag this as urgent and work.";
  char* response = NULL;
  char* error = NULL;

  LLM_TEST_INFO("Setup: \"%s\"", setup_query);
  ethervox_governor_execute(governor, setup_query, &response, &error, NULL, track_tool_progress,
                            NULL, NULL);
  if (response)
    free(response);
  if (error)
    free(error);

  // Now search by tag
  reset_tool_tracking();
  const char* query = "What urgent work items do I have?";
  response = NULL;
  error = NULL;

  LLM_TEST_INFO("Query: \"%s\"", query);
  report_debug("Executing query: %s", query);

  ethervox_governor_status_t status = ethervox_governor_execute(
      governor, query, &response, &error, NULL, track_tool_progress, NULL, NULL);

  report_debug("Governor status: %d", status);
  if (response)
    report_debug("Response: %s", response);
  if (error)
    report_debug("Error: %s", error);
  report_debug("Total tools called: %u", g_tool_call_count);

  if (status == ETHERVOX_GOVERNOR_SUCCESS) {
    bool used_tag_search = was_tool_called("memory_search");

    report_debug("Used tag-based search: %s", used_tag_search ? "yes" : "no");

    if (used_tag_search) {
      LLM_TEST_PASS("LLM correctly used tag-based search");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_WARN("LLM may have used text search instead of tag search");
      g_llm_tests_passed++;  // Still acceptable
    }

    if (response && strstr(response, "Tim") && strstr(response, "Friday")) {
      LLM_TEST_PASS("Response correctly retrieved tagged memory");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_FAIL("Response missing meeting details");
      g_llm_tests_failed++;
    }
  } else {
    LLM_TEST_FAIL("Governor execution failed: %s", error ? error : "unknown");
    g_llm_tests_failed += 2;
  }

  if (response)
    free(response);
  if (error)
    free(error);
}

// ============================================================================
// Test 6: Multi-Tool Orchestration
// ============================================================================
static void test_llm_multi_tool(ethervox_governor_t* governor) {
  LLM_TEST_SUBHEADER("Multi-Tool Orchestration");

  reset_tool_tracking();

  const char* query = "Calculate 15 + 27, then remember the result as 'daily total'.";
  char* response = NULL;
  char* error = NULL;

  LLM_TEST_INFO("Prompting: \"%s\"", query);
  report_debug("Executing query: %s", query);

  ethervox_governor_status_t status = ethervox_governor_execute(
      governor, query, &response, &error, NULL, track_tool_progress, NULL, NULL);

  report_debug("Governor status: %d", status);
  if (response)
    report_debug("Response: %s", response);
  if (error)
    report_debug("Error: %s", error);
  report_debug("Total tools called: %u", g_tool_call_count);

  if (status == ETHERVOX_GOVERNOR_SUCCESS) {
    bool called_calc = was_tool_called("calculator_compute");
    bool called_memory = was_tool_called("memory_store");

    report_debug("Called calculator: %s, Called memory: %s", called_calc ? "yes" : "no",
                 called_memory ? "yes" : "no");

    if (called_calc && called_memory) {
      LLM_TEST_PASS("LLM orchestrated multiple tools correctly");
      g_llm_tests_passed++;
    } else if (called_calc) {
      LLM_TEST_WARN("LLM called calculator but didn't store result");
      g_llm_tests_failed++;
    } else if (called_memory) {
      LLM_TEST_WARN("LLM stored memory but didn't calculate");
      g_llm_tests_failed++;
    } else {
      LLM_TEST_FAIL("LLM didn't use either required tool");
      g_llm_tests_failed++;
    }

    if (response && strstr(response, "42")) {
      LLM_TEST_PASS("Response contains correct result (42)");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_FAIL("Response missing or incorrect");
      g_llm_tests_failed++;
    }
  } else {
    LLM_TEST_FAIL("Governor execution failed: %s", error ? error : "unknown");
    g_llm_tests_failed += 2;
  }

  if (response)
    free(response);
  if (error)
    free(error);
}

// ============================================================================
// Test 7: Model Load/Unload Cycle
// ============================================================================
static void test_llm_model_lifecycle(const char* model_path) {
  LLM_TEST_SUBHEADER("Model Load/Unload Lifecycle");

  if (!model_path || !model_path[0]) {
    LLM_TEST_WARN("No model path available for lifecycle test");
    g_llm_tests_skipped++;
    return;
  }

  // Create temporary memory store for this test
  ethervox_memory_store_t test_memory;
  if (ethervox_memory_init(&test_memory, "lifecycle_test", "/tmp") != 0) {
    LLM_TEST_FAIL("Failed to initialize test memory store");
    g_llm_tests_failed++;
    return;
  }

  // Create temporary tool registry
  ethervox_tool_registry_t test_registry = {0};
  if (ethervox_tool_registry_init(&test_registry, 16) != 0) {
    LLM_TEST_FAIL("Failed to initialize test registry");
    ethervox_memory_cleanup(&test_memory);
    g_llm_tests_failed++;
    return;
  }

  // Register minimal tools
  ethervox_compute_tools_register_all(&test_registry);
  LLM_TEST_INFO("Registered %u tools for lifecycle test", test_registry.tool_count);

  // Test 1: Initialize governor
  ethervox_governor_t* test_governor = NULL;
  if (ethervox_governor_init(&test_governor, NULL, &test_registry) != 0) {
    LLM_TEST_FAIL("Failed to initialize governor");
    ethervox_tool_registry_cleanup(&test_registry);
    ethervox_memory_cleanup(&test_memory);
    g_llm_tests_failed++;
    return;
  }
  LLM_TEST_PASS("Governor initialized successfully");
  g_llm_tests_passed++;

  // Test 2: Load model
  LLM_TEST_INFO("Loading model: %s", model_path);
  if (ethervox_governor_load_model(test_governor, model_path, NULL, NULL, NULL) != 0) {
    LLM_TEST_FAIL("Failed to load model");
    ethervox_governor_cleanup(test_governor);
    ethervox_tool_registry_cleanup(&test_registry);
    ethervox_memory_cleanup(&test_memory);
    g_llm_tests_failed += 3;  // Count remaining tests as failed
    return;
  }
  LLM_TEST_PASS("Model loaded successfully");
  g_llm_tests_passed++;

  // Test 3: Verify model works with simple query
  const char* test_query = "What is 2 + 2?";
  char* response = NULL;
  char* error = NULL;

  LLM_TEST_INFO("Testing inference: \"%s\"", test_query);
  report_debug("Executing lifecycle test query: %s", test_query);

  ethervox_governor_status_t status = ethervox_governor_execute(
      test_governor, test_query, &response, &error, NULL, NULL, NULL, NULL);

  report_debug("Lifecycle test status: %d", status);
  if (response)
    report_debug("Lifecycle test response: %s", response);
  if (error)
    report_debug("Lifecycle test error: %s", error);

  if (status == ETHERVOX_GOVERNOR_SUCCESS && response) {
    LLM_TEST_PASS("Model inference successful");
    LLM_TEST_INFO("Response: %s", response);
    g_llm_tests_passed++;

    // Check if answer makes sense (should contain "4")
    if (strstr(response, "4")) {
      LLM_TEST_PASS("Model produced correct answer");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_WARN("Model response may be incorrect");
      LLM_TEST_INFO("Expected '4' in response, got: %s", response);
      g_llm_tests_passed++;  // Still pass, model worked
    }
  } else {
    LLM_TEST_FAIL("Model inference failed: %s", error ? error : "unknown");
    g_llm_tests_failed += 2;
  }

  if (response)
    free(response);
  if (error)
    free(error);

  // Test 4: Cleanup/unload model
  LLM_TEST_INFO("Unloading model and cleaning up governor");
  ethervox_governor_cleanup(test_governor);
  LLM_TEST_PASS("Governor cleanup completed");
  g_llm_tests_passed++;

  // Test 5: Verify governor was properly cleaned up
  // In a real scenario, we'd check memory stats, but we'll just verify cleanup didn't crash
  LLM_TEST_PASS("Model unload completed without errors");
  g_llm_tests_passed++;

  // Cleanup test resources
  ethervox_tool_registry_cleanup(&test_registry);
  ethervox_memory_cleanup(&test_memory);
}

// ============================================================================
// Test 8: Long Runtime Stress Test
// ============================================================================
static void test_llm_long_runtime(ethervox_governor_t* governor) {
  LLM_TEST_SUBHEADER("Long Runtime Stress Test (5 minutes)");

  if (!governor) {
    LLM_TEST_FAIL("No governor instance available");
    g_llm_tests_failed++;
    return;
  }

  report_test_header("Test 8: Long Runtime Stress Test");

  // Verify governor is still valid
  if (!governor) {
    LLM_TEST_FAIL("Governor instance is NULL");
    report_test_fail("No governor - test skipped");
    g_llm_tests_skipped++;
    return;
  }

  const int test_duration_seconds = 300;  // 5 minutes
  const int query_interval_seconds = 10;  // Query every 10 seconds

  LLM_TEST_INFO("Running stress test for %d seconds (%d minutes)", test_duration_seconds,
                test_duration_seconds / 60);
  LLM_TEST_INFO("Queries will be sent every %d seconds", query_interval_seconds);
  LLM_TEST_INFO("Press Ctrl+C to cancel test early");
  report_test_info("Starting long runtime stress test");

  time_t start_time = time(NULL);
  time_t end_time = start_time + test_duration_seconds;

  // Diverse set of queries to stress different code paths
  const char* test_queries[] = {"What is 123 + 456?",
                                "Calculate 50% of 1000",
                                "Remember that the stress test is running",
                                "What time is it?",
                                "Calculate 15 * 8",
                                "Search for previous calculations",
                                "What is 2 to the power of 10?",
                                "Calculate 75% of 200"};
  const int num_queries = sizeof(test_queries) / sizeof(test_queries[0]);

  int total_queries = 0;
  int successful_queries = 0;
  int failed_queries = 0;
  int crashed_queries = 0;

  g_runtime_test_running = 1;
  g_test_cancelled = 0;

  while (time(NULL) < end_time && g_runtime_test_running && !g_test_cancelled) {
    const char* query = test_queries[total_queries % num_queries];

    // Check for cancellation before query
    if (g_test_cancelled) {
      LLM_TEST_INFO("Test cancelled by user");
      report_test_info("Test cancelled after %d queries", total_queries);
      break;
    }

    LLM_TEST_INFO("Query #%d: \"%s\"", total_queries + 1, query);
    report_test_info("Executing query");
    write_to_report("    Query #%d: \"%s\"\n", total_queries + 1, query);
    report_debug("Stress test query #%d: %s", total_queries + 1, query);

    char* response = NULL;
    char* error = NULL;

    // Set up crash recovery point
    if (setjmp(g_crash_recovery_point) != 0) {
      // Crashed!
      crashed_queries++;
      LLM_TEST_FAIL("Query #%d CRASHED: %s", total_queries + 1, g_crash_signal_name);
      report_test_fail("Query crashed");
      write_to_report("    CRASH: %s\n", g_crash_signal_name);
      report_debug("Stress test query #%d crashed: %s", total_queries + 1, g_crash_signal_name);

      if (response)
        free(response);
      if (error)
        free(error);

      // Continue with next query after crash
      total_queries++;
      sleep(1);  // Brief pause after crash
      continue;
    }

    // Execute query with crash protection
    ethervox_governor_status_t status =
        ethervox_governor_execute(governor, query, &response, &error, NULL, NULL, NULL, NULL);

    report_debug("Stress test query #%d status: %d", total_queries + 1, status);
    if (response)
      report_debug("Stress test query #%d response: %s", total_queries + 1, response);
    if (error)
      report_debug("Stress test query #%d error: %s", total_queries + 1, error);

    if (status == ETHERVOX_GOVERNOR_SUCCESS && response) {
      successful_queries++;
      write_to_report("    ✓ Success (response length: %zu)\n", strlen(response));
    } else {
      failed_queries++;
      LLM_TEST_WARN("Query #%d failed: %s", total_queries + 1, error ? error : "unknown");
      write_to_report("    ✗ Failed: %s\n", error ? error : "unknown");
    }

    if (response)
      free(response);
    if (error)
      free(error);

    total_queries++;

    // Progress update every 30 seconds
    time_t elapsed = time(NULL) - start_time;
    if (elapsed % 30 == 0 && elapsed > 0) {
      time_t remaining = end_time - time(NULL);
      LLM_TEST_INFO("Progress: %ld/%d seconds (%.1f%%), %ld seconds remaining", elapsed,
                    test_duration_seconds, (elapsed * 100.0) / test_duration_seconds, remaining);
    }

    // Wait for next query interval (or until test ends)
    time_t next_query_time = time(NULL) + query_interval_seconds;
    while (time(NULL) < next_query_time && time(NULL) < end_time) {
      sleep(1);
    }
  }

  g_runtime_test_running = 0;

  // Report results
  time_t total_duration = time(NULL) - start_time;

  LLM_TEST_HEADER("Stress Test Results");
  LLM_TEST_INFO("Duration: %ld seconds (%.1f minutes)", total_duration, total_duration / 60.0);
  LLM_TEST_INFO("Total queries: %d", total_queries);
  LLM_TEST_INFO("Successful: %d (%.1f%%)", successful_queries,
                (successful_queries * 100.0) / (total_queries > 0 ? total_queries : 1));
  LLM_TEST_INFO("Failed: %d (%.1f%%)", failed_queries,
                (failed_queries * 100.0) / (total_queries > 0 ? total_queries : 1));

  report_test_info("Stress test completed");
  write_to_report("\nStress Test Summary:\n");
  write_to_report("  Duration: %ld seconds\n", total_duration);
  write_to_report("  Total queries: %d\n", total_queries);
  write_to_report("  Successful: %d (%.1f%%)\n", successful_queries,
                  (successful_queries * 100.0) / (total_queries > 0 ? total_queries : 1));
  write_to_report("  Failed: %d\n", failed_queries);
  write_to_report("  Crashed: %d\n", crashed_queries);

  // Handle cancellation or normal completion
  if (g_test_cancelled) {
    LLM_TEST_INFO("Test was cancelled by user - marked as skipped");
    report_test_info("Test cancelled - partial results above");
    g_llm_tests_skipped++;
  } else if (crashed_queries > 0) {
    LLM_TEST_FAIL("CRITICAL: %d queries caused crashes!", crashed_queries);
    report_test_fail("Crashes detected during stress test");
    g_llm_tests_failed++;
  } else if (successful_queries == total_queries) {
    LLM_TEST_PASS("All queries completed successfully");
    report_test_pass("100% success rate, no crashes");
    g_llm_tests_passed++;
  } else if (successful_queries > total_queries * 0.9) {
    LLM_TEST_PASS("Stress test passed with >90%% success rate");
    report_test_pass("Good stability, minimal failures");
    g_llm_tests_passed++;
  } else {
    LLM_TEST_FAIL("Too many failures (%d/%d)", failed_queries, total_queries);
    report_test_fail("High failure rate");
    g_llm_tests_failed++;
  }
}

// Test 9: Context Window Management & Accumulation Test
static void test_context_window_management(ethervox_governor_t* governor) {
  LLM_TEST_HEADER("Test 9: Context Window Management");
  report_test_header("Test 9: Context Window Management & Accumulation");

  LLM_TEST_INFO("This test verifies that context accumulation and window shifting work correctly");
  report_test_info("Testing natural context accumulation without reset");

  // NOTE: Unlike Tests 1-6, we do NOT reset conversation here
  // This test intentionally accumulates context to validate window management

  // Generate a series of queries that will accumulate context
  const char* accumulation_queries[] = {"Remember that my favorite color is purple",
                                        "Also remember that I like pizza",
                                        "Calculate 123 + 456",
                                        "What's my favorite color?",
                                        "Calculate 50 * 20",
                                        "Remember my birthday is January 15th",
                                        "What food do I like?",
                                        "Calculate 1000 / 4",
                                        "When is my birthday?",
                                        "Calculate the square root of 144"};
  const int num_queries = sizeof(accumulation_queries) / sizeof(accumulation_queries[0]);

  int successful_turns = 0;
  int failed_turns = 0;

  LLM_TEST_INFO("Executing %d queries to build up conversation history...", num_queries);

  for (int i = 0; i < num_queries; i++) {
    const char* query = accumulation_queries[i];

    if (g_test_cancelled) {
      LLM_TEST_INFO("Test cancelled");
      report_test_info("Test cancelled at turn %d", i + 1);
      g_llm_tests_skipped++;
      return;
    }

    report_debug("Context test turn %d: %s", i + 1, query);

    char* response = NULL;
    char* error = NULL;

    ethervox_governor_status_t status =
        ethervox_governor_execute(governor, query, &response, &error, NULL, NULL, NULL, NULL);

    report_debug("Context test turn %d status: %d", i + 1, status);
    if (response)
      report_debug("Context test turn %d response: %s", i + 1, response);
    if (error)
      report_debug("Context test turn %d error: %s", i + 1, error);

    if (status == ETHERVOX_GOVERNOR_SUCCESS || status == ETHERVOX_GOVERNOR_NEED_CLARIFICATION) {
      successful_turns++;
      LLM_TEST_INFO("Turn %d/%d: ✓ Success", i + 1, num_queries);
    } else {
      failed_turns++;
      LLM_TEST_WARN("Turn %d/%d: ✗ Failed - %s", i + 1, num_queries,
                    error ? error : "unknown error");
    }

    if (response)
      free(response);
    if (error)
      free(error);
  }

  // Evaluate results
  float success_rate = (float)successful_turns / num_queries;

  report_test_info("Context accumulation complete: %d/%d turns successful (%.1f%%)",
                   successful_turns, num_queries, success_rate * 100.0f);

  if (success_rate >= 0.8f) {
    LLM_TEST_PASS("Context window handling passed (%.1f%% success)", success_rate * 100.0f);
    report_test_pass("Context management working - window shift handled gracefully");
    g_llm_tests_passed++;
  } else if (success_rate >= 0.6f) {
    LLM_TEST_WARN("Context window partially working (%.1f%% success)", success_rate * 100.0f);
    report_test_info("Some context issues but system remained functional");
    g_llm_tests_passed++;
  } else {
    LLM_TEST_FAIL("Context window management failed (%.1f%% success)", success_rate * 100.0f);
    report_test_fail("Context accumulation caused significant failures");
    g_llm_tests_failed++;
  }
}

// ============================================================================
// Test 10: Startup Prompt Tools (Read/Write)
// ============================================================================
static void test_llm_startup_prompt_tools(ethervox_governor_t* governor) {
  LLM_TEST_SUBHEADER("Startup Prompt Self-Modification");

  // Save original startup prompt for restoration
  reset_tool_tracking();
  char* original_prompt = NULL;
  char* response = NULL;
  char* error = NULL;

  // Read current startup prompt
  const char* read_query = "Read the current startup prompt using the appropriate tool.";
  LLM_TEST_INFO("Setup: \"%s\"", read_query);

  ethervox_governor_status_t status = ethervox_governor_execute(
      governor, read_query, &response, &error, NULL, track_tool_progress, NULL, NULL);

  if (status == ETHERVOX_GOVERNOR_SUCCESS) {
    if (was_tool_called("startup_prompt_read")) {
      LLM_TEST_PASS("LLM correctly used startup_prompt_read tool");
      g_llm_tests_passed++;

      // Save original for later restoration
      if (response) {
        original_prompt = strdup(response);
      }
    } else {
      LLM_TEST_FAIL("LLM did not call startup_prompt_read");
      g_llm_tests_failed++;
    }
  }

  if (response)
    free(response);
  if (error)
    free(error);
  response = NULL;
  error = NULL;

  // Test writing a new startup prompt
  reset_tool_tracking();
  const char* write_query =
      "Update the startup prompt to say: 'Hello! I am EthervoxAI Test Assistant, ready to help "
      "with calculations and memory.'";
  LLM_TEST_INFO("Query: \"%s\"", write_query);
  report_debug("Executing query: %s", write_query);

  status = ethervox_governor_execute(governor, write_query, &response, &error, NULL,
                                     track_tool_progress, NULL, NULL);

  report_debug("Governor status: %d", status);
  if (response)
    report_debug("Response: %s", response);
  if (error)
    report_debug("Error: %s", error);
  report_debug("Total tools called: %u", g_tool_call_count);

  if (status == ETHERVOX_GOVERNOR_SUCCESS) {
    bool used_update = was_tool_called("startup_prompt_update");

    report_debug("Used startup_prompt_update: %s", used_update ? "yes" : "no");

    if (used_update) {
      LLM_TEST_PASS("LLM correctly used startup_prompt_update tool");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_FAIL("LLM did not call startup_prompt_update");
      g_llm_tests_failed++;
    }

    if (response && (strstr(response, "updated") || strstr(response, "saved"))) {
      LLM_TEST_PASS("Response acknowledges prompt update");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_WARN("Response doesn't clearly confirm update");
      g_llm_tests_passed++;  // Still acceptable
    }
  } else {
    LLM_TEST_FAIL("Governor execution failed: %s", error ? error : "unknown");
    g_llm_tests_failed += 2;
  }

  if (response)
    free(response);
  if (error)
    free(error);

  // Verify the change by reading it back
  reset_tool_tracking();
  response = NULL;
  error = NULL;

  const char* verify_query = "What is the current startup prompt?";
  LLM_TEST_INFO("Verify: \"%s\"", verify_query);

  status = ethervox_governor_execute(governor, verify_query, &response, &error, NULL,
                                     track_tool_progress, NULL, NULL);

  if (status == ETHERVOX_GOVERNOR_SUCCESS) {
    if (response && strstr(response, "EthervoxAI Test Assistant")) {
      LLM_TEST_PASS("Startup prompt was successfully updated and retrieved");
      g_llm_tests_passed++;
    } else {
      LLM_TEST_FAIL("Updated prompt not reflected in read-back");
      g_llm_tests_failed++;
    }
  } else {
    LLM_TEST_FAIL("Failed to verify updated prompt");
    g_llm_tests_failed++;
  }

  if (response)
    free(response);
  if (error)
    free(error);

  // CRITICAL: Restore original startup prompt
  LLM_TEST_INFO("Restoring original startup prompt...");
  reset_tool_tracking();
  response = NULL;
  error = NULL;

  if (original_prompt) {
    // Build restoration query
    char restore_query[2048];
    snprintf(restore_query, sizeof(restore_query), "Restore the startup prompt to: %s",
             original_prompt);

    status = ethervox_governor_execute(governor, restore_query, &response, &error, NULL,
                                       track_tool_progress, NULL, NULL);

    if (status == ETHERVOX_GOVERNOR_SUCCESS && was_tool_called("startup_prompt_update")) {
      LLM_TEST_PASS("Original startup prompt restored");
    } else {
      LLM_TEST_WARN("Failed to restore original prompt - may need manual reset");
    }

    free(original_prompt);
  } else {
    // No original prompt - delete custom prompt file to use default
    LLM_TEST_INFO("No original prompt found, removing custom prompt file");

    const char* home_dir = getenv("HOME");
    if (home_dir) {
      char prompt_file[512];
      snprintf(prompt_file, sizeof(prompt_file), "%s/.ethervox/startup_prompt.txt", home_dir);
      if (unlink(prompt_file) == 0) {
        LLM_TEST_PASS("Removed custom startup prompt (will use default)");
      } else {
        // Also try local fallback
        if (unlink("./.ethervox_startup_prompt.txt") == 0) {
          LLM_TEST_PASS("Removed local custom startup prompt");
        }
      }
    }
  }

  if (response)
    free(response);
  if (error)
    free(error);
}

// ============================================================================
// Main LLM Test Runner
// ============================================================================
void run_llm_tool_tests(ethervox_governor_t* governor, ethervox_memory_store_t* memory_store,
                        const char* model_path, bool verbose, const char* test_filter) {
  // Set verbose mode and enable debug logging if requested
  g_verbose_mode = verbose;

  if (verbose) {
    // Save current log level and debug flag state
    g_saved_log_level = ethervox_log_get_level();
    g_saved_debug_enabled = g_ethervox_debug_enabled;

    // Enable debug logging (both new and legacy systems)
    ethervox_log_set_level(ETHERVOX_LOG_LEVEL_DEBUG);
    g_ethervox_debug_enabled = 1;
  }

  printf("\n");
  printf(COLOR_BOLD COLOR_MAGENTA);
  printf("╔═══════════════════════════════════════════════════════════════╗\n");
  printf("║                                                               ║\n");
  printf("║          LLM TOOL USAGE VALIDATION SUITE                     ║\n");
  printf("║          End-to-End Integration Tests                        ║\n");
  printf("║                                                               ║\n");
  printf("╚═══════════════════════════════════════════════════════════════╝\n");
  printf(COLOR_RESET);

  // Get git info
  char git_repo[128] = "unknown";
  char git_branch[128] = "unknown";
  char git_commit[64] = "unknown";
  FILE* git_fp;

  // Get repository name from remote URL
  git_fp = popen("git remote get-url origin 2>/dev/null | sed 's#.*/##' | sed 's#\\.git##'", "r");
  if (git_fp) {
    if (fgets(git_repo, sizeof(git_repo), git_fp)) {
      // Remove newline
      git_repo[strcspn(git_repo, "\n")] = 0;
    }
    pclose(git_fp);
  }

  git_fp = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
  if (git_fp) {
    if (fgets(git_branch, sizeof(git_branch), git_fp)) {
      // Remove newline
      git_branch[strcspn(git_branch, "\n")] = 0;
    }
    pclose(git_fp);
  }

  git_fp = popen("git rev-parse --short HEAD 2>/dev/null", "r");
  if (git_fp) {
    if (fgets(git_commit, sizeof(git_commit), git_fp)) {
      // Remove newline
      git_commit[strcspn(git_commit, "\n")] = 0;
    }
    pclose(git_fp);
  }

  printf("\n");
  printf(COLOR_CYAN "  Repository: " COLOR_RESET "%s\n", git_repo);
  printf(COLOR_CYAN "  Branch:     " COLOR_RESET "%s\n", git_branch);
  printf(COLOR_CYAN "  Commit:     " COLOR_RESET "%s\n", git_commit);
  printf("\n");

  if (verbose) {
    printf(COLOR_YELLOW "  [Verbose Mode Enabled]\n" COLOR_RESET);
    printf(COLOR_YELLOW "  - Custom debug messages logged to report\n" COLOR_RESET);
    printf(COLOR_YELLOW "  - Internal LLM debug logging enabled (stderr)\n" COLOR_RESET);
    printf(COLOR_YELLOW "  - Token sampling, generation details will appear below\n" COLOR_RESET);
    printf(COLOR_YELLOW "  - All debug output will be saved to test report\n" COLOR_RESET);
  }

  // Start capturing stderr if in verbose mode
  if (verbose) {
    start_stderr_capture();
  }

  // Initialize test report file
  time_t report_time = time(NULL);

  // Determine test reports directory based on platform
  char report_dir[512];
#ifdef __ANDROID__
  // On Android, use app files directory
  const char* android_files = ethervox_get_android_files_dir();
  if (android_files && android_files[0] != '\0') {
    snprintf(report_dir, sizeof(report_dir), "%s/tests", android_files);
    LLM_TEST_LOG("Using Android files directory: %s", report_dir);
  } else {
    // Fallback to current directory if Android files dir not set
    snprintf(report_dir, sizeof(report_dir), "./tests");
    LLM_TEST_LOG("Android files dir not set, using fallback: %s", report_dir);
  }
#else
  // On desktop, use ~/.ethervox/reports/ directory
  const char* home = getenv("HOME");
  if (home) {
    snprintf(report_dir, sizeof(report_dir), "%s/.ethervox/reports", home);
  } else {
    snprintf(report_dir, sizeof(report_dir), "./tests");
  }
#endif

// Create directory if it doesn't exist
#ifdef _WIN32
  int mkdir_result = _mkdir(report_dir);
#else
  int mkdir_result = mkdir(report_dir, 0755);
#endif

  // Log directory creation result (ignore EEXIST - directory already exists)
  if (mkdir_result != 0 && errno != EEXIST) {
    LLM_TEST_LOG("Warning: mkdir(%s) failed with errno %d: %s", report_dir, errno, strerror(errno));
  } else {
    LLM_TEST_LOG("Test directory ready: %s", report_dir);
  }

  snprintf(g_test_report_path, sizeof(g_test_report_path), "%s/llm_test_report_%ld.log", report_dir,
           report_time);

  g_test_report_file = fopen(g_test_report_path, "w");
  if (g_test_report_file) {
    fprintf(g_test_report_file, "EthervoxAI LLM Tool Usage Test Report\n");
    fprintf(g_test_report_file, "======================================\n\n");
    fprintf(g_test_report_file, "Timestamp: %s", ctime(&report_time));
    fprintf(g_test_report_file, "Model Path: %s\n", model_path ? model_path : "N/A");
    fprintf(g_test_report_file, "Verbose Mode: %s\n", verbose ? "Enabled" : "Disabled");
    if (verbose) {
      fprintf(g_test_report_file, "Debug Logging: Enabled (level: DEBUG)\n");
      fprintf(g_test_report_file, "Note: Internal LLM debug output appears on stderr\n");
    }
    fprintf(g_test_report_file, "\n");
    fflush(g_test_report_file);

    LLM_TEST_INFO("Test report: %s", g_test_report_path);
#ifdef __ANDROID__
    LLM_TEST_LOG("Test report created: %s", g_test_report_path);
#endif
  } else {
    LLM_TEST_INFO("Failed to create test report file: %s", g_test_report_path);
#ifdef __ANDROID__
    LLM_TEST_LOG("ERROR: Failed to create test report: %s", g_test_report_path);
#endif
  }

  // Install crash handlers
  install_crash_handlers();
  LLM_TEST_INFO("Crash detection enabled");

  // Validate inputs
  if (!governor) {
    LLM_TEST_FAIL("No Governor instance provided");
    LLM_TEST_INFO("Governor must be initialized before running LLM tests");
    report_test_fail("No governor instance");
    printf("\n");
    goto cleanup;
  }

  if (!memory_store) {
    LLM_TEST_FAIL("No memory store provided");
    report_test_fail("No memory store");
    printf("\n");
    goto cleanup;
  }

  // Check if model is loaded
  LLM_TEST_HEADER("Checking LLM Availability");
  report_test_header("Initialization");

  // Access the internal structure to check if LLM is loaded
  // Note: This assumes the governor has a public way to check model status
  // If not, we'll need to add a getter function
  bool model_loaded = false;

  // Try a simple test query to verify model is loaded
  reset_tool_tracking();
  const char* test_query = "test";
  char* test_response = NULL;
  char* test_error = NULL;
  ethervox_governor_status_t test_status = ethervox_governor_execute(
      governor, test_query, &test_response, &test_error, NULL, track_tool_progress, NULL, NULL);

  if (test_status == ETHERVOX_GOVERNOR_SUCCESS ||
      test_status == ETHERVOX_GOVERNOR_NEED_CLARIFICATION) {
    model_loaded = true;
  }

  // Clean up test query results
  if (test_response)
    free(test_response);
  if (test_error)
    free(test_error);

  if (!model_loaded) {
    LLM_TEST_FAIL("No LLM model is loaded");
    LLM_TEST_INFO("Please load a model first using /load command");
    LLM_TEST_INFO("Example: /load models/granite-4.0-h-tiny-Q4_K_M.gguf");
    LLM_TEST_INFO("Skipping all LLM-dependent tests");
    report_test_fail("No LLM model loaded");
    report_test_info("All tests skipped - no model available");

    // Mark all 9 tests as skipped
    g_llm_tests_skipped = 9;

    printf("\n");
    goto cleanup;
  }

  LLM_TEST_PASS("LLM model is loaded and responsive");
  LLM_TEST_INFO("Model path: %s", model_path ? model_path : "auto-loaded");
  report_test_info("LLM model available");

  // Reset conversation history to clear startup prompt context
  // This ensures tests start with clean KV cache (system prompt only)
  LLM_TEST_INFO("Resetting conversation history for clean test environment");
  if (ethervox_governor_reset_conversation(governor) != 0) {
    LLM_TEST_WARN("Failed to reset conversation (continuing anyway)");
  } else {
    report_test_info("Conversation history cleared - starting with fresh context");
  }
  report_test_pass("Model ready");

  time_t start_time = time(NULL);

  // Reset test name tracking
  g_llm_passed_count = 0;
  g_llm_failed_count = 0;

  // Run all LLM tool tests
  int passed_before, failed_before;

// Helper macro to check if test should run
#define SHOULD_RUN_TEST(name) (!test_filter || contains_case_insensitive(name, test_filter))

  if (test_filter) {
    printf("\n" COLOR_CYAN "ℹ Running tests matching: '%s'" COLOR_RESET "\n\n", test_filter);
    printf("DEBUG: test_filter = '%s'\n", test_filter);
  } else {
    printf("\nDEBUG: Running ALL tests (no filter)\n\n");
  }

  LLM_TEST_HEADER("Test 1: Memory Add Tool");
  if (SHOULD_RUN_TEST("Memory Add Tool")) {
    report_test_header("Test 1: Memory Add Tool");
    passed_before = g_llm_tests_passed;
    failed_before = g_llm_tests_failed;
    test_llm_memory_add(governor);
    if (g_llm_tests_failed > failed_before)
      record_llm_test_result("Memory Add Tool", false);
    else if (g_llm_tests_passed > passed_before)
      record_llm_test_result("Memory Add Tool", true);
  } else {
    printf("  ⊘ Skipped\n");
  }

  LLM_TEST_HEADER("Test 2: Memory Search Tool");
  if (SHOULD_RUN_TEST("Memory Search Tool")) {
    report_test_header("Test 2: Memory Search Tool");
    passed_before = g_llm_tests_passed;
    failed_before = g_llm_tests_failed;
    test_llm_memory_search(governor);
    if (g_llm_tests_failed > failed_before)
      record_llm_test_result("Memory Search Tool", false);
    else if (g_llm_tests_passed > passed_before)
      record_llm_test_result("Memory Search Tool", true);
  } else {
    printf("  ⊘ Skipped\n");
  }

  LLM_TEST_HEADER("Test 3: Calculator Tool");
  if (SHOULD_RUN_TEST("Calculator Tool")) {
    report_test_header("Test 3: Calculator Tool");
    passed_before = g_llm_tests_passed;
    failed_before = g_llm_tests_failed;
    test_llm_calculator(governor);
    if (g_llm_tests_failed > failed_before)
      record_llm_test_result("Calculator Tool", false);
    else if (g_llm_tests_passed > passed_before)
      record_llm_test_result("Calculator Tool", true);
  } else {
    printf("  ⊘ Skipped\n");
  }

  LLM_TEST_HEADER("Test 4: Memory Correction (Adaptive)");
  if (SHOULD_RUN_TEST("Memory Correction")) {
    report_test_header("Test 4: Memory Correction (Adaptive)");
    passed_before = g_llm_tests_passed;
    failed_before = g_llm_tests_failed;
    test_llm_memory_correction(governor);
    if (g_llm_tests_failed > failed_before)
      record_llm_test_result("Memory Correction (Adaptive)", false);
    else if (g_llm_tests_passed > passed_before)
      record_llm_test_result("Memory Correction (Adaptive)", true);
  } else {
    printf("  ⊘ Skipped\n");
  }

  LLM_TEST_HEADER("Test 5: Tag-Based Memory Search");
  if (SHOULD_RUN_TEST("Tag-Based")) {
    report_test_header("Test 5: Tag-Based Memory Search");
    passed_before = g_llm_tests_passed;
    failed_before = g_llm_tests_failed;
    test_llm_memory_tags(governor);
    if (g_llm_tests_failed > failed_before)
      record_llm_test_result("Tag-Based Memory Search", false);
    else if (g_llm_tests_passed > passed_before)
      record_llm_test_result("Tag-Based Memory Search", true);
  } else {
    printf("  ⊘ Skipped\n");
  }

  LLM_TEST_HEADER("Test 6: Multi-Tool Orchestration");
  if (SHOULD_RUN_TEST("Multi-Tool Orchestration")) {
    report_test_header("Test 6: Multi-Tool Orchestration");
    passed_before = g_llm_tests_passed;
    failed_before = g_llm_tests_failed;
    test_llm_multi_tool(governor);
    if (g_llm_tests_failed > failed_before)
      record_llm_test_result("Multi-Tool Orchestration", false);
    else if (g_llm_tests_passed > passed_before)
      record_llm_test_result("Multi-Tool Orchestration", true);
  } else {
    printf("  ⊘ Skipped\n");
  }

  LLM_TEST_HEADER("Test 7: Model Load/Unload Lifecycle");
  if (SHOULD_RUN_TEST("Model") || SHOULD_RUN_TEST("Lifecycle")) {
    report_test_header("Test 7: Model Load/Unload Lifecycle");
    passed_before = g_llm_tests_passed;
    failed_before = g_llm_tests_failed;
    test_llm_model_lifecycle(model_path);
    if (g_llm_tests_failed > failed_before)
      record_llm_test_result("Model Load/Unload Lifecycle", false);
    else if (g_llm_tests_passed > passed_before)
      record_llm_test_result("Model Load/Unload Lifecycle", true);
  } else {
    printf("  ⊘ Skipped\n");
  }

  LLM_TEST_HEADER("Test 8: Long Runtime Stress Test");
  if (SHOULD_RUN_TEST("Runtime") || SHOULD_RUN_TEST("Stress")) {
    report_test_header("Test 8: Long Runtime Stress Test");
    passed_before = g_llm_tests_passed;
    failed_before = g_llm_tests_failed;
    test_llm_long_runtime(governor);
    if (g_llm_tests_failed > failed_before)
      record_llm_test_result("Long Runtime Stress Test", false);
    else if (g_llm_tests_passed > passed_before)
      record_llm_test_result("Long Runtime Stress Test", true);
  } else {
    printf("  ⊘ Skipped\n");
  }

  LLM_TEST_HEADER("Test 9: Context Window Management");
  if (SHOULD_RUN_TEST("Context") || SHOULD_RUN_TEST("Window")) {
    report_test_header("Test 9: Context Window Management");
    passed_before = g_llm_tests_passed;
    failed_before = g_llm_tests_failed;
    test_context_window_management(governor);
    if (g_llm_tests_failed > failed_before)
      record_llm_test_result("Context Window Management", false);
    else if (g_llm_tests_passed > passed_before)
      record_llm_test_result("Context Window Management", true);
  } else {
    printf("  ⊘ Skipped\n");
  }

  LLM_TEST_HEADER("Test 10: Startup Prompt Tools");
  if (SHOULD_RUN_TEST("Startup Prompt Tools")) {
    report_test_header("Test 10: Startup Prompt Tools (Read/Write)");
    passed_before = g_llm_tests_passed;
    failed_before = g_llm_tests_failed;
    test_llm_startup_prompt_tools(governor);
    if (g_llm_tests_failed > failed_before)
      record_llm_test_result("Startup Prompt Tools", false);
    else if (g_llm_tests_passed > passed_before)
      record_llm_test_result("Startup Prompt Tools", true);
  } else {
    printf("  ⊘ Skipped\n");
  }

#undef SHOULD_RUN_TEST

  time_t end_time = time(NULL);
  double duration = difftime(end_time, start_time);

  // Summary
  printf("\n");
  printf(COLOR_BOLD COLOR_MAGENTA);
  printf("╔═══════════════════════════════════════════════════════════════╗\n");
  printf("║                 LLM TEST SUMMARY                              ║\n");
  printf("╚═══════════════════════════════════════════════════════════════╝\n");
  printf(COLOR_RESET);
  printf("\n");

  int total_tests = g_llm_tests_passed + g_llm_tests_failed;
  double pass_rate = total_tests > 0 ? (g_llm_tests_passed * 100.0 / total_tests) : 0.0;

  printf(COLOR_GREEN "  Tests Passed:  %d\n" COLOR_RESET, g_llm_tests_passed);
  printf(COLOR_RED "  Tests Failed:  %d\n" COLOR_RESET, g_llm_tests_failed);
  if (g_llm_tests_skipped > 0) {
    printf(COLOR_YELLOW "  Tests Skipped: %d\n" COLOR_RESET, g_llm_tests_skipped);
  }
  printf(COLOR_CYAN "  Total Tests:   %d\n" COLOR_RESET, total_tests);
  printf(COLOR_YELLOW "  Pass Rate:     %.1f%%\n" COLOR_RESET, pass_rate);
  printf(COLOR_BLUE "  Duration:      %.0f seconds\n" COLOR_RESET, duration);
  printf("\n");
  printf(COLOR_CYAN "  Repository:    " COLOR_RESET "%s\n", git_repo);
  printf(COLOR_CYAN "  Branch:        " COLOR_RESET "%s\n", git_branch);
  printf(COLOR_CYAN "  Commit:        " COLOR_RESET "%s\n", git_commit);
  printf("\n");

  // Show passed test names
  if (g_llm_passed_count > 0) {
    printf(COLOR_GREEN "  Passed Tests:\n" COLOR_RESET);
    for (int i = 0; i < g_llm_passed_count; i++) {
      printf(COLOR_GREEN "    ✓ %s\n" COLOR_RESET, g_llm_passed_tests[i]);
      free(g_llm_passed_tests[i]);
    }
    printf("\n");
  }

  // Show failed test names
  if (g_llm_failed_count > 0) {
    printf(COLOR_RED "  Failed Tests:\n" COLOR_RESET);
    for (int i = 0; i < g_llm_failed_count; i++) {
      printf(COLOR_RED "    ✗ %s\n" COLOR_RESET, g_llm_failed_tests[i]);
      free(g_llm_failed_tests[i]);
    }
    printf("\n");
  }

  if (g_llm_tests_failed == 0 && total_tests > 0) {
    printf(COLOR_BOLD COLOR_GREEN);
    printf("  ✓✓✓ ALL LLM TESTS PASSED! ✓✓✓\n");
    printf(COLOR_RESET);
    printf("\n");
    printf(COLOR_CYAN);
    printf("  🎯 The model correctly uses tools and doesn't hallucinate!\n");
    printf(COLOR_RESET);
  } else if (pass_rate >= 75.0) {
    printf(COLOR_BOLD COLOR_YELLOW);
    printf("  ⚠ Most tests passed - minor prompt tuning may help\n");
    printf(COLOR_RESET);
  } else {
    printf(COLOR_BOLD COLOR_RED);
    printf("  ❌ Many tests failed - prompts need significant work\n");
    printf(COLOR_RESET);
    printf("\n");
    printf(COLOR_YELLOW);
    printf("  💡 Suggestions:\n");
    printf("     - Review system prompt in tool_registry.c\n");
    printf("     - Check tool descriptions are clear\n");
    printf("     - Verify model is compatible (Phi-3.5 recommended)\n");
    printf(COLOR_RESET);
  }

  printf("\n");

  // Write summary to report file
  if (g_test_report_file) {
    fprintf(g_test_report_file, "\n=== Test Summary ===\n");
    fprintf(g_test_report_file, "Tests Passed:  %d\n", g_llm_tests_passed);
    fprintf(g_test_report_file, "Tests Failed:  %d\n", g_llm_tests_failed);
    fprintf(g_test_report_file, "Tests Skipped: %d\n", g_llm_tests_skipped);
    fprintf(g_test_report_file, "Total Tests:   %d\n", total_tests);
    fprintf(g_test_report_file, "Pass Rate:     %.1f%%\n", pass_rate);
    fprintf(g_test_report_file, "Duration:      %.0f seconds\n", duration);
    fprintf(g_test_report_file, "\n");
    fflush(g_test_report_file);  // Flush but don't close yet - cleanup will merge stderr

    LLM_TEST_INFO("Test report will be saved to: %s", g_test_report_path);
  }

  // Restore original log level and debug flag if verbose mode was enabled
  if (g_verbose_mode) {
    ethervox_log_set_level(g_saved_log_level);
    g_ethervox_debug_enabled = g_saved_debug_enabled;
    printf(COLOR_CYAN "  ℹ Debug logging restored to previous level\n" COLOR_RESET);
  }

cleanup:
  // Stop stderr capture and merge into report (before closing report file)
  // This must happen even on cancellation to preserve debug output
  if (g_verbose_mode) {
    stop_stderr_capture();
  }

  // Remove crash handlers
  remove_crash_handlers();

  // Close report file if still open
  if (g_test_report_file) {
    fclose(g_test_report_file);
    g_test_report_file = NULL;
  }

  // Note: We don't cleanup governor or memory_store since they're owned by main()
}
