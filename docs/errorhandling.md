# Error Handling Specification

**Version:** 1.0  
**Status:** Draft  
**Last Updated:** October 31, 2025

## Overview

This document specifies a consistent error handling approach across the EtherVoxAI codebase, covering both the C runtime and Node.js/TypeScript implementations.

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

#### Basic Error Handling

```c
ethervox_result_t ethervox_audio_init(ethervox_audio_t* audio) {
    ETHERVOX_CHECK_PTR(audio);
    
    ethervox_result_t result = platform_audio_init();
    if (ethervox_is_error(result)) {
        ETHERVOX_LOG_ERROR("Platform audio init failed: %s", 
                          ethervox_error_string(result));
        return result;
    }
    
    return ETHERVOX_SUCCESS;
}
```

#### Error Propagation

```c
ethervox_result_t initialize_system(void) {
    // Automatically propagates error if init fails
    ETHERVOX_CHECK(ethervox_platform_init());
    ETHERVOX_CHECK(ethervox_audio_init(&g_audio));
    ETHERVOX_CHECK(ethervox_stt_init(&g_stt));
    
    return ETHERVOX_SUCCESS;
}
```

#### Caller Error Handling

```c
ethervox_result_t result = ethervox_audio_init(&audio);
if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    fprintf(stderr, "Error: %s at %s:%d in %s()\n", 
            ethervox_error_string(result), 
            ctx->file, ctx->line, ctx->function);
    // Handle error appropriately
}
```

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

### Phase 1: Core Infrastructure (Week 1)

- [ ] Create `include/ethervox/error.h` with error codes and API
- [ ] Implement `src/common/error.c` with thread-local context
- [ ] Create `include/ethervox/logging.h` with logging API
- [ ] Implement `src/common/logging.c`
- [ ] Create TypeScript error classes in `src/common/errors.ts`
- [ ] Update `CMakeLists.txt` to include new files
- [ ] Write unit tests for error handling (`tests/unit/test_error.c`)

### Phase 2: Critical Path (Weeks 2-3)

- [ ] Migrate `src/audio/audio_core.c` to use new error codes
- [ ] Migrate platform HAL files (`src/platform/*.c`)
- [ ] Update plugin manager (`src/plugins/plugin_manager.c`)
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
├── error.h           # Error codes, context, and macros
└── logging.h         # Logging API and macros

src/common/
├── error.c           # Error implementation
└── logging.c         # Logging implementation

src/common/
└── errors.ts         # TypeScript error classes

tests/unit/
├── test_error.c      # C error handling tests
└── test_errors.ts    # TypeScript error tests

docs/
└── error-handling-spec.md  # This document
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

1. **Always check pointers**: Use `ETHERVOX_CHECK_PTR(ptr)` for all pointer arguments
2. **Propagate errors**: Use `ETHERVOX_CHECK(expr)` to automatically propagate errors
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

### C Runtime

1. Test error code to string conversion
2. Test error context preservation across function calls
3. Test thread-local storage (if available)
4. Test macro expansion and usage
5. Test graceful degradation with missing hardware

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