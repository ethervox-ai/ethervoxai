// SPDX-License-Identifier: CC-BY-NC-SA-4.0

// Windows-specific defines must come before any Windows headers
#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600  // Windows Vista or later for GetTickCount64
#endif
#include <windows.h>
#endif

#include "ethervox/error.h"
#include <string.h>
#include <time.h>

// Thread-local storage for error context (if available)
#if defined(_MSC_VER)
// MSVC extension - use this first for Windows
static __declspec(thread) ethervox_error_context_t g_error_context = {0};
#define HAS_THREAD_LOCAL 1
#elif defined(__GNUC__) || defined(__clang__)
// GCC/Clang extension (including MinGW)
static __thread ethervox_error_context_t g_error_context = {0};
#define HAS_THREAD_LOCAL 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#include <threads.h>
static thread_local ethervox_error_context_t g_error_context = {0};
#define HAS_THREAD_LOCAL 1
#else
// Fallback to global for platforms without thread_local
static ethervox_error_context_t g_error_context = {0};
#define HAS_THREAD_LOCAL 0
#endif

const char* ethervox_error_string(ethervox_result_t result) {
    switch (result) {
        case ETHERVOX_SUCCESS: return "Success";
        
        // General errors
        case ETHERVOX_ERROR_GENERIC: return "Generic error";
        case ETHERVOX_ERROR_NULL_POINTER: return "NULL pointer";
        case ETHERVOX_ERROR_INVALID_ARGUMENT: return "Invalid argument";
        case ETHERVOX_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case ETHERVOX_ERROR_NOT_INITIALIZED: return "Not initialized";
        case ETHERVOX_ERROR_ALREADY_INITIALIZED: return "Already initialized";
        case ETHERVOX_ERROR_TIMEOUT: return "Timeout";
        case ETHERVOX_ERROR_NOT_SUPPORTED: return "Not supported";
        case ETHERVOX_ERROR_BUFFER_TOO_SMALL: return "Buffer too small";
        case ETHERVOX_ERROR_NOT_IMPLEMENTED: return "Not implemented";
        case ETHERVOX_ERROR_FAILED: return "Operation failed";
        case ETHERVOX_ERROR_NOT_FOUND: return "Not found";
        
        // Platform/HAL errors
        case ETHERVOX_ERROR_PLATFORM_INIT: return "Platform initialization failed";
        case ETHERVOX_ERROR_HAL_NOT_FOUND: return "HAL not found";
        case ETHERVOX_ERROR_GPIO_FAILURE: return "GPIO operation failed";
        case ETHERVOX_ERROR_HARDWARE_NOT_AVAILABLE: return "Hardware not available";
        
        // Audio errors
        case ETHERVOX_ERROR_AUDIO_INIT: return "Audio initialization failed";
        case ETHERVOX_ERROR_AUDIO_DEVICE_NOT_FOUND: return "Audio device not found";
        case ETHERVOX_ERROR_AUDIO_FORMAT_UNSUPPORTED: return "Audio format unsupported";
        case ETHERVOX_ERROR_AUDIO_BUFFER_OVERFLOW: return "Audio buffer overflow";
        case ETHERVOX_ERROR_AUDIO_BUFFER_UNDERFLOW: return "Audio buffer underflow";
        case ETHERVOX_ERROR_AUDIO_DEVICE_BUSY: return "Audio device busy";
        
        // STT errors
        case ETHERVOX_ERROR_STT_INIT: return "STT initialization failed";
        case ETHERVOX_ERROR_STT_MODEL_NOT_FOUND: return "STT model not found";
        case ETHERVOX_ERROR_STT_PROCESSING: return "STT processing failed";
        
        // Wake word errors
        case ETHERVOX_ERROR_WAKEWORD_INIT: return "Wake word initialization failed";
        case ETHERVOX_ERROR_WAKEWORD_MODEL_NOT_FOUND: return "Wake word model not found";
        
        // Plugin errors
        case ETHERVOX_ERROR_PLUGIN_NOT_FOUND: return "Plugin not found";
        case ETHERVOX_ERROR_PLUGIN_INIT: return "Plugin initialization failed";
        case ETHERVOX_ERROR_PLUGIN_EXECUTION: return "Plugin execution failed";
        case ETHERVOX_ERROR_PLUGIN_MAX_REACHED: return "Maximum plugins reached";
        
        // Network/API errors
        case ETHERVOX_ERROR_NETWORK: return "Network error";
        case ETHERVOX_ERROR_API_CALL: return "API call failed";
        case ETHERVOX_ERROR_API_RESPONSE: return "Invalid API response";
        case ETHERVOX_ERROR_API_RATE_LIMIT: return "API rate limit exceeded";
        
        // File I/O errors
        case ETHERVOX_ERROR_FILE_NOT_FOUND: return "File not found";
        case ETHERVOX_ERROR_FILE_READ: return "File read error";
        case ETHERVOX_ERROR_FILE_WRITE: return "File write error";
        case ETHERVOX_ERROR_FILE_PERMISSION: return "File permission denied";
        
        default: return "Unknown error";
    }
}

void ethervox_error_set_context(
    ethervox_result_t code,
    const char* message,
    const char* file,
    int line,
    const char* function
) {
    g_error_context.code = code;
    g_error_context.message = message;
    g_error_context.file = file;
    g_error_context.line = line;
    g_error_context.function = function;
    
    // Platform-specific timestamp
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        g_error_context.timestamp_ms = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    } else {
        g_error_context.timestamp_ms = 0;
    }
#elif defined(_WIN32)
    // Windows implementation
    g_error_context.timestamp_ms = (uint64_t)GetTickCount64();
#else
    // Fallback: use time() which has 1-second resolution
    g_error_context.timestamp_ms = (uint64_t)time(NULL) * 1000;
#endif
}

const ethervox_error_context_t* ethervox_error_get_context(void) {
    return &g_error_context;
}

void ethervox_error_clear(void) {
    memset(&g_error_context, 0, sizeof(g_error_context));
}