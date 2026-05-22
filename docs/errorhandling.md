# Error Handling Specification

**Version:** 1.2  
**Status:** Implemented & Tested  
**Last Updated:** March 30, 2026

## Overview

This document specifies a consistent error handling approach across the EtherVoxAI codebase, covering both the C runtime and Node.js/TypeScript implementations.

**March 2026 Philosophy Update:** Error handling verbosity should match operation criticality. Critical subsystems get full context logging, optional features get simple error codes, and independent batch operations use array-based patterns to avoid boilerplate while maintaining error visibility.

## Goals

- **Consistency**: Uniform error handling patterns across all modules
- **Context preservation**: Rich error information for debugging
- **Type safety**: Leverage type systems where available
- **Graceful degradation**: Handle missing hardware without catastrophic failures
- **Migration path**: Incremental adoption without breaking existing code

## C Runtime Error Handling

### Error Code System

All functions should return `ethervox_result_t` for consistency. Success is `ETHERVOX_SUCCESS` (0), errors are negative values.

#### Error Code Categories

```c
// General errors (-1 to -99)
ETHERVOX_SUCCESS = 0
ETHERVOX_ERROR_GENERIC = -1
ETHERVOX_ERROR_NULL_POINTER = -2
ETHERVOX_ERROR_INVALID_ARGUMENT = -3
ETHERVOX_ERROR_OUT_OF_MEMORY = -4
ETHERVOX_ERROR_NOT_INITIALIZED = -5
ETHERVOX_ERROR_ALREADY_INITIALIZED = -6
ETHERVOX_ERROR_TIMEOUT = -7
ETHERVOX_ERROR_NOT_SUPPORTED = -8
ETHERVOX_ERROR_BUFFER_TOO_SMALL = -9
ETHERVOX_ERROR_NOT_IMPLEMENTED = -10
ETHERVOX_ERROR_FAILED = -11
ETHERVOX_ERROR_NOT_FOUND = -12

// Platform/HAL errors (-100 to -199)
ETHERVOX_ERROR_PLATFORM_INIT = -100
ETHERVOX_ERROR_HAL_NOT_FOUND = -101
ETHERVOX_ERROR_GPIO_FAILURE = -102
ETHERVOX_ERROR_HARDWARE_NOT_AVAILABLE = -103

// Audio errors (-200 to -299)
ETHERVOX_ERROR_AUDIO_INIT = -200
ETHERVOX_ERROR_AUDIO_DEVICE_NOT_FOUND = -201
ETHERVOX_ERROR_AUDIO_FORMAT_UNSUPPORTED = -202
ETHERVOX_ERROR_AUDIO_BUFFER_OVERFLOW = -203
ETHERVOX_ERROR_AUDIO_BUFFER_UNDERFLOW = -204
ETHERVOX_ERROR_AUDIO_DEVICE_BUSY = -205

// STT errors (-300 to -399)
ETHERVOX_ERROR_STT_INIT = -300
ETHERVOX_ERROR_STT_MODEL_NOT_FOUND = -301
ETHERVOX_ERROR_STT_PROCESSING = -302

// Wake word errors (-400 to -499)
ETHERVOX_ERROR_WAKEWORD_INIT = -400
ETHERVOX_ERROR_WAKEWORD_MODEL_NOT_FOUND = -401

// Plugin errors (-500 to -599)
ETHERVOX_ERROR_PLUGIN_NOT_FOUND = -500
ETHERVOX_ERROR_PLUGIN_INIT = -501
ETHERVOX_ERROR_PLUGIN_EXECUTION = -502
ETHERVOX_ERROR_PLUGIN_MAX_REACHED = -503

// Network/API errors (-600 to -699)
ETHERVOX_ERROR_NETWORK = -600
ETHERVOX_ERROR_API_CALL = -601
ETHERVOX_ERROR_API_RESPONSE = -602
ETHERVOX_ERROR_API_RATE_LIMIT = -603

// File I/O errors (-700 to -799)
ETHERVOX_ERROR_FILE_NOT_FOUND = -700
ETHERVOX_ERROR_FILE_READ = -701
ETHERVOX_ERROR_FILE_WRITE = -702
ETHERVOX_ERROR_FILE_PERMISSION = -703
```

### Error Context

Thread-local error context provides detailed diagnostic information:

```c
typedef struct {
    ethervox_result_t code;
    const char* message;
    const char* file;
    int line;
    const char* function;
    uint64_t timestamp_ms;
} ethervox_error_context_t;
```

### Core API

```c
// Convert error code to string
const char* ethervox_error_string(ethervox_result_t result);

// Check if result is success/error
bool ethervox_is_success(ethervox_result_t result);
bool ethervox_is_error(ethervox_result_t result);

// Error context management
void ethervox_error_set_context(ethervox_result_t code, const char* message,
                                const char* file, int line, const char* function);
const ethervox_error_context_t* ethervox_error_get_context(void);
void ethervox_error_clear(void);
```

### Convenience Macros

```c
// Set error with context
ETHERVOX_ERROR_SET(code, msg)

// Return error with context
ETHERVOX_RETURN_ERROR(code, msg)

// Check result and propagate error
ETHERVOX_CHECK(expr)

// Check pointer and return error if NULL
ETHERVOX_CHECK_PTR(ptr)
```

### Usage Examples

#### Error Handling Philosophy (March 2026 Update)

**Critical initialization operations** require verbose error handling with full context extraction.
**Non-critical operations** use simple error code logging without context extraction.
**Batch operations** use array-based pattern for independent optional features.

#### Critical Initialization (Verbose)

For subsystems that must succeed (audio, memory, STT, TTS, config):

```c
ethervox_result_t result = ethervox_audio_init(&audio);
if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    fprintf(stderr, "Failed to initialize audio: %s\n", 
            ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    if (ctx) {
        fprintf(stderr, "  at %s:%d in %s()\n", ctx->file, ctx->line, ctx->function);
    }
    return result;  // Cannot continue
}
```

#### Non-Critical Operations (Simple)

For optional features (tools, plugins, non-essential checks):

```c
ethervox_result_t result = ethervox_tool_register(&registry);
if (ethervox_is_error(result)) {
    fprintf(stderr, "[WARNING] Tool registration failed: Error %d occurred\n", result);
    // Continue - tool is optional
}
```

#### Batch Registration Pattern

For multiple independent optional operations:

```c
typedef struct {
    const char* name;
    ethervox_result_t result;
} tool_reg_result_t;

tool_reg_result_t tools[] = {
    {"Path Config", ethervox_path_config_register(&registry, &path_config)},
    {"Unit Conversion", ethervox_unit_conversion_register(&registry)},
    {"Conversation", ethervox_conversation_tools_register(&registry)},
    {NULL, ETHERVOX_SUCCESS}
};

int succeeded = 0, failed = 0;
for (int i = 0; tools[i].name != NULL; i++) {
    if (ethervox_is_success(tools[i].result)) {
        succeeded++;
    } else {
        failed++;
        fprintf(stderr, "[WARNING] Tool %s registration failed: Error %d occurred\n", 
               tools[i].name, tools[i].result);
    }
}

if (g_debug_enabled) {
    printf("Tools: %d registered successfully, %d failed\n", succeeded, failed);
}
```

#### Error Propagation with ETHERVOX_CHECK

For simple initialization sequences with no cleanup needed:

```c
ethervox_result_t initialize_system(void) {
    ETHERVOX_CHECK(ethervox_platform_init());
    ETHERVOX_CHECK(ethervox_audio_init(&g_audio));
    ETHERVOX_CHECK(ethervox_stt_init(&g_stt));
    
    return ETHERVOX_SUCCESS;
}
```

**Note:** Only use `ETHERVOX_CHECK()` when you have no allocated resources to clean up and failure is truly fatal.

## Logging Integration

### Log Levels

```c
typedef enum {
    ETHERVOX_LOG_LEVEL_TRACE = 0,
    ETHERVOX_LOG_LEVEL_DEBUG = 1,
    ETHERVOX_LOG_LEVEL_INFO = 2,
    ETHERVOX_LOG_LEVEL_WARN = 3,
    ETHERVOX_LOG_LEVEL_ERROR = 4,
    ETHERVOX_LOG_LEVEL_FATAL = 5,
    ETHERVOX_LOG_LEVEL_OFF = 6
} ethervox_log_level_t;
```

### Logging API

```c
// Set global log level
void ethervox_log_set_level(ethervox_log_level_t level);

// Log formatted message
void ethervox_log(ethervox_log_level_t level, const char* file, int line, 
                  const char* func, const char* fmt, ...);

// Log error with context
void ethervox_log_error_context(const ethervox_error_context_t* ctx);
```

### Logging Macros

```c
ETHERVOX_LOG_TRACE(...)
ETHERVOX_LOG_DEBUG(...)
ETHERVOX_LOG_INFO(...)
ETHERVOX_LOG_WARN(...)
ETHERVOX_LOG_ERROR(...)

// Log and return error
ETHERVOX_LOG_RETURN_ERROR(code, ...)
```

## Node.js/TypeScript Error Handling

### Custom Error Classes

```typescript
// Base error class
export class EthervoxError extends Error {
  public readonly code: string;
  public readonly timestamp: Date;
  public readonly context?: Record<string, unknown>;

  constructor(message: string, code: string, context?: Record<string, unknown>) {
    super(message);
    this.name = this.constructor.name;
    this.code = code;
    this.timestamp = new Date();
    this.context = context;
    Error.captureStackTrace(this, this.constructor);
  }

  toJSON() {
    return {
      name: this.name,
      message: this.message,
      code: this.code,
      timestamp: this.timestamp,
      context: this.context,
      stack: this.stack,
    };
  }
}
```

### Specific Error Types

```typescript
export class AudioError extends EthervoxError {
  constructor(message: string, context?: Record<string, unknown>) {
    super(message, 'AUDIO_ERROR', context);
  }
}

export class ModelError extends EthervoxError {
  constructor(message: string, context?: Record<string, unknown>) {
    super(message, 'MODEL_ERROR', context);
  }
}

export class PlatformError extends EthervoxError {
  constructor(message: string, context?: Record<string, unknown>) {
    super(message, 'PLATFORM_ERROR', context);
  }
}

export class NetworkError extends EthervoxError {
  constructor(message: string, context?: Record<string, unknown>) {
    super(message, 'NETWORK_ERROR', context);
  }
}

export class PluginError extends EthervoxError {
  constructor(message: string, context?: Record<string, unknown>) {
    super(message, 'PLUGIN_ERROR', context);
  }
}
```

### Result Type Pattern

```typescript
// Result type for operations that may fail
export type Result<T, E = EthervoxError> = 
  | { success: true; value: T }
  | { success: false; error: E };

// Helper to create success result
export function Ok<T>(value: T): Result<T, never> {
  return { success: true, value };
}

// Helper to create error result
export function Err<E extends EthervoxError>(error: E): Result<never, E> {
  return { success: false, error };
}
```

### TypeScript Usage Examples

#### Function Returning Result

```typescript
async function downloadModel(url: string): Promise<Result<string>> {
  try {
    const response = await fetch(url);
    if (!response.ok) {
      return Err(new NetworkError('Download failed', {
        url,
        status: response.status,
        statusText: response.statusText
      }));
    }
    return Ok(await response.text());
  } catch (err) {
    return Err(new NetworkError('Network request failed', {
      url,
      originalError: err
    }));
  }
}
```

#### Consuming Result

```typescript
const result = await downloadModel('https://example.com/model');
if (!result.success) {
  console.error('Download failed:', result.error.toJSON());
  return;
}
console.log('Model downloaded:', result.value);
```

#### Exception-Based Alternative

```typescript
try {
  await initializeAudioSystem();
} catch (err) {
  if (err instanceof AudioError) {
    console.error('Audio initialization failed:', err.context);
    // Handle audio-specific error
  } else {
    throw err; // Re-throw unknown errors
  }
}
```

## Migration Strategy

### Migration Status Summary

**Phase 1: Infrastructure** ✅ **COMPLETED** - All error handling infrastructure is implemented and tested (16/16 tests passing).

**Phase 2: Call Site Migration** ⚠️ **IN PROGRESS** - Critical audio bugs fixed, but systematic audit needed for remaining ~40+ locations.

**Critical Bug Pattern Discovered (January 28, 2026):**
During transcription feature debugging, we discovered that many call sites were incorrectly treating `ethervox_result_t` return values as data instead of error codes. This occurred because functions were migrated from returning `int` (data/count) to `ethervox_result_t` (error code) with data moved to out-parameters, but not all call sites were updated.

**Bug Pattern:**
```c
// ❌ WRONG: Treats ETHERVOX_SUCCESS (0) as "0 samples"
int samples_read = ethervox_audio_read(&runtime, &buffer);
if (samples_read > 0) { /* never executes */ }

// ✅ CORRECT: Check result code, use out-parameter for data
ethervox_result_t result = ethervox_audio_read(&runtime, &buffer);
if (ethervox_is_success(result) && buffer.size > 0) {
    int samples_read = (int)buffer.size;  // Get actual sample count
}
```

**Fixed Locations:**
- `src/plugins/voice_tools/voice_tools.c` (lines 130-240) - audio capture thread
- `src/audio/audio_recording.c` (line 214) - recording utility
- `src/dialogue/voice_conversation.c` (lines 310, 330, 565, 599) - voice conversation

**Remaining Work:**
Grep search (`int\s+\w+\s*=\s*ethervox_\w+\(`) found ~40 additional locations in:
- `src/platform/ethervox_android_core.c` (~15 instances) - Type consistency, logic correct
- `src/main.c` (~20 instances) - Type consistency, logic correct  
- Various test files - Need case-by-case review

Most remaining instances correctly check `result == 0` for success, but use `int` instead of `ethervox_result_t` for type consistency. Priority is fixing any that treat return values as data.

### Phase 1: Core Infrastructure ✅ COMPLETED

- [x] Create `include/ethervox/error.h` with error codes and API
- [x] Implement `src/common/error.c` with thread-local context
- [x] Create `include/ethervox/logging.h` with logging API
- [x] Implement `src/common/logging.c`
- [x] Update `CMakeLists.txt` to include new files
- [x] Write comprehensive unit tests for error handling (`tests/unit/test_error.c`)
- [x] Integrate tests into CTest with 100% pass rate
- [ ] Create TypeScript error classes in `src/common/errors.ts` (if needed)

### Phase 2: Critical Path (Weeks 2-3) ⚠️ IN PROGRESS

- [x] Fix critical call site bugs treating `ethervox_result_t` as data
  - [x] `src/audio/audio_recording.c` (line 214)
  - [x] `src/dialogue/voice_conversation.c` (lines 310, 330, 565, 599)
  - [x] `src/plugins/voice_tools/voice_tools.c` (lines 130-240)
- [x] Implement batch registration pattern for optional tools (March 30, 2026)
  - [x] `src/main.c` tool registration block (~54 lines → ~28 lines)
  - [x] Array-based pattern shows per-tool failures with error codes
  - [x] Summary reporting: "Tools: X registered successfully, Y failed"
- [ ] Migrate `src/audio/audio_core.c` to use new error codes
- [ ] Migrate platform HAL files (`src/platform/*.c`)
- [ ] Update plugin manager (`src/plugins/plugin_manager.c`)
- [ ] Systematic audit: Convert remaining `int result = ethervox_*()` to `ethervox_result_t`
- [ ] Apply batch pattern to other independent optional operations
- [ ] Ensure all tests pass with graceful degradation
- [ ] Update SDK wrappers to use new error types

### Phase 3: Full Migration (Weeks 4-6)

- [ ] Migrate STT module (`src/stt/`)
- [ ] Migrate wake word module (`src/wakeword/`)
- [ ] Migrate dialogue manager (`src/dialogue/`)
- [ ] Update dashboard API error handling
- [ ] Update all Node.js/TypeScript modules
- [ ] Update all documentation and examples

### Phase 4: Enforcement (Week 7)

- [ ] Add clang-tidy checks for error handling patterns
- [ ] Update CI/CD to enforce conventions
- [ ] Add migration guide to `docs/`
- [ ] Update `README.md` with error handling examples

## File Structure

```
include/ethervox/
├── error.h           # Error codes, context, and macros ✅ IMPLEMENTED
└── logging.h         # Logging API and macros ✅ IMPLEMENTED

src/common/
├── error.c           # Error implementation ✅ IMPLEMENTED
└── logging.c         # Logging implementation ✅ IMPLEMENTED

src/common/
└── errors.ts         # TypeScript error classes (optional)

tests/unit/
├── test_error.c      # C error handling tests ✅ COMPREHENSIVE (16 tests)
└── test_errors.ts    # TypeScript error tests (if needed)

docs/
└── errorhandling.md  # This document
```

## Implementation Files

### C Runtime

1. **`include/ethervox/error.h`**: Error codes, types, and macros
2. **`src/common/error.c`**: Error context management
3. **`include/ethervox/logging.h`**: Logging types and macros
4. **`src/common/logging.c`**: Logging implementation
5. **`tests/unit/test_error.c`**: Unit tests

### TypeScript

1. **`src/common/errors.ts`**: Error classes and Result type
2. **`tests/unit/test_errors.ts`**: Unit tests

## Best Practices

### C Runtime

**When to use verbose error handling (with context extraction):**
- Critical subsystem initialization (audio, STT, TTS, memory, config, platform)
- User-facing command execution (/load, /optimize, /train, etc.)
- Operations where failure requires cleanup or specific recovery actions
- Situations where detailed diagnostics help users fix the problem

**When to use simple error handling (error code only):**
- Optional tool/plugin registrations
- Non-critical features that gracefully degrade
- Operations inside debug-only blocks
- Batch operations where summary reporting is sufficient

**When to use batch registration pattern:**
- Multiple independent optional operations (tool registrations, plugin loads)
- When you want aggregate reporting (X succeeded, Y failed)
- Operations where individual failures don't affect others

**Core patterns:**

1. **Always check pointers**: Use `ETHERVOX_CHECK_PTR(ptr)` for all pointer arguments
2. **Propagate errors**: Use `ETHERVOX_CHECK(expr)` to automatically propagate errors (only when no cleanup needed)
3. **Log before returning**: Use `ETHERVOX_LOG_RETURN_ERROR` for errors that need logging
4. **Set context**: Use `ETHERVOX_ERROR_SET` or `ETHERVOX_RETURN_ERROR` to preserve context
5. **Clear errors**: Call `ethervox_error_clear()` after handling errors
6. **Thread safety**: Error context is thread-local where supported

### TypeScript

1. **Use specific error types**: Throw `AudioError`, `NetworkError`, etc., not generic `Error`
2. **Preserve context**: Always include relevant context in error constructors
3. **Result type for fallible operations**: Prefer `Result<T>` over exceptions for expected failures
4. **Async error handling**: Always wrap async operations in try-catch
5. **Error serialization**: Use `.toJSON()` for logging/transport

## Testing Requirements

### C Runtime ✅ ALL IMPLEMENTED

1. ✅ Test error code to string conversion (all 45+ error codes)
2. ✅ Test error context preservation across function calls
3. ✅ Test thread-local storage (Windows, GCC/Clang, C11)
4. ✅ Test macro expansion and usage (`ETHERVOX_CHECK`, `ETHERVOX_CHECK_PTR`, `ETHERVOX_RETURN_ERROR`)
5. ✅ Test error propagation chains (3-level deep call stacks)
6. ✅ Test edge cases (NULL messages, long messages, multiple errors)
7. ✅ Test realistic initialization scenarios
8. ✅ Test logging integration

**Test Results:** 16/16 tests passing, 100% success rate, integrated into CTest

### TypeScript

1. Test error class hierarchy
2. Test error serialization
3. Test Result type pattern
4. Test error context preservation
5. Test error handling in async operations

## Compatibility Notes

- **C89/C99**: Error handling works on all C standards; thread-local requires C11+
- **Platforms**: All supported platforms (Linux, Raspberry Pi, Windows, ESP32)
- **Node.js**: Requires Node.js 16+ for Error.captureStackTrace
- **TypeScript**: Requires TypeScript 4.5+ for const type parameters

## License

All error handling code must include the standard SPDX header:

```c
// SPDX-License-Identifier: CC-BY-NC-SA-4.0
```

## References

- `docs/mvp.md`: MVP scope and principles
- `docs/cicd-pipeline.md`: CI/CD requirements
- `.github/copilot-instructions.md`: Coding conventions