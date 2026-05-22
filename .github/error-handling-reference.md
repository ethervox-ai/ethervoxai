# EthervoxAI Error Handling Reference

## Quick Reference for AI Assistants

This document provides a quick reference for the error handling system used throughout EthervoxAI. Use this as a guide when migrating functions or writing new code.

---

## Core Type: `ethervox_result_t`

All functions that can fail should return `ethervox_result_t` instead of `int`.

```c
typedef enum {
    ETHERVOX_SUCCESS = 0,           // Success (always 0)
    ETHERVOX_ERROR_* = negative     // All errors are negative
} ethervox_result_t;
```

---

## Essential Macros

### 1. **ETHERVOX_CHECK_PTR(ptr)** - NULL Pointer Validation
```c
ethervox_result_t my_function(void* data) {
    ETHERVOX_CHECK_PTR(data);  // Returns ETHERVOX_ERROR_NULL_POINTER if NULL
    // ... rest of function
}
```

### 2. **ETHERVOX_RETURN_ERROR(code, msg)** - Return Error with Context
```c
if (something_failed) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate buffer");
}
```

### 3. **ETHERVOX_CHECK(expr)** - Propagate Errors
```c
// Calls function and immediately returns if it returns an error
ETHERVOX_CHECK(some_function_that_returns_result());

// Equivalent to:
ethervox_result_t result = some_function_that_returns_result();
if (ethervox_is_error(result)) {
    return result;
}
```

### 4. **return ETHERVOX_SUCCESS** - Success Return
```c
// Replace: return 0;
// With:    return ETHERVOX_SUCCESS;
```

---

## Helper Functions

### Check Result Status
```c
if (ethervox_is_success(result)) {
    // Success path
}

if (ethervox_is_error(result)) {
    // Error path
}
```

### Get Error Information
```c
const char* error_msg = ethervox_error_string(result);
const ethervox_error_context_t* ctx = ethervox_error_get_context();
```

---

## Error Code Categories (80+ codes total)

### General (-1 to -99)
- `ETHERVOX_SUCCESS` (0)
- `ETHERVOX_ERROR_NULL_POINTER` (-2)
- `ETHERVOX_ERROR_INVALID_ARGUMENT` (-3)
- `ETHERVOX_ERROR_OUT_OF_MEMORY` (-4)
- `ETHERVOX_ERROR_NOT_INITIALIZED` (-5)
- `ETHERVOX_ERROR_BUFFER_TOO_SMALL` (-9)
- `ETHERVOX_ERROR_NOT_IMPLEMENTED` (-10)
- `ETHERVOX_ERROR_NOT_FOUND` (-12)
- `ETHERVOX_ERROR_NOT_SUPPORTED` (-8)

### Platform (-100 to -199)
- `ETHERVOX_ERROR_PLATFORM_INIT` (-100)
- `ETHERVOX_ERROR_HAL_NOT_FOUND` (-101)
- `ETHERVOX_ERROR_GPIO_FAILURE` (-102)

### Audio (-200 to -299)
- `ETHERVOX_ERROR_AUDIO_INIT` (-200)
- `ETHERVOX_ERROR_AUDIO_DEVICE_NOT_FOUND` (-201)
- `ETHERVOX_ERROR_AUDIO_BUFFER_OVERFLOW` (-203)

### STT (-300 to -349)
- `ETHERVOX_ERROR_STT_INIT` (-300)
- `ETHERVOX_ERROR_STT_MODEL_NOT_FOUND` (-301)
- `ETHERVOX_ERROR_STT_TRANSCRIPTION_FAILED` (-304)

### TTS (-350 to -399)
- `ETHERVOX_ERROR_TTS_INIT` (-350)
- `ETHERVOX_ERROR_TTS_SYNTHESIS_FAILED` (-352)
- `ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED` (-354)

### Wake Word (-400 to -449)
- `ETHERVOX_ERROR_WAKEWORD_INIT` (-400)
- `ETHERVOX_ERROR_WAKEWORD_DETECTION_FAILED` (-402)

### LLM (-450 to -499)
- `ETHERVOX_ERROR_LLM_INIT` (-450)
- `ETHERVOX_ERROR_LLM_MODEL_LOAD_FAILED` (-452)
- `ETHERVOX_ERROR_LLM_INFERENCE_FAILED` (-453)
- `ETHERVOX_ERROR_LLM_CONTEXT_OVERFLOW` (-454)

### Plugin (-500 to -549)
- `ETHERVOX_ERROR_PLUGIN_NOT_FOUND` (-500)
- `ETHERVOX_ERROR_PLUGIN_INIT` (-501)
- `ETHERVOX_ERROR_PLUGIN_EXECUTION` (-502)

### Governor/Tool (-550 to -599)
- `ETHERVOX_ERROR_GOVERNOR_INIT` (-550)
- `ETHERVOX_ERROR_TOOL_NOT_FOUND` (-551)
- `ETHERVOX_ERROR_TOOL_EXECUTION_FAILED` (-552)

### Network (-600 to -649)
- `ETHERVOX_ERROR_DOWNLOAD_FAILED` (-604)
- `ETHERVOX_ERROR_NETWORK_CONNECTION_FAILED` (-607)

### Memory (-650 to -699)
- `ETHERVOX_ERROR_MEMORY_INIT` (-650)
- `ETHERVOX_ERROR_MEMORY_STORE_FAILED` (-652)

### File I/O (-700 to -749)
- `ETHERVOX_ERROR_FILE_NOT_FOUND` (-700)
- `ETHERVOX_ERROR_FILE_READ` (-701)
- `ETHERVOX_ERROR_FILE_WRITE` (-702)
- `ETHERVOX_ERROR_FILE_DELETE_FAILED` (-705)
- `ETHERVOX_ERROR_PATH_INVALID` (-709)

### Configuration (-750 to -799)
- `ETHERVOX_ERROR_CONFIG_LOAD_FAILED` (-751)
- `ETHERVOX_ERROR_CONFIG_SAVE_FAILED` (-752)
- `ETHERVOX_ERROR_CONFIG_PARSE_ERROR` (-753)

### Dialogue (-800 to -849)
- `ETHERVOX_ERROR_DIALOGUE_INIT` (-800)
- `ETHERVOX_ERROR_CONVERSATION_FAILED` (-801)
- `ETHERVOX_ERROR_VOICE_TRAINING_FAILED` (-802)

---

## Migration Pattern

### Before (Old Style)
```c
int my_function(void* data) {
    if (!data) {
        return -1;
    }
    
    if (allocate_failed) {
        return -2;
    }
    
    int result = some_other_function();
    if (result != 0) {
        return result;
    }
    
    return 0;
}
```

### After (New Style)
```c
ethervox_result_t my_function(void* data) {
    ETHERVOX_CHECK_PTR(data);
    
    if (allocate_failed) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Allocation failed");
    }
    
    ETHERVOX_CHECK(some_other_function());
    
    return ETHERVOX_SUCCESS;
}
```

### Updating Callers
```c
// Old style:
if (my_function(ptr) != 0) {
    // Handle error
}

// New style:
if (ethervox_is_error(my_function(ptr))) {
    // Handle error
}

// Or:
if (ethervox_is_success(my_function(ptr))) {
    // Success path
}
```

---

## Common Patterns

### 1. Function with Multiple Error Paths
```c
ethervox_result_t complex_function(const char* input, int size, void* output) {
    ETHERVOX_CHECK_PTR(input);
    ETHERVOX_CHECK_PTR(output);
    
    if (size <= 0) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_INVALID_ARGUMENT, "Size must be positive");
    }
    
    void* buffer = malloc(size);
    if (!buffer) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate buffer");
    }
    
    // Propagate errors from other functions
    ethervox_result_t result = process_data(buffer, size);
    if (ethervox_is_error(result)) {
        free(buffer);
        return result;
    }
    
    free(buffer);
    return ETHERVOX_SUCCESS;
}
```

### 2. Platform-Specific Error Handling
```c
ethervox_result_t platform_specific_init(void) {
#ifdef __ANDROID__
    const char* path = ethervox_get_android_files_dir();
    if (!path) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_PLATFORM_INIT, "Android files dir unavailable");
    }
#elif defined(__linux__)
    const char* home = getenv("HOME");
    if (!home) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_PLATFORM_INIT, "HOME not set");
    }
#endif
    
    return ETHERVOX_SUCCESS;
}
```

### 3. HAL Function Pointers
```c
// HAL functions should return ethervox_result_t
typedef struct {
    ethervox_result_t (*init)(void);
    ethervox_result_t (*gpio_write)(uint32_t pin, bool state);
    void (*cleanup)(void);  // Cleanup functions can remain void
} my_hal_t;
```

---

## Testing Error Paths

```c
void test_error_handling(void) {
    // Test NULL pointer
    assert(ethervox_is_error(my_function(NULL)));
    
    // Test success
    assert(ethervox_is_success(my_function(valid_ptr)));
    
    // Check error context
    my_function(NULL);
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    assert(ctx != NULL);
    assert(ctx->code == ETHERVOX_ERROR_NULL_POINTER);
}
```

---

## Important Notes

1. **Always include error.h**: `#include "ethervox/error.h"`
2. **Update headers**: Change function signatures from `int` to `ethervox_result_t`
3. **Update callers**: Replace `!= 0` checks with `ethervox_is_error()`
4. **Provide context**: Use descriptive messages in `ETHERVOX_RETURN_ERROR`
5. **Propagate errors**: Use `ETHERVOX_CHECK()` to bubble up errors
6. **Don't use printf for errors**: Use `ETHERVOX_RETURN_ERROR` which sets context
7. **Thread-safety**: Error context is thread-local where supported

---

## Where to Find More Info

- **Full error code list**: `include/ethervox/error.h`
- **Implementation**: `src/common/error.c`
- **Tests**: `tests/unit/test_error.c`
- **Migration checklist**: `MIGRATION_CHECKLIST.md`
- **Detailed migration guide**: `ERROR_MIGRATION_SUMMARY.md`

---

**Last Updated:** 2026-01-03
**Version:** 1.2 (80+ error codes)
