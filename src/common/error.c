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
        case ETHERVOX_ERROR_STT_MODEL_LOAD_FAILED: return "STT model load failed";
        case ETHERVOX_ERROR_STT_TRANSCRIPTION_FAILED: return "STT transcription failed";
        case ETHERVOX_ERROR_STT_LANGUAGE_NOT_SUPPORTED: return "STT language not supported";
        case ETHERVOX_ERROR_STT_AUDIO_FORMAT_INVALID: return "STT audio format invalid";
        
        // TTS errors
        case ETHERVOX_ERROR_TTS_INIT: return "TTS initialization failed";
        case ETHERVOX_ERROR_TTS_MODEL_NOT_FOUND: return "TTS model not found";
        case ETHERVOX_ERROR_TTS_SYNTHESIS_FAILED: return "TTS synthesis failed";
        case ETHERVOX_ERROR_TTS_VOICE_NOT_FOUND: return "TTS voice not found";
        case ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED: return "TTS phonemization failed";
        case ETHERVOX_ERROR_TTS_AUDIO_OUTPUT_FAILED: return "TTS audio output failed";
        case ETHERVOX_ERROR_TTS_LANGUAGE_NOT_SUPPORTED: return "TTS language not supported";
        case ETHERVOX_ERROR_TTS_TEXT_TOO_LONG: return "TTS text too long";
        
        // Wake word errors
        case ETHERVOX_ERROR_WAKEWORD_INIT: return "Wake word initialization failed";
        case ETHERVOX_ERROR_WAKEWORD_MODEL_NOT_FOUND: return "Wake word model not found";
        case ETHERVOX_ERROR_WAKEWORD_DETECTION_FAILED: return "Wake word detection failed";
        case ETHERVOX_ERROR_WAKEWORD_TEMPLATE_RECORDING_FAILED: return "Wake word template recording failed";
        
        // LLM errors
        case ETHERVOX_ERROR_LLM_INIT: return "LLM initialization failed";
        case ETHERVOX_ERROR_LLM_MODEL_NOT_FOUND: return "LLM model not found";
        case ETHERVOX_ERROR_LLM_MODEL_LOAD_FAILED: return "LLM model load failed";
        case ETHERVOX_ERROR_LLM_INFERENCE_FAILED: return "LLM inference failed";
        case ETHERVOX_ERROR_LLM_CONTEXT_OVERFLOW: return "LLM context overflow";
        case ETHERVOX_ERROR_LLM_BACKEND_NOT_SUPPORTED: return "LLM backend not supported";
        case ETHERVOX_ERROR_LLM_TOKENIZATION_FAILED: return "LLM tokenization failed";
        case ETHERVOX_ERROR_LLM_GENERATION_TIMEOUT: return "LLM generation timeout";
        
        // Plugin errors
        case ETHERVOX_ERROR_PLUGIN_NOT_FOUND: return "Plugin not found";
        case ETHERVOX_ERROR_PLUGIN_INIT: return "Plugin initialization failed";
        case ETHERVOX_ERROR_PLUGIN_EXECUTION: return "Plugin execution failed";
        case ETHERVOX_ERROR_PLUGIN_MAX_REACHED: return "Maximum plugins reached";
        case ETHERVOX_ERROR_PLUGIN_INVALID_MANIFEST: return "Plugin invalid manifest";
        case ETHERVOX_ERROR_PLUGIN_DEPENDENCY_MISSING: return "Plugin dependency missing";
        
        // Governor/Tool errors
        case ETHERVOX_ERROR_GOVERNOR_INIT: return "Governor initialization failed";
        case ETHERVOX_ERROR_TOOL_NOT_FOUND: return "Tool not found";
        case ETHERVOX_ERROR_TOOL_EXECUTION_FAILED: return "Tool execution failed";
        case ETHERVOX_ERROR_TOOL_REGISTRY_FULL: return "Tool registry full";
        case ETHERVOX_ERROR_TOOL_INVALID_ARGUMENTS: return "Tool invalid arguments";
        case ETHERVOX_ERROR_TOOL_TIMEOUT: return "Tool execution timeout";
        case ETHERVOX_ERROR_MANIFEST_PARSE_ERROR: return "Manifest parse error";
        case ETHERVOX_ERROR_MANIFEST_VALIDATION_FAILED: return "Manifest validation failed";
        
        // Network/API errors
        case ETHERVOX_ERROR_NETWORK: return "Network error";
        case ETHERVOX_ERROR_API_CALL: return "API call failed";
        case ETHERVOX_ERROR_API_RESPONSE: return "Invalid API response";
        case ETHERVOX_ERROR_API_RATE_LIMIT: return "API rate limit exceeded";
        case ETHERVOX_ERROR_DOWNLOAD_FAILED: return "Download failed";
        case ETHERVOX_ERROR_DOWNLOAD_TIMEOUT: return "Download timeout";
        case ETHERVOX_ERROR_DOWNLOAD_CHECKSUM_MISMATCH: return "Download checksum mismatch";
        case ETHERVOX_ERROR_NETWORK_CONNECTION_FAILED: return "Network connection failed";
        
        // Memory system errors
        case ETHERVOX_ERROR_MEMORY_INIT: return "Memory system initialization failed";
        case ETHERVOX_ERROR_MEMORY_SEARCH_FAILED: return "Memory search failed";
        case ETHERVOX_ERROR_MEMORY_STORE_FAILED: return "Memory store failed";
        case ETHERVOX_ERROR_MEMORY_EXPORT_FAILED: return "Memory export failed";
        case ETHERVOX_ERROR_MEMORY_IMPORT_FAILED: return "Memory import failed";
        case ETHERVOX_ERROR_MEMORY_ARCHIVE_FAILED: return "Memory archive failed";
        case ETHERVOX_ERROR_MEMORY_CORRUPTION: return "Memory corruption detected";
        
        // File I/O errors
        case ETHERVOX_ERROR_FILE_NOT_FOUND: return "File not found";
        case ETHERVOX_ERROR_FILE_READ: return "File read error";
        case ETHERVOX_ERROR_FILE_WRITE: return "File write error";
        case ETHERVOX_ERROR_FILE_PERMISSION: return "File permission denied";
        case ETHERVOX_ERROR_FILE_EXISTS: return "File already exists";
        case ETHERVOX_ERROR_FILE_DELETE_FAILED: return "File delete failed";
        case ETHERVOX_ERROR_DIRECTORY_NOT_FOUND: return "Directory not found";
        case ETHERVOX_ERROR_DIRECTORY_CREATE_FAILED: return "Directory create failed";
        case ETHERVOX_ERROR_PATH_TOO_LONG: return "Path too long";
        case ETHERVOX_ERROR_PATH_INVALID: return "Path invalid";
        
        // Configuration/Settings errors
        case ETHERVOX_ERROR_CONFIG_INIT: return "Configuration initialization failed";
        case ETHERVOX_ERROR_CONFIG_LOAD_FAILED: return "Configuration load failed";
        case ETHERVOX_ERROR_CONFIG_SAVE_FAILED: return "Configuration save failed";
        case ETHERVOX_ERROR_CONFIG_PARSE_ERROR: return "Configuration parse error";
        case ETHERVOX_ERROR_CONFIG_VALIDATION_FAILED: return "Configuration validation failed";
        case ETHERVOX_ERROR_SETTING_NOT_FOUND: return "Setting not found";
        case ETHERVOX_ERROR_SETTING_INVALID_VALUE: return "Setting invalid value";
        
        // Dialogue/Conversation errors
        case ETHERVOX_ERROR_DIALOGUE_INIT: return "Dialogue initialization failed";
        case ETHERVOX_ERROR_CONVERSATION_FAILED: return "Conversation failed";
        case ETHERVOX_ERROR_VOICE_TRAINING_FAILED: return "Voice training failed";
        case ETHERVOX_ERROR_SESSION_NOT_FOUND: return "Session not found";
        case ETHERVOX_ERROR_SESSION_EXPIRED: return "Session expired";
        
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